#include "game_app.h"

#include <gdiplus.h>
#include <windowsx.h>

#include <algorithm>

bool GameApp::Initialize(HINSTANCE instance, int showCommand) {
    instance_ = instance;

    Gdiplus::GdiplusStartupInput gdiplusInput;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        return false;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT desiredRect{0, 0, kDesignWidth, kDesignHeight};
    AdjustWindowRect(&desiredRect, style, FALSE);

    hwnd_ = CreateWindowExW(0,
                            kWindowClassName,
                            L"Ticket Shooter",
                            style,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            desiredRect.right - desiredRect.left,
                            desiredRect.bottom - desiredRect.top,
                            nullptr,
                            nullptr,
                            instance_,
                            this);

    if (!hwnd_) {
        return false;
    }

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    return true;
}

int GameApp::Run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

GameApp::~GameApp() {
    if (gdiplusToken_ != 0) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
    }
}

LRESULT CALLBACK GameApp::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    GameApp* app = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = reinterpret_cast<GameApp*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd_ = hwnd;
    } else {
        app = reinterpret_cast<GameApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app) {
        return app->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT GameApp::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        game_.Initialize();
        SetTimer(hwnd_, kFrameTimerId, kFrameIntervalMs, nullptr);
        return 0;
    case WM_SIZE:
        clientWidth_ = std::max(1, static_cast<int>(LOWORD(lParam)));
        clientHeight_ = std::max(1, static_cast<int>(HIWORD(lParam)));
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_TIMER:
        if (wParam == kFrameTimerId) {
            game_.Update(1.0f / 60.0f);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            return 0;
        }
        game_.HandleKeyDown(static_cast<UINT>(wParam));
        return 0;
    case WM_KEYUP:
        game_.HandleKeyUp(static_cast<UINT>(wParam));
        return 0;
    case WM_KILLFOCUS:
        game_.ClearInput();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd_, kFrameTimerId);
        game_.Shutdown();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

void GameApp::Paint() {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd_, &ps);

    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP backBuffer = CreateCompatibleBitmap(hdc, clientWidth_, clientHeight_);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDc, backBuffer));

    game_.Render(memDc, clientWidth_, clientHeight_);
    BitBlt(hdc, 0, 0, clientWidth_, clientHeight_, memDc, 0, 0, SRCCOPY);

    SelectObject(memDc, oldBitmap);
    DeleteObject(backBuffer);
    DeleteDC(memDc);
    EndPaint(hwnd_, &ps);
}
