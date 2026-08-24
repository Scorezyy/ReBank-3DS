#include "gui/Theme.hpp"

bool UiRect::contains(touchPosition point) const {
    return point.px >= x && point.px <= x + width && point.py >= y && point.py <= y + height;
}
