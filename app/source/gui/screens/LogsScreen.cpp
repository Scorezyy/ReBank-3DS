#include "gui/screens/LogsScreen.hpp"
#include "core/Logger.hpp"
#include "gui/UiRenderer.hpp"

using namespace Gui;

namespace {
constexpr std::size_t VisibleLines = 10;
constexpr std::size_t MaxLineLength = 52;

u32 colorFor(LogLevel level) {
    switch (level) {
        case LogLevel::Warning:
            return Accent;
        case LogLevel::Error:
            return Error;
        case LogLevel::Info:
            break;
    }
    return Ink;
}
}

void LogsScreen::render() {
    ui().drawText("ReBank Log", 12.0F, 10.0F, 0.62F, Ink);
    ui().drawText("SELECT", 253.0F, 13.0F, 0.38F, Brand);
    const std::vector<LogEntry> entries = Logger::instance().recent(VisibleLines);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        ui().drawText(entries[index].message.substr(0, MaxLineLength), 12.0F,
                      39.0F + static_cast<float>(index) * 18.0F, 0.38F, colorFor(entries[index].level));
    }
}
