#pragma once

class App;

// Shows the last lines of the in-app log, toggled by SELECT from any screen.
class LogsScreen {
public:
    explicit LogsScreen(App& app) : app_(app) {}
    void render();

private:
    App& app_;
};
