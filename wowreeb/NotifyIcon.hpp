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

#pragma once

#include <Windows.h>
#include <functional>
#include <mutex>
#include <strsafe.h>
#include <vector>

class NotifyIcon
{
public:
    static constexpr int IconBits = 7;
    static constexpr int MenuBits = 14 - IconBits;

private:
    static_assert(IconBits > 0 && MenuBits > 0 && IconBits + MenuBits == 14,
                  "IconBits + MenuBits must be 14");

    NOTIFYICONDATA _addMessage;

    HWND _window;
    unsigned int _id;

    struct MenuEntry
    {
        TCHAR text[32];
        std::function<void()> callback;

        MenuEntry(const TCHAR* t, std::function<void()> cb) : callback(cb)
        {
            StringCchCopy(text, ARRAYSIZE(text), t);
        }
    };

    mutable std::mutex _mutex;

    std::vector<MenuEntry> _menuEntries;

public:
    NotifyIcon(HWND window, unsigned int id, HICON icon, const TCHAR* tip);
    ~NotifyIcon();

    void CreateIcon();

    void ToggleMenu();

    void ClearMenu();
    void AddMenu(const TCHAR* text, std::function<void()> callback = nullptr,
                 int position = -1);
    void ClickMenu(unsigned int menuId) const;
};