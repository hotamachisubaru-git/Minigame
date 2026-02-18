#pragma once

#include "shooter_game.h"

#include <windows.h>

class GameApp {
public:
    bool Initialize(HINSTANCE instance, int showCommand);
    int Run();
    ~GameApp();

private:
    static constexpr const wchar_t* kWindowClassName = L"TicketShooterWindowClass";
    static constexpr UINT_PTR kFrameTimerId = 2001;
    static constexpr UINT kFrameIntervalMs = 16;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void Paint();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    ULONG_PTR gdiplusToken_ = 0;
    int clientWidth_ = kDesignWidth;
    int clientHeight_ = kDesignHeight;

    ShooterGame game_;
};
