/*
  MIT License

  Copyright (c) 2018-2023 namreeb http://github.com/namreeb legal@namreeb.org

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

#include <Shlwapi.h>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "mincore.lib")

namespace
{
#define THROW_IF(expr, message)        \
    if (expr)                          \
    {                                  \
        throw std::exception(message); \
    }

unsigned int GetBuild()
{
    wchar_t filename[255];
    DWORD size = sizeof(filename) / sizeof(filename[0]);

    THROW_IF(!QueryFullProcessImageName(::GetCurrentProcess(), 0,
                                        reinterpret_cast<LPWSTR>(&filename),
                                        &size),
             "QueryFullProcessImageName failed");

    size = GetFileVersionInfoSize(filename, nullptr);

    THROW_IF(!size, "Bad VersionInfo size");

    std::vector<std::uint8_t> versionInfo(size);

    THROW_IF(!::GetFileVersionInfo(filename, 0, size, &versionInfo[0]),
             "GetFileVersionInfo failed");

    VS_FIXEDFILEINFO* verInfo;
    UINT length;

    THROW_IF(!::VerQueryValue(&versionInfo[0], L"\\",
                              reinterpret_cast<LPVOID*>(&verInfo), &length),
             "VerQueryValue failed");
    THROW_IF(verInfo->dwSignature != 0xFEEF04BD, "Incorrect version signature");

    return static_cast<unsigned int>(verInfo->dwFileVersionLS & 0xFFFF);
}
} // namespace

#define BUILD_CLASSIC 5875
#define BUILD_TBC     8606
#define BUILD_WOTLK   12340
#define BUILD_CATA    15595

// this function is executed in the context of the wow process
extern "C" __declspec(dllexport) unsigned int Load(GameSettings* settings)
{
    if (!settings->AuthServer[0])
        return EXIT_FAILURE;

    auto const build = GetBuild();

    switch (build)
    {
        case BUILD_CLASSIC:
            Classic::ApplyClientInitHook(settings);
            break;
        case BUILD_TBC:
            TBC::ApplyClientInitHook(settings);
            break;
        case BUILD_WOTLK:
            WOTLK::ApplyClientInitHook(settings);
            break;
        case BUILD_CATA:
            if (sizeof(void*) == 4)
                Cata32::ApplyClientInitHook(settings);
            else
                Cata64::ApplyClientInitHook(settings);
            break;
        default:
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}