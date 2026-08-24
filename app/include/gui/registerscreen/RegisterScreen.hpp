#pragma once

#include <3ds.h>

#include <string>

class App;

// The account-creation form: username, email, password, auto-login, submit.
class RegisterScreen {
public:
    explicit RegisterScreen(App& app) : app_(app) {}

    void open();
    void update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched);
    void render();
    void reset();

private:
    enum class Focus {
        Username,
        Email,
        Password,
        AutoLogin,
        Submit,
        Back
    };

    bool usernameValid() const;
    void step(int delta);
    void backToWelcome();

    App& app_;
    std::string username_;
    std::string email_;
    std::string password_;
    Focus focus_ = Focus::Username;
    u64 animationStartedAt_ = 0;
};
