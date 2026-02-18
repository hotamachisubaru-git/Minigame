#pragma once

#include <string>

constexpr int kDesignWidth = 720;
constexpr int kDesignHeight = 1280;

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
    float radius = 24.0f;
    int hp = 4;
    float wobble = 0.0f;
    bool active = true;
};

struct EnemyBullet {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float radius = 8.0f;
    bool active = true;
};

struct TicketDrop {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 100.0f;
    float size = 34.0f;
    bool active = true;
};

struct Star {
    float x = 0.0f;
    float y = 0.0f;
    float speed = 55.0f;
    float size = 2.0f;
};

struct BossState {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 56.0f;
    float movePhase = 0.0f;
    float shotCooldown = 0.0f;
    long long hp = 0;
    long long maxHp = 0;
};

struct StageSpec {
    int stageNumber = 1;
    float stageDurationSec = 20.0f;
    float enemySpawnMinSec = 0.65f;
    float enemySpawnMaxSec = 1.10f;
    float enemySpeedMin = 95.0f;
    float enemySpeedMax = 145.0f;
    int enemyHpMin = 1;
    int enemyHpMax = 2;
    float ticketSpawnMinSec = 1.3f;
    float ticketSpawnMaxSec = 2.1f;
    float ticketSpeedMin = 72.0f;
    float ticketSpeedMax = 118.0f;
    long long bossHp = 80;
    float bossMoveAmplitude = 170.0f;
    float bossShotInterval = 1.1f;
    int bossBulletCount = 6;
    std::wstring bossName;
};
