#pragma once

#include "gui/Theme.hpp"

#include <3ds.h>

struct InputFrame {
    static constexpr s16 StickThreshold = 60;

    u32 down = 0;
    u32 held = 0;
    circlePosition circle{};
    touchPosition touch{};

    static InputFrame read() {
        hidScanInput();
        InputFrame input{hidKeysDown(), hidKeysHeld(), {}, {}};
        hidCircleRead(&input.circle);
        hidTouchRead(&input.touch);
        return input;
    }

    bool pressed(u32 keys) const { return (down & keys) != 0; }
    bool touched() const { return pressed(KEY_TOUCH); }
    bool tapped(const UiRect& rect) const { return touched() && rect.contains(touch); }
    int verticalStep() const {
        if (pressed(KEY_UP) || circle.dy > StickThreshold) {
            return -1;
        }
        return pressed(KEY_DOWN) || circle.dy < -StickThreshold ? 1 : 0;
    }
};
