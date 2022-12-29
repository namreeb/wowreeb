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

#include "Config.hpp"

#include "rapidxml/rapidxml.hpp"
#include "tiny-AES-c/aes.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tchar.h>
#include <vector>

namespace
{
std::vector<char> ReadFile(const fs::path& file)
{
    std::ifstream f(file.string());
    f.seekg(0, std::ios::end);
    const size_t size = static_cast<size_t>(f.tellg());

    std::vector<char> buff(size);
    f.seekg(0);
    f.read(&buff[0], size);

    return buff;
}

std::uint8_t HexCharToByte(char b)
{
    if (b >= '0' && b <= '9')
        return b - '0';
    if (b >= 'A' && b <= 'Z')
        return b - 'A' + 10;
    if (b >= 'a' && b <= 'z')
        return b - 'a' + 10;

    throw std::invalid_argument("Unable to convert hex character to byte");
}

std::uint8_t HexCharsToByte(char a, char b)
{
    auto const aVal = HexCharToByte(a);
    auto const bVal = HexCharToByte(b);

    return (aVal << 4) | bVal;
}
} // namespace

Config::Config(const TCHAR* filename)
{
    // first try the filename as-is.  this will handle absolute paths and paths
    // relative to the current directory
    fs::path f(filename);

    TCHAR filebuff[1024];
    auto const len = ::GetModuleFileName(nullptr, filebuff, ARRAYSIZE(filebuff));

    if (!len)
        throw std::runtime_error("GetModuleFileName failed");

    auto const parent = fs::path(filebuff).parent_path();

    // if the file is not found, try relative to the directory of the executable
    if (!fs::exists(f))
    {
        f = parent / filename;

        if (!fs::exists(f))
            throw std::runtime_error("Config file not found");
    }

    _path = f;

    const bool us32 = sizeof(void*) == 4;

    _ourDll = parent / (us32 ? "wowreeb32.dll" : "wowreeb64.dll");
}

