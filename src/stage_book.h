#pragma once

#include "game_types.h"

#include <vector>

class StageBook {
public:
    static constexpr int kTotalStages = 10;

    StageBook();

    const StageSpec& Get(int stage) const;
    int TotalStages() const;

private:
    std::vector<StageSpec> specs_;
};
