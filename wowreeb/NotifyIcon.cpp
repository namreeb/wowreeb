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

#include "NotifyIcon.hpp"

#include "resource.h"

#include <Shellapi.h>
#include <Shlwapi.h>
#include <Windows.h>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <strsafe.h>
#include <tchar.h>
#include <vector>

NotifyIcon::NotifyIcon(HWND window, unsigned int id, HICON icon, const TCHAR* tip)
    : _window(window), _id(id)
{
    constexpr int maxId = (1 << IconBits) - 1;

    if (id > maxId)
        throw std::runtime_error("Too many icons");

    ZeroMemory(&_addMessage, sizeof(_addMessage));

    _addMessage.cbSize = sizeof(_addMessage);
    _addMessage.hWnd = window;
    _addMessage.uID = id;
    _addMessage.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    _addMessage.uCallbackMessage = WM_APP | (id << MenuBits);
    _addMessage.hIcon = icon;

    StringCchCopy(_addMessage.szTip, ARRAYSIZE(_addMessage.szTip), tip);

    _addMessage.uVersion = NOTIFYICON_VERSION_4;

    CreateIcon();
}

NotifyIcon::~NotifyIcon()
{
    NOTIFYICONDATA del;

    ZeroMemory(&del, sizeof(del));

    del.cbSize = sizeof(NOTIFYICONDATA);
    del.hWnd = _window;
    del.uID = _id;

    Shell_NotifyIcon(NIM_DELETE, &del);
}

void NotifyIcon::CreateIcon()
{
    if (!Shell_NotifyIcon(NIM_ADD, &_addMessage))
        throw std::runtime_error("Shell_NotifyIcon failed");
}

void NotifyIcon::ToggleMenu()
{
    auto menu = CreatePopupMenu();

    if (!menu)
        throw std::runtime_error("CreatePopupMenu failed");

    const UINT baseButtonId = WM_APP | (_id << MenuBits);

    {
        std::lock_guard<std::mutex> guard(_mutex);

        for (auto i = 0u; i < _menuEntries.size(); ++i)
        {
            if (_menuEntries[i].text[0] == '-' && _menuEntries[i].text[1] == '\0')
                InsertMenu(menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0,
                           _menuEntries[i].text);
            else
            {
                const UINT buttonId = baseButtonId | i;
                InsertMenu(menu, -1, MF_BYPOSITION, buttonId,
                           _menuEntries[i].text);
            }
        }
    }

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(_window);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, _window, nullptr);
    DestroyMenu(menu);
}

void NotifyIcon::ClearMenu()
{
    std::lock_guard<std::mutex> guard(_mutex);
    _menuEntries.clear();
}

void NotifyIcon::AddMenu(const TCHAR* text, std::function<void()> callback,
                         int position)
{
    constexpr int maxEntries = (1 << MenuBits) - 1;

    std::lock_guard<std::mutex> guard(_mutex);

    if (static_cast<int>(_menuEntries.size()) >= maxEntries)
        throw std::runtime_error("Too many menu entries");

    auto const pos =
        (position >= static_cast<int>(_menuEntries.size()) || position < 0) ?
            (_menuEntries.end()) :
            (_menuEntries.begin() + position);

    _menuEntries.emplace(pos, text, callback);
}

void NotifyIcon::ClickMenu(unsigned int menuId) const
{
    std::lock_guard<std::mutex> guard(_mutex);

    if (menuId >= _menuEntries.size())
        throw std::runtime_error("Invalid menuId for ClickMenu");

    if (_menuEntries[menuId].callback)
        _menuEntries[menuId].callback();
}