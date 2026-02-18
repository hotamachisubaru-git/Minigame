#include <windows.h>
#include <gdiplus.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

#if !defined(_WIN64)
#error "PticketGetter is x64-only. Build with an x64 toolchain."
#endif

namespace {
using Gdiplus::Color;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Image;
using Gdiplus::LinearGradientBrush;
using Gdiplus::Pen;
using Gdiplus::PointF;
using Gdiplus::RectF;
using Gdiplus::SolidBrush;
using Gdiplus::StringAlignment;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringAlignmentNear;
using Gdiplus::StringFormat;
using Gdiplus::FontStyleBold;

constexpr int kDesignWidth = 720;
constexpr int kDesignHeight = 1280;
constexpr UINT_PTR kFrameTimerId = 2001;
constexpr UINT kFrameIntervalMs = 16;
constexpr float kFixedDt = 1.0f / 60.0f;
constexpr float kPi = 3.14159265358979323846f;

struct Bullet {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float radius = 6.0f;
    int damage = 1;
    bool active = true;
};

struct Enemy {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 120.0f;
    float radius = 26.0f;
    int hp = 1;
    float wobble = 0.0f;
    bool active = true;
};

struct TicketDrop {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 140.0f;
    float size = 34.0f;
    bool active = true;
};

struct Star {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 55.0f;
    float size = 2.0f;
};

std::wstring FormatNumber(int value) {
    std::wstring text = std::to_wstring(value);
    for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3) {
        text.insert(i, L",");
    }
    return text;
}

void BuildRoundedRect(GraphicsPath* path, const RectF& rect, float radius) {
    if (radius <= 0.0f) {
        path->Reset();
        path->AddRectangle(rect);
        return;
    }

    const float diameter = radius * 2.0f;
    path->Reset();
    path->AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path->AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path->AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0.0f, 90.0f);
    path->AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
    path->CloseFigure();
}

float Clamp(float v, float low, float high) {
    return std::max(low, std::min(v, high));
}

float DistanceSq(float x1, float y1, float x2, float y2) {
    const float dx = x1 - x2;
    const float dy = y1 - y2;
    return dx * dx + dy * dy;
}

std::wstring ToLower(const std::wstring& src) {
    std::wstring lowered = src;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return lowered;
}

}  // namespace

