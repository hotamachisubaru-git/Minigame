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

}  // namespace

bool AssetCatalog::Load() {
    pointIcon_ = LoadImageByCandidates(
        {L"assets/p_icon.png", L"assets/point.png", L"assets/icon_point.png", L"assets/1.png"});
    ticketIcon_ = LoadImageByCandidates(
        {L"assets/ticket_icon.png", L"assets/ticket.png", L"assets/icon_ticket.png", L"assets/3.png"});
    bgmPath_ = FindFileByCandidates({L"assets/BGM/bgm_loop.wav", L"assets/bgm_loop.wav"});
    return true;
}

Gdiplus::Image* AssetCatalog::PointIcon() {
    return pointIcon_.get();
}

Gdiplus::Image* AssetCatalog::TicketIcon() {
    return ticketIcon_.get();
}

const std::filesystem::path& AssetCatalog::BgmPath() const {
    return bgmPath_;
}
