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

#include <Windows.h>
#include <metahost.h>
#include <CorError.h>

#include <cassert>
#include <sstream>

#pragma comment(lib, "mscoree.lib")

#define MB(s) MessageBox(nullptr, s, nullptr, MB_OK)

// this function is executed in the context of the wow process and assumes will block until the specified method returns
// the intended usage is for the invoked method to create its own thread in order to persist and then return immediately
extern "C" __declspec(dllexport) unsigned int CLRLoad(wchar_t *dll, wchar_t *typeName, wchar_t *methodName)
{
    if (!dll || !typeName || !methodName)
        return EXIT_FAILURE;

    ICLRMetaHostPolicy *metaHost = nullptr;
    ICLRRuntimeInfo *runtimeInfo = nullptr;
    ICLRRuntimeHost *clrHost = nullptr;

    auto hr = CLRCreateInstance(CLSID_CLRMetaHostPolicy, IID_ICLRMetaHostPolicy, reinterpret_cast<LPVOID *>(&metaHost));

    if (FAILED(hr))
    {
        MB(L"Could not create instance of ICLRMetaHost");
        return EXIT_FAILURE;
    }

    // Use this if your lazy. Just make sure to change the ICLRMetaHost
    // to an ICLRMetaHostPolicy instead.
    DWORD pcchVersion;
    DWORD dwConfigFlags;

    hr = metaHost->GetRequestedRuntime(METAHOST_POLICY_HIGHCOMPAT, dll, nullptr, nullptr, &pcchVersion, nullptr, nullptr, &dwConfigFlags, IID_ICLRRuntimeInfo,
        reinterpret_cast<LPVOID *>(&runtimeInfo));

    if (FAILED(hr))
    {
        MB(L"Could not get an instance of ICLRRuntimeInfo");
        return EXIT_FAILURE;
    }

    hr = runtimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_ICLRRuntimeHost, reinterpret_cast<LPVOID *>(&clrHost));

    if (FAILED(hr))
    {
        MB(L"Could not get an instance of ICLRRuntimeHost!");
        return EXIT_FAILURE;
    }

    hr = clrHost->Start();

    if (FAILED(hr))
    {
        MB(L"Failed to start the CLR!");
        return EXIT_FAILURE;
    }

    // assumed to return zero on success
    DWORD dwRet = 0;
    hr = clrHost->ExecuteInDefaultAppDomain(dll, typeName, methodName, L"", &dwRet);

    if (FAILED(hr))
    {
        std::wstringstream str;

        str << "Failed to execute in default app domain:\n";

        switch (hr)
        {
        case HOST_E_CLRNOTAVAILABLE:
            str << "CLR not available";
            break;

        case HOST_E_TIMEOUT:
            str << "Call timed out";
            break;

        case HOST_E_NOT_OWNER:
            str << "Caller does not own lock";
            break;

        case HOST_E_ABANDONED:
            str << "An event was cancelled while a blocked thread or fiber was waiting on it";
            break;

        case E_FAIL:
            str << "Unspecified catastrophic failure";
            break;

        default:
            str << "Result is 0x" << std::hex << std::uppercase << hr << std::dec;
            break;
        }

        MB(str.str().c_str());

        return EXIT_FAILURE;
    }

    return static_cast<unsigned int>(dwRet);
}