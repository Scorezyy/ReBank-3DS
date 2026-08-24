#pragma once

#include <3ds.h>

class App;

// Landing screen: auto-login on A, or open Login/Register/Reset by touch.
class WelcomeScreen {
public:
    explicit WelcomeScreen(App& app) : app_(app) {}

    void update(u32 keysDown, touchPosition touch, bool touched);
    void render();

private:
    App& app_;
};
