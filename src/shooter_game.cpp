#include "shooter_game.h"

#include <mmsystem.h>

#include <algorithm>
#include <cmath>
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

std::wstring FormatNumber(int value) {
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

    boss_ = BossState{};

    playerX_ = static_cast<float>(kDesignWidth) * 0.5f;
    playerY_ = static_cast<float>(kDesignHeight) - 140.0f;
    maxPlayerHp_ = 5;
    playerHp_ = maxPlayerHp_;
    playerInvincibleTimer_ = 0.0f;

    ticketPoints_ = 0;
    powerLevel_ = 1;
    attackUpgrade_ = 0;
    shopCoins_ = 0;
    score_ = 0;

    currentStage_ = 1;
    stageElapsed_ = 0.0f;

    shopOpen_ = false;
    paused_ = false;
    gameOver_ = false;
    gameClear_ = false;

    fireCooldown_ = 0.06f;
    enemySpawnCooldown_ = 0.6f;
    ticketSpawnCooldown_ = 1.1f;

    statusText_ = L"ステージ1 開始";

    if (bgmEnabled_) {
        StartBgm();
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
            StartBgm();
            statusText_ = L"BGM を有効にしました";
        } else {
            StopBgm();
            statusText_ = L"BGM を無効にしました";
        }
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

    if (keyCode == '1') {
        if (shopCoins_ < kAtkCost) {
            statusText_ = L"コイン不足: 火力強化には2コイン必要";
            return;
        }
        if (attackUpgrade_ >= 8) {
            statusText_ = L"火力強化は上限です";
            return;
        }
        shopCoins_ -= kAtkCost;
        ++attackUpgrade_;
        statusText_ = L"火力強化を購入: 攻撃力+" + std::to_wstring(attackUpgrade_);
        return;
    }

    if (keyCode == '2') {
        if (shopCoins_ < kHpCost) {
            statusText_ = L"コイン不足: 最大HP強化には3コイン必要";
            return;
        }
        if (maxPlayerHp_ >= 12) {
            statusText_ = L"最大HPは上限です";
            return;
        }
        shopCoins_ -= kHpCost;
        ++maxPlayerHp_;
        playerHp_ = maxPlayerHp_;
        statusText_ = L"最大HP強化を購入: HP " + std::to_wstring(maxPlayerHp_);
        return;
    }

    if (keyCode == VK_RETURN || keyCode == VK_SPACE || keyCode == 'N') {
        shopOpen_ = false;
        BeginNextStage();
    }
}

