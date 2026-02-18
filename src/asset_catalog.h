#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <filesystem>
#include <memory>
#include <vector>

class AssetCatalog {
public:
    bool Load();

    Gdiplus::Image* PointIcon();
    Gdiplus::Image* TicketIcon();
    Gdiplus::Image* BossImage(int stageNumber);
    const std::filesystem::path& BgmPath() const;
    const std::filesystem::path& StageBgmPath(int stageNumber) const;
    const std::filesystem::path& BossStageBgmPath(int stageNumber) const;

private:
    std::unique_ptr<Gdiplus::Image> pointIcon_;
    std::unique_ptr<Gdiplus::Image> ticketIcon_;
    std::vector<std::unique_ptr<Gdiplus::Image>> bossStageImages_;
    std::filesystem::path bgmPath_;
    std::vector<std::filesystem::path> stageBgmPaths_;
    std::vector<std::filesystem::path> bossStageBgmPaths_;
};
