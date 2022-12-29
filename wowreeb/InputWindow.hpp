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

#include <Windows.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

class InputWindow
{
private:
    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                LPARAM lParam);

    const HWND m_window;
    const HINSTANCE m_instance;
    const HFONT m_labelFont;
    const HFONT m_textBoxFont;

    bool m_keyReady;
    std::string m_key;

    std::map<int, HWND> m_controls;
    std::map<int, std::function<void()>> m_buttonHandlers;

    // adding controls
    void AddLabel(const std::string& text, int x, int y);
    void AddTextBox(int id, const std::string& text, int x, int y, int width,
                    int height);
    void AddButton(int id, const std::string& text, int x, int y, int width,
                   int height, std::function<void()> handler);

    const std::string GetText(int id) const;

    // control window
    void Enable(int id, bool enabled) const;
    void SetFocus(int id) const;

public:
    InputWindow(HINSTANCE hInstance, int nCmdShow, const std::string& title);
    ~InputWindow();

    std::string ReadKey() const;
};