void Config::Reload()
{
    entries.clear();

    auto text = ReadFile(_path);

    rapidxml::xml_document<> doc;
    doc.parse<0>(&text[0]);

    auto const root = doc.first_node();

    if (!root)
        throw std::runtime_error("No root node in config file");

    // XML is case sensitive
    if (std::string(root->name()) != "wowreeb")
        throw std::runtime_error("No wowreeb node found in config file");

    for (auto n = root->first_node(); !!n; n = n->next_sibling())
    {
        const std::string name(n->name());

        if (name == "Realm")
        {
            ConfigEntry ins;

            ins.OurDll = _ourDll;
            ins.OurMethod = "Load";
            ZeroMemory(&ins.SHA256, sizeof(ins.SHA256));
            ins.Console = false;
            ins.Fov = 0.f;

            for (auto r = n->first_attribute(); !!r; r = r->next_attribute())
            {
                const std::string rname(r->name());

                if (rname == "Name")
                    ins.Name = r->value();
                else
                {
                    std::stringstream str;
                    str << "Unexpected " << name << " attribute \"" << rname
                        << "\"";
                    throw std::runtime_error(str.str().c_str());
                }
            }

            if (ins.Name.empty())
                throw std::runtime_error("Realm entries must have a name");

            for (auto c = n->first_node(); !!c; c = c->next_sibling())
            {
                const std::string cname(c->name());

                if (cname == "Exe")
                {
                    for (auto r = c->first_attribute(); !!r;
                         r = r->next_attribute())
                    {
                        const std::string rname(r->name());

                        if (rname == "Path")
                            ins.Path = r->value();
                        else if (rname == "SHA256")
                        {
                            if (r->value_size() != 2 * picosha2::k_digest_size)
                            {
                                std::stringstream str;
                                str << "Exe SHA256 for \"" << ins.Name
                                    << "\" is wrong size.  Should be "
                                    << picosha2::k_digest_size << " bytes";
                                throw std::runtime_error(str.str().c_str());
                            }

                            const std::string hash(r->value());

                            for (auto i = 0u; i < sizeof(ins.SHA256); ++i)
                                ins.SHA256[i] =
                                    HexCharsToByte(hash[i * 2], hash[i * 2 + 1]);
                        }
                        else
                        {
                            std::stringstream str;
                            str << "Unexpected " << cname << " attribute \""
                                << rname << "\"";
                            throw std::runtime_error(str.str().c_str());
                        }
                    }
                }
                else if (cname == "AuthServer")
                {
                    for (auto r = c->first_attribute(); !!r;
                         r = r->next_attribute())
                    {
                        const std::string rname(r->name());

                        if (rname == "Host")
                            ins.AuthServer = r->value();
                        else
                        {
                            std::stringstream str;
                            str << "Unexpected " << cname << " attribute \""
                                << rname << "\"";
                            throw std::runtime_error(str.str().c_str());
                        }
                    }
                }
                else if (cname == "Console")
                {
                    std::string consoleValue;

                    for (auto r = c->first_attribute(); !!r;
                         r = r->next_attribute())
                    {
                        const std::string rname(r->name());

                        if (rname == "Value")
                        {
                            consoleValue = std::string(r->value());

                            std::transform(consoleValue.begin(),
                                           consoleValue.end(),
                                           consoleValue.begin(), ::toupper);
                        }
                        else
                        {
                            std::stringstream str;
                            str << "Unexpected " << cname << " attribute \""
                                << rname << "\"";
                            throw std::runtime_error(str.str().c_str());
                        }
                    }

                    ins.Console = consoleValue == "1" || consoleValue == "TRUE";
                }
                else if (cname == "Fov")
                {
                    for (auto r = c->first_attribute(); !!r;
                         r = r->next_attribute())
                    {
                        const std::string rname(r->name());

                        if (rname == "Value")
                        {
                            try
                            {
                                ins.Fov = std::stof(r->value());
                            }
                            catch (std::invalid_argument const&)
                            {
                                std::stringstream str;
                                str << "Failed to parse Fov string \""
                                    << r->value() << "\" for \"" << ins.Name
                                    << "\"";
                                throw std::runtime_error(str.str().c_str());
                            }
                        }
                        else
                        {
                            std::stringstream str;
                            str << "Unexpected " << cname << " attribute \""
                                << rname << "\"";
                            throw std::runtime_error(str.str().c_str());
                        }
                    }
                }
                else if (cname == "CLR")
                {
                    for (auto r = c->first_attribute(); !!r;
                         r = r->next_attribute())
                    {
                        const std::string rname(r->name());

                        if (rname == "Path")
                            ins.CLRDll = r->value();
                        else if (rname == "Type")
                            ins.CLRTypeName = r->value();
                        else if (rname == "Method")
                            ins.CLRMethodName = r->value();
                        else
                        {
                            std::stringstream str;
                            str << "Unexpected " << cname << " attribute \""
                                << rname << "\"";
                            throw std::runtime_error(str.str().c_str());
                        }
                    }
                }
                else if (cname == "DLL")
                {
                    fs::path dll_path {};
                    std::string dll_method {};

                    for (auto r = c->first_attribute(); !!r;
                         r = r->next_attribute())
                    {
                        const std::string rname(r->name());

                        if (rname == "Path")
                            dll_path = r->value();
                        else if (rname == "Method")
                            dll_method = r->value();
                        else
                        {
                            std::stringstream str;
                            str << "Unexpected " << cname << " attribute \""
                                << rname << "\"";
                            throw std::runtime_error(str.str().c_str());
                        }
                    }

                    if (!dll_path.empty())
                        ins.NativeDlls.emplace_back(dll_path, dll_method);
                }
                else if (cname == "Credentials")
                {
                    for (auto r = c->first_attribute(); !!r;
                         r = r->next_attribute())
                    {
                        const std::string rname(r->name());

                        if (rname == "Username")
                            ins.Username = r->value();
                        else if (rname == "Password")
                            ins.Password = r->value();
                        else
                        {
                            std::stringstream str;
                            str << "Unexpected " << cname << " attribute \""
                                << rname << "\"";
                            throw std::runtime_error(str.str().c_str());
                        }
                    }
                }
                else
                {
                    std::stringstream str;
                    str << "Unexpected " << name << " node \"" << cname << "\"";
                    throw std::runtime_error(str.str().c_str());
                }
            }

            entries.push_back(ins);
        }
        else if (name == "Config")
        {
            std::string configName;
            std::string configValue;

            for (auto a = n->first_attribute(); !!a; a = a->next_attribute())
            {
                const std::string aname(a->name());

                if (aname == "Name")
                    configName = std::string(a->value());
                else if (aname == "Value")
                {
                    configValue = std::string(a->value());

                    std::transform(configValue.begin(), configValue.end(),
                                   configValue.begin(), ::toupper);
                }
                else
                {
                    std::stringstream str;
                    str << "Unexpected " << name << " attribute \"" << name
                        << "\"";
                    throw std::runtime_error(str.str().c_str());
                }
            }

            if (configName == "ClearWDB")
                clearWDB = configValue == "1" || configValue == "TRUE";
            else
            {
                std::stringstream str;
                str << "Unrecognized Config entry \"" << configName << "\"";
                throw std::runtime_error(str.str().c_str());
            }
        }
        else
        {
            std::stringstream str;
            str << "Unexpected node \"" << name << "\"";
            throw std::runtime_error(str.str().c_str());
        }
    }
}

bool Config::VerifyKey(const std::string& key)
{
    std::uint8_t keyRaw[AES_KEYLEN];
    ::memset(keyRaw, 0, sizeof(keyRaw));
    ::memcpy(keyRaw, key.c_str(), key.length());

    for (auto& entry : entries)
    {
        if (entry.Password.empty())
            continue;

        std::vector<std::uint8_t> buffer(entry.Password.length() / 2);

        // encrypted buffers must be mutile of AES_BLOCKLEN
        if (buffer.size() % AES_BLOCKLEN)
            return false;

        // convert hex string into raw data inside the vector
        for (auto i = 0u; i < buffer.size(); ++i)
            buffer[i] =
                HexCharsToByte(entry.Password[i * 2], entry.Password[i * 2 + 1]);

        AES_ctx ctx;
        ZeroMemory(&ctx, sizeof(ctx));
        AES_init_ctx_iv(&ctx, keyRaw, Iv);

        AES_CBC_decrypt_buffer(&ctx, &buffer[0], buffer.size());

        // remove PKCS7 padding
        auto const pad = buffer[buffer.size() - 1];

        // values greater than this cannot be padding
        if (pad < AES_BLOCKLEN)
        {
            // this should not be possible
            if (pad >= buffer.size())
                return false;

            for (auto i = 1; i <= pad; ++i)
            {
                if (buffer[buffer.size() - i] != pad)
                    return false;

                buffer[buffer.size() - i] = 0;
            }
        }

        // check for magic string to verify correctness
        auto constexpr magicLen = sizeof(Magic) - 1;

        const std::string pass(reinterpret_cast<const char*>(&buffer[0]));

        if (pass.substr(0, magicLen) != Magic)
            return false;

        entry.Password = pass.substr(magicLen);
    }

    this->key = key;

    return true;
}