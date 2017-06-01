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

#include <vector>
#include <thread>
#include <cassert>
#include <cstdint>

#pragma comment(lib, "mscoree.lib")

#define MB(s) MessageBox(nullptr, s, nullptr, MB_OK)

namespace
{
std::thread gCLRThread;

unsigned __stdcall ThreadMain(const wchar_t *dllLocation)
{
    ICLRMetaHostPolicy *metaHost = nullptr;
    ICLRRuntimeInfo *runtimeInfo = nullptr;
    ICLRRuntimeHost *clrHost = nullptr;

    HRESULT hr = CLRCreateInstance(CLSID_CLRMetaHostPolicy, IID_ICLRMetaHostPolicy, reinterpret_cast<LPVOID *>(&metaHost));

    if (FAILED(hr))
    {
        MB(L"Could not create instance of ICLRMetaHost");
        return EXIT_FAILURE;
    }

    // Use this if your lazy. Just make sure to change the ICLRMetaHost
    // to an ICLRMetaHostPolicy instead.
    DWORD pcchVersion;
    DWORD dwConfigFlags;

    hr = metaHost->GetRequestedRuntime(METAHOST_POLICY_HIGHCOMPAT, dllLocation, nullptr, nullptr, &pcchVersion, nullptr, nullptr, &dwConfigFlags, IID_ICLRRuntimeInfo,
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

    // Execute the Main func in the domain manager, this will block indefinitely.  Hence why we're in our own thread
    // TODO: Add parameters for type and method names
    DWORD dwRet = 0;
    hr = clrHost->ExecuteInDefaultAppDomain(dllLocation, L"wcs.DomainManager.EntryPoint", L"Main", L"", &dwRet);

    delete[] dllLocation;

    if (FAILED(hr))
    {
        MB(L"Failed to execute in the default app domain!");
        
        switch (hr)
        {
            case HOST_E_CLRNOTAVAILABLE:
                MB(L"CLR Not available");
                break;

            case HOST_E_TIMEOUT:
                MB(L"Call timed out");
                break;

            case HOST_E_NOT_OWNER:
                MB(L"Caller does not own lock");
                break;

            case HOST_E_ABANDONED:
                MB(L"An event was canceled while a blocked thread or fiber was waiting on it");
                break;

            case E_FAIL:
                MB(L"Unspecified catastrophic failure");
                break;

            default:
                char buff[128];
                sprintf(buff, "Result is: 0x%lx", hr);
                MessageBoxA(nullptr, buff, "Info", 0);
                break;
        }

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

HMODULE GetCurrentModule()
{
    HMODULE hModule = nullptr;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(GetCurrentModule), &hModule);
    return hModule;
}
}

// this function is executed in the context of the wow process
extern "C" __declspec(dllexport) unsigned int CLRLoad(wchar_t *dll)
{
    const std::wstring dllPath(dll);

    auto moduleBuffer = new wchar_t[dllPath.length() + 1];
    wcscpy(moduleBuffer, dllPath.c_str());
    moduleBuffer[dllPath.length()] = L'\0';

    // Start a new thread for CLR to use
    // NOTE: moduleBuffer must be freed by the CLR thread!
    gCLRThread = std::thread(ThreadMain, moduleBuffer);
    gCLRThread.detach();

    return EXIT_SUCCESS;
}