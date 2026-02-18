#include "shooter_game.h"

#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <sstream>

namespace {

using Gdiplus::Color;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::FontStyleBold;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::LinearGradientBrush;
using Gdiplus::Pen;
using Gdiplus::PointF;
using Gdiplus::RectF;
using Gdiplus::SolidBrush;
using Gdiplus::StringAlignment;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringAlignmentNear;
using Gdiplus::StringFormat;

constexpr float kPi = 3.14159265358979323846f;
constexpr int kInitialPlayerHp = 10;
constexpr int kPowerTicketStep = 3;
constexpr int kAttackUpgradeCap = 24;
constexpr int kMaxPowerLevel = 99;
constexpr int kMaxPlayerHp = 99;
constexpr int kMaxScore = 9999999;
constexpr int kMaxTicketPoints = 999;
constexpr int kOperationMenuItemCount = 7;
constexpr int kOperationTicketAdjustStep = 5;
constexpr int kMaxBulletsPerShot = 18;
constexpr int kMaxActivePlayerBullets = 420;
constexpr int kMaxHitEffects = 220;
constexpr float kMinShotInterval = 0.040f;
constexpr float kBossHitFxInterval = 0.028f;
constexpr float kSpecialCooldownSec = 10.0f;
constexpr float kSpecialInvincibleSec = 1.5f;
constexpr float kSpecialBossStunSec = 1.0f;
constexpr float kHomingTurnRate = 7.5f;
constexpr float kInvincibleMinSec = 5.0f;
constexpr float kInvincibleMaxSec = 8.0f;
constexpr float kContinueGraceSec = 4.0f;
constexpr int kStage10BossRounds = 10;
constexpr float kStage10DefenseMaxRate = 0.70f;
constexpr float kStage10DefenseGraceSec = 4.0f;
constexpr float kStage10DefenseRampSec = 18.0f;
constexpr float kStage10FleeLockSec = 0.72f;
constexpr float kStage10FleeNoticeSec = 1.8f;
constexpr long long kBossDamageScaleBase = 100LL;
constexpr long long kBossDamageScalePerStage = 40LL;
constexpr const wchar_t* kBgmAlias = L"pticket_bgm";

std::wstring FormatNumber(long long value) {
    std::wstring text = std::to_wstring(value);
    for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3) {
        text.insert(i, L",");
    }
    return text;
}

float Clamp(float v, float low, float high) {
    return std::max(low, std::min(v, high));
}

float DistanceSq(float x1, float y1, float x2, float y2) {
    const float dx = x1 - x2;
    const float dy = y1 - y2;
    return dx * dx + dy * dy;
}

int AddScoreCapped(int currentScore, int delta) {
    const long long next = static_cast<long long>(currentScore) + static_cast<long long>(delta);
    return static_cast<int>(std::min<long long>(kMaxScore, next));
}

bool IsCheatDetected(int powerLevel, int playerHp, int maxPlayerHp, int score) {
    return powerLevel > kMaxPowerLevel || playerHp > kMaxPlayerHp || maxPlayerHp > kMaxPlayerHp ||
           score > kMaxScore;
}

void ForceQuitForCheat() {
    ::ExitProcess(1);
}

long long SafeMultiplyClamped(long long a, long long b) {
    if (a <= 0 || b <= 0) {
        return 0LL;
    }
    constexpr long long kLLMax = std::numeric_limits<long long>::max();
    if (a > kLLMax / b) {
        return kLLMax;
    }
    return a * b;
}

float Stage10DefenseRate(float roundElapsedSec) {
    const float afterGrace = std::max(0.0f, roundElapsedSec - kStage10DefenseGraceSec);
    const float progress = Clamp(afterGrace / kStage10DefenseRampSec, 0.0f, 1.0f);
    return progress * kStage10DefenseMaxRate;
}

long long Stage10RoundHp(long long totalHp, int roundIndex) {
    const int safeIndex = std::clamp(roundIndex, 0, kStage10BossRounds - 1);
    const long long base = totalHp / static_cast<long long>(kStage10BossRounds);
    const long long remain = totalHp % static_cast<long long>(kStage10BossRounds);
    const long long bonus = (static_cast<long long>(safeIndex) < remain) ? 1LL : 0LL;
    return std::max(1LL, base + bonus);
}

void BuildRoundedRect(GraphicsPath* path, const RectF& rect, float radius) {
    if (radius <= 0.0f) {
        path->Reset();
        path->AddRectangle(rect);
        return;
    }

    const float d = radius * 2.0f;
    path->Reset();
    path->AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path->AddArc(rect.GetRight() - d, rect.Y, d, d, 270.0f, 90.0f);
    path->AddArc(rect.GetRight() - d, rect.GetBottom() - d, d, d, 0.0f, 90.0f);
    path->AddArc(rect.X, rect.GetBottom() - d, d, d, 90.0f, 90.0f);
    path->CloseFigure();
}

void FillRoundRect(Graphics& g, const RectF& rect, float radius, const Color& color) {
    GraphicsPath path;
    BuildRoundedRect(&path, rect, radius);
    SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

void DrawText(Graphics& g, const std::wstring& text, const RectF& rect, float size, const Color& color,
              StringAlignment align, int style, const wchar_t* preferredFont) {
    StringFormat format;
    format.SetAlignment(align);
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
    SolidBrush brush(color);
    g.DrawString(text.c_str(), -1, &fallback, rect, &format, &brush);
}

}  // namespace

void ShooterGame::Initialize() {
    assets_.Load();
    InitStars();
    ResetRun();
}

void ShooterGame::Shutdown() {
    StopBgm();
}

void ShooterGame::ResetRun() {
    bullets_.clear();
    enemies_.clear();
    enemyBullets_.clear();
    tickets_.clear();
    hitEffects_.clear();
    bullets_.reserve(kMaxActivePlayerBullets + 32);

    boss_ = BossState{};

    playerX_ = static_cast<float>(kDesignWidth) * 0.5f;
    playerY_ = static_cast<float>(kDesignHeight) - 140.0f;
    maxPlayerHp_ = kInitialPlayerHp;
    playerHp_ = maxPlayerHp_;
    playerInvincibleTimer_ = 0.0f;

    ticketPoints_ = 0;
    totalTicketsCollected_ = 0;
    powerLevel_ = 1;
    attackUpgrade_ = 0;
    score_ = 0;

    currentStage_ = 1;
    stageElapsed_ = 0.0f;
    operationMode_ = OperationMode::Normal;
    endlessMode_ = false;
    endlessLoop_ = 0;

    shopOpen_ = false;
    paused_ = false;
    gameOver_ = false;
    gameClear_ = false;
    gameClearChoiceOpen_ = false;
    gameClearChoiceIndex_ = 0;
    continueCount_ = 0;
    operationJumpToBoss_ = false;
    homingShotEnabled_ = false;
    operationMenuOpen_ = false;
    operationMenuCursor_ = 0;
    ResetStage10BossFleeState();

    fireCooldown_ = 0.06f;
    enemySpawnCooldown_ = 0.6f;
    ticketSpawnCooldown_ = 1.1f;
    specialCooldown_ = 0.0f;
    bossHitFxCooldown_ = 0.0f;

    statusText_ = L"ステージ1 開始";

    if (bgmEnabled_) {
        StartBgm(currentStage_, false);
    }
}

void ShooterGame::HandleKeyDown(UINT keyCode) {
    bool wasDown = false;
    if (keyCode < keys_.size()) {
        wasDown = keys_[keyCode];
        keys_[keyCode] = true;
    }
    if (wasDown) {
        return;
    }

    if (keyCode == 'R') {
        ResetRun();
        return;
    }

    if (gameClear_) {
        HandleGameClearInput(keyCode);
        return;
    }

    if (gameOver_) {
        HandleGameOverInput(keyCode);
        return;
    }

    if (keyCode == 'O') {
        operationMenuOpen_ = !operationMenuOpen_;
        operationMenuCursor_ = std::clamp(operationMenuCursor_, 0, kOperationMenuItemCount - 1);
        statusText_ = operationMenuOpen_ ? L"オペレーション設定を開きました" : L"オペレーション設定を閉じました";
        if (operationMenuOpen_) {
            paused_ = false;
        }
        return;
    }

    if (operationMenuOpen_) {
        HandleOperationMenuInput(keyCode);
        return;
    }

    if (shopOpen_) {
        HandleShopInput(keyCode);
        return;
    }

    if (keyCode == 'P') {
        if (!gameOver_ && !gameClear_) {
            paused_ = !paused_;
            statusText_ = paused_ ? L"ポーズ中 (Pで再開)" : L"ポーズ解除";
        }
        return;
    }

    if (keyCode == 'M') {
        bgmEnabled_ = !bgmEnabled_;
        if (bgmEnabled_) {
            StartBgm(currentStage_, boss_.active);
            statusText_ = L"BGM を有効にしました";
        } else {
            StopBgm();
            statusText_ = L"BGM を無効にしました";
        }
        return;
    }

    if (keyCode == 'H') {
        homingShotEnabled_ = !homingShotEnabled_;
        statusText_ = std::wstring(L"ショットタイプ: ") + ShotTypeLabel();
        return;
    }

    if (keyCode == 'X') {
        ActivateSpecialMove();
        return;
    }
}

void ShooterGame::HandleKeyUp(UINT keyCode) {
    if (keyCode < keys_.size()) {
        keys_[keyCode] = false;
    }
}

