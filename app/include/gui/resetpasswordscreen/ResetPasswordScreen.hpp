#pragma once

#include <3ds.h>

#include <string>

class App;

// The password-reset form: an email field plus submit.
class ResetPasswordScreen {
public:
    explicit ResetPasswordScreen(App& app) : app_(app) {}

    void open();
    void update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched);
    void render();
    void reset();

private:
    enum class Focus {
        Email,
        Submit,
        Back
    };

    void step(int delta);
    void backToWelcome();

    App& app_;
    std::string email_;
    Focus focus_ = Focus::Email;
    u64 animationStartedAt_ = 0;
};
