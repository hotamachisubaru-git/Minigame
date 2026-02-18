#pragma once

#include "asset_catalog.h"
#include "game_types.h"
#include "stage_book.h"

#include <windows.h>
#include <gdiplus.h>

#include <array>
#include <random>
#include <string>
#include <vector>

class ShooterGame {
public:
    void Initialize();
    void Shutdown();

    void Update(float dt);
    void Render(HDC hdc, int clientWidth, int clientHeight);

    void HandleKeyDown(UINT keyCode);
    void HandleKeyUp(UINT keyCode);
    void ClearInput();

private:
    void ResetRun();
    void BeginNextStage();
    void SpawnBoss();
    void CompleteStage();

    void UpdatePlayerMovement(float dt);
    void UpdateShooting(float dt);
    void UpdateSpawning(float dt);
    void UpdateBullets(float dt);
    void UpdateEnemies(float dt);
    void UpdateBoss(float dt);
    void UpdateEnemyBullets(float dt);
    void UpdateTickets(float dt);
    void UpdateStars(float dt);
    void ResolveCollisions();
    void CleanupEntities();

    void FireShotPattern();
    void SpawnBossShotPattern();
    void DamagePlayer(int amount, const wchar_t* reason);

    void InitStars();
    bool IsDown(UINT keyCode) const;
    int RandomInt(int minValue, int maxValue);
    float RandomFloat(float minValue, float maxValue);
    bool Chance(int percent);

    void DrawScene(Gdiplus::Graphics& g);
    void DrawStars(Gdiplus::Graphics& g);
    void DrawPlayer(Gdiplus::Graphics& g);
    void DrawBullets(Gdiplus::Graphics& g);
    void DrawEnemies(Gdiplus::Graphics& g);
    void DrawBoss(Gdiplus::Graphics& g);
    void DrawEnemyBullets(Gdiplus::Graphics& g);
    void DrawTickets(Gdiplus::Graphics& g);
    void DrawHud(Gdiplus::Graphics& g);
    void DrawPauseOverlay(Gdiplus::Graphics& g);
    void DrawGameOver(Gdiplus::Graphics& g);
    void DrawGameClear(Gdiplus::Graphics& g);
    void DrawStageTransition(Gdiplus::Graphics& g);

    void StartBgm();
    void StopBgm();

    AssetCatalog assets_;
    StageBook stageBook_;
    std::mt19937 rng_{std::random_device{}()};
    std::array<bool, 256> keys_{};

    float playerX_ = static_cast<float>(kDesignWidth) * 0.5f;
    float playerY_ = static_cast<float>(kDesignHeight) - 140.0f;
    int playerHp_ = 5;
    float playerInvincibleTimer_ = 0.0f;

    int ticketPoints_ = 0;
    int powerLevel_ = 1;
    int score_ = 0;

    int currentStage_ = 1;
    float stageElapsed_ = 0.0f;
    bool stageTransition_ = false;
    float stageTransitionTimer_ = 0.0f;

    bool paused_ = false;
    bool gameOver_ = false;
    bool gameClear_ = false;
    bool bgmEnabled_ = true;

    float fireCooldown_ = 0.0f;
    float enemySpawnCooldown_ = 0.0f;
    float ticketSpawnCooldown_ = 0.0f;

    std::wstring statusText_ = L"初期化中...";

    BossState boss_;
    std::vector<Bullet> bullets_;
    std::vector<Enemy> enemies_;
    std::vector<EnemyBullet> enemyBullets_;
    std::vector<TicketDrop> tickets_;
    std::vector<Star> stars_;
};