void ShooterGame::ClearInput() {
    keys_.fill(false);
}

void ShooterGame::HandleShopInput(UINT keyCode) {
    if (!shopOpen_) {
        return;
    }

    constexpr int kAtkCost = 2;
    constexpr int kHpCost = 3;

    if (keyCode == '3') {
        CycleOperationMode();
        return;
    }

    if (keyCode == '1') {
        if (ticketPoints_ < kAtkCost) {
            statusText_ = L"チケット不足: 火力強化には2枚必要";
            return;
        }
        if (attackUpgrade_ >= kAttackUpgradeCap) {
            statusText_ = L"火力強化は上限です";
            return;
        }
        ticketPoints_ -= kAtkCost;
        ++attackUpgrade_;
        RecalculatePowerLevel();
        statusText_ = L"火力強化を購入: 攻撃力+" + std::to_wstring(attackUpgrade_) + L" / 射角拡張";
        return;
    }

    if (keyCode == '2') {
        if (ticketPoints_ < kHpCost) {
            statusText_ = L"チケット不足: 最大HP強化には3枚必要";
            return;
        }
        if (maxPlayerHp_ >= kMaxPlayerHp) {
            statusText_ = L"最大HPは上限です";
            return;
        }
        ticketPoints_ -= kHpCost;
        ++maxPlayerHp_;
        playerHp_ = maxPlayerHp_;
        RecalculatePowerLevel();
        statusText_ = L"最大HP強化を購入: HP " + std::to_wstring(maxPlayerHp_) + L" / 射角拡張";
        return;
    }

    if (keyCode == VK_RETURN || keyCode == VK_SPACE || keyCode == 'N') {
        shopOpen_ = false;
        BeginNextStage();
    }
}

void ShooterGame::HandleGameOverInput(UINT keyCode) {
    if (!gameOver_) {
        return;
    }

    if (keyCode == 'C' || keyCode == VK_RETURN || keyCode == VK_SPACE) {
        ContinueFromGameOver();
    }
}

void ShooterGame::HandleOperationMenuInput(UINT keyCode) {
    if (!operationMenuOpen_) {
        return;
    }

    if (keyCode == VK_ESCAPE || keyCode == VK_BACK || keyCode == VK_RETURN || keyCode == VK_SPACE) {
        operationMenuOpen_ = false;
        statusText_ = L"オペレーション設定を閉じました";
        return;
    }

    if (keyCode == VK_UP || keyCode == 'W') {
        operationMenuCursor_ = (operationMenuCursor_ + kOperationMenuItemCount - 1) % kOperationMenuItemCount;
        return;
    }
    if (keyCode == VK_DOWN || keyCode == 'S') {
        operationMenuCursor_ = (operationMenuCursor_ + 1) % kOperationMenuItemCount;
        return;
    }

    int delta = 0;
    if (keyCode == VK_LEFT || keyCode == 'A') {
        delta = -1;
    } else if (keyCode == VK_RIGHT || keyCode == 'D') {
        delta = 1;
    }
    if (delta == 0) {
        return;
    }

    switch (operationMenuCursor_) {
    case 0: {
        const int prev = attackUpgrade_;
        attackUpgrade_ = std::clamp(attackUpgrade_ + delta, 0, kAttackUpgradeCap);
        if (attackUpgrade_ != prev) {
            RecalculatePowerLevel();
            statusText_ = L"オペレーション: 攻撃力 " + std::to_wstring(attackUpgrade_);
        }
        break;
    }
    case 1: {
        const int targetPower = std::clamp(powerLevel_ + delta, 1, kMaxPowerLevel);
        SetPowerLevelFromOperation(targetPower);
        statusText_ = L"オペレーション: 強化Lv " + std::to_wstring(powerLevel_);
        break;
    }
    case 2: {
        const int prevMax = maxPlayerHp_;
        maxPlayerHp_ = std::clamp(maxPlayerHp_ + delta, 1, kMaxPlayerHp);
        if (maxPlayerHp_ != prevMax) {
            if (delta > 0) {
                playerHp_ = maxPlayerHp_;
            } else {
                playerHp_ = std::clamp(playerHp_, 1, maxPlayerHp_);
            }
            RecalculatePowerLevel();
            statusText_ = L"オペレーション: HP " + std::to_wstring(playerHp_) + L"/" + std::to_wstring(maxPlayerHp_);
        }
        break;
    }
    case 3:
        operationJumpToBoss_ = (delta > 0);
        statusText_ = std::wstring(L"オペレーション: 移動先 ") +
                      (operationJumpToBoss_ ? L"ステージボス" : L"ステージ");
        break;
    case 4: {
        const int targetStage = std::clamp(currentStage_ + delta, 1, stageBook_.TotalStages());
        if (targetStage != currentStage_) {
            ApplyOperationStageJump(targetStage, operationJumpToBoss_);
        }
        break;
    }
    case 5:
        if (delta > 0) {
            CycleOperationMode();
        } else {
            switch (operationMode_) {
            case OperationMode::Normal:
                operationMode_ = OperationMode::Focus;
                break;
            case OperationMode::Wide:
                operationMode_ = OperationMode::Normal;
                break;
            case OperationMode::Focus:
            default:
                operationMode_ = OperationMode::Wide;
                break;
            }
            statusText_ = std::wstring(L"オペレーションモード: ") + OperationModeLabel();
        }
        break;
    case 6: {
        const long long next = static_cast<long long>(ticketPoints_) +
                               static_cast<long long>(delta) * static_cast<long long>(kOperationTicketAdjustStep);
        ticketPoints_ = static_cast<int>(std::clamp(next, 0LL, static_cast<long long>(kMaxTicketPoints)));
        statusText_ = L"オペレーション: チケット " + FormatNumber(ticketPoints_);
        break;
    }
    default:
        break;
    }
}

void ShooterGame::ApplyOperationStageJump(int stageNumber, bool toBoss) {
    currentStage_ = std::clamp(stageNumber, 1, stageBook_.TotalStages());
    const StageSpec& spec = stageBook_.Get(currentStage_);
    stageElapsed_ = 0.0f;
    boss_ = BossState{};
    ResetStage10BossFleeState();
    bullets_.clear();
    enemies_.clear();
    enemyBullets_.clear();
    tickets_.clear();
    hitEffects_.clear();
    shopOpen_ = false;
    gameOver_ = false;
    gameClear_ = false;
    gameClearChoiceOpen_ = false;
    playerHp_ = std::clamp(playerHp_, 1, maxPlayerHp_);
    enemySpawnCooldown_ = 0.28f;
    ticketSpawnCooldown_ = 0.85f;
    if (toBoss) {
        stageElapsed_ = spec.stageDurationSec;
        SpawnBoss();
        statusText_ = L"オペレーション: ステージ" + std::to_wstring(currentStage_) + L" ボスへ移動";
    } else {
        statusText_ = L"オペレーション: ステージ" + std::to_wstring(currentStage_) + L"へ移動";
        if (bgmEnabled_) {
            StartBgm(currentStage_, false);
        }
    }
}

void ShooterGame::SetPowerLevelFromOperation(int targetPowerLevel) {
    const int clampedTarget = std::clamp(targetPowerLevel, 1, kMaxPowerLevel);
    const int hpUpgradeCount = std::max(0, maxPlayerHp_ - kInitialPlayerHp);
    const int baseLevel = 1 + attackUpgrade_ + hpUpgradeCount / 2;
    const int levelFromTickets = std::max(0, clampedTarget - baseLevel);
    totalTicketsCollected_ = levelFromTickets * kPowerTicketStep;
    RecalculatePowerLevel();
}

void ShooterGame::ContinueFromGameOver() {
    if (!gameOver_) {
        return;
    }

    ++continueCount_;
    gameOver_ = false;
    paused_ = false;
    operationMenuOpen_ = false;
    playerX_ = static_cast<float>(kDesignWidth) * 0.5f;
    playerY_ = static_cast<float>(kDesignHeight) - 140.0f;
    playerHp_ = maxPlayerHp_;
    playerInvincibleTimer_ = kContinueGraceSec;

    bullets_.clear();
    enemies_.clear();
    enemyBullets_.clear();
    tickets_.clear();
    hitEffects_.clear();

    if (boss_.active) {
        const StageSpec& spec = stageBook_.Get(currentStage_);
        boss_.shotCooldown = std::max(spec.bossShotInterval * 0.75f, 0.45f);
    } else {
        enemySpawnCooldown_ = std::max(enemySpawnCooldown_, 0.35f);
        ticketSpawnCooldown_ = std::max(ticketSpawnCooldown_, 0.70f);
    }

    statusText_ = L"コンティニュー x" + std::to_wstring(continueCount_) + L" (無敵 " +
                  std::to_wstring(static_cast<int>(kContinueGraceSec)) + L" 秒)";
}

