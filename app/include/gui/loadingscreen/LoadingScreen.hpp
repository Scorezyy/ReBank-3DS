#pragma once

class App;

// The loading spinner shown whenever an update check, sign-in, or a
// background fetch that should block interaction is in progress.
class LoadingScreen {
public:
    explicit LoadingScreen(App& app) : app_(app) {}

    void renderTop(float eyeOffset);
    void render();

private:
    enum class Stage {
        CheckingUpdates,
        SigningIn,
        SearchingGames,
        ReadingIcons,
        ReadingSave,
        SearchingPokemon,
        LoadingBank,
        Waiting
    };

    Stage currentStage() const;

    App& app_;
};
