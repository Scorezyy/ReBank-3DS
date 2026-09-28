#pragma once

#include <3ds.h>

#include <string_view>

namespace Gui {
struct GameVisual {
    u32 primary;
    u32 secondary;
};

GameVisual gameVisual(std::string_view code);
}
