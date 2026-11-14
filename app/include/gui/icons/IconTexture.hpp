#pragma once

#include "gui/icons/IconPixels.hpp"

#include <citro2d.h>

class IconTexture {
public:
    enum class Layout { Linear, Tiled };

    IconTexture() = default;
    ~IconTexture();
    IconTexture(const IconTexture&) = delete;
    IconTexture& operator=(const IconTexture&) = delete;

    bool create(const IconPixels& pixels, Layout layout);
    void release();
    bool loaded() const { return loaded_; }
    void draw(float x, float y, float depth, float size) const;

private:
    C3D_Tex texture_{};
    Tex3DS_SubTexture subtexture_{};
    bool loaded_ = false;
};
