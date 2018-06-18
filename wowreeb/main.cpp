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

#include "resource.h"
#include "NotifyIconMgr.hpp"
#include "NotifyIcon.hpp"
#include "Config.hpp"
#include "Injector.hpp"
#include "PicoSHA2/picosha2.h"

#include <thread>
#include <chrono>
#include <string>
#include <locale>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <vector>
#include <Windows.h>
#include <tchar.h>
#include <ImageHlp.h>

#pragma comment (lib, "imagehlp.lib")

namespace fs = std::experimental::filesystem;

namespace
{
std::wstring make_wstring(const std::string &in)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(in);
}

void Launch(const Config::ConfigEntry &entry, bool clearWDB, bool verifyChecksum)
{
    // step 1: ensure exe exists
    if (!fs::exists(entry.Path))
        throw std::runtime_error("Exe file not found");

    if (verifyChecksum)
    {
        // step 2: verify checksum, if present
        for (auto i = 0u; i < sizeof(entry.SHA256); ++i)
        {
            if (!entry.SHA256[i])
                continue;

            std::ifstream exe(entry.Path, std::ios::binary);

            std::vector<std::uint8_t> hash(picosha2::k_digest_size);
            picosha2::hash256(std::istreambuf_iterator<char>(exe), std::istreambuf_iterator<char>(), hash.begin(), hash.end());

            if (!!memcmp(entry.SHA256, &hash[0], hash.size()))
                throw std::runtime_error("Checksum failed");

            break;
        }
    }

    // step 3: ensure our dll exists
    if (!fs::exists(entry.OurDll))
        throw std::runtime_error("wowreeb.dll not found");

    // step 4: ensure native dll exists, if present
    if (!entry.NativeDll.empty() && !fs::exists(entry.NativeDll))
        throw std::runtime_error("Native DLL not found");

    // step 5: ensure clr dll exists, if present
    if (!entry.CLRDll.empty() && !fs::exists(entry.CLRDll))
        throw std::runtime_error("CLR DLL not found");

    // step 6: reset cache if requested
    if (clearWDB)
    {
        try
        {
            auto const wdb1 = entry.Path.parent_path() / "WDB";
            auto const wdb2 = entry.Path.parent_path() / "Cache" / "WDB";

            if (fs::exists(wdb1))
                fs::remove_all(wdb1);

            if (fs::exists(wdb2))
                fs::remove_all(wdb2);
        }
        catch (fs::filesystem_error const &)
        {
            throw std::runtime_error("Failed to clear cache folder");
        }
    }

    // step 7: ensure 32 bit launcher creating 32 bit process, or 64 bit launcher creating 64 bit process
    DWORD themType;

    if (!::GetBinaryTypeA(entry.Path.string().c_str(), &themType))
        throw std::runtime_error("GetBinaryType failed");

    const bool them32 = themType == SCS_32BIT_BINARY;
    const bool us32 = sizeof(void *) == 4;

    // if the architectures match, inject and stop
    if (us32 == them32)
    {
        ::Inject(entry.Path,
            entry.OurDll, entry.OurMethod,
            entry.AuthServer, entry.Console, entry.Fov,
            entry.NativeDll, entry.NativeMethod,
            entry.CLRDll, make_wstring(entry.CLRTypeName), make_wstring(entry.CLRMethodName));
        return;
    }

    auto const hmod = ::GetModuleHandle(nullptr);
    TCHAR path[MAX_PATH];
    ::GetModuleFileName(hmod, path, MAX_PATH);

    const fs::path exePath(path);
    auto const exe = exePath.parent_path() / (us32 ? "wowreeb64.exe" : "wowreeb32.exe");

    if (!fs::exists(exe))
    {
        std::stringstream msg;
        msg << "Launcher " << exe << " not found";
        throw std::runtime_error(msg.str());
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    si.cb = sizeof(si);

    std::vector<char> buff(entry.Name.begin(), entry.Name.end());
    buff.push_back(0);

    if (!::CreateProcessA(exe.string().c_str(), &buff[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        throw std::runtime_error("CreateProcess failed");
}
}

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow
)
{
    Config config(_T("config.xml"));

    try
    {
        auto const cmdline = ::GetCommandLineA();

        if (strlen(cmdline) > 0)
        {
            for (auto const &entry : config.entries)
            {
                if (entry.Name == cmdline)
                {
                    // no need to verify checksum a second time
                    Launch(entry, config.clearWDB, false);
                    return EXIT_SUCCESS;
                }
            }
        }

        NotifyIconMgr iconMgr(hInstance);

        auto icon = iconMgr.Create(LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON1)), _T("Wowreeb Launcher"));

        for (auto const &entry : config.entries)
        {
            icon->AddMenu(
#ifdef UNICODE
                make_wstring(entry.Name).c_str(),
#else
                entry.Name.c_str(),
#endif
                [&entry, clearWDB = config.clearWDB] ()
            {
                try
                {
                    Launch(entry, clearWDB, true);
                }
                catch (std::exception const &e)
                {
                    MessageBoxA(nullptr, e.what(), "Launch Exception", 0);
                }
            });
        }

        icon->AddMenu(_T("-"));

        bool shutdown = false;

        icon->AddMenu(_T("Exit"), [&shutdown] () { shutdown = true; });

        // TODO: might as well use this thread to poll the config file for changes
        while (!shutdown)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    catch (std::exception const &e)
    {
        MessageBoxA(nullptr, e.what(), "Exception", 0);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}