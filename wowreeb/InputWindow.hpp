#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>

class InputWindow
{
    private:
        LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

        const HWND m_window;
        const HINSTANCE m_instance;
        const HFONT m_labelFont;
        const HFONT m_textBoxFont;

        bool m_keyReady;
        std::string m_key;

        std::map<int, HWND> m_controls;
        std::map<int, std::function<void()>> m_buttonHandlers;

        // adding controls
        void AddLabel(const std::string &text, int x, int y);
        void AddTextBox(int id, const std::string &text, int x, int y, int width, int height);
        void AddButton(int id, const std::string &text, int x, int y, int width, int height, std::function<void()> handler);

        const std::string GetText(int id) const;

        // control window
        void Enable(int id, bool enabled) const;
        void SetFocus(int id) const;

    public:
        InputWindow(HINSTANCE hInstance, int nCmdShow, const std::string &title);
        ~InputWindow();

        std::string ReadKey() const;
};