void ShooterGame::HandleGameClearInput(UINT keyCode) {
    if (!gameClear_ || !gameClearChoiceOpen_) {
        return;
    }

    if (keyCode == VK_UP || keyCode == VK_LEFT) {
        gameClearChoiceIndex_ = std::max(0, gameClearChoiceIndex_ - 1);
        return;
    }
    if (keyCode == VK_DOWN || keyCode == VK_RIGHT) {
        gameClearChoiceIndex_ = std::min(1, gameClearChoiceIndex_ + 1);
        return;
    }
    if (keyCode == '1') {
        gameClearChoiceIndex_ = 0;
    } else if (keyCode == '2') {
        gameClearChoiceIndex_ = 1;
    } else if (keyCode != VK_RETURN && keyCode != VK_SPACE) {
        return;
    }

    if (gameClearChoiceIndex_ == 0) {
        StopBgm();
        ::ExitProcess(0);
        return;
    }

    gameClear_ = false;
    gameClearChoiceOpen_ = false;
    endlessMode_ = true;
    ++endlessLoop_;
    currentStage_ = 1;
    stageElapsed_ = 0.0f;
    boss_ = BossState{};
    ResetStage10BossFleeState();
    bullets_.clear();
    enemies_.clear();
    enemyBullets_.clear();
    tickets_.clear();
    playerHp_ = maxPlayerHp_;
    enemySpawnCooldown_ = 0.6f;
    ticketSpawnCooldown_ = 1.0f;
    statusText_ = L"ENDLESS " + std::to_wstring(endlessLoop_) + L" - ステージ1 開始";
    if (bgmEnabled_) {
        StartBgm(currentStage_, false);
    }
}

void ShooterGame::ResetStage10BossFleeState() {
    stage10BossFleeActive_ = false;
    stage10BossFleeCount_ = 0;
    stage10NextFleeHp_ = -1;
    stage10FleeTargetX_ = 0.0f;
    stage10FleeTargetY_ = 0.0f;
    stage10FleeTimer_ = 0.0f;
    stage10FleeLockTimer_ = 0.0f;
    stage10FleeNoticeTimer_ = 0.0f;
}

void ShooterGame::TryTriggerStage10BossFlee() {
    if (!boss_.active || currentStage_ != stageBook_.TotalStages()) {
        return;
    }
    if (stage10BossFleeActive_) {
        return;
    }
    if (stage10NextFleeHp_ <= 0 || boss_.hp > stage10NextFleeHp_) {
        return;
    }

    stage10BossFleeActive_ = true;
    stage10FleeLockTimer_ = kStage10FleeLockSec;
    stage10FleeTimer_ = 0.0f;

    const float center = static_cast<float>(kDesignWidth) * 0.5f;
    const float margin = 94.0f;
    if (playerX_ < center) {
        stage10FleeTargetX_ = RandomFloat(center + 72.0f, static_cast<float>(kDesignWidth) - margin);
    } else {
        stage10FleeTargetX_ = RandomFloat(margin, center - 72.0f);
    }
    stage10FleeTargetY_ = RandomFloat(86.0f, 196.0f);

    boss_.x = stage10FleeTargetX_;
    boss_.y = stage10FleeTargetY_;
    boss_.shotCooldown = std::max(boss_.shotCooldown, 0.58f);
    enemyBullets_.clear();
    SpawnHitEffect(boss_.x, boss_.y, true);
    stage10NextFleeHp_ = -1;
    stage10FleeNoticeTimer_ = kStage10FleeNoticeSec;
    statusText_ = L"ボスが逃げた！！追いかけろ！！";
}

void ShooterGame::RecalculatePowerLevel() {
    const int hpUpgradeCount = std::max(0, maxPlayerHp_ - kInitialPlayerHp);
    const int levelFromTickets = totalTicketsCollected_ / kPowerTicketStep;
    const int levelFromUpgrades = attackUpgrade_ + hpUpgradeCount / 2;
    powerLevel_ = std::min(kMaxPowerLevel, 1 + levelFromTickets + levelFromUpgrades);
}

void ShooterGame::CycleOperationMode() {
    switch (operationMode_) {
    case OperationMode::Normal:
        operationMode_ = OperationMode::Wide;
        break;
    case OperationMode::Wide:
        operationMode_ = OperationMode::Focus;
        break;
    case OperationMode::Focus:
    default:
        operationMode_ = OperationMode::Normal;
        break;
    }
    statusText_ = std::wstring(L"オペレーションモード: ") + OperationModeLabel();
}

const wchar_t* ShooterGame::OperationModeLabel() const {
    switch (operationMode_) {
    case OperationMode::Wide:
        return L"WIDE";
    case OperationMode::Focus:
        return L"FOCUS";
    case OperationMode::Normal:
    default:
        return L"NORMAL";
    }
}

const wchar_t* ShooterGame::ShotTypeLabel() const {
    return homingShotEnabled_ ? L"HOMING" : L"STRAIGHT";
}

void ShooterGame::Update(float dt) {
    if (IsCheatDetected(powerLevel_, playerHp_, maxPlayerHp_, score_)) {
        StopBgm();
        ForceQuitForCheat();
        return;
    }

    if (shopOpen_ && boss_.active) {
        boss_ = BossState{};
        ResetStage10BossFleeState();
        enemies_.clear();
        enemyBullets_.clear();
        bullets_.clear();
        hitEffects_.clear();
        statusText_ = L"ステージ" + std::to_wstring(currentStage_) + L" クリア! ショップを開きました";
    }
    if (gameClear_ && boss_.active) {
        boss_ = BossState{};
        ResetStage10BossFleeState();
        enemies_.clear();
        enemyBullets_.clear();
        bullets_.clear();
        hitEffects_.clear();
    }

    if (paused_) {
        return;
    }

    UpdateStars(dt);
    UpdateHitEffects(dt);

    if (operationMenuOpen_) {
        return;
    }

    if (shopOpen_) {
        return;
    }

    if (gameOver_ || gameClear_) {
        return;
    }

    if (playerInvincibleTimer_ > 0.0f) {
        playerInvincibleTimer_ = std::max(0.0f, playerInvincibleTimer_ - dt);
    }
    if (specialCooldown_ > 0.0f) {
        specialCooldown_ = std::max(0.0f, specialCooldown_ - dt);
    }
    if (stage10FleeNoticeTimer_ > 0.0f) {
        stage10FleeNoticeTimer_ = std::max(0.0f, stage10FleeNoticeTimer_ - dt);
    }

    stageElapsed_ += dt;

    UpdatePlayerMovement(dt);
    UpdateShooting(dt);
    UpdateSpawning(dt);
    UpdateBullets(dt);
    UpdateEnemies(dt);
    UpdateBoss(dt);
    UpdateEnemyBullets(dt);
    UpdateTickets(dt);
    ResolveCollisions();
    CleanupEntities();

    RecalculatePowerLevel();

    if (!boss_.active && !shopOpen_ && !gameOver_ && !gameClear_) {
        const StageSpec& spec = stageBook_.Get(currentStage_);
        if (stageElapsed_ >= spec.stageDurationSec) {
            SpawnBoss();
        }
    }

    if (playerHp_ <= 0) {
        playerHp_ = 0;
        gameOver_ = true;
        statusText_ = L"ゲームオーバー - Rキーでリトライ";
    }
}

void ShooterGame::BeginNextStage() {
    stageElapsed_ = 0.0f;
    boss_ = BossState{};
    ResetStage10BossFleeState();
    enemies_.clear();
    enemyBullets_.clear();
    bullets_.clear();
    hitEffects_.clear();

    ++currentStage_;
    if (currentStage_ > stageBook_.TotalStages()) {
        currentStage_ = 1;
        if (!endlessMode_) {
            endlessMode_ = true;
        }
        ++endlessLoop_;
    }

    enemySpawnCooldown_ = 0.6f;
    ticketSpawnCooldown_ = 1.0f;
    if (endlessMode_) {
        statusText_ = L"ENDLESS " + std::to_wstring(endlessLoop_) + L" - ステージ" +
                      std::to_wstring(currentStage_) + L" 開始";
    } else {
        statusText_ = L"ステージ" + std::to_wstring(currentStage_) + L" 開始";
    }
    if (bgmEnabled_) {
        StartBgm(currentStage_, false);
    }
}

void ShooterGame::SpawnBoss() {
    const StageSpec& spec = stageBook_.Get(currentStage_);
    ResetStage10BossFleeState();
    boss_.active = true;
    boss_.x = static_cast<float>(kDesignWidth) * 0.5f;
    boss_.y = 180.0f;
    boss_.radius = 54.0f;
    boss_.movePhase = 0.0f;
    boss_.shotCooldown = spec.bossShotInterval;
    if (currentStage_ == stageBook_.TotalStages()) {
        const long long roundHp = Stage10RoundHp(spec.bossHp, stage10BossFleeCount_);
        boss_.hp = roundHp;
        boss_.maxHp = roundHp;
        const long long fleeTriggerGap = std::max(1LL, roundHp / 3LL);
        stage10NextFleeHp_ = std::max(1LL, roundHp - fleeTriggerGap);
    } else {
        boss_.hp = spec.bossHp;
        boss_.maxHp = spec.bossHp;
        stage10NextFleeHp_ = -1;
    }
    stage10FleeTimer_ = 0.0f;

    enemies_.clear();
    enemyBullets_.clear();
    if (currentStage_ == stageBook_.TotalStages()) {
        statusText_ = L"ステージ10 ボス出現: " + spec.bossName + L" 第1/" + std::to_wstring(kStage10BossRounds) +
                      L"戦";
    } else {
        statusText_ = L"ステージ" + std::to_wstring(currentStage_) + L" ボス出現: " + spec.bossName;
    }
    if (bgmEnabled_) {
        StartBgm(currentStage_, true);
    }
}

