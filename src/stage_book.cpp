#include "stage_book.h"

#include <algorithm>

namespace {
constexpr int kEnemyHpN = 1;
constexpr float kStageDurationScale = 2.0f;
constexpr long long kBossHpStage1 = 9000LL;
constexpr long long kBossHpStage2 = 400000LL;
constexpr long long kBossHpStage3 = 800000LL;
constexpr long long kBossHpStage4 = 36000000LL;
constexpr long long kBossHpStage5 = 72000000LL;
constexpr long long kBossHpStage6 = 144000000LL;
constexpr long long kBossHpStage7 = 288800000LL;
constexpr long long kBossHpStage8 = 1073741823LL;
constexpr long long kBossHpStage9 = 2147483647LL;
constexpr long long kBossHpStage10 = 9223372036LL;
}

StageBook::StageBook() {
    specs_.reserve(kTotalStages);

    for (int i = 0; i < kTotalStages; ++i) {
        StageSpec spec;
        spec.stageNumber = i + 1;
        spec.stageDurationSec = (16.0f + static_cast<float>(i) * 2.5f) * kStageDurationScale;
        spec.enemySpawnMinSec = std::max(0.20f, 0.70f - static_cast<float>(i) * 0.04f);
        spec.enemySpawnMaxSec = std::max(0.36f, 1.10f - static_cast<float>(i) * 0.04f);
        spec.enemySpeedMin = 96.0f + static_cast<float>(i) * 10.0f;
        spec.enemySpeedMax = 155.0f + static_cast<float>(i) * 12.0f;
        spec.enemyHpMin = std::max(1, 3 * kEnemyHpN * spec.stageNumber);
        spec.enemyHpMax = spec.enemyHpMin;
        spec.ticketSpawnMinSec = std::max(0.85f, 1.45f - static_cast<float>(i) * 0.05f);
        spec.ticketSpawnMaxSec = std::max(1.35f, 2.25f - static_cast<float>(i) * 0.05f);
        spec.ticketSpeedMin = 66.0f + static_cast<float>(i) * 2.6f;
        spec.ticketSpeedMax = 108.0f + static_cast<float>(i) * 2.8f;
        spec.bossHp = kBossHpStage1;
        spec.bossMoveAmplitude = 140.0f + static_cast<float>(i) * 9.0f;
        spec.bossShotInterval = std::max(0.36f, 1.08f - static_cast<float>(i) * 0.05f);
        spec.bossBulletCount = std::min(14, 6 + i);

        switch (spec.stageNumber) {
        case 1:
            spec.bossName = L"スカイ・ガード";
            spec.bossHp = kBossHpStage1;
            break;
        case 2:
            spec.bossName = L"ルイン・ビーター";
            spec.bossHp = kBossHpStage2;
            break;
        case 3:
            spec.bossName = L"エレキ・リッパー";
            spec.bossHp = kBossHpStage3;
            break;
        case 4:
            spec.bossName = L"ヴォイド・コア";
            spec.bossHp = kBossHpStage4;
            break;
        case 5:
            spec.bossName = L"クロム・ファング";
            spec.bossHp = kBossHpStage5;
            break;
        case 6:
            spec.bossName = L"レイザー・ハーピー";
            spec.bossHp = kBossHpStage6;
            break;
        case 7:
            spec.bossName = L"アーク・ドミナ";
            spec.bossHp = kBossHpStage7;
            break;
        case 8:
            spec.bossName = L"ネオン・ウォーデン";
            spec.bossHp = kBossHpStage8;
            break;
        case 9:
            spec.bossName = L"ストーム・レクス";
            spec.bossHp = kBossHpStage9;
            break;
        case 10:
            spec.bossName = L"ラスト・オーバーロード";
            spec.bossHp = kBossHpStage10;
            break;
        default:
            spec.bossName = L"未知のボス";
            break;
        }

        specs_.push_back(spec);
    }
}

const StageSpec& StageBook::Get(int stage) const {
    const int clamped = std::clamp(stage, 1, kTotalStages);
    return specs_[static_cast<std::size_t>(clamped - 1)];
}

int StageBook::TotalStages() const {
    return kTotalStages;
}
