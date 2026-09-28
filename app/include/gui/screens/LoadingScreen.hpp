#pragma once

#include "gui/Screen.hpp"

#include <string>

class LoadingScreen : public Screen {
public:
    using Screen::Screen;

    void tick();
    bool finishing() const { return finishing_; }
    void update(const InputFrame&) override {}
    void renderTop(float eyeOffset) override;
    void render() override;

private:
    enum class Stage {
        CheckingUpdates,
        SigningIn,
        SearchingGames,
        ReadingIcons,
        ReadingSave,
        SearchingPokemon,
        LoadingBank,
        WelcomeBack,
        Waiting
    };

    struct StageText {
        TextId headline;
        TextId detail;
        bool determinate;
    };

    static StageText textFor(Stage stage, bool autoLogin);
    Stage currentStage() const;
    int serviceProgress(Stage stage) const;
    std::string welcomeBack() const;

    Stage shownStage_ = Stage::Waiting;
    float displayed_ = 0.0F;
    bool finishing_ = false;
};