void ShooterGame::CompleteStage() {
    score_ = AddScoreCapped(score_, 1500 + currentStage_ * 550);
    boss_ = BossState{};
    enemies_.clear();
    enemyBullets_.clear();
    bullets_.clear();
    hitEffects_.clear();

    if (!endlessMode_ && currentStage_ >= stageBook_.TotalStages()) {
        gameClear_ = true;
        gameClearChoiceOpen_ = true;
        gameClearChoiceIndex_ = 1;
        shopOpen_ = false;
        statusText_ = L"全10ステージ制覇! 進行先を選択してください";
        return;
    }

    playerHp_ = maxPlayerHp_;
    shopOpen_ = true;
    statusText_ = L"ステージ" + std::to_wstring(currentStage_) + L" クリア! ショップを開きました";
    if (bgmEnabled_) {
        StartBgm(currentStage_, false);
    }
}

void ShooterGame::UpdatePlayerMovement(float dt) {
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
        const float len = std::sqrt(moveX * moveX + moveY * moveY);
        moveX /= len;
        moveY /= len;
    }

    float speed = 390.0f + static_cast<float>(powerLevel_ - 1) * 12.0f;
    if (IsDown(VK_SHIFT) || IsDown(VK_LSHIFT) || IsDown(VK_RSHIFT)) {
        speed *= 0.45f;
    }

    playerX_ += moveX * speed * dt;
    playerY_ += moveY * speed * dt;

    playerX_ = Clamp(playerX_, 34.0f, static_cast<float>(kDesignWidth) - 34.0f);
    playerY_ = Clamp(playerY_, 180.0f, static_cast<float>(kDesignHeight) - 36.0f);
}

void ShooterGame::UpdateShooting(float dt) {
    fireCooldown_ -= dt;
    float interval = 0.28f - static_cast<float>(powerLevel_ - 1) * 0.022f -
                     static_cast<float>(attackUpgrade_) * 0.006f;
    if (operationMode_ == OperationMode::Wide) {
        interval += 0.010f;
    } else if (operationMode_ == OperationMode::Focus) {
        interval -= 0.010f;
    }
    interval = Clamp(interval, kMinShotInterval, 0.28f);

    if (!IsDown('Z')) {
        fireCooldown_ = std::max(0.0f, fireCooldown_);
        return;
    }

    while (fireCooldown_ <= 0.0f) {
        FireShotPattern();
        fireCooldown_ += interval;
    }
}

void ShooterGame::UpdateSpawning(float dt) {
    const StageSpec& spec = stageBook_.Get(currentStage_);

    if (!boss_.active && stageElapsed_ < spec.stageDurationSec) {
        enemySpawnCooldown_ -= dt;
        if (enemySpawnCooldown_ <= 0.0f) {
            Enemy enemy;
            enemy.x = RandomFloat(40.0f, static_cast<float>(kDesignWidth) - 40.0f);
            enemy.y = -42.0f;
            enemy.speed = RandomFloat(spec.enemySpeedMin, spec.enemySpeedMax);
            enemy.hp = RandomInt(spec.enemyHpMin, spec.enemyHpMax);
            enemy.radius = RandomFloat(20.0f, 30.0f) + static_cast<float>(enemy.hp - 1) * 1.4f;
            enemy.wobble = RandomFloat(0.0f, kPi * 2.0f);
            enemies_.push_back(enemy);

            enemySpawnCooldown_ = RandomFloat(spec.enemySpawnMinSec, spec.enemySpawnMaxSec);
        }
    }

    ticketSpawnCooldown_ -= dt;
    if (ticketSpawnCooldown_ <= 0.0f) {
        TicketDrop ticket;
        ticket.x = RandomFloat(45.0f, static_cast<float>(kDesignWidth) - 45.0f);
        ticket.y = -34.0f;
        ticket.speed = RandomFloat(spec.ticketSpeedMin, spec.ticketSpeedMax);
        ticket.size = RandomFloat(30.0f, 38.0f);
        tickets_.push_back(ticket);

        ticketSpawnCooldown_ = RandomFloat(spec.ticketSpawnMinSec, spec.ticketSpawnMaxSec);
    }
}

void ShooterGame::UpdateBullets(float dt) {
    const bool canHome = homingShotEnabled_ && (boss_.active || !enemies_.empty());
    for (auto& bullet : bullets_) {
        if (canHome) {
            float targetX = 0.0f;
            float targetY = 0.0f;
            float bestDistSq = std::numeric_limits<float>::max();
            bool foundTarget = false;

            if (boss_.active) {
                bestDistSq = DistanceSq(bullet.x, bullet.y, boss_.x, boss_.y);
                targetX = boss_.x;
                targetY = boss_.y;
                foundTarget = true;
            }

            for (const auto& enemy : enemies_) {
                if (!enemy.active) {
                    continue;
                }
                const float d = DistanceSq(bullet.x, bullet.y, enemy.x, enemy.y);
                if (!foundTarget || d < bestDistSq) {
                    bestDistSq = d;
                    targetX = enemy.x;
                    targetY = enemy.y;
                    foundTarget = true;
                }
            }

            if (foundTarget) {
                const float dx = targetX - bullet.x;
                const float dy = targetY - bullet.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.001f) {
                    const float speed = std::max(160.0f, std::sqrt(bullet.vx * bullet.vx + bullet.vy * bullet.vy));
                    const float desiredVx = (dx / len) * speed;
                    const float desiredVy = (dy / len) * speed;
                    const float turn = Clamp(dt * kHomingTurnRate, 0.0f, 1.0f);
                    bullet.vx += (desiredVx - bullet.vx) * turn;
                    bullet.vy += (desiredVy - bullet.vy) * turn;
                }
            }
        }

        bullet.x += bullet.vx * dt;
        bullet.y += bullet.vy * dt;
    }
}

void ShooterGame::UpdateEnemies(float dt) {
    for (auto& enemy : enemies_) {
        enemy.wobble += dt * 4.2f;
        enemy.x += std::sin(enemy.wobble) * 25.0f * dt;
        enemy.y += enemy.speed * dt;

        if (enemy.y > static_cast<float>(kDesignHeight) + 44.0f && enemy.active) {
            enemy.active = false;
            DamagePlayer(1, L"敵を取り逃がした - HP -1");
        }
    }
}

void ShooterGame::UpdateBoss(float dt) {
    if (!boss_.active) {
        return;
    }

    const StageSpec& spec = stageBook_.Get(currentStage_);
    if (currentStage_ == stageBook_.TotalStages()) {
        stage10FleeTimer_ += dt;
        if (stage10BossFleeActive_) {
            stage10FleeLockTimer_ = std::max(0.0f, stage10FleeLockTimer_ - dt);
            boss_.x = stage10FleeTargetX_;
            boss_.y = stage10FleeTargetY_;
            if (stage10FleeLockTimer_ > 0.0f) {
                boss_.shotCooldown -= dt;
                while (boss_.shotCooldown <= 0.0f) {
                    SpawnBossShotPattern();
                    boss_.shotCooldown += spec.bossShotInterval;
                }
                return;
            }
            stage10BossFleeActive_ = false;
            boss_.shotCooldown = std::max(boss_.shotCooldown, 0.45f);
        }
    }

    boss_.movePhase += dt * (0.92f + static_cast<float>(currentStage_) * 0.07f);
    boss_.x = static_cast<float>(kDesignWidth) * 0.5f + std::sin(boss_.movePhase) * spec.bossMoveAmplitude;
    boss_.y = 175.0f + std::cos(boss_.movePhase * 0.66f) * 32.0f;

    boss_.shotCooldown -= dt;
    while (boss_.shotCooldown <= 0.0f) {
        SpawnBossShotPattern();
        boss_.shotCooldown += spec.bossShotInterval;
    }
}

void ShooterGame::UpdateEnemyBullets(float dt) {
    for (auto& bullet : enemyBullets_) {
        bullet.x += bullet.vx * dt;
        bullet.y += bullet.vy * dt;
    }
}

void ShooterGame::UpdateTickets(float dt) {
    for (auto& ticket : tickets_) {
        ticket.y += ticket.speed * dt;
    }
}

