#include "game_app.h"

#include <windows.h>

#if !defined(_WIN64)
#error "PticketGetter is x64-only. Build with an x64 toolchain."
#endif

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware();

    GameApp app;
    if (!app.Initialize(instance, showCommand)) {
        MessageBoxW(nullptr, L"Ticket Shooter の初期化に失敗しました。", L"Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    return app.Run();
}
