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

#include "NotifyIconMgr.hpp"
#include "NotifyIcon.hpp"

#include <Windows.h>
#include <stdexcept>
#include <tchar.h>
#include <thread>
#include <mutex>
#include <memory>
#include <sstream>

namespace
{
static constexpr TCHAR className[] = _T("WowreebWindowClass");
static NotifyIconMgr *singleton = nullptr;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // first offer the message to our handler
    if (!!singleton)
    {
        if (msg == WM_COMMAND && wParam >= WM_APP)
        {
            auto iconId = (wParam & ~WM_APP) >> NotifyIcon::MenuBits;
            auto menuId = (wParam & ((1 << NotifyIcon::MenuBits) - 1));

            if (singleton->Command(hWnd, iconId, menuId))
                return TRUE;
        }
        else if (msg >= WM_APP)
        {
            if (singleton->WindowProc(hWnd, msg - WM_APP, wParam, lParam))
                return TRUE;
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
}

NotifyIconMgr::NotifyIconMgr(HINSTANCE hInstance) : _shutdown(false), _messageThread(&NotifyIconMgr::MessageLoop, this, hInstance)
{
    if (!!singleton)
        throw std::runtime_error("Cannot instantiate more than one NotifyIconMgr");

    singleton = this;
}

void NotifyIconMgr::MessageLoop(HINSTANCE hInstance)
{
    WNDCLASS wc;

    ZeroMemory(&wc, sizeof(wc));

    wc.lpfnWndProc = ::WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;

    if (!RegisterClass(&wc))
        throw std::runtime_error("Failed to register window class");

    auto window = ::CreateWindow(className, nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

    if (!window)
    {
        std::stringstream str;

        str << "Error number = " << ::GetLastError();
        MessageBoxA(nullptr, str.str().c_str(), "CreateWindow() failed", MB_ICONERROR);
        return;
    }

    do
    {
        MSG msg;

        if (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // only lock the mutex for as long as necessary
        {
            std::lock_guard<std::mutex> guard(_mutex);

            for (auto const &i : _pending)
                _icons.emplace_back(std::make_shared<NotifyIcon>(window, static_cast<unsigned int>(_icons.size()), i.first, i.second));

            _pending.clear();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (!_shutdown);

    {
        std::lock_guard<std::mutex> guard(_mutex);
        _icons.clear();
    }
}

bool NotifyIconMgr::Command(HWND hWnd, unsigned int iconId, unsigned int menuId)
{
    std::shared_ptr<NotifyIcon> icon;

    // this function can recursively call itself so we need to unlock the mutex as soon as possible
    {
        std::lock_guard<std::mutex> guard(_mutex);

        if (iconId >= _icons.size() || !_icons[iconId])
            return false;

        icon = _icons[iconId];
    }


    icon->ClickMenu(menuId);

    return false;
}

bool NotifyIconMgr::WindowProc(HWND hWnd, unsigned int iconId, WPARAM wParam, LPARAM lParam)
{
    std::shared_ptr<NotifyIcon> icon;

    // this function can recursively call itself so we need to unlock the mutex as soon as possible
    {
        std::lock_guard<std::mutex> guard(_mutex);

        if (iconId >= _icons.size() || !_icons[iconId])
            return false;

        icon = _icons[iconId];
    }

    switch (lParam)
    {
        case WM_RBUTTONDOWN:
        case WM_LBUTTONDOWN:
        {
            icon->ToggleMenu();
            return true;
        }
    }

    return false;
}

std::shared_ptr<NotifyIcon> NotifyIconMgr::Create(HICON icon, const TCHAR *tip)
{
    size_t idx;

    {
        // these must be created in the same thread which owns the window, which is the event processing thread
        std::lock_guard<std::mutex> guard(_mutex);

        _pending.emplace_back(icon, tip);

        idx = _icons.size() + _pending.size() - 1;
    }

    // monitor for the creation of this icon so we can return it
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        std::lock_guard<std::mutex> guard(_mutex);

        if (_icons.size() > idx)
            return _icons[idx];
    } while (true);
}