class GameApp {
public:
    bool Initialize(HINSTANCE instance, int showCommand) {
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

        hwnd_ = CreateWindowExW(
            0,
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

    int Run() {
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

    ~GameApp() {
        StopBgm();
        if (gdiplusToken_ != 0) {
            Gdiplus::GdiplusShutdown(gdiplusToken_);
        }
    }

private:
    static constexpr const wchar_t* kWindowClassName = L"TicketShooterWindowClass";

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
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

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_CREATE:
            LoadAssets();
            InitStars();
            ResetGame();
            SetTimer(hwnd_, kFrameTimerId, kFrameIntervalMs, nullptr);
            StartBgm();
            return 0;
        case WM_SIZE:
            clientWidth_ = std::max(1, static_cast<int>(LOWORD(lParam)));
            clientHeight_ = std::max(1, static_cast<int>(HIWORD(lParam)));
            UpdateViewport();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        case WM_TIMER:
            if (wParam == kFrameTimerId) {
                UpdateGame(kFixedDt);
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        case WM_PAINT:
            Paint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_KEYDOWN:
            HandleKeyDown(static_cast<UINT>(wParam));
            return 0;
        case WM_KEYUP:
            HandleKeyUp(static_cast<UINT>(wParam));
            return 0;
        case WM_KILLFOCUS:
            keys_.fill(false);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kFrameTimerId);
            StopBgm();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        }
    }

    void HandleKeyDown(UINT keyCode) {
        if (keyCode < keys_.size()) {
            keys_[keyCode] = true;
        }

        if (keyCode == VK_ESCAPE) {
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            return;
        }

        if (gameOver_ && keyCode == 'R') {
            ResetGame();
            return;
        }

        if (keyCode == 'M') {
            bgmEnabled_ = !bgmEnabled_;
            if (bgmEnabled_) {
                StartBgm();
                statusText_ = L"BGM を有効にしました";
            } else {
                StopBgm();
                statusText_ = L"BGM を無効にしました";
            }
        }
    }

    void HandleKeyUp(UINT keyCode) {
        if (keyCode < keys_.size()) {
            keys_[keyCode] = false;
        }
    }

    void UpdateViewport() {
        scale_ = std::min(static_cast<float>(clientWidth_) / static_cast<float>(kDesignWidth),
                          static_cast<float>(clientHeight_) / static_cast<float>(kDesignHeight));
        if (scale_ < 0.01f) {
            scale_ = 1.0f;
        }
        offsetX_ = (static_cast<float>(clientWidth_) - static_cast<float>(kDesignWidth) * scale_) * 0.5f;
        offsetY_ = (static_cast<float>(clientHeight_) - static_cast<float>(kDesignHeight) * scale_) * 0.5f;
    }

    void ResetGame() {
        bullets_.clear();
        enemies_.clear();
        tickets_.clear();

        playerX_ = static_cast<float>(kDesignWidth) * 0.5f;
        playerY_ = static_cast<float>(kDesignHeight) - 135.0f;
        playerHp_ = 5;

        ticketPoints_ = 0;
        powerLevel_ = 1;
        score_ = 0;

        fireCooldown_ = 0.08f;
        enemySpawnCooldown_ = 0.75f;
        ticketSpawnCooldown_ = 1.2f;

        gameOver_ = false;
        statusText_ = L"WASD / 矢印で移動。Shiftで低速移動";
    }

    void UpdateGame(float dt) {
        UpdateStars(dt);

        if (gameOver_) {
            return;
        }

        UpdatePlayerMovement(dt);
        UpdateShooting(dt);
        SpawnEnemies(dt);
        SpawnTickets(dt);
        UpdateBullets(dt);
        UpdateEnemies(dt);
        UpdateTickets(dt);
        ResolveCollisions();
        CleanupEntities();

        powerLevel_ = std::min(8, 1 + ticketPoints_ / 5);

        if (playerHp_ <= 0) {
            playerHp_ = 0;
            gameOver_ = true;
            statusText_ = L"ゲームオーバー - Rキーで再開";
        }
    }

    void UpdatePlayerMovement(float dt) {
        float moveX = 0.0f;
        float moveY = 0.0f;

        if (IsDown('A') || IsDown(VK_LEFT)) {
            moveX -= 1.0f;
        }
        if (IsDown('D') || IsDown(VK_RIGHT)) {
            moveX += 1.0f;
        }
        if (IsDown('W') || IsDown(VK_UP)) {
            moveY -= 1.0f;
        }
        if (IsDown('S') || IsDown(VK_DOWN)) {
            moveY += 1.0f;
        }

        if (moveX != 0.0f || moveY != 0.0f) {
            const float length = std::sqrt(moveX * moveX + moveY * moveY);
            moveX /= length;
            moveY /= length;
        }

        float speed = 390.0f + static_cast<float>(powerLevel_ - 1) * 12.0f;
        const bool slowMode = IsDown(VK_SHIFT) || IsDown(VK_LSHIFT) || IsDown(VK_RSHIFT);
        if (slowMode) {
            speed *= 0.45f;
        }
        playerX_ += moveX * speed * dt;
        playerY_ += moveY * speed * dt;

        playerX_ = Clamp(playerX_, 34.0f, static_cast<float>(kDesignWidth) - 34.0f);
        playerY_ = Clamp(playerY_, 180.0f, static_cast<float>(kDesignHeight) - 36.0f);
    }

    void UpdateShooting(float dt) {
        fireCooldown_ -= dt;

        const float interval = std::max(0.085f, 0.32f - static_cast<float>(powerLevel_ - 1) * 0.025f);
        while (fireCooldown_ <= 0.0f) {
            FireShotPattern();
            fireCooldown_ += interval;
        }
    }

    void FireShotPattern() {
        int bulletCount = 1;
        if (powerLevel_ >= 3) {
            bulletCount = 2;
        }
        if (powerLevel_ >= 5) {
            bulletCount = 3;
        }
        if (powerLevel_ >= 7) {
            bulletCount = 5;
        }

        const float spreadDeg = 26.0f;
        const float speed = 680.0f;
        const int damage = std::max(1, powerLevel_ / 3);

        for (int i = 0; i < bulletCount; ++i) {
            float t = 0.5f;
            if (bulletCount > 1) {
                t = static_cast<float>(i) / static_cast<float>(bulletCount - 1);
            }

            const float angleDeg = (t - 0.5f) * spreadDeg;
            const float angleRad = angleDeg * (kPi / 180.0f);

            Bullet bullet;
            bullet.x = playerX_;
            bullet.y = playerY_ - 28.0f;
            bullet.vx = std::sin(angleRad) * speed;
            bullet.vy = -std::cos(angleRad) * speed;
            bullet.damage = damage;
            bullet.radius = 4.5f + static_cast<float>(powerLevel_) * 0.35f;
            bullets_.push_back(bullet);
        }
    }

    void SpawnEnemies(float dt) {
        enemySpawnCooldown_ -= dt;
        if (enemySpawnCooldown_ > 0.0f) {
            return;
        }

        Enemy enemy;
        enemy.x = RandomFloat(40.0f, static_cast<float>(kDesignWidth) - 40.0f);
        enemy.y = -42.0f;
        enemy.speed = RandomFloat(92.0f, 168.0f) + static_cast<float>(score_ / 1800);
        enemy.radius = RandomFloat(20.0f, 30.0f);
        enemy.hp = 1 + RandomInt(0, std::min(3, score_ / 2500));
        enemy.wobble = RandomFloat(0.0f, kPi * 2.0f);
        enemies_.push_back(enemy);

        const float minInterval = std::max(0.28f, 0.85f - static_cast<float>(powerLevel_) * 0.06f);
        const float maxInterval = std::max(0.45f, 1.25f - static_cast<float>(powerLevel_) * 0.05f);
        enemySpawnCooldown_ = RandomFloat(minInterval, maxInterval);
    }

    void SpawnTickets(float dt) {
        ticketSpawnCooldown_ -= dt;
        if (ticketSpawnCooldown_ > 0.0f) {
            return;
        }

        TicketDrop ticket;
        ticket.x = RandomFloat(45.0f, static_cast<float>(kDesignWidth) - 45.0f);
        ticket.y = -34.0f;
        ticket.speed = RandomFloat(72.0f, 118.0f);
        ticket.size = RandomFloat(30.0f, 38.0f);
        tickets_.push_back(ticket);

        ticketSpawnCooldown_ = RandomFloat(1.0f, 2.1f);
    }

    void UpdateBullets(float dt) {
        for (auto& bullet : bullets_) {
            bullet.x += bullet.vx * dt;
            bullet.y += bullet.vy * dt;
        }
    }

    void UpdateEnemies(float dt) {
        for (auto& enemy : enemies_) {
            enemy.wobble += dt * 4.0f;
            enemy.x += std::sin(enemy.wobble) * 25.0f * dt;
            enemy.y += enemy.speed * dt;

            if (enemy.y > static_cast<float>(kDesignHeight) + 45.0f && enemy.active) {
                enemy.active = false;
                --playerHp_;
                statusText_ = L"敵を取り逃がした - HP -1";
            }
        }
    }

    void UpdateTickets(float dt) {
        for (auto& ticket : tickets_) {
            ticket.y += ticket.speed * dt;
        }
    }

    void ResolveCollisions() {
        for (auto& enemy : enemies_) {
            if (!enemy.active) {
                continue;
            }

            for (auto& bullet : bullets_) {
                if (!bullet.active) {
                    continue;
                }

                const float hitRadius = enemy.radius + bullet.radius;
                if (DistanceSq(enemy.x, enemy.y, bullet.x, bullet.y) > hitRadius * hitRadius) {
                    continue;
                }

                bullet.active = false;
                enemy.hp -= bullet.damage;
                if (enemy.hp <= 0) {
                    enemy.active = false;
                    score_ += 90 + powerLevel_ * 25;
                    if (Chance(24)) {
                        TicketDrop bonus;
                        bonus.x = enemy.x;
                        bonus.y = enemy.y;
                        bonus.speed = RandomFloat(75.0f, 112.0f);
                        bonus.size = 32.0f;
                        tickets_.push_back(bonus);
                    }
                    break;
                }
            }
        }

        const float playerRadius = 28.0f;

        for (auto& enemy : enemies_) {
            if (!enemy.active) {
                continue;
            }
            const float hitRadius = enemy.radius + playerRadius;
            if (DistanceSq(enemy.x, enemy.y, playerX_, playerY_) <= hitRadius * hitRadius) {
                enemy.active = false;
                --playerHp_;
                statusText_ = L"敵に接触 - HP -1";
            }
        }

        for (auto& ticket : tickets_) {
            if (!ticket.active) {
                continue;
            }
            const float hitRadius = (ticket.size * 0.42f) + playerRadius;
            if (DistanceSq(ticket.x, ticket.y, playerX_, playerY_) <= hitRadius * hitRadius) {
                ticket.active = false;
                ++ticketPoints_;

                if (ticketPoints_ % 8 == 0 && playerHp_ < 9) {
                    ++playerHp_;
                    statusText_ = L"チケット回収でHP回復";
                } else {
                    statusText_ = L"チケット回収 +1";
                }
            }
        }
    }

    void CleanupEntities() {
        bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet& bullet) {
            if (!bullet.active) {
                return true;
            }
            return bullet.y < -70.0f || bullet.y > static_cast<float>(kDesignHeight) + 70.0f ||
                   bullet.x < -70.0f || bullet.x > static_cast<float>(kDesignWidth) + 70.0f;
        }),
                       bullets_.end());

        enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& enemy) {
            return !enemy.active;
        }),
                       enemies_.end());

        tickets_.erase(std::remove_if(tickets_.begin(), tickets_.end(), [](const TicketDrop& ticket) {
            if (!ticket.active) {
                return true;
            }
            return ticket.y > static_cast<float>(kDesignHeight) + 80.0f;
        }),
                       tickets_.end());
    }

    void InitStars() {
        stars_.clear();
        stars_.reserve(95);
        for (int i = 0; i < 95; ++i) {
            Star star;
            star.x = RandomFloat(0.0f, static_cast<float>(kDesignWidth));
            star.y = RandomFloat(0.0f, static_cast<float>(kDesignHeight));
            star.speed = RandomFloat(45.0f, 155.0f);
            star.size = RandomFloat(1.2f, 3.2f);
            stars_.push_back(star);
        }
    }

    void UpdateStars(float dt) {
        for (auto& star : stars_) {
            star.y += star.speed * dt;
            if (star.y > static_cast<float>(kDesignHeight) + 5.0f) {
                star.y = -5.0f;
                star.x = RandomFloat(0.0f, static_cast<float>(kDesignWidth));
                star.speed = RandomFloat(45.0f, 155.0f);
                star.size = RandomFloat(1.2f, 3.2f);
            }
        }
    }

    void Paint() {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd_, &ps);

        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP backBuffer = CreateCompatibleBitmap(hdc, clientWidth_, clientHeight_);
        HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDc, backBuffer));

        Render(memDc);
        BitBlt(hdc, 0, 0, clientWidth_, clientHeight_, memDc, 0, 0, SRCCOPY);

        SelectObject(memDc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(memDc);
        EndPaint(hwnd_, &ps);
    }

    void Render(HDC hdc) {
        Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        g.Clear(Color(255, 10, 16, 26));

        const auto state = g.Save();
        g.TranslateTransform(offsetX_, offsetY_);
        g.ScaleTransform(scale_, scale_);
        g.SetClip(RectF(0.0f, 0.0f, static_cast<float>(kDesignWidth), static_cast<float>(kDesignHeight)));

        DrawScene(g);

        g.Restore(state);
    }

    void DrawScene(Graphics& g) {
        const RectF whole(0.0f, 0.0f, static_cast<float>(kDesignWidth), static_cast<float>(kDesignHeight));
        LinearGradientBrush bg(whole, Color(255, 13, 24, 44), Color(255, 10, 70, 120),
                               Gdiplus::LinearGradientModeVertical);
        g.FillRectangle(&bg, whole);

        DrawStars(g);
        DrawBullets(g);
        DrawEnemies(g);
        DrawTickets(g);
        DrawPlayer(g);
        DrawHud(g);

        if (gameOver_) {
            DrawGameOver(g);
        }
    }

    void DrawStars(Graphics& g) {
        for (const auto& star : stars_) {
            const int alpha = static_cast<int>(80.0f + star.speed * 0.9f);
            const BYTE a = static_cast<BYTE>(std::clamp(alpha, 0, 255));
            SolidBrush brush(Color(a, 230, 245, 255));
            g.FillEllipse(&brush, RectF(star.x, star.y, star.size, star.size));
        }
    }

    void DrawPlayer(Graphics& g) {
        const float r = 27.0f;
        PointF ship[3] = {
            PointF(playerX_, playerY_ - r),
            PointF(playerX_ - r * 0.78f, playerY_ + r * 0.9f),
            PointF(playerX_ + r * 0.78f, playerY_ + r * 0.9f),
        };

        SolidBrush shipBrush(Color(255, 112, 236, 255));
        g.FillPolygon(&shipBrush, ship, 3);

        Pen shipOutline(Color(255, 226, 252, 255), 2.6f);
        g.DrawPolygon(&shipOutline, ship, 3);

        SolidBrush coreBrush(Color(255, 255, 210, 96));
        g.FillEllipse(&coreBrush, RectF(playerX_ - 8.0f, playerY_ - 6.0f, 16.0f, 16.0f));
    }

    void DrawBullets(Graphics& g) {
        SolidBrush bulletBrush(Color(255, 255, 225, 112));
        Pen bulletStroke(Color(255, 255, 248, 170), 1.5f);

        for (const auto& bullet : bullets_) {
            const RectF r(bullet.x - bullet.radius, bullet.y - bullet.radius, bullet.radius * 2.0f,
                          bullet.radius * 2.0f);
            g.FillEllipse(&bulletBrush, r);
            g.DrawEllipse(&bulletStroke, r);
        }
    }

    void DrawEnemies(Graphics& g) {
        for (const auto& enemy : enemies_) {
            const RectF body(enemy.x - enemy.radius, enemy.y - enemy.radius, enemy.radius * 2.0f,
                             enemy.radius * 2.0f);
            SolidBrush bodyBrush(Color(255, 255, 124, 134));
            g.FillEllipse(&bodyBrush, body);

            Pen bodyStroke(Color(255, 255, 214, 220), 2.0f);
            g.DrawEllipse(&bodyStroke, body);

            const float eyeR = std::max(3.0f, enemy.radius * 0.17f);
            SolidBrush eyeBrush(Color(255, 255, 248, 250));
            g.FillEllipse(&eyeBrush, RectF(enemy.x - enemy.radius * 0.35f - eyeR, enemy.y - eyeR, eyeR * 2.0f, eyeR * 2.0f));
            g.FillEllipse(&eyeBrush, RectF(enemy.x + enemy.radius * 0.35f - eyeR, enemy.y - eyeR, eyeR * 2.0f, eyeR * 2.0f));

            if (enemy.hp > 1) {
                std::wstring hpText = L"x" + std::to_wstring(enemy.hp);
                DrawText(g, hpText, RectF(enemy.x - 20.0f, enemy.y + enemy.radius - 4.0f, 40.0f, 24.0f), 18.0f,
                         Color(255, 255, 248, 190), StringAlignmentCenter, FontStyleBold, L"Arial Black");
            }
        }
    }

    void DrawTickets(Graphics& g) {
        for (const auto& ticket : tickets_) {
            const RectF rect(ticket.x - ticket.size * 0.5f, ticket.y - ticket.size * 0.5f, ticket.size, ticket.size);

            if (ticketIcon_) {
                g.DrawImage(ticketIcon_.get(), rect);
                continue;
            }

            FillRoundRect(g, rect, 6.0f, Color(255, 222, 54, 54));
            DrawText(g, L"T", rect, 18.0f, Color(255, 250, 250, 250), StringAlignmentCenter, FontStyleBold,
                     L"Arial Black");
        }
    }

    void DrawHud(Graphics& g) {
        FillRoundRect(g, RectF(0.0f, 0.0f, 720.0f, 136.0f), 0.0f, Color(205, 4, 12, 22));

        RectF iconRect(24.0f, 20.0f, 42.0f, 42.0f);
        if (pointIcon_) {
            g.DrawImage(pointIcon_.get(), iconRect);
        } else {
            FillRoundRect(g, iconRect, 10.0f, Color(255, 225, 240, 255));
            DrawText(g, L"P", iconRect, 22.0f, Color(255, 24, 48, 80), StringAlignmentCenter, FontStyleBold,
                     L"Arial Black");
        }

        DrawText(g, FormatNumber(ticketPoints_), RectF(74.0f, 18.0f, 180.0f, 42.0f), 36.0f,
                 Color(255, 248, 250, 255), StringAlignmentNear, FontStyleBold, L"Arial Black");
        DrawText(g, L"チケット", RectF(74.0f, 60.0f, 180.0f, 22.0f), 20.0f,
                 Color(255, 180, 216, 242), StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

        std::wstringstream upper;
        upper << L"強化 Lv." << powerLevel_ << L"    HP " << playerHp_;
        DrawText(g, upper.str(), RectF(260.0f, 20.0f, 430.0f, 28.0f), 20.0f, Color(255, 251, 240, 170),
                 StringAlignmentNear, FontStyleBold, L"Arial Black");

        std::wstring scoreLabel = L"スコア " + FormatNumber(score_);
        DrawText(g, scoreLabel, RectF(260.0f, 48.0f, 430.0f, 34.0f), 31.0f, Color(255, 241, 248, 255),
                 StringAlignmentNear, FontStyleBold, L"Arial Black");

        DrawText(g, statusText_, RectF(26.0f, 98.0f, 668.0f, 30.0f), 17.0f, Color(255, 220, 236, 248),
                 StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

        FillRoundRect(g, RectF(16.0f, 1188.0f, 688.0f, 70.0f), 14.0f, Color(165, 6, 19, 33));
        DrawText(g, L"移動 WASD/矢印  低速 Shift  R 再開  M BGM切替", RectF(32.0f, 1205.0f, 656.0f, 34.0f),
                 18.0f, Color(255, 228, 244, 255), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
    }

    void DrawGameOver(Graphics& g) {
        FillRoundRect(g, RectF(100.0f, 430.0f, 520.0f, 310.0f), 24.0f, Color(220, 8, 19, 38));
        DrawText(g, L"ゲームオーバー", RectF(130.0f, 470.0f, 460.0f, 78.0f), 56.0f, Color(255, 255, 212, 118),
                 StringAlignmentCenter, FontStyleBold, L"Arial Black");

        std::wstring result = L"スコア " + FormatNumber(score_);
        DrawText(g, result, RectF(140.0f, 558.0f, 440.0f, 48.0f), 36.0f, Color(255, 241, 248, 255),
                 StringAlignmentCenter, FontStyleBold, L"Arial Black");

        std::wstring ticketText = L"回収チケット: " + FormatNumber(ticketPoints_);
        DrawText(g, ticketText, RectF(120.0f, 618.0f, 480.0f, 34.0f), 24.0f, Color(255, 223, 238, 255),
                 StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");

        DrawText(g, L"Rキーで再開", RectF(140.0f, 668.0f, 440.0f, 34.0f), 27.0f,
                 Color(255, 255, 232, 156), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
    }

    void DrawText(Graphics& g, const std::wstring& text, const RectF& rect, float size, const Color& color,
                  StringAlignment alignment, int style, const wchar_t* preferredFont) {
        StringFormat format;
        format.SetAlignment(alignment);
        format.SetLineAlignment(StringAlignmentCenter);
        format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        FontFamily family(preferredFont);
        if (family.GetLastStatus() == Gdiplus::Ok) {
            Font font(&family, size, style, Gdiplus::UnitPixel);
            SolidBrush brush(color);
            g.DrawString(text.c_str(), -1, &font, rect, &format, &brush);
            return;
        }

        Font fallback(FontFamily::GenericSansSerif(), size, style, Gdiplus::UnitPixel);
        SolidBrush fallbackBrush(color);
        g.DrawString(text.c_str(), -1, &fallback, rect, &format, &fallbackBrush);
    }

    void FillRoundRect(Graphics& g, const RectF& rect, float radius, const Color& color) {
        GraphicsPath path;
        BuildRoundedRect(&path, rect, radius);
        SolidBrush brush(color);
        g.FillPath(&brush, &path);
    }

    bool IsDown(UINT keyCode) const {
        return keyCode < keys_.size() ? keys_[keyCode] : false;
    }

    int RandomInt(int minValue, int maxValue) {
        std::uniform_int_distribution<int> dist(minValue, maxValue);
        return dist(rng_);
    }

    float RandomFloat(float minValue, float maxValue) {
        std::uniform_real_distribution<float> dist(minValue, maxValue);
        return dist(rng_);
    }

    bool Chance(int percent) {
        return RandomInt(1, 100) <= percent;
    }

    std::filesystem::path GetModuleDirectory() const {
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        return std::filesystem::path(modulePath).parent_path();
    }

    std::vector<std::filesystem::path> BuildSearchRoots() const {
        std::vector<std::filesystem::path> roots;

        try {
            roots.push_back(std::filesystem::current_path());
        } catch (...) {
        }

        auto moduleDir = GetModuleDirectory();
        roots.push_back(moduleDir);

        std::filesystem::path cursor = moduleDir;
        for (int i = 0; i < 4; ++i) {
            if (!cursor.has_parent_path()) {
                break;
            }
            cursor = cursor.parent_path();
            roots.push_back(cursor);
        }

        std::set<std::wstring> seen;
        std::vector<std::filesystem::path> unique;
        for (const auto& root : roots) {
            const std::wstring key = ToLower(root.wstring());
            if (seen.insert(key).second) {
                unique.push_back(root);
            }
        }

        return unique;
    }

    std::unique_ptr<Image> LoadImageFromPath(const std::filesystem::path& filePath) const {
        auto image = std::unique_ptr<Image>(Image::FromFile(filePath.c_str(), FALSE));
        if (!image || image->GetLastStatus() != Gdiplus::Ok) {
            return nullptr;
        }
        return image;
    }

    std::unique_ptr<Image> LoadImageByCandidates(const std::vector<std::wstring>& candidates) const {
        const auto roots = BuildSearchRoots();

        for (const auto& candidate : candidates) {
            const std::filesystem::path candidatePath(candidate);

            if (candidatePath.is_absolute() && std::filesystem::exists(candidatePath)) {
                auto image = LoadImageFromPath(candidatePath);
                if (image) {
                    return image;
                }
            }

            for (const auto& root : roots) {
                const auto fullPath = root / candidatePath;
                if (!std::filesystem::exists(fullPath)) {
                    continue;
                }
                auto image = LoadImageFromPath(fullPath);
                if (image) {
                    return image;
                }
            }
        }

        return nullptr;
    }

    std::filesystem::path FindFileByCandidates(const std::vector<std::wstring>& candidates) const {
        const auto roots = BuildSearchRoots();

        for (const auto& candidate : candidates) {
            const std::filesystem::path candidatePath(candidate);

            if (candidatePath.is_absolute() && std::filesystem::exists(candidatePath)) {
                return candidatePath;
            }

            for (const auto& root : roots) {
                const auto fullPath = root / candidatePath;
                if (std::filesystem::exists(fullPath)) {
                    return fullPath;
                }
            }
        }

        return {};
    }

    void LoadAssets() {
        pointIcon_ = LoadImageByCandidates(
            {L"assets/p_icon.png", L"assets/point.png", L"assets/icon_point.png", L"assets/1.png"});
        ticketIcon_ = LoadImageByCandidates(
            {L"assets/ticket_icon.png", L"assets/ticket.png", L"assets/icon_ticket.png", L"assets/3.png"});

        bgmPath_ = FindFileByCandidates({L"assets/BGM/bgm_loop.wav", L"assets/bgm_loop.wav"});

        if (bgmPath_.empty()) {
            statusText_ = L"assets/BGM/bgm_loop.wav が見つかりません";
        }
    }

    void StartBgm() {
        if (!bgmEnabled_) {
            return;
        }
        if (bgmPath_.empty()) {
            return;
        }

        const BOOL ok = PlaySoundW(bgmPath_.c_str(), nullptr,
                                   SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
        if (!ok) {
            statusText_ = L"BGM の再生に失敗";
        }
    }

    void StopBgm() {
        PlaySoundW(nullptr, nullptr, 0);
    }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    ULONG_PTR gdiplusToken_ = 0;

    int clientWidth_ = kDesignWidth;
    int clientHeight_ = kDesignHeight;
    float scale_ = 1.0f;
    float offsetX_ = 0.0f;
    float offsetY_ = 0.0f;

    std::mt19937 rng_{std::random_device{}()};
    std::array<bool, 256> keys_{};

    float playerX_ = static_cast<float>(kDesignWidth) * 0.5f;
    float playerY_ = static_cast<float>(kDesignHeight) - 135.0f;
    int playerHp_ = 5;

    int ticketPoints_ = 0;
    int powerLevel_ = 1;
    int score_ = 0;

    float fireCooldown_ = 0.0f;
    float enemySpawnCooldown_ = 0.0f;
    float ticketSpawnCooldown_ = 0.0f;

    bool gameOver_ = false;
    bool bgmEnabled_ = true;

    std::wstring statusText_ = L"起動中...";

    std::vector<Bullet> bullets_;
    std::vector<Enemy> enemies_;
    std::vector<TicketDrop> tickets_;
    std::vector<Star> stars_;

    std::unique_ptr<Image> pointIcon_;
    std::unique_ptr<Image> ticketIcon_;
    std::filesystem::path bgmPath_;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware();

    GameApp app;
    if (!app.Initialize(instance, showCommand)) {
        MessageBoxW(nullptr, L"Ticket Shooter の初期化に失敗しました。", L"Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    return app.Run();
}
