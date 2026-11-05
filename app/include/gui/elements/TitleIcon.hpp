#pragma once

#include <citro2d.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

struct InstalledTitles {
    std::vector<std::uint64_t> sd;
    std::uint64_t cartridge = 0;

    static InstalledTitles scan();
    std::optional<std::pair<std::uint64_t, FS_MediaType>> find(std::span<const std::string_view> titleIds) const;
};

class TitleIcon {
public:
    static constexpr int Size = 48;
    using Pixels = std::array<std::uint16_t, Size * Size>;

    TitleIcon() = default;
    ~TitleIcon();
    TitleIcon(const TitleIcon&) = delete;
    TitleIcon& operator=(const TitleIcon&) = delete;

    static std::optional<Pixels> read(std::span<const std::string_view> titleIds, const InstalledTitles& installed);
    bool create(const Pixels& pixels);
    void release();
    bool loaded() const { return loaded_; }
    void draw(float x, float y, float depth, float size) const;

private:
    C3D_Tex texture_{};
    Tex3DS_SubTexture subtexture_{};
    bool loaded_ = false;
};
