#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <filesystem>
#include <memory>

class AssetCatalog {
public:
    bool Load();

    Gdiplus::Image* PointIcon();
    Gdiplus::Image* TicketIcon();
    const std::filesystem::path& BgmPath() const;

private:
    std::unique_ptr<Gdiplus::Image> pointIcon_;
    std::unique_ptr<Gdiplus::Image> ticketIcon_;
    std::filesystem::path bgmPath_;
};
