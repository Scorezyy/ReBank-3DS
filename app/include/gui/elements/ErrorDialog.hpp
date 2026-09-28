#pragma once

#include "core/ErrorNotice.hpp"
#include "gui/UiRenderer.hpp"

#include <string_view>

namespace Gui {
inline constexpr UiRect ErrorDialogOkButton{92.0F, 190.0F, 136.0F, 34.0F};

void drawErrorDialog(UiRenderer& ui, const ErrorNotice& notice, std::string_view fallbackMessage,
                     std::string_view okLabel);
}
