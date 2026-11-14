#pragma once

#include "bank/selection/CursorDirection.hpp"
#include "gui/InputFrame.hpp"

#include <3ds.h>

#include <cstdint>

class DirectionRepeat {
public:
    static CursorDirection fromKeys(u32 keys);

    CursorDirection next(const InputFrame& input);

private:
    CursorDirection held_ = CursorDirection::None;
    std::uint64_t repeatAt_ = 0;
};
