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
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class NotifyIcon;

class NotifyIconMgr
{
private:
    UINT _taskbarCreated;

    bool _shutdown;

    std::thread _messageThread;
    mutable std::mutex _mutex;

    std::vector<std::pair<HICON, const TCHAR*>> _pending;
    std::vector<std::shared_ptr<NotifyIcon>> _icons;

    void MessageLoop(HINSTANCE hInstance);

public:
    NotifyIconMgr(HINSTANCE hInstance);
    ~NotifyIconMgr()
    {
        _shutdown = true;
        _messageThread.join();
    }

    void Create();
    bool IsTaskbarCreated(UINT msg) const
    {
        return _taskbarCreated > 0 && _taskbarCreated == msg;
    }
    void TaskbarCreated();

    bool Command(HWND hWnd, unsigned int iconId, unsigned int menuId);
    bool WindowProc(HWND hWnd, unsigned int iconId, WPARAM wParam, LPARAM lParam);

    std::shared_ptr<NotifyIcon> Create(HICON icon, const TCHAR* tip);
};