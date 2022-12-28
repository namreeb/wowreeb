/*
    MIT License

    Copyright (c) 2018-2019 namreeb http://github.com/namreeb legal@namreeb.org

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
#include "InputWindow.hpp"
#include "tiny-AES-c/aes.hpp"

#include <thread>
#include <chrono>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <vector>
#include <memory>
#include <Windows.h>
#include <tchar.h>
#include <ImageHlp.h>

#pragma comment (lib, "imagehlp.lib")

extern std::wstring make_wstring(const std::string &in);

namespace
{
static constexpr char EnvEntry[] = "WOWREEB_ENTRY";
static constexpr char EnvKey[] = "WOWREEB_KEY";

std::vector<std::uint8_t> ReadFile(const fs::path &file)
{
    std::ifstream fd(file, std::ios::binary | std::ios::ate);
    const size_t size = static_cast<size_t>(fd.tellg());
    fd.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> result(size);

    if (size > 0)
        fd.read(reinterpret_cast<char *>(&result[0]), result.size());

    return std::move(result);
}

void Launch(const ConfigEntry &entry, bool clearWDB, const std::string &key)
{
    // step 1: ensure exe exists
    if (!fs::exists(entry.Path))
        throw std::runtime_error("Exe file not found");

    // step 2: determine whether the launcher and target binary are running in 32 bit mode
    DWORD themType;

    if (!::GetBinaryTypeA(entry.Path.string().c_str(), &themType))
        throw std::runtime_error("GetBinaryType failed");

    const bool them32 = themType == SCS_32BIT_BINARY;
    const bool us32 = sizeof(void *) == 4;

    // step 3: verify checksum, if present, only if the platform matches
    if (them32 == us32 && !!entry.SHA256[0])
    {
        auto const data = ReadFile(entry.Path);
        std::vector<std::uint8_t> hash(picosha2::k_digest_size);

        picosha2::hash256(data, hash);

        if (!!memcmp(entry.SHA256, &hash[0], hash.size()))
            throw std::runtime_error("Checksum failed");
    }

    // step 4: ensure our dll exists
    if (!fs::exists(entry.OurDll))
        throw std::runtime_error("wowreeb.dll not found");

    // step 5: ensure native dlls exists, if present
    for (auto const &dll : entry.NativeDlls)
        if (!fs::exists(dll.first))
            throw std::runtime_error("Native DLL not found");

    // step 6: ensure clr dll exists, if present
    if (!entry.CLRDll.empty() && !fs::exists(entry.CLRDll))
        throw std::runtime_error("CLR DLL not found");

    // step 7: reset cache if requested
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

    // step 8: if the architectures match, inject and stop
    if (us32 == them32)
    {
        ::Inject(entry);
        return;
    }

    // if we reach here it is because we need to run the other launcher executable
    // so that we can inject the right dll.  find the launcher and setup the environment...

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

    std::string envEntry(EnvEntry);
    envEntry += "=" + entry.Name;

    std::string envKey(EnvKey);
    envKey += "=" + key;

    // step 9: setup environment so the other launcher executable knows not to load the GUI
    _putenv(envEntry.c_str());
    _putenv(envKey.c_str());

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    // step 10: run the other launcher executable
    if (!::CreateProcessA(exe.string().c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        throw std::runtime_error("CreateProcess failed");
}

template <typename T>
T RoundUp(T val, T mul)
{
    return mul * (1 + ((val - 1) / mul));
}

std::string DataToHex(const std::vector<std::uint8_t> &data)
{
    std::stringstream str;

    str << std::hex << std::nouppercase << std::setfill('0');

    for (auto const &c : data)
        str << std::setw(2) << static_cast<std::uint32_t>(c);

    return str.str();
}

bool ReadAndEncryptPassword(HINSTANCE hInstance, int nCmdShow, std::string &key, std::string &result)
{
    // if there are not already credentials in the config file, no key will have been input yet.  therefore we must read one.
    if (key.empty())
    {
        InputWindow keyWindow(hInstance, nCmdShow, "Enter your desired key...");

        key = keyWindow.ReadKey();

        // window was aborted
        if (key.empty())
            return false;
    }

    if (key.length() > AES_KEYLEN)
        return false;

    InputWindow passWindow(hInstance, nCmdShow, "Enter password to encrypt...");

    // the only purpose of this is to give us a way to determine if the password is decrypted successfully later
    auto newPass = Config::Magic + passWindow.ReadKey();

    // window was aborted
    if (newPass.empty())
        return false;

    std::uint8_t keyRaw[AES_KEYLEN];
    ::memset(keyRaw, 0, sizeof(keyRaw));
    ::memcpy(keyRaw, key.c_str(), key.length());

    AES_ctx ctx;
    ZeroMemory(&ctx, sizeof(ctx));
    AES_init_ctx_iv(&ctx, keyRaw, Config::Iv);

    std::vector<std::uint8_t> buffer(RoundUp(newPass.length(), static_cast<size_t>(AES_BLOCKLEN)));

    ::memcpy(&buffer[0], newPass.c_str(), newPass.length());

    // PKCS7 padding
    const std::uint8_t pad = static_cast<std::uint8_t>(buffer.size() - newPass.length());

    if (pad > 0)
        ::memset(&buffer[newPass.length()], pad, pad);

    AES_CBC_encrypt_buffer(&ctx, &buffer[0], buffer.size());

    result = DataToHex(buffer);

    return true;
}

bool SetClipboardText(const std::string &text)
{
    if (!::OpenClipboard(nullptr))
        return false;

    if (!::EmptyClipboard())
        return false;

    auto hmem = ::GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
    ::memcpy(::GlobalLock(hmem), text.c_str(), text.length() + 1);
    ::GlobalUnlock(hmem);

    if (!::SetClipboardData(CF_TEXT, hmem))
        return false;

    if (!::CloseClipboard())
        return false;

    return true;
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
        config.Reload();
    }
    catch (std::runtime_error const &e)
    {
        ::MessageBoxA(nullptr, e.what(), "Configuration Parsing Error", MB_ICONERROR);
        return EXIT_FAILURE;
    }

    if (auto const envKey = getenv(EnvKey))
    {
        if (!config.VerifyKey(envKey))
        {
            ::MessageBoxA(nullptr, "Incorrect key", "Failure", MB_ICONERROR);
            return EXIT_FAILURE;
        }
    }
    else
    {
        bool needAuthentication = false;

        for (auto const &entry : config.entries)
        {
            if (!entry.Username.empty() && !entry.Password.empty())
            {
                needAuthentication = true;
                break;
            }
        }

        // if some settings provide credentials, we must authenticate the user before we proceed
        if (needAuthentication)
        {
            do
            {
                InputWindow authWindow(hInstance, nCmdShow, "Enter your wowreeb key...");

                auto const key = authWindow.ReadKey();

                // window was aborted
                if (key.empty())
                    return EXIT_FAILURE;

                if (config.VerifyKey(key))
                    break;

                ::MessageBoxA(nullptr, "Incorrect key", "Failure", MB_ICONERROR);
            } while (true);
        }
    }

    try
    {
        if (auto const envEntry = getenv(EnvEntry))
        {
            for (auto const &entry : config.entries)
            {
                if (entry.Name == envEntry)
                {
                    Launch(entry, config.clearWDB, config.key);
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
                [&entry, clearWDB = config.clearWDB, &key = config.key] ()
            {
                try
                {
                    Launch(entry, clearWDB, key);
                }
                catch (std::exception const &e)
                {
                    MessageBoxA(nullptr, e.what(), "Launch Exception", MB_ICONERROR);
                }
            });
        }

        icon->AddMenu(_T("-"));

        bool shutdown = false;

        icon->AddMenu(_T("Encrypt Password"),
            [hInstance, nCmdShow, &key = config.key] ()
            {
                std::string pw;
                if (!ReadAndEncryptPassword(hInstance, nCmdShow, key, pw))
                    return;

                if (!SetClipboardText(pw))
                {
                    ::MessageBoxA(nullptr, "Failed to set clipboard data", "Error", MB_ICONERROR);
                    return;
                }

                ::MessageBoxA(nullptr, "Encrypted password copied to clipboard", "Success!", MB_ICONINFORMATION);
            });

        icon->AddMenu(_T("Exit"), [&shutdown] () { shutdown = true; });

        // TODO: might as well use this thread to poll the config file for changes
        while (!shutdown)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    catch (std::exception const &e)
    {
        ::MessageBoxA(nullptr, e.what(), "Exception", MB_ICONERROR);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}