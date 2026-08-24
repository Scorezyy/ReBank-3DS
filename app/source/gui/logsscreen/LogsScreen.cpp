#include "gui/logsscreen/LogsScreen.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"
#include "gui/Theme.hpp"

void LogsScreen::render() {
    app_.drawText("ReBank Log", 12.0F, 10.0F, 0.62F, Gui::Ink);
    app_.drawText("SELECT", 253.0F, 13.0F, 0.38F, Gui::Brand);
    const auto& entries = Logger::instance().entries();
    const std::size_t visible = 10;
    const std::size_t first = entries.size() > visible ? entries.size() - visible : 0;
    for (std::size_t index = first; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        u32 color = Gui::Ink;
        if (entry.level == LogLevel::Warning) {
            color = Gui::Accent;
        } else if (entry.level == LogLevel::Error) {
            color = Gui::Error;
        }
        const std::string line = entry.message.substr(0, 52);
        app_.drawText(line, 12.0F, 39.0F + static_cast<float>(index - first) * 18.0F, 0.38F, color);
    }
}
