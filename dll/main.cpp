/*
    MIT License

    Copyright (c) 2017 namreeb http://github.com/namreeb legal@namreeb.org

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

#include "InitializeHooks.hpp"

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
#include <experimental/filesystem>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "asmjit.lib")

namespace
{
void EjectionPoll(hadesmem::Process process, HMODULE dll, PVOID remoteBuffer, size_t offset)
{
    auto const readyByte = static_cast<PVOID>(static_cast<PCHAR>(remoteBuffer) + offset);

    do
    {
        try
        {
            auto const complete = hadesmem::Read<std::uint8_t>(process, readyByte);

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

                    str << "Ejection error: " << std::endl;
                    str << boost::diagnostic_information(e) << std::endl;

                    MessageBoxA(nullptr, str.str().c_str(), "Ejection failure", 0);
                }

                return;
            }
        }
        catch (std::exception const &)
        {
            // can happen if the process is terminated before initialization has completed.  silently abort this thread..
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (true);
}

#define THROW_IF(expr, message) if (expr) { throw std::exception(message); }

unsigned int GetBuild()
{
    wchar_t filename[255];
    DWORD size = sizeof(filename) / sizeof(filename[0]);

    THROW_IF(!QueryFullProcessImageName(::GetCurrentProcess(), 0, reinterpret_cast<LPWSTR>(&filename), &size), "QueryFullProcessImageName failed");

    size = GetFileVersionInfoSize(filename, nullptr);

    THROW_IF(!size, "Bad VersionInfo size");

    std::vector<std::uint8_t> versionInfo(size);

    THROW_IF(!::GetFileVersionInfo(filename, 0, size, &versionInfo[0]), "GetFileVersionInfo failed");

    VS_FIXEDFILEINFO *verInfo;
    UINT length;

    THROW_IF(!::VerQueryValue(&versionInfo[0], L"\\", reinterpret_cast<LPVOID *>(&verInfo), &length), "VerQueryValue failed");
    THROW_IF(verInfo->dwSignature != 0xFEEF04BD, "Incorrect version signature");

    return static_cast<unsigned int>(verInfo->dwFileVersionLS & 0xFFFF);
}
}

// this function is executed in the context of the launcher process
extern "C" __declspec(dllexport) unsigned int Inject(const wchar_t *exePath, const wchar_t *dllPath, const char *authServer, float fov, const wchar_t *domainDll)
{
    const std::vector<std::wstring> createArgs { L"-console" };
    const std::wstring exe(exePath);
    const std::wstring dll(dllPath);

    try
    {
        auto const injectData = hadesmem::CreateAndInject(exe, L"", createArgs.cbegin(), createArgs.cend(), dll, "",
            hadesmem::InjectFlags::kPathResolution | hadesmem::InjectFlags::kKeepSuspended);

        auto const process = injectData.GetProcess();
        const hadesmem::Module module(process, injectData.GetModule());

        auto const authServerLength = strlen(authServer);

        // we need enough memory to write the AuthServer, with null terminator, and a bool which the
        // initialization routine will toggle once it has finished, signaling that we can eject the loader
        std::vector<std::uint8_t> buffer(authServerLength + 2, 0);
        std::copy(authServer, authServer + authServerLength, buffer.begin());

        // write the authServer into wow's memory
        auto const remoteBuffer = hadesmem::Alloc(process, buffer.size());
        hadesmem::Write(process, remoteBuffer, &buffer[0], buffer.size());;

        // get the address of our load function
        auto const func = reinterpret_cast<unsigned int(*)(PVOID, float)>(hadesmem::FindProcedure(process, module, "Load"));

        // call our load function with a pointer to our realm list
        hadesmem::Call(process, func, hadesmem::CallConv::kDefault, remoteBuffer, fov);

        // if a CLR domain manager dll was specified, create a CLR instance in the remote process
        if (!!domainDll && !!*domainDll)
        {
            std::experimental::filesystem::path domainDllPath(domainDll);

            // if the path is relative, make it relative to the wow executable
            if (domainDllPath.is_relative())
                domainDllPath = std::experimental::filesystem::path(exePath).append(domainDllPath);

            auto const domainFullPath = domainDllPath.wstring();

            // allocate enough memory in the remote process to write the path to the dll
            auto const clrPathBufferSize = sizeof(wchar_t) * (domainFullPath.length() + 1);
            auto const clrPathBuffer = hadesmem::Alloc(process, clrPathBufferSize);

            // write the path to the dll
            hadesmem::Write(process, clrPathBuffer, domainFullPath.c_str(), clrPathBufferSize);

            // call our CLR load in the remote process
            auto const clrLoad = reinterpret_cast<unsigned int(*)(PVOID)>(hadesmem::FindProcedure(process, module, "CLRLoad"));
            hadesmem::Call(process, clrLoad, hadesmem::CallConv::kDefault, clrPathBuffer);

            // free the remote buffer
            hadesmem::Free(process, clrPathBuffer);
        }

        // allow WoW to continue loading
        injectData.ResumeThread();

        // create a thread whose purpose is to monitor our remote heap allocated space for the completion byte to be toggled
        std::thread poll(EjectionPoll, process, injectData.GetModule(), remoteBuffer, buffer.size() - 1);
        poll.detach();

        return static_cast<unsigned int>(injectData.GetProcess().GetId());
    }
    catch (std::exception const &e)
    {
        std::stringstream str;

        str << "Injection error: " << std::endl;
        str << boost::diagnostic_information(e) << std::endl;

        MessageBoxA(nullptr, str.str().c_str(), "Injection failure", 0);
    }

    return 0;
}

#define BUILD_CLASSIC   5875
#define BUILD_TBC       8606
#define BUILD_WOTLK     12340
#define BUILD_CATA      15595

// this function is executed in the context of the wow process
extern "C" __declspec(dllexport) unsigned int Load(char *authServer, float fov)
{
    if (!authServer)
        return EXIT_FAILURE;

    auto const build = GetBuild();

    switch (build)
    {
        case BUILD_CLASSIC:
            Classic::ApplyClientInitHook(authServer, fov);
            break;
        case BUILD_TBC:
            TBC::ApplyClientInitHook(authServer, fov);
            break;
        case BUILD_WOTLK:
            WOTLK::ApplyClientInitHook(authServer, fov);
            break;
        case BUILD_CATA:
            Cata::ApplyClientInitHook(authServer, fov);
            break;
        default:
            assert(false);
            break;
    }

    return EXIT_SUCCESS;
}