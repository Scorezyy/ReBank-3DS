#include "gui/screens/system/IntroScreen.hpp"


void IntroScreen::update(const InputFrame&) {
    app_.navigator.finishIntro();
}

void IntroScreen::render() {
    ui().drawCentered("ReBank", 160.0F, 94.0F, 1.15F, Gui::Ink);
    ui().drawCentered("A", 160.0F, 154.0F, 0.55F, Gui::Muted);
}
