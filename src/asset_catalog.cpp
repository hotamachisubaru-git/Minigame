#include "asset_catalog.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <set>
#include <string>
#include <vector>

namespace {

std::wstring ToLower(const std::wstring& src) {
    std::wstring lowered = src;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return lowered;
}

std::filesystem::path GetModuleDirectory() {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    return std::filesystem::path(modulePath).parent_path();
}

std::vector<std::filesystem::path> BuildSearchRoots() {
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

std::unique_ptr<Gdiplus::Image> LoadImageFromPath(const std::filesystem::path& filePath) {
    auto image = std::unique_ptr<Gdiplus::Image>(Gdiplus::Image::FromFile(filePath.c_str(), FALSE));
    if (!image || image->GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    return image;
}

std::unique_ptr<Gdiplus::Image> LoadImageByCandidates(const std::vector<std::wstring>& candidates) {
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

std::filesystem::path FindFileByCandidates(const std::vector<std::wstring>& candidates) {
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

bool IsBossImageFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return false;
    }
    std::wstring ext = ToLower(path.extension().wstring());
    return ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp";
}

std::vector<std::filesystem::path> DiscoverBossImageFiles() {
    const auto roots = BuildSearchRoots();
    std::vector<std::filesystem::path> files;
    std::set<std::wstring> seen;

    const std::vector<std::filesystem::path> bossDirs = {std::filesystem::path(L"assets/BOSS"),
                                                          std::filesystem::path(L"assets/boss")};

    for (const auto& root : roots) {
        for (const auto& relDir : bossDirs) {
            const auto fullDir = root / relDir;
            if (!std::filesystem::exists(fullDir) || !std::filesystem::is_directory(fullDir)) {
                continue;
            }

            try {
                for (const auto& entry : std::filesystem::directory_iterator(fullDir)) {
                    const auto file = entry.path();
                    if (!IsBossImageFile(file)) {
                        continue;
                    }
                    const auto key = ToLower(file.wstring());
                    if (seen.insert(key).second) {
                        files.push_back(file);
                    }
                }
            } catch (...) {
            }
        }
    }

    std::sort(files.begin(), files.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
        return ToLower(a.filename().wstring()) < ToLower(b.filename().wstring());
    });
    return files;
}

}  // namespace

bool AssetCatalog::Load() {
    pointIcon_ = LoadImageByCandidates(
        {L"assets/p_icon.png", L"assets/point.png", L"assets/icon_point.png", L"assets/1.png"});
    ticketIcon_ = LoadImageByCandidates(
        {L"assets/ticket_icon.png", L"assets/ticket.png", L"assets/icon_ticket.png", L"assets/3.png"});
    const auto discoveredBossImages = DiscoverBossImageFiles();

    bossStageImages_.clear();
    bossStageImages_.resize(10);
    for (int stage = 1; stage <= 10; ++stage) {
        const std::wstring stageNumber = std::to_wstring(stage);
        const std::wstring stageNumber2 = (stage < 10) ? (L"0" + stageNumber) : stageNumber;
        auto stageImage = LoadImageByCandidates({
            L"assets/BOSS/stage" + stageNumber + L".png",
            L"assets/BOSS/stage" + stageNumber2 + L".png",
            L"assets/BOSS/boss_stage" + stageNumber + L".png",
            L"assets/BOSS/boss" + stageNumber + L".png",
            L"assets/BOSS/" + stageNumber + L".png",
            L"assets/stage" + stageNumber + L"_boss.png",
            L"assets/boss_stage" + stageNumber + L".png",
            L"assets/boss" + stageNumber + L".png",
            L"assets/stage" + stageNumber + L".png",
            L"assets/BOSS/stage" + stageNumber + L".jpg",
            L"assets/BOSS/stage" + stageNumber2 + L".jpg",
            L"assets/BOSS/boss_stage" + stageNumber + L".jpg",
            L"assets/BOSS/boss" + stageNumber + L".jpg",
            L"assets/BOSS/" + stageNumber + L".jpg",
            L"assets/stage" + stageNumber + L"_boss.jpg",
            L"assets/boss_stage" + stageNumber + L".jpg",
            L"assets/boss" + stageNumber + L".jpg",
            L"assets/stage" + stageNumber + L".jpg",
            L"assets/BOSS/stage" + stageNumber + L".jpeg",
            L"assets/BOSS/stage" + stageNumber2 + L".jpeg",
            L"assets/BOSS/boss_stage" + stageNumber + L".jpeg",
            L"assets/BOSS/boss" + stageNumber + L".jpeg",
            L"assets/BOSS/" + stageNumber + L".jpeg",
            L"assets/stage" + stageNumber + L"_boss.jpeg",
            L"assets/boss_stage" + stageNumber + L".jpeg",
            L"assets/boss" + stageNumber + L".jpeg",
            L"assets/stage" + stageNumber + L".jpeg",
        });

        if (!stageImage) {
            const std::size_t index = static_cast<std::size_t>(stage - 1);
            if (index < discoveredBossImages.size()) {
                stageImage = LoadImageFromPath(discoveredBossImages[index]);
            }
        }

        bossStageImages_[static_cast<std::size_t>(stage - 1)] = std::move(stageImage);
    }
    bgmPath_ = FindFileByCandidates(
        {L"assets/BGM/bgm_loop.mp3", L"assets/BGM/bgm_loop.wav", L"assets/bgm_loop.mp3", L"assets/bgm_loop.wav"});

    stageBgmPaths_.clear();
    stageBgmPaths_.resize(10);
    bossStageBgmPaths_.clear();
    bossStageBgmPaths_.resize(10);
    for (int stage = 1; stage <= 10; ++stage) {
        const std::wstring stageNumber = std::to_wstring(stage);
        const std::wstring stageNumber2 = (stage < 10) ? (L"0" + stageNumber) : stageNumber;
        stageBgmPaths_[static_cast<std::size_t>(stage - 1)] = FindFileByCandidates({
            L"assets/BGM/stage" + stageNumber + L".mp3",
            L"assets/BGM/stage" + stageNumber2 + L".mp3",
            L"assets/BGM/bgm_stage" + stageNumber + L".mp3",
            L"assets/BGM/" + stageNumber + L".mp3",
            L"assets/BGM/stage" + stageNumber + L".wav",
            L"assets/BGM/stage" + stageNumber2 + L".wav",
            L"assets/BGM/bgm_stage" + stageNumber + L".wav",
            L"assets/BGM/" + stageNumber + L".wav",
        });
        bossStageBgmPaths_[static_cast<std::size_t>(stage - 1)] = FindFileByCandidates({
            L"assets/BGM/boss_stage" + stageNumber + L".mp3",
            L"assets/BGM/boss_stage" + stageNumber2 + L".mp3",
            L"assets/BGM/stage" + stageNumber + L"_boss.mp3",
            L"assets/BGM/stage" + stageNumber2 + L"_boss.mp3",
            L"assets/BGM/boss" + stageNumber + L".mp3",
            L"assets/BGM/boss" + stageNumber2 + L".mp3",
            L"assets/BGM/boss/stage" + stageNumber + L".mp3",
            L"assets/BGM/boss/stage" + stageNumber2 + L".mp3",
            L"assets/BGM/boss_stage" + stageNumber + L".wav",
            L"assets/BGM/boss_stage" + stageNumber2 + L".wav",
            L"assets/BGM/stage" + stageNumber + L"_boss.wav",
            L"assets/BGM/stage" + stageNumber2 + L"_boss.wav",
            L"assets/BGM/boss" + stageNumber + L".wav",
            L"assets/BGM/boss" + stageNumber2 + L".wav",
            L"assets/BGM/boss/stage" + stageNumber + L".wav",
            L"assets/BGM/boss/stage" + stageNumber2 + L".wav",
        });
    }
    return true;
}

Gdiplus::Image* AssetCatalog::PointIcon() {
    return pointIcon_.get();
}

Gdiplus::Image* AssetCatalog::TicketIcon() {
    return ticketIcon_.get();
}

Gdiplus::Image* AssetCatalog::BossImage(int stageNumber) {
    if (stageNumber >= 1 && stageNumber <= static_cast<int>(bossStageImages_.size())) {
        return bossStageImages_[static_cast<std::size_t>(stageNumber - 1)].get();
    }
    return nullptr;
}

const std::filesystem::path& AssetCatalog::BgmPath() const {
    return bgmPath_;
}

const std::filesystem::path& AssetCatalog::StageBgmPath(int stageNumber) const {
    if (stageNumber >= 1 && stageNumber <= static_cast<int>(stageBgmPaths_.size())) {
        const auto& stagePath = stageBgmPaths_[static_cast<std::size_t>(stageNumber - 1)];
        if (!stagePath.empty()) {
            return stagePath;
        }
    }
    return bgmPath_;
}

const std::filesystem::path& AssetCatalog::BossStageBgmPath(int stageNumber) const {
    if (stageNumber >= 1 && stageNumber <= static_cast<int>(bossStageBgmPaths_.size())) {
        const auto& bossPath = bossStageBgmPaths_[static_cast<std::size_t>(stageNumber - 1)];
        if (!bossPath.empty()) {
            return bossPath;
        }
    }
    return StageBgmPath(stageNumber);
}
