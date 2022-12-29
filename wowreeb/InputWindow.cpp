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

#include "InputWindow.hpp"

#include "resource.h"

#include <cassert>
#include <chrono>
#include <codecvt>
#include <functional>
#include <locale>
#include <map>
#include <string>
#include <tchar.h>
#include <thread>
#include <vector>

std::wstring make_wstring(const std::string& in);

namespace
{
void GetStartingPosition(int& x, int& y, int& width, int& height)
{
    x = y = 100;
    width = 340;
    height = 80;
}

HWND MakeWindow(HINSTANCE hInstance, const std::string& title)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    int x, y, width, height;
    GetStartingPosition(x, y, width, height);

    RECT wr = {x, y, x + width, y + height};

    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, true);

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = _T("InputWindow");
    wc.hIconSm =
        (HICON)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON1),
                         IMAGE_ICON, 16, 16, 0);
    wc.hIcon =
        (HICON)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON1),
                         IMAGE_ICON, 32, 32, 0);

    RegisterClassEx(&wc);

#ifdef UNICODE
    auto const t = make_wstring(title);
#else
    auto const t = tile;
#endif

    return CreateWindowEx(0, _T("InputWindow"), t.c_str(),
                          WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU, wr.right,
                          wr.top, width, height, HWND_DESKTOP, nullptr, hInstance,
                          nullptr);
}
} // namespace

std::wstring make_wstring(const std::string& in)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(in);
}

InputWindow::InputWindow(HINSTANCE hInstance, int nCmdShow,
                         const std::string& title)
    : m_window(MakeWindow(hInstance, title)),
      m_instance(reinterpret_cast<HINSTANCE>(
          GetWindowLongPtr(m_window, GWLP_HINSTANCE))),
      m_labelFont(CreateFont(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             VARIABLE_PITCH, _T("Microsoft Sans Serif"))),
      m_textBoxFont(CreateFont(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                               VARIABLE_PITCH, _T("Microsoft Sans Serif"))),
      m_keyReady(false)
{
    auto const handler = [](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto const ctrl =
            reinterpret_cast<InputWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        return ctrl->WindowProc(hwnd, message, wParam, lParam);
    };

    SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);
    SetWindowLongPtr(m_window, GWLP_WNDPROC,
                     reinterpret_cast<LONG_PTR>(
                         static_cast<decltype(&DefWindowProc)>(handler)));

    AddLabel("Key:", 15, 13);
    AddTextBox(0, "", 45, 10, 175, 25);
    this->SetFocus(0); // explicitly use member function
    AddButton(1, "Go", 225, 10, 90, 25,
              [this]()
              {
                  this->m_key = this->GetText(0);
                  this->m_keyReady = true;
              });

    ShowWindow(m_window, nCmdShow);
}

InputWindow::~InputWindow()
{
    DestroyWindow(m_window);
}

LRESULT InputWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                LPARAM lParam)
{
    // our custom message handler, which will only return if it actually handles
    // something
    switch (message)
    {
        case WM_COMMAND:
        {
            TCHAR className[16];

            auto const control = reinterpret_cast<HWND>(lParam);

            if (!GetClassName(control, className,
                              sizeof(className) / sizeof(className[0])))
                break;

            if (!_tcsnccmp(className, _T("Button"), _tcslen(className)))
            {
                auto handler = m_buttonHandlers.find(static_cast<int>(wParam));

                if (handler != m_buttonHandlers.end())
                {
                    handler->second();
                    return TRUE;
                }
            }

            break;
        }

        case WM_CLOSE:
        {
            m_key.clear();
            m_keyReady = true;
            break;
        }
    }

    // if we reach here, we haven't found anything that is any concern of ours.
    // pass it off to the default handler
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void InputWindow::AddLabel(const std::string& text, int x, int y)
{
    auto hdc = GetDC(m_window);

    assert(!!hdc);

    SIZE textSize;
    auto result = GetTextExtentPoint(hdc,
#ifdef UNICODE
                                     make_wstring(text).c_str(),
#else
                                     text.c_str(),
#endif
                                     static_cast<int>(text.length()), &textSize);

    assert(result);

#ifdef UNICODE
    auto control =
        CreateWindow(_T("STATIC"), make_wstring(text).c_str(),
                     WS_CHILD | WS_VISIBLE | WS_TABSTOP, x, y, textSize.cx,
                     textSize.cy, m_window, nullptr, m_instance, nullptr);
#else
    auto control = CreateWindow(
        _T("STATIC"), text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP, x, y,
        textSize.cx, textSize.cy, m_window, nullptr, m_instance, nullptr);
#endif

    SendMessage(control, WM_SETFONT, (WPARAM)m_labelFont, MAKELPARAM(TRUE, 0));
}

void InputWindow::AddTextBox(int id, const std::string& text, int x, int y,
                             int width, int height)
{
#ifdef UNICODE
    auto control =
        CreateWindow(_T("EDIT"), make_wstring(text).c_str(),
                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_PASSWORD,
                     x, y, width, height, m_window, nullptr, m_instance, nullptr);
#else
    auto control = CreateWindow(
        _T("EDIT"), text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER,
        x, y, width, height, m_window, nullptr, m_instance, nullptr);
#endif

    SendMessage(control, WM_SETFONT, (WPARAM)m_textBoxFont, MAKELPARAM(TRUE, 0));

    m_controls.emplace(id, control);
}

void InputWindow::AddButton(int id, const std::string& text, int x, int y,
                            int width, int height, std::function<void()> handler)
{
#ifdef UNICODE
    auto control =
        CreateWindow(_T("BUTTON"), make_wstring(text).c_str(),
                     WS_TABSTOP | WS_VISIBLE | WS_CHILD, x, y, width, height,
                     m_window, (HMENU)(LONG_PTR)id, m_instance, nullptr);
#else
    auto control = CreateWindow(
        _T("BUTTON"), text.c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD, x, y,
        width, height, m_window, (HMENU)(LONG_PTR)id, m_instance, nullptr);
#endif

    SendMessage(control, WM_SETFONT, (WPARAM)m_labelFont, MAKELPARAM(TRUE, 0));

    m_controls.emplace(id, control);
    m_buttonHandlers.emplace(id, handler);
}

const std::string InputWindow::GetText(int id) const
{
    auto control = m_controls.find(id);

    assert(control != m_controls.cend());

    std::vector<char> buffer(GetWindowTextLengthA(control->second) + 1);
    GetWindowTextA(control->second, &buffer[0], static_cast<int>(buffer.size()));

    return std::string(&buffer[0]);
}

void InputWindow::Enable(int id, bool enabled) const
{
    auto control = m_controls.find(id);

    assert(control != m_controls.cend());

    EnableWindow(control->second, static_cast<BOOL>(enabled));
}

void InputWindow::SetFocus(int id) const
{
    auto control = m_controls.find(id);

    assert(control != m_controls.cend());

    ::SetFocus(control->second); // explicitly use windows API
}

std::string InputWindow::ReadKey() const
{
    while (!m_keyReady)
    {
        MSG msg;

        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
                break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return m_key;
}