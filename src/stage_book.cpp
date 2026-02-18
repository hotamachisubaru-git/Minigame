#include "stage_book.h"

#include <algorithm>

namespace {
constexpr int kBossHpN = 10;
constexpr int kEnemyHpN = 1;
}

StageBook::StageBook() {
    specs_.reserve(kTotalStages);

    for (int i = 0; i < kTotalStages; ++i) {
        StageSpec spec;
        spec.stageNumber = i + 1;
        spec.stageDurationSec = 16.0f + static_cast<float>(i) * 2.5f;
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
        spec.bossHp = std::max(1, 30 * kBossHpN * spec.stageNumber); // 300, 600, 900, ..., 3000
        spec.bossMoveAmplitude = 140.0f + static_cast<float>(i) * 9.0f;
        spec.bossShotInterval = std::max(0.36f, 1.08f - static_cast<float>(i) * 0.05f);
        spec.bossBulletCount = std::min(14, 6 + i);

        switch (spec.stageNumber) {
        case 1:  spec.bossName = L"スカイ・ガード"; break;
        case 2:  spec.bossName = L"ルイン・ビーター"; break;
        case 3:  spec.bossName = L"エレキ・リッパー"; break;
        case 4:  spec.bossName = L"ヴォイド・コア"; break;
        case 5:  spec.bossName = L"クロム・ファング"; break;
        case 6:  spec.bossName = L"レイザー・ハーピー"; break;
        case 7:  spec.bossName = L"アーク・ドミナ"; break;
        case 8:  spec.bossName = L"ネオン・ウォーデン"; break;
        case 9:  spec.bossName = L"ストーム・レクス"; break;
        case 10: spec.bossName = L"ラスト・オーバーロード"; break;
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
