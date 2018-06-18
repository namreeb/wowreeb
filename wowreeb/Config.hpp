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

#pragma once

#include "../PicoSHA2/picosha2.h"

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <tchar.h>

// eventually the 'experimental' will be dropped
namespace fs = std::experimental::filesystem;

class Config
{
private:
    fs::path _path;
    fs::path _ourDll;

public:
    struct ConfigEntry
    {
        std::string Name;

        fs::path Path;
        std::uint8_t SHA256[picosha2::k_digest_size];

        std::string AuthServer;

        bool Console;

        float Fov;

        fs::path OurDll;
        std::string OurMethod;

        fs::path NativeDll;
        std::string NativeMethod;

        fs::path CLRDll;
        std::string CLRTypeName;
        std::string CLRMethodName;
    };

    // this isn't thread safe, but what could go wrong?
    std::vector<ConfigEntry> entries;

    // when true, remove entire WDB folder before launching the client
    bool clearWDB;

    Config(const TCHAR *filename);

    void Reload();
};