void ShooterGame::Update(float dt) {
    if (paused_) {
        return;
    }

    UpdateStars(dt);

    if (shopOpen_) {
        return;
    }

    if (gameOver_ || gameClear_) {
        return;
    }

    if (playerInvincibleTimer_ > 0.0f) {
        playerInvincibleTimer_ = std::max(0.0f, playerInvincibleTimer_ - dt);
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

    powerLevel_ = std::min(10, 1 + ticketPoints_ / 4);

    if (!boss_.active) {
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
    enemies_.clear();
    enemyBullets_.clear();
    bullets_.clear();

    ++currentStage_;
    if (currentStage_ > stageBook_.TotalStages()) {
        gameClear_ = true;
        statusText_ = L"全10ステージ制覇! Rキーでリトライ";
        return;
    }

    enemySpawnCooldown_ = 0.6f;
    ticketSpawnCooldown_ = 1.0f;
    statusText_ = L"ステージ" + std::to_wstring(currentStage_) + L" 開始";
}

void ShooterGame::SpawnBoss() {
    const StageSpec& spec = stageBook_.Get(currentStage_);
    boss_.active = true;
    boss_.x = static_cast<float>(kDesignWidth) * 0.5f;
    boss_.y = 180.0f;
    boss_.radius = 54.0f;
    boss_.movePhase = 0.0f;
    boss_.shotCooldown = spec.bossShotInterval;
    boss_.hp = spec.bossHp;
    boss_.maxHp = spec.bossHp;

    enemies_.clear();
    enemyBullets_.clear();
    statusText_ = L"ステージ" + std::to_wstring(currentStage_) + L" ボス出現: " + spec.bossName;
}

void ShooterGame::CompleteStage() {
    score_ += 1500 + currentStage_ * 550;
    boss_ = BossState{};
    enemies_.clear();
    enemyBullets_.clear();
    bullets_.clear();

    if (currentStage_ >= stageBook_.TotalStages()) {
        gameClear_ = true;
        statusText_ = L"全10ステージ制覇! Rキーでリトライ";
        return;
    }

    const bool healed = playerHp_ < maxPlayerHp_;
    playerHp_ = maxPlayerHp_;

    const int rewardCoins = 3 + currentStage_;
    shopCoins_ += rewardCoins;
    shopOpen_ = true;

    std::wstringstream ss;
    ss << L"ステージ" << currentStage_ << L" クリア! ショップ開店 (コイン+" << rewardCoins << L")";
    if (healed) {
        ss << L" HP全回復";
    }
    statusText_ = ss.str();
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
    const float interval = std::max(0.075f, 0.30f - static_cast<float>(powerLevel_ - 1) * 0.02f);
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
    for (auto& bullet : bullets_) {
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
            if (enemy.hp <= 0) {
                enemy.active = false;
                score_ += 90 + powerLevel_ * 22;
                if (Chance(24)) {
                    TicketDrop bonus;
                    bonus.x = enemy.x;
                    bonus.y = enemy.y;
                    bonus.speed = RandomFloat(72.0f, 110.0f);
                    bonus.size = 31.0f;
                    tickets_.push_back(bonus);
                }
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
            boss_.hp -= bullet.damage;
            score_ += 3;
            if (boss_.hp <= 0) {
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
            ++ticketPoints_;
            score_ += 30;

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

    const float spreadDeg = 30.0f;
    const float speed = 720.0f;
    const int damage = std::max(1, powerLevel_ / 3) + attackUpgrade_;

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
        bullet.radius = 4.0f + static_cast<float>(powerLevel_) * 0.32f;
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

void ShooterGame::DamagePlayer(int amount, const wchar_t* reason) {
    if (playerInvincibleTimer_ > 0.0f) {
        return;
    }
    playerHp_ -= amount;
    playerInvincibleTimer_ = 0.65f;
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
    DrawPlayer(g);
    DrawHud(g);

    if (shopOpen_) {
        DrawShopOverlay(g);
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
    SolidBrush bulletBrush(Color(255, 255, 225, 112));
    Pen bulletStroke(Color(255, 255, 248, 170), 1.4f);

    for (const auto& bullet : bullets_) {
        const RectF rect(bullet.x - bullet.radius, bullet.y - bullet.radius, bullet.radius * 2.0f, bullet.radius * 2.0f);
        g.FillEllipse(&bulletBrush, rect);
        g.DrawEllipse(&bulletStroke, rect);
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
    if (!boss_.active) {
        return;
    }

    const RectF body(boss_.x - boss_.radius, boss_.y - boss_.radius, boss_.radius * 2.0f, boss_.radius * 2.0f);
    LinearGradientBrush fill(body, Color(255, 255, 112, 112), Color(255, 203, 62, 204),
                             Gdiplus::LinearGradientModeForwardDiagonal);
    g.FillEllipse(&fill, body);
    Pen stroke(Color(255, 255, 227, 236), 3.0f);
    g.DrawEllipse(&stroke, body);

    DrawText(g, stageBook_.Get(currentStage_).bossName,
             RectF(boss_.x - 140.0f, boss_.y - boss_.radius - 28.0f, 280.0f, 22.0f), 20.0f,
             Color(255, 255, 240, 186), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawEnemyBullets(Graphics& g) {
    SolidBrush fill(Color(255, 255, 145, 92));
    Pen stroke(Color(255, 255, 220, 180), 1.4f);
    for (const auto& bullet : enemyBullets_) {
        const RectF r(bullet.x - bullet.radius, bullet.y - bullet.radius, bullet.radius * 2.0f, bullet.radius * 2.0f);
        g.FillEllipse(&fill, r);
        g.DrawEllipse(&stroke, r);
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
    upper << L"ステージ " << currentStage_ << L"/" << stageBook_.TotalStages() << L"   強化 Lv." << powerLevel_
          << L"   HP " << playerHp_ << L"/" << maxPlayerHp_;
    DrawText(g, upper.str(), RectF(250.0f, 18.0f, 452.0f, 28.0f), 18.0f, Color(255, 251, 240, 170),
             StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

    DrawText(g, L"スコア " + FormatNumber(score_), RectF(250.0f, 46.0f, 452.0f, 34.0f), 30.0f,
             Color(255, 241, 248, 255), StringAlignmentNear, FontStyleBold, L"Arial Black");

    DrawText(g, statusText_, RectF(26.0f, 94.0f, 668.0f, 24.0f), 16.0f, Color(255, 220, 236, 248),
             StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

    if (boss_.active && boss_.maxHp > 0) {
        const float rate = Clamp(static_cast<float>(boss_.hp) / static_cast<float>(boss_.maxHp), 0.0f, 1.0f);
        FillRoundRect(g, RectF(26.0f, 121.0f, 668.0f, 14.0f), 7.0f, Color(160, 10, 26, 44));
        FillRoundRect(g, RectF(26.0f, 121.0f, 668.0f * rate, 14.0f), 7.0f, Color(220, 255, 112, 120));
    }

    FillRoundRect(g, RectF(16.0f, 1188.0f, 688.0f, 70.0f), 14.0f, Color(165, 6, 19, 33));
    DrawText(g, L"移動 WASD/矢印  低速 Shift  P ポーズ/再開  R リトライ  M BGM", RectF(32.0f, 1205.0f, 656.0f, 34.0f),
             18.0f, Color(255, 228, 244, 255), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawShopOverlay(Graphics& g) {
    FillRoundRect(g, RectF(90.0f, 420.0f, 540.0f, 420.0f), 24.0f, Color(228, 8, 22, 40));
    DrawText(g, L"STAGE CLEAR SHOP", RectF(120.0f, 452.0f, 480.0f, 56.0f), 42.0f, Color(255, 255, 221, 132),
             StringAlignmentCenter, FontStyleBold, L"Arial Black");

    DrawText(g, L"コイン: " + FormatNumber(shopCoins_), RectF(120.0f, 514.0f, 480.0f, 34.0f), 28.0f,
             Color(255, 241, 248, 255), StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"HPは全回復済み", RectF(120.0f, 548.0f, 480.0f, 28.0f), 22.0f, Color(255, 225, 238, 250),
             StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");

    FillRoundRect(g, RectF(132.0f, 586.0f, 456.0f, 66.0f), 14.0f, Color(190, 17, 47, 78));
    DrawText(g, L"[1] 火力強化 (2コイン)", RectF(144.0f, 600.0f, 432.0f, 36.0f), 24.0f,
             Color(255, 245, 247, 250), StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

    FillRoundRect(g, RectF(132.0f, 662.0f, 456.0f, 66.0f), 14.0f, Color(190, 17, 47, 78));
    DrawText(g, L"[2] 最大HP+1 (3コイン)", RectF(144.0f, 676.0f, 432.0f, 36.0f), 24.0f,
             Color(255, 245, 247, 250), StringAlignmentNear, FontStyleBold, L"Yu Gothic UI");

    DrawText(g, L"ENTER / SPACE で次のステージへ", RectF(120.0f, 748.0f, 480.0f, 32.0f), 22.0f,
             Color(255, 255, 232, 156), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawPauseOverlay(Graphics& g) {
    FillRoundRect(g, RectF(190.0f, 515.0f, 340.0f, 150.0f), 20.0f, Color(220, 8, 18, 34));
    DrawText(g, L"PAUSE", RectF(210.0f, 540.0f, 300.0f, 46.0f), 48.0f, Color(255, 255, 223, 136),
             StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"Pキーで再開", RectF(210.0f, 602.0f, 300.0f, 30.0f), 24.0f, Color(255, 225, 238, 250),
             StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawGameOver(Graphics& g) {
    FillRoundRect(g, RectF(110.0f, 430.0f, 500.0f, 310.0f), 24.0f, Color(220, 8, 19, 38));
    DrawText(g, L"ゲームオーバー", RectF(140.0f, 470.0f, 440.0f, 78.0f), 56.0f, Color(255, 255, 212, 118),
             StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"スコア " + FormatNumber(score_), RectF(140.0f, 558.0f, 440.0f, 48.0f), 36.0f,
             Color(255, 241, 248, 255), StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"回収チケット: " + FormatNumber(ticketPoints_), RectF(130.0f, 618.0f, 460.0f, 34.0f), 24.0f,
             Color(255, 223, 238, 255), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
    DrawText(g, L"Rキーでリトライ", RectF(140.0f, 668.0f, 440.0f, 34.0f), 27.0f, Color(255, 255, 232, 156),
             StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::DrawGameClear(Graphics& g) {
    FillRoundRect(g, RectF(80.0f, 400.0f, 560.0f, 360.0f), 24.0f, Color(225, 8, 22, 40));
    DrawText(g, L"ALL CLEAR", RectF(110.0f, 438.0f, 500.0f, 86.0f), 72.0f, Color(255, 255, 220, 122),
             StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"総合スコア " + FormatNumber(score_), RectF(120.0f, 548.0f, 480.0f, 48.0f), 38.0f,
             Color(255, 241, 248, 255), StringAlignmentCenter, FontStyleBold, L"Arial Black");
    DrawText(g, L"回収チケット: " + FormatNumber(ticketPoints_), RectF(120.0f, 606.0f, 480.0f, 34.0f), 24.0f,
             Color(255, 223, 238, 255), StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
    DrawText(g, L"Rキーでリトライ", RectF(120.0f, 668.0f, 480.0f, 34.0f), 26.0f, Color(255, 255, 232, 156),
             StringAlignmentCenter, FontStyleBold, L"Yu Gothic UI");
}

void ShooterGame::StartBgm() {
    const auto& bgmPath = assets_.BgmPath();
    if (bgmPath.empty()) {
        statusText_ = L"assets/BGM/bgm_loop.wav が見つかりません";
        return;
    }

    const BOOL ok = PlaySoundW(bgmPath.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
    if (!ok) {
        statusText_ = L"BGM の再生に失敗";
    }
}

void ShooterGame::StopBgm() {
    PlaySoundW(nullptr, nullptr, 0);
}