void ShooterGame::ResolveCollisions() {
    const bool isStage10 = (currentStage_ == stageBook_.TotalStages());

    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }
        for (auto& bullet : bullets_) {
            if (!bullet.active) {
                continue;
            }
            const float hitR = enemy.radius + bullet.radius;
            if (DistanceSq(enemy.x, enemy.y, bullet.x, bullet.y) > hitR * hitR) {
                continue;
            }

            bullet.active = false;
            enemy.hp -= bullet.damage;
            SpawnHitEffect(bullet.x, bullet.y, false);
            if (enemy.hp <= 0) {
                enemy.active = false;
                score_ = AddScoreCapped(score_, 90 + powerLevel_ * 22);
                if (Chance(24)) {
                    TicketDrop bonus;
                    bonus.x = enemy.x;
                    bonus.y = enemy.y;
                    bonus.speed = RandomFloat(72.0f, 110.0f);
                    bonus.size = 31.0f;
                    tickets_.push_back(bonus);
                }
                break;
            }
        }
    }

    if (boss_.active) {
        for (auto& bullet : bullets_) {
            if (!bullet.active) {
                continue;
            }

            const float hitR = boss_.radius + bullet.radius;
            if (DistanceSq(boss_.x, boss_.y, bullet.x, bullet.y) > hitR * hitR) {
                continue;
            }

            bullet.active = false;
            const long long boostedDamage = std::max(1LL, static_cast<long long>(bullet.damage));
            const long long bossDamageScale =
                kBossDamageScaleBase + static_cast<long long>(currentStage_) * kBossDamageScalePerStage;
            long long bossDamage = SafeMultiplyClamped(boostedDamage, bossDamageScale);
            if (isStage10) {
                const float defenseRate = Stage10DefenseRate(stage10FleeTimer_);
                const long double damageRate = std::max(0.0L, 1.0L - static_cast<long double>(defenseRate));
                const long long reduced = static_cast<long long>(static_cast<long double>(bossDamage) * damageRate);
                bossDamage = std::max(1LL, reduced);
            }
            long long nextBossHp = std::max(0LL, boss_.hp - bossDamage);
            boss_.hp = nextBossHp;
            SpawnHitEffect(bullet.x, bullet.y, true);
            score_ = AddScoreCapped(score_, 3);
            if (isStage10 && stage10NextFleeHp_ > 0 && boss_.hp <= stage10NextFleeHp_) {
                if (boss_.hp <= 0) {
                    boss_.hp = 1;
                }
                TryTriggerStage10BossFlee();
            }
            if (boss_.hp <= 0) {
                if (isStage10 && stage10BossFleeCount_ < (kStage10BossRounds - 1)) {
                    ++stage10BossFleeCount_;
                    const StageSpec& spec = stageBook_.Get(currentStage_);
                    boss_.active = true;
                    boss_.x = static_cast<float>(kDesignWidth) * 0.5f;
                    boss_.y = 180.0f;
                    boss_.radius = 54.0f;
                    boss_.movePhase = 0.0f;
                    boss_.shotCooldown = spec.bossShotInterval;
                    const long long roundHp = Stage10RoundHp(spec.bossHp, stage10BossFleeCount_);
                    boss_.hp = roundHp;
                    boss_.maxHp = roundHp;
                    const long long fleeTriggerGap = std::max(1LL, roundHp / 3LL);
                    stage10NextFleeHp_ = std::max(1LL, roundHp - fleeTriggerGap);
                    stage10FleeTimer_ = 0.0f;
                    stage10FleeLockTimer_ = 0.0f;
                    stage10FleeNoticeTimer_ = 0.0f;
                    enemies_.clear();
                    enemyBullets_.clear();
                    bullets_.clear();
                    hitEffects_.clear();
                    statusText_ = spec.bossName + L" 第" + std::to_wstring(stage10BossFleeCount_ + 1) + L"/" +
                                  std::to_wstring(kStage10BossRounds) + L"戦";
                    break;
                }

                CompleteStage();
                break;
            }
        }
    }

    const float playerR = 28.0f;
    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }
        const float hitR = enemy.radius + playerR;
        if (DistanceSq(enemy.x, enemy.y, playerX_, playerY_) <= hitR * hitR) {
            enemy.active = false;
            DamagePlayer(1, L"敵に接触 - HP -1");
        }
    }

    if (boss_.active) {
        const float hitR = boss_.radius + playerR;
        if (DistanceSq(boss_.x, boss_.y, playerX_, playerY_) <= hitR * hitR) {
            DamagePlayer(1, L"ボスに接触 - HP -1");
        }
    }

    for (auto& bullet : enemyBullets_) {
        if (!bullet.active) {
            continue;
        }
        const float hitR = bullet.radius + playerR;
        if (DistanceSq(bullet.x, bullet.y, playerX_, playerY_) <= hitR * hitR) {
            bullet.active = false;
            DamagePlayer(1, L"被弾 - HP -1");
        }
    }

    for (auto& ticket : tickets_) {
        if (!ticket.active) {
            continue;
        }
        const float hitR = (ticket.size * 0.42f) + playerR;
        if (DistanceSq(ticket.x, ticket.y, playerX_, playerY_) <= hitR * hitR) {
            ticket.active = false;
            ticketPoints_ = std::min(kMaxTicketPoints, ticketPoints_ + 1);
            ++totalTicketsCollected_;
            score_ = AddScoreCapped(score_, 30);

            if (ticketPoints_ % 8 == 0 && playerHp_ < maxPlayerHp_) {
                ++playerHp_;
                statusText_ = L"チケット回収でHP回復";
            } else {
                statusText_ = L"チケット回収 +1";
            }
        }
    }
}

void ShooterGame::CleanupEntities() {
    bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet& b) {
                       if (!b.active) {
                           return true;
                       }
                       return b.y < -60.0f || b.y > static_cast<float>(kDesignHeight) + 60.0f ||
                              b.x < -60.0f || b.x > static_cast<float>(kDesignWidth) + 60.0f;
                   }),
                   bullets_.end());

    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) {
                       return !e.active;
                   }),
                   enemies_.end());

    enemyBullets_.erase(std::remove_if(enemyBullets_.begin(), enemyBullets_.end(), [](const EnemyBullet& b) {
                            if (!b.active) {
                                return true;
                            }
                            return b.y < -70.0f || b.y > static_cast<float>(kDesignHeight) + 70.0f ||
                                   b.x < -70.0f || b.x > static_cast<float>(kDesignWidth) + 70.0f;
                        }),
                        enemyBullets_.end());

    tickets_.erase(std::remove_if(tickets_.begin(), tickets_.end(), [](const TicketDrop& t) {
                       if (!t.active) {
                           return true;
                       }
                       return t.y > static_cast<float>(kDesignHeight) + 80.0f;
                   }),
                   tickets_.end());
}

void ShooterGame::FireShotPattern() {
    if (bullets_.size() >= static_cast<std::size_t>(kMaxActivePlayerBullets)) {
        return;
    }

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
    if (powerLevel_ >= 9) {
        bulletCount = 7;
    }

    if (powerLevel_ > 9) {
        bulletCount += (powerLevel_ - 9);
    }
    bulletCount += attackUpgrade_ / 2;

    const int hpUpgradeCount = std::max(0, maxPlayerHp_ - kInitialPlayerHp);
    float spreadDeg = 30.0f + static_cast<float>(attackUpgrade_) * 2.6f + static_cast<float>(hpUpgradeCount) * 0.9f;
    if (powerLevel_ > 9) {
        spreadDeg += static_cast<float>(powerLevel_ - 9) * 0.8f;
    }

    float speed = 760.0f;
    int damage = 18 + powerLevel_ * 2 + (powerLevel_ * powerLevel_) / 5 + attackUpgrade_ * 7;

    if (operationMode_ == OperationMode::Wide) {
        spreadDeg += 22.0f;
        speed -= 60.0f;
        damage = std::max(1, damage - 6);
        bulletCount += 2;
    } else if (operationMode_ == OperationMode::Focus) {
        spreadDeg -= 14.0f;
        speed += 80.0f;
        damage += 10;
        bulletCount -= 1;
    }

    spreadDeg = Clamp(spreadDeg, 12.0f, 165.0f);
    bulletCount = std::clamp(bulletCount, 1, kMaxBulletsPerShot);
    const int availableSlots = kMaxActivePlayerBullets - static_cast<int>(bullets_.size());
    bulletCount = std::min(bulletCount, availableSlots);
    if (bulletCount <= 0) {
        return;
    }

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
        bullet.radius = std::min(16.0f, 4.0f + static_cast<float>(powerLevel_) * 0.20f +
                                           static_cast<float>(attackUpgrade_) * 0.10f);
        bullets_.push_back(bullet);
    }
}

void ShooterGame::SpawnBossShotPattern() {
    const StageSpec& spec = stageBook_.Get(currentStage_);
    const int count = spec.bossBulletCount;
    const float spreadDeg = 115.0f;
    const float speed = 185.0f + static_cast<float>(currentStage_) * 13.0f;

    for (int i = 0; i < count; ++i) {
        float t = 0.5f;
        if (count > 1) {
            t = static_cast<float>(i) / static_cast<float>(count - 1);
        }
        const float deg = 90.0f - spreadDeg * 0.5f + spreadDeg * t;
        const float rad = deg * (kPi / 180.0f);

        EnemyBullet bullet;
        bullet.x = boss_.x;
        bullet.y = boss_.y + boss_.radius * 0.45f;
        bullet.vx = std::cos(rad) * speed;
        bullet.vy = std::sin(rad) * speed;
        bullet.radius = 7.0f;
        enemyBullets_.push_back(bullet);
    }

    const float dx = playerX_ - boss_.x;
    const float dy = playerY_ - boss_.y;
    const float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));

    EnemyBullet aimed;
    aimed.x = boss_.x;
    aimed.y = boss_.y + boss_.radius * 0.45f;
    aimed.vx = (dx / len) * (speed + 70.0f);
    aimed.vy = (dy / len) * (speed + 70.0f);
    aimed.radius = 8.0f;
    enemyBullets_.push_back(aimed);
}

