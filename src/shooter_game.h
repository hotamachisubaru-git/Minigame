#pragma once

#include "asset_catalog.h"
#include "game_types.h"
#include "stage_book.h"

#include <windows.h>
#include <gdiplus.h>

#include <array>
#include <filesystem>
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
    enum class OperationMode {
        Normal,
        Wide,
        Focus,
    };

    void ResetRun();
    void BeginNextStage();
    void SpawnBoss();
    void CompleteStage();
    void HandleShopInput(UINT keyCode);
    void HandleOperationMenuInput(UINT keyCode);
    void HandleGameOverInput(UINT keyCode);
    void HandleGameClearInput(UINT keyCode);
    void ResetStage10BossFleeState();
    void TryTriggerStage10BossFlee();
    void ApplyOperationStageJump(int stageNumber, bool toBoss);
    void SetPowerLevelFromOperation(int targetPowerLevel);
    void ContinueFromGameOver();
    void RecalculatePowerLevel();
    void CycleOperationMode();
    const wchar_t* OperationModeLabel() const;
    const wchar_t* ShotTypeLabel() const;

    void UpdatePlayerMovement(float dt);
    void UpdateShooting(float dt);
    void UpdateSpawning(float dt);
    void UpdateBullets(float dt);
    void UpdateEnemies(float dt);
    void UpdateBoss(float dt);
    void UpdateEnemyBullets(float dt);
    void UpdateTickets(float dt);
    void UpdateStars(float dt);
    void UpdateHitEffects(float dt);
    void ResolveCollisions();
    void CleanupEntities();

    void FireShotPattern();
    void SpawnBossShotPattern();
    void ActivateSpecialMove();
    void DamagePlayer(int amount, const wchar_t* reason);

    void InitStars();
    bool IsDown(UINT keyCode) const;
    int RandomInt(int minValue, int maxValue);
    float RandomFloat(float minValue, float maxValue);
    bool Chance(int percent);
    void SpawnHitEffect(float x, float y, bool bossHit);

    void DrawScene(Gdiplus::Graphics& g);
    void DrawStars(Gdiplus::Graphics& g);
    void DrawPlayer(Gdiplus::Graphics& g);
    void DrawBullets(Gdiplus::Graphics& g);
    void DrawEnemies(Gdiplus::Graphics& g);
    void DrawBoss(Gdiplus::Graphics& g);
    void DrawEnemyBullets(Gdiplus::Graphics& g);
    void DrawTickets(Gdiplus::Graphics& g);
    void DrawHitEffects(Gdiplus::Graphics& g);
    void DrawHud(Gdiplus::Graphics& g);
    void DrawShopOverlay(Gdiplus::Graphics& g);
    void DrawOperationOverlay(Gdiplus::Graphics& g);
    void DrawPauseOverlay(Gdiplus::Graphics& g);
    void DrawGameOver(Gdiplus::Graphics& g);
    void DrawGameClear(Gdiplus::Graphics& g);

    void StartBgm(int stageNumber, bool bossPhase = false);
    void StopBgm();

    AssetCatalog assets_;
    StageBook stageBook_;
    std::mt19937 rng_{std::random_device{}()};
    std::array<bool, 256> keys_{};

    float playerX_ = static_cast<float>(kDesignWidth) * 0.5f;
    float playerY_ = static_cast<float>(kDesignHeight) - 140.0f;
    int playerHp_ = 10;
    int maxPlayerHp_ = 10;
    float playerInvincibleTimer_ = 0.0f;

    int ticketPoints_ = 0;
    int totalTicketsCollected_ = 0;
    int powerLevel_ = 1;
    int attackUpgrade_ = 0;
    int score_ = 0;

    int currentStage_ = 1;
    float stageElapsed_ = 0.0f;
    OperationMode operationMode_ = OperationMode::Normal;
    bool endlessMode_ = false;
    int endlessLoop_ = 0;

    bool shopOpen_ = false;
    bool paused_ = false;
    bool gameOver_ = false;
    bool gameClear_ = false;
    bool gameClearChoiceOpen_ = false;
    int gameClearChoiceIndex_ = 0;
    int continueCount_ = 0;
    bool operationJumpToBoss_ = false;
    bool operationMenuOpen_ = false;
    int operationMenuCursor_ = 0;
    bool homingShotEnabled_ = false;
    bool bgmEnabled_ = true;
    bool stage10BossFleeActive_ = false;
    int stage10BossFleeCount_ = 0;
    long long stage10NextFleeHp_ = -1;
    float stage10FleeTargetX_ = 0.0f;
    float stage10FleeTargetY_ = 0.0f;
    float stage10FleeTimer_ = 0.0f;
    float stage10FleeLockTimer_ = 0.0f;
    float stage10FleeNoticeTimer_ = 0.0f;

    float fireCooldown_ = 0.0f;
    float enemySpawnCooldown_ = 0.0f;
    float ticketSpawnCooldown_ = 0.0f;
    float specialCooldown_ = 0.0f;
    float bossHitFxCooldown_ = 0.0f;

    std::wstring statusText_ = L"初期化中...";
    std::filesystem::path currentBgmPath_;
    bool bgmUsingMci_ = false;

    BossState boss_;
    struct HitEffect {
        float x = 0.0f;
        float y = 0.0f;
        float radius = 10.0f;
        float life = 0.0f;
        float maxLife = 0.18f;
        bool boss = false;
    };
    std::vector<HitEffect> hitEffects_;
    std::vector<Bullet> bullets_;
    std::vector<Enemy> enemies_;
    std::vector<EnemyBullet> enemyBullets_;
    std::vector<TicketDrop> tickets_;
    std::vector<Star> stars_;
};
