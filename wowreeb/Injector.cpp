/*
    MIT License

    Copyright (c) 2018 namreeb http://github.com/namreeb legal@namreeb.org

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

*/

#include "Injector.hpp"
#include "Config.hpp"

#include <hadesmem/injector.hpp>
#include <hadesmem/call.hpp>
#include <hadesmem/alloc.hpp>
#include <hadesmem/write.hpp>
#include <hadesmem/find_procedure.hpp>
#include <hadesmem/acl.hpp>

#include <Shlwapi.h>

#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <thread>
#include <filesystem>

#pragma comment (lib, "shlwapi.lib")
#pragma comment (lib, "asmjit.lib")
#pragma comment (lib, "udis86.lib")

namespace
{
void EjectionPoll(hadesmem::Process process, HMODULE dll, PVOID remoteBuffer)
{
    auto const remoteLocation = reinterpret_cast<bool *>(static_cast<PCHAR>(remoteBuffer) + offsetof(GameSettings, LoadComplete));

    do
    {
        try
        {
            auto const complete = hadesmem::Read<bool>(process, remoteLocation);

            if (complete)
            {
                // free the remotely allocated heap space
                hadesmem::Free(process, remoteBuffer);

                // eject the dll
                try
                {
                    hadesmem::CloneDaclsToRemoteProcess(process.GetId());
                    hadesmem::FreeDll(process, dll);
                }
                catch (std::exception const& e)
                {
                    std::stringstream str;

                    str << "Ejection error: \n" << boost::diagnostic_information(e);

                    ::MessageBoxA(nullptr, str.str().c_str(), "Ejection failure", MB_ICONWARNING);
                }

                return;
            }
        }
        catch (std::exception const &e)
        {
            std::stringstream str;

            str << "EjectionPoll silent exception:\n\n" << boost::diagnostic_information(e);

            ::MessageBoxA(nullptr, str.str().c_str(), "DEBUG", MB_ICONERROR);

            // can happen if the process is terminated before initialization has completed.  silently abort this thread..
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (true);
}
}

extern std::wstring make_wstring(const std::string &in);

unsigned int Inject(const ConfigEntry &config)
{
    if (config.AuthServer.length() >= sizeof(GameSettings::AuthServer))
        throw std::runtime_error("Authentication server address too long");

    std::vector<std::wstring> createArgs;

    if (config.Console)
        createArgs.emplace_back(L"-console");

    try
    {
        auto const injectData = hadesmem::CreateAndInject(config.Path, L"", createArgs.cbegin(), createArgs.cend(), config.OurDll,
            "", hadesmem::InjectFlags::kPathResolution | hadesmem::InjectFlags::kKeepSuspended);

        auto const process = injectData.GetProcess();
        const hadesmem::Module module(process, injectData.GetModule());

        GameSettings gameSettings;

        ::memset(&gameSettings, 0, sizeof(gameSettings));

        // include null terminator
        ::memcpy(gameSettings.AuthServer, config.AuthServer.c_str(), config.AuthServer.length() + 1);

        if (config.Fov > 0.1f)
        {
            gameSettings.FoVSet = true;
            gameSettings.FoV = config.Fov;
        }

        if (!config.Username.empty() && !config.Password.empty())
        {
            // TODO: Move this check into config parsing
            if (config.Username.length() >= sizeof(GameSettings::Username))
                throw std::runtime_error("Username is too long");

            if (config.Password.length() >= sizeof(GameSettings::Password))
                throw std::runtime_error("Password is too long");

            gameSettings.CredentialsSet = true;
            ::memcpy(gameSettings.Username, config.Username.c_str(), config.Username.length() + 1);
            ::memcpy(gameSettings.Password, config.Password.c_str(), config.Password.length() + 1);
        }

        // write the game settings into wow's memory
        auto const remoteBuffer = hadesmem::Alloc(process, sizeof(gameSettings));
        hadesmem::Write(process, remoteBuffer, &gameSettings, 1);

        // get the address of our load function
        auto const func = reinterpret_cast<void(*)(PVOID)>(hadesmem::FindProcedure(process, module, config.OurMethod));

        // call our load function with a pointer to our realm list
        hadesmem::Call(process, func, hadesmem::CallConv::kDefault, remoteBuffer);

        // if a native dll was specified, inject and call given function
        if (!config.NativeDll.empty())
        {
            auto const nativeHandle = hadesmem::InjectDll(process, config.NativeDll, hadesmem::InjectFlags::kNone);
            auto const result = hadesmem::CallExport(process, nativeHandle, config.NativeMethod);

            if (!!result.GetReturnValue())
                ::MessageBoxA(nullptr, "Native DLL load failed", "Injection failure", MB_ICONERROR);
        }

        // if a CLR domain manager dll was specified, create a CLR instance in the remote process
        if (!config.CLRDll.empty())
        {
            fs::path domainDllPath(config.CLRDll);

            // if the path is relative, make it relative to the wow executable
            if (domainDllPath.is_relative())
                domainDllPath = config.Path.parent_path() / domainDllPath;

            auto const domainFullPath = domainDllPath.wstring();

            // allocate enough memory in the remote process to write the path to the dll, type name, and method name
            auto const clrPathBufferSize = sizeof(wchar_t) * (domainFullPath.length() + 1);
            auto const clrPathBuffer = hadesmem::Alloc(process, clrPathBufferSize);

            auto const clrTypeName = make_wstring(config.CLRTypeName);

            auto const typeNameBufferSize = sizeof(wchar_t) * (clrTypeName.length() + 1);
            auto const typeNameBuffer = hadesmem::Alloc(process, typeNameBufferSize);

            auto const clrMethodName = make_wstring(config.CLRMethodName);

            auto const methodNameBufferSize = sizeof(wchar_t) * (clrMethodName.length() + 1);
            auto const methodNameBuffer = hadesmem::Alloc(process, methodNameBufferSize);

            // write the path to the dll, the type name, and the method name
            hadesmem::Write(process, clrPathBuffer, domainFullPath.c_str(), clrPathBufferSize);
            hadesmem::Write(process, typeNameBuffer, clrTypeName.c_str(), typeNameBufferSize);
            hadesmem::Write(process, methodNameBuffer, clrMethodName.c_str(), methodNameBufferSize);

            // call our CLR load in the remote process
            auto const clrLoad = reinterpret_cast<unsigned int(*)(PVOID, PVOID, PVOID)>(hadesmem::FindProcedure(process, module, "CLRLoad"));
            auto const result = hadesmem::Call(process, clrLoad, hadesmem::CallConv::kDefault, clrPathBuffer, typeNameBuffer, methodNameBuffer);
            
            if (!!result.GetReturnValue())
                ::MessageBoxA(nullptr, "CLRLoad failed", "Injection failure", MB_ICONERROR);

            // free the remote buffers
            hadesmem::Free(process, clrPathBuffer);
            hadesmem::Free(process, typeNameBuffer);
            hadesmem::Free(process, methodNameBuffer);
        }

        // allow WoW to continue loading
        injectData.ResumeThread();

        // create a thread whose purpose is to monitor our remote heap allocated space for the completion byte to be toggled
        std::thread poll(EjectionPoll, process, injectData.GetModule(), remoteBuffer);
        poll.detach();

        return static_cast<unsigned int>(injectData.GetProcess().GetId());
    }
    catch (std::exception const &e)
    {
        std::stringstream str;

        str << "Injection error: " << std::endl;
        str << boost::diagnostic_information(e) << std::endl;

        ::MessageBoxA(nullptr, str.str().c_str(), "Injection failure", MB_ICONERROR);
    }

    return 0;
}