void ShooterGame::ActivateSpecialMove() {
    if (gameOver_ || gameClear_ || shopOpen_ || operationMenuOpen_ || paused_) {
        return;
    }
    if (specialCooldown_ > 0.0f) {
        statusText_ = L"必殺技チャージ中: " + std::to_wstring(static_cast<int>(std::ceil(specialCooldown_))) + L"秒";
        return;
    }

    specialCooldown_ = kSpecialCooldownSec;
    playerInvincibleTimer_ = std::max(playerInvincibleTimer_, kSpecialInvincibleSec);

    int clearedEnemies = 0;
    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }
        enemy.active = false;
        ++clearedEnemies;
        score_ = AddScoreCapped(score_, 90 + powerLevel_ * 22);
        SpawnHitEffect(enemy.x, enemy.y, false);
    }

    const int clearedBullets = static_cast<int>(enemyBullets_.size());
    enemyBullets_.clear();

    if (boss_.active) {
        boss_.shotCooldown += kSpecialBossStunSec;
        SpawnHitEffect(boss_.x, boss_.y, true);
    }

    statusText_ = L"必殺技発動! 敵" + std::to_wstring(clearedEnemies) + L" / 弾" + std::to_wstring(clearedBullets) +
                  L"を消去";
}

void ShooterGame::DamagePlayer(int amount, const wchar_t* reason) {
    if (playerInvincibleTimer_ > 0.0f) {
        return;
    }
    playerHp_ -= amount;
    playerInvincibleTimer_ = RandomFloat(kInvincibleMinSec, kInvincibleMaxSec);
    statusText_ = reason;
}

void ShooterGame::InitStars() {
    stars_.clear();
    stars_.reserve(100);
    for (int i = 0; i < 100; ++i) {
        Star star;
        star.x = RandomFloat(0.0f, static_cast<float>(kDesignWidth));
        star.y = RandomFloat(0.0f, static_cast<float>(kDesignHeight));
        star.speed = RandomFloat(45.0f, 158.0f);
        star.size = RandomFloat(1.2f, 3.2f);
        stars_.push_back(star);
    }
}

void ShooterGame::UpdateStars(float dt) {
    for (auto& star : stars_) {
        star.y += star.speed * dt;
        if (star.y > static_cast<float>(kDesignHeight) + 5.0f) {
            star.y = -5.0f;
            star.x = RandomFloat(0.0f, static_cast<float>(kDesignWidth));
            star.speed = RandomFloat(45.0f, 158.0f);
            star.size = RandomFloat(1.2f, 3.2f);
        }
    }
}

void ShooterGame::UpdateHitEffects(float dt) {
    bossHitFxCooldown_ = std::max(0.0f, bossHitFxCooldown_ - dt);

    for (auto& fx : hitEffects_) {
        fx.life += dt;
    }

    hitEffects_.erase(std::remove_if(hitEffects_.begin(), hitEffects_.end(), [](const HitEffect& fx) {
                          return fx.life >= fx.maxLife;
                      }),
                      hitEffects_.end());
}

bool ShooterGame::IsDown(UINT keyCode) const {
    return keyCode < keys_.size() ? keys_[keyCode] : false;
}

int ShooterGame::RandomInt(int minValue, int maxValue) {
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    return dist(rng_);
}

float ShooterGame::RandomFloat(float minValue, float maxValue) {
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(rng_);
}

bool ShooterGame::Chance(int percent) {
    return RandomInt(1, 100) <= percent;
}

void ShooterGame::SpawnHitEffect(float x, float y, bool bossHit) {
    if (bossHit) {
        if (bossHitFxCooldown_ > 0.0f) {
            return;
        }
        bossHitFxCooldown_ = kBossHitFxInterval;
    }

    if (hitEffects_.size() >= static_cast<std::size_t>(kMaxHitEffects)) {
        const std::size_t trim = std::min<std::size_t>(6, hitEffects_.size());
        hitEffects_.erase(hitEffects_.begin(), hitEffects_.begin() + trim);
    }

    HitEffect fx;
    fx.x = x + RandomFloat(-3.2f, 3.2f);
    fx.y = y + RandomFloat(-3.2f, 3.2f);
    fx.radius = bossHit ? RandomFloat(15.0f, 24.0f) : RandomFloat(8.0f, 13.5f);
    fx.maxLife = bossHit ? 0.24f : 0.17f;
    fx.boss = bossHit;
    hitEffects_.push_back(fx);
}

void ShooterGame::Render(HDC hdc, int clientWidth, int clientHeight) {
    Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(255, 10, 16, 26));

    float scale = std::min(static_cast<float>(clientWidth) / static_cast<float>(kDesignWidth),
                           static_cast<float>(clientHeight) / static_cast<float>(kDesignHeight));
    if (scale < 0.01f) {
        scale = 1.0f;
    }
    const float offsetX = (static_cast<float>(clientWidth) - static_cast<float>(kDesignWidth) * scale) * 0.5f;
    const float offsetY = (static_cast<float>(clientHeight) - static_cast<float>(kDesignHeight) * scale) * 0.5f;

    const auto state = g.Save();
    g.TranslateTransform(offsetX, offsetY);
    g.ScaleTransform(scale, scale);
    g.SetClip(RectF(0.0f, 0.0f, static_cast<float>(kDesignWidth), static_cast<float>(kDesignHeight)));

    DrawScene(g);

    g.Restore(state);
}

void ShooterGame::DrawScene(Graphics& g) {
    const RectF whole(0.0f, 0.0f, static_cast<float>(kDesignWidth), static_cast<float>(kDesignHeight));
    LinearGradientBrush bg(whole, Color(255, 10, 22, 42), Color(255, 12, 72, 122),
                           Gdiplus::LinearGradientModeVertical);
    g.FillRectangle(&bg, whole);

    DrawStars(g);
    DrawTickets(g);
    DrawEnemies(g);
    DrawBoss(g);
    DrawEnemyBullets(g);
    DrawBullets(g);
    DrawHitEffects(g);
    DrawPlayer(g);
    DrawHud(g);

    if (shopOpen_) {
        DrawShopOverlay(g);
    }
    if (operationMenuOpen_) {
        DrawOperationOverlay(g);
        return;
    }
    if (paused_) {
        DrawPauseOverlay(g);
    } else if (gameOver_) {
        DrawGameOver(g);
    } else if (gameClear_) {
        DrawGameClear(g);
    }
}

void ShooterGame::DrawStars(Graphics& g) {
    for (const auto& star : stars_) {
        const int alphaInt = static_cast<int>(75.0f + star.speed * 0.95f);
        const BYTE alpha = static_cast<BYTE>(std::clamp(alphaInt, 0, 255));
        SolidBrush brush(Color(alpha, 226, 245, 255));
        g.FillEllipse(&brush, RectF(star.x, star.y, star.size, star.size));
    }
}

