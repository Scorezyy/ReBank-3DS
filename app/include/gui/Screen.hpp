#pragma once

#include "gui/InputFrame.hpp"
#include "i18n/Localization.hpp"

#include <string>
#include <string_view>

class App;
class UiRenderer;

enum class ScreenId {
    Intro,
    Welcome,
    Login,
    Register,
    ResetPassword,
    GameSelect,
    Bank,
    Logs
};

class Screen {
public:
    explicit Screen(App& app) : app_(app) {}
    virtual ~Screen() = default;
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    virtual void update(const InputFrame& input) = 0;
    virtual void renderTop(float eyeOffset);
    virtual void render() = 0;

protected:
    UiRenderer& ui() const;
    std::string_view text(TextId id) const;
    std::string format(TextId id, std::initializer_list<std::string_view> arguments) const;

    App& app_;
};
