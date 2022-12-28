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

#pragma once

#include "PicoSHA2/picosha2.h"
#include "tiny-AES-c/aes.hpp"

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <tchar.h>

namespace fs = std::filesystem;

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

    std::vector<std::pair<fs::path, std::string>> NativeDlls;

    fs::path CLRDll;
    std::string CLRTypeName;
    std::string CLRMethodName;

    std::string Username;
    std::string Password;
};

class Config
{
private:
    fs::path _path;
    fs::path _ourDll;

public:
    static constexpr char Magic[] = "WOWREEB:";
    static constexpr std::uint8_t Iv[AES_BLOCKLEN] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xFA, 0xCE, 0x01, 0x02, 0x03, 0x04, 0x08, 0x07, 0x06, 0x05 };

    // this isn't thread safe, but what could go wrong?
    std::vector<ConfigEntry> entries;

    std::string key;

    // when true, remove entire WDB folder before launching the client
    bool clearWDB;

    Config(const TCHAR *filename);

    void Reload();

    bool VerifyKey(const std::string &key);
};