void ShooterGame::DrawPlayer(Graphics& g) {
    const bool blink = playerInvincibleTimer_ > 0.0f &&
                       static_cast<int>(playerInvincibleTimer_ * 15.0f) % 2 == 0;
    if (blink) {
        return;
    }

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

void ShooterGame::DrawBullets(Graphics& g) {
    SolidBrush bulletBrush(Color(214, 112, 244, 255));
    Pen bulletStroke(Color(245, 222, 252, 255), 1.1f);
    const bool highDensity = bullets_.size() > 200;

    for (const auto& bullet : bullets_) {
        const float drawRadius = Clamp(bullet.radius * 0.56f, 2.4f, 8.6f);
        const RectF rect(bullet.x - drawRadius, bullet.y - drawRadius, drawRadius * 2.0f, drawRadius * 2.0f);
        g.FillEllipse(&bulletBrush, rect);
        if (!highDensity) {
            g.DrawEllipse(&bulletStroke, rect);
        }
    }
}

void ShooterGame::DrawHitEffects(Graphics& g) {
    for (const auto& fx : hitEffects_) {
        const float t = Clamp(fx.life / std::max(0.001f, fx.maxLife), 0.0f, 1.0f);
        const float grow = 1.0f + t * 0.95f;
        const float radius = fx.radius * grow;
        const float coreRadius = radius * (0.44f - t * 0.16f);

        const int ringAlpha = static_cast<int>((1.0f - t) * (fx.boss ? 240.0f : 210.0f));
        const int coreAlpha = static_cast<int>((1.0f - t) * (fx.boss ? 200.0f : 175.0f));
        const BYTE ringA = static_cast<BYTE>(std::clamp(ringAlpha, 0, 255));
        const BYTE coreA = static_cast<BYTE>(std::clamp(coreAlpha, 0, 255));

        const Color ringColor =
            fx.boss ? Color(ringA, 255, 188, 216) : Color(ringA, 255, 245, 180);
        const Color coreColor =
            fx.boss ? Color(coreA, 255, 128, 158) : Color(coreA, 255, 236, 128);
        const Color sparkColor =
            fx.boss ? Color(ringA, 255, 208, 224) : Color(ringA, 255, 252, 196);

        SolidBrush coreBrush(coreColor);
        Pen ringPen(ringColor, fx.boss ? 2.8f : 2.0f);
        Pen sparkPen(sparkColor, fx.boss ? 1.9f : 1.5f);

        const RectF coreRect(fx.x - coreRadius, fx.y - coreRadius, coreRadius * 2.0f, coreRadius * 2.0f);
        const RectF ringRect(fx.x - radius, fx.y - radius, radius * 2.0f, radius * 2.0f);
        g.FillEllipse(&coreBrush, coreRect);
        g.DrawEllipse(&ringPen, ringRect);

        const float spark = radius * (0.72f + (1.0f - t) * 0.48f);
        g.DrawLine(&sparkPen, fx.x - spark, fx.y, fx.x + spark, fx.y);
        g.DrawLine(&sparkPen, fx.x, fx.y - spark, fx.x, fx.y + spark);
    }
}

void ShooterGame::DrawEnemies(Graphics& g) {
    for (const auto& enemy : enemies_) {
        const RectF body(enemy.x - enemy.radius, enemy.y - enemy.radius, enemy.radius * 2.0f, enemy.radius * 2.0f);
        SolidBrush bodyBrush(Color(255, 248, 112, 128));
        g.FillEllipse(&bodyBrush, body);

        Pen stroke(Color(255, 255, 215, 226), 2.0f);
        g.DrawEllipse(&stroke, body);

        const float eyeR = std::max(3.0f, enemy.radius * 0.16f);
        SolidBrush eyeBrush(Color(255, 255, 248, 250));
        g.FillEllipse(&eyeBrush, RectF(enemy.x - enemy.radius * 0.35f - eyeR, enemy.y - eyeR, eyeR * 2.0f, eyeR * 2.0f));
        g.FillEllipse(&eyeBrush, RectF(enemy.x + enemy.radius * 0.35f - eyeR, enemy.y - eyeR, eyeR * 2.0f, eyeR * 2.0f));
    }
}

void ShooterGame::DrawBoss(Graphics& g) {
    if (!boss_.active || shopOpen_) {
        return;
    }

    bool drawnByAsset = false;
    if (auto* bossAsset = assets_.BossImage(currentStage_)) {
        const float drawRadius =
            (currentStage_ == stageBook_.TotalStages()) ? (boss_.radius * 1.45f) : (boss_.radius * 1.30f);
        const RectF body(boss_.x - drawRadius, boss_.y - drawRadius, drawRadius * 2.0f, drawRadius * 2.0f);
        g.DrawImage(bossAsset, body);
        drawnByAsset = true;
    }
    if (!drawnByAsset) {
        const RectF body(boss_.x - boss_.radius, boss_.y - boss_.radius, boss_.radius * 2.0f, boss_.radius * 2.0f);
        LinearGradientBrush fill(body, Color(255, 255, 112, 112), Color(255, 203, 62, 204),
                                 Gdiplus::LinearGradientModeForwardDiagonal);
        g.FillEllipse(&fill, body);
        Pen stroke(Color(255, 255, 227, 236), 3.0f);
        g.DrawEllipse(&stroke, body);
    }

    DrawText(g, stageBook_.Get(currentStage_).bossName,
             RectF(boss_.x - 140.0f, boss_.y - boss_.radius - 28.0f, 280.0f, 22.0f), 20.0f,
             Color(255, 255, 240, 186), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawEnemyBullets(Graphics& g) {
    SolidBrush fill(Color(236, 255, 92, 126));
    Pen stroke(Color(255, 70, 16, 34), 2.1f);
    SolidBrush core(Color(255, 255, 236, 204));
    const bool highDensity = enemyBullets_.size() > 260;
    for (const auto& bullet : enemyBullets_) {
        const RectF r(bullet.x - bullet.radius, bullet.y - bullet.radius, bullet.radius * 2.0f, bullet.radius * 2.0f);
        g.FillEllipse(&fill, r);
        g.DrawEllipse(&stroke, r);
        if (!highDensity) {
            const float cr = std::max(2.1f, bullet.radius * 0.28f);
            g.FillEllipse(&core, RectF(bullet.x - cr, bullet.y - cr, cr * 2.0f, cr * 2.0f));
        }
    }
}

void ShooterGame::DrawTickets(Graphics& g) {
    for (const auto& ticket : tickets_) {
        const RectF rect(ticket.x - ticket.size * 0.5f, ticket.y - ticket.size * 0.5f, ticket.size, ticket.size);
        if (auto* icon = assets_.TicketIcon()) {
            g.DrawImage(icon, rect);
        } else {
            FillRoundRect(g, rect, 6.0f, Color(255, 222, 54, 54));
            DrawText(g, L"T", rect, 17.0f, Color(255, 250, 250, 250), StringAlignmentCenter, FontStyleBold,
                     L"Arial Black");
        }
    }
}

void ShooterGame::DrawHud(Graphics& g) {
    FillRoundRect(g, RectF(0.0f, 0.0f, 720.0f, 144.0f), 0.0f, Color(205, 4, 12, 22));

    RectF iconRect(24.0f, 20.0f, 42.0f, 42.0f);
    if (auto* icon = assets_.PointIcon()) {
        g.DrawImage(icon, iconRect);
    } else {
        FillRoundRect(g, iconRect, 10.0f, Color(255, 225, 240, 255));
        DrawText(g, L"P", iconRect, 22.0f, Color(255, 24, 48, 80), StringAlignmentCenter, FontStyleBold, L"Arial Black");
    }

    DrawText(g, FormatNumber(ticketPoints_), RectF(74.0f, 18.0f, 190.0f, 42.0f), 36.0f, Color(255, 248, 250, 255),
             StringAlignmentNear, FontStyleBold, L"Arial Black");
    DrawText(g, L"チケット", RectF(74.0f, 60.0f, 160.0f, 22.0f), 20.0f, Color(255, 180, 216, 242),
             StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

    std::wstringstream upper;
    upper << L"ステージ " << currentStage_ << L"/" << stageBook_.TotalStages();
    if (endlessMode_) {
        upper << L"  ENDLESS " << endlessLoop_;
    }
    upper << L"   強化 Lv." << powerLevel_ << L"   HP " << playerHp_ << L"/" << maxPlayerHp_ << L"   MODE "
          << OperationModeLabel() << L"   SHOT " << ShotTypeLabel();
    DrawText(g, upper.str(), RectF(236.0f, 18.0f, 466.0f, 28.0f), 18.0f, Color(255, 251, 240, 170),
             StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

    DrawText(g, L"スコア " + FormatNumber(score_), RectF(236.0f, 46.0f, 466.0f, 34.0f), 30.0f,
             Color(255, 241, 248, 255), StringAlignmentNear, FontStyleBold, L"Arial Black");

    const bool showBossHud = boss_.active && !shopOpen_ && boss_.maxHp > 0;
    if (showBossHud) {
        const long long remainHp = std::max(0LL, boss_.hp);
        DrawText(g, L"最大HP " + FormatNumber(boss_.maxHp), RectF(26.0f, 82.0f, 668.0f, 16.0f), 12.0f,
                 Color(255, 236, 192, 156), StringAlignmentNear,
                 FontStyleBold, L"Yu Gothic UI");
        DrawText(g, L"残りHP " + FormatNumber(remainHp), RectF(26.0f, 98.0f, 668.0f, 16.0f), 12.0f,
                 Color(255, 255, 223, 182), StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");
        if (currentStage_ == stageBook_.TotalStages()) {
            const int round = std::clamp(stage10BossFleeCount_ + 1, 1, kStage10BossRounds);
            const std::wstring roundText = L"第" + std::to_wstring(round) + L"/" + std::to_wstring(kStage10BossRounds) + L"戦";
            DrawText(g, roundText, RectF(526.0f, 98.0f, 168.0f, 16.0f), 12.0f, Color(255, 255, 238, 164),
                     StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");
            if (stage10FleeNoticeTimer_ > 0.0f) {
                DrawText(g, L"ボスが逃げた！！追いかけろ！！", RectF(248.0f, 82.0f, 446.0f, 16.0f), 12.0f,
                         Color(255, 255, 205, 126), StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");
            }
        }
    }

    const bool hideOperationStatus = statusText_.rfind(L"オペレーション:", 0) == 0;
    if (!showBossHud && !hideOperationStatus) {
        DrawText(g, statusText_, RectF(26.0f, 94.0f, 668.0f, 24.0f), 16.0f, Color(255, 220, 236, 248),
                 StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");
    }

    if (showBossHud) {
        const float rate = Clamp(static_cast<float>(boss_.hp) / static_cast<float>(boss_.maxHp), 0.0f, 1.0f);
        FillRoundRect(g, RectF(26.0f, 121.0f, 668.0f, 14.0f), 7.0f, Color(160, 10, 26, 44));
        FillRoundRect(g, RectF(26.0f, 121.0f, 668.0f * rate, 14.0f), 7.0f, Color(220, 255, 112, 120));
    }

    FillRoundRect(g, RectF(16.0f, 1188.0f, 688.0f, 70.0f), 14.0f, Color(165, 6, 19, 33));
    DrawText(g, L"移動 WASD/矢印  ショット Z  ホーミング H  必殺技 X  低速 Shift  P ポーズ  R リトライ  M BGM  O オペレーション",
             RectF(32.0f, 1205.0f, 656.0f, 34.0f), 16.0f, Color(255, 228, 244, 255), StringAlignmentCenter,
             FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawShopOverlay(Graphics& g) {
    FillRoundRect(g, RectF(90.0f, 420.0f, 540.0f, 420.0f), 24.0f, Color(228, 8, 22, 40));
    DrawText(g, L"ショップ", RectF(110.0f, 454.0f, 500.0f, 54.0f), 34.0f,
             Color(255, 255, 221, 132), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");

    DrawText(g, L"所持チケット: " + FormatNumber(ticketPoints_), RectF(120.0f, 514.0f, 480.0f, 34.0f), 28.0f,
             Color(255, 241, 248, 255), StringAlignmentCenter, FontStyleBold, L"Arial Black");

    FillRoundRect(g, RectF(132.0f, 570.0f, 456.0f, 66.0f), 14.0f, Color(190, 17, 47, 78));
    DrawText(g, L"[1] 火力強化 (2チケット)", RectF(144.0f, 584.0f, 432.0f, 36.0f), 24.0f,
             Color(255, 245, 247, 250), StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

    FillRoundRect(g, RectF(132.0f, 646.0f, 456.0f, 66.0f), 14.0f, Color(190, 17, 47, 78));
    DrawText(g, L"[2] 最大HP+1 (3チケット)", RectF(144.0f, 660.0f, 432.0f, 36.0f), 24.0f,
             Color(255, 245, 247, 250), StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

    FillRoundRect(g, RectF(132.0f, 722.0f, 456.0f, 66.0f), 14.0f, Color(190, 17, 47, 78));
    DrawText(g, std::wstring(L"[3] モード切替 (現在: ") + OperationModeLabel() + L")",
             RectF(144.0f, 736.0f, 432.0f, 36.0f), 24.0f, Color(255, 245, 247, 250), StringAlignmentNear,
             FontStyleBold, L"Yu Gothic UI");

    DrawText(g, L"ENTER / SPACE で次のステージへ", RectF(120.0f, 804.0f, 480.0f, 32.0f), 20.0f,
             Color(255, 255, 232, 156), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawOperationOverlay(Graphics& g) {
    FillRoundRect(g, RectF(62.0f, 292.0f, 596.0f, 688.0f), 24.0f, Color(232, 8, 22, 40));
    DrawText(g, L"オペレーション設定", RectF(86.0f, 332.0f, 548.0f, 56.0f), 38.0f,
             Color(255, 255, 221, 132), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");

    const std::wstring rows[kOperationMenuItemCount] = {
        L"攻撃力: " + std::to_wstring(attackUpgrade_) + L" / " + std::to_wstring(kAttackUpgradeCap),
        L"強化Lv: " + std::to_wstring(powerLevel_) + L" / " + std::to_wstring(kMaxPowerLevel),
        L"HP: " + std::to_wstring(playerHp_) + L" / " + std::to_wstring(maxPlayerHp_),
        std::wstring(L"移動先: ") + (operationJumpToBoss_ ? L"ステージボス" : L"ステージ"),
        L"ステージ移動: " + std::to_wstring(currentStage_) + L" / " + std::to_wstring(stageBook_.TotalStages()),
        std::wstring(L"モード: ") + OperationModeLabel(),
        L"チケット: " + FormatNumber(ticketPoints_),
    };

    const float itemTop = 384.0f;
    const float itemHeight = 66.0f;
    const float itemGap = 8.0f;
    for (int i = 0; i < kOperationMenuItemCount; ++i) {
        const float y = itemTop + static_cast<float>(i) * (itemHeight + itemGap);
        const bool selected = (i == operationMenuCursor_);
        const Color back = selected ? Color(214, 36, 86, 138) : Color(188, 16, 44, 74);
        FillRoundRect(g, RectF(96.0f, y, 528.0f, itemHeight), 14.0f, back);
        DrawText(g, rows[i], RectF(116.0f, y + 15.0f, 488.0f, 32.0f), 26.0f,
                 Color(255, 245, 247, 250), StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");
    }

    DrawText(g, L"UP/DOWN で選択  LEFT/RIGHT で変更", RectF(90.0f, 918.0f, 540.0f, 28.0f), 19.0f,
             Color(255, 230, 240, 252), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
    DrawText(g, L"O / ESC / ENTER で閉じる", RectF(90.0f, 948.0f, 540.0f, 28.0f), 19.0f,
             Color(255, 255, 232, 156), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawPauseOverlay(Graphics& g) {
    FillRoundRect(g, RectF(190.0f, 515.0f, 340.0f, 150.0f), 20.0f, Color(220, 8, 18, 34));
    DrawText(g, L"ポーズ", RectF(210.0f, 540.0f, 300.0f, 46.0f), 48.0f, Color(255, 255, 223, 136),
             StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"Pキーで再開", RectF(210.0f, 602.0f, 300.0f, 30.0f), 24.0f, Color(255, 225, 238, 250),
             StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawGameOver(Graphics& g) {
    FillRoundRect(g, RectF(110.0f, 410.0f, 500.0f, 370.0f), 24.0f, Color(220, 8, 19, 38));
    DrawText(g, L"ゲームオーバー", RectF(140.0f, 470.0f, 440.0f, 78.0f), 56.0f, Color(255, 255, 212, 118),
             StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"スコア " + FormatNumber(score_), RectF(140.0f, 558.0f, 440.0f, 48.0f), 36.0f,
             Color(255, 241, 248, 255), StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"回収チケット: " + FormatNumber(totalTicketsCollected_), RectF(130.0f, 618.0f, 460.0f, 34.0f), 24.0f,
              Color(255, 223, 238, 255), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
    DrawText(g, L"コンティニュー回数: " + FormatNumber(continueCount_), RectF(130.0f, 652.0f, 460.0f, 30.0f), 21.0f,
             Color(255, 220, 236, 248), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
    DrawText(g, L"C / ENTER / SPACE でコンティニュー", RectF(130.0f, 686.0f, 460.0f, 34.0f), 24.0f,
             Color(255, 255, 232, 156), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
    DrawText(g, L"Rキーでリトライ", RectF(140.0f, 726.0f, 440.0f, 30.0f), 23.0f, Color(255, 220, 236, 248),
             StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawGameClear(Graphics& g) {
    FillRoundRect(g, RectF(80.0f, 380.0f, 560.0f, 430.0f), 24.0f, Color(225, 8, 22, 40));
    DrawText(g, L"全クリおめでとう", RectF(110.0f, 438.0f, 500.0f, 86.0f), 72.0f, Color(255, 255, 220, 122),
             StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"総合スコア " + FormatNumber(score_), RectF(120.0f, 548.0f, 480.0f, 48.0f), 38.0f,
             Color(255, 241, 248, 255), StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"回収チケット: " + FormatNumber(totalTicketsCollected_), RectF(120.0f, 606.0f, 480.0f, 34.0f), 24.0f,
              Color(255, 223, 238, 255), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");

    const Color endColor = (gameClearChoiceIndex_ == 0) ? Color(255, 255, 238, 164) : Color(255, 210, 218, 228);
    const Color endlessColor = (gameClearChoiceIndex_ == 1) ? Color(255, 255, 238, 164) : Color(255, 210, 218, 228);
    DrawText(g, L"[1] 終了", RectF(120.0f, 666.0f, 480.0f, 34.0f), 28.0f, endColor, StringAlignmentCenter,
             FontStyleBold, L"Yu Gothic UI");
    DrawText(g, L"[2] エンドレスへ移行", RectF(120.0f, 708.0f, 480.0f, 34.0f), 28.0f, endlessColor, StringAlignmentCenter,
             FontStyleBold, L"Yu Gothic UI");
    DrawText(g, L"方向キー / 1,2 で選択  ENTER/SPACE で決定", RectF(90.0f, 754.0f, 540.0f, 28.0f), 18.0f,
             Color(255, 220, 230, 242), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::StartBgm(int stageNumber, bool bossPhase) {
    const auto& bgmPath = bossPhase ? assets_.BossStageBgmPath(stageNumber) : assets_.StageBgmPath(stageNumber);
    if (bgmPath.empty()) {
        statusText_ = L"assets/BGM のBGMファイルが見つかりません";
        return;
    }

    if (currentBgmPath_ == bgmPath) {
        return;
    }

    StopBgm();

    std::wstring ext = bgmPath.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });

    if (ext == L".wav") {
        const BOOL ok = PlaySoundW(bgmPath.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
        if (!ok) {
            statusText_ = L"BGM の再生に失敗";
            return;
        }
        currentBgmPath_ = bgmPath;
        bgmUsingMci_ = false;
        return;
    }

    const std::wstring openCmd = L"open \"" + bgmPath.wstring() + L"\" type mpegvideo alias " + kBgmAlias;
    if (mciSendStringW(openCmd.c_str(), nullptr, 0, nullptr) != 0) {
        statusText_ = L"MP3 BGM のオープンに失敗";
        return;
    }
    const std::wstring playCmd = L"play " + std::wstring(kBgmAlias) + L" repeat";
    if (mciSendStringW(playCmd.c_str(), nullptr, 0, nullptr) != 0) {
        const std::wstring closeCmd = L"close " + std::wstring(kBgmAlias);
        mciSendStringW(closeCmd.c_str(), nullptr, 0, nullptr);
        statusText_ = L"MP3 BGM の再生に失敗";
        return;
    }

    currentBgmPath_ = bgmPath;
    bgmUsingMci_ = true;
}

void ShooterGame::StopBgm() {
    PlaySoundW(nullptr, nullptr, 0);
    const std::wstring stopCmd = L"stop " + std::wstring(kBgmAlias);
    const std::wstring closeCmd = L"close " + std::wstring(kBgmAlias);
    if (bgmUsingMci_) {
        mciSendStringW(stopCmd.c_str(), nullptr, 0, nullptr);
        mciSendStringW(closeCmd.c_str(), nullptr, 0, nullptr);
        bgmUsingMci_ = false;
    } else {
        mciSendStringW(closeCmd.c_str(), nullptr, 0, nullptr);
    }
    currentBgmPath_.clear();
}
