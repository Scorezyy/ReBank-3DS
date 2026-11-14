#pragma once

#include "app/AppContext.hpp"
#include "gui/InputFrame.hpp"

#include <string>
#include <string_view>

class Screen {
public:
    explicit Screen(AppContext& app) : app_(app) {}
    virtual ~Screen() = default;
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    virtual void enter() {}
    virtual void update(const InputFrame& input) = 0;
    virtual void renderTop(float eyeOffset);
    virtual void render() = 0;

protected:
    UiRenderer& ui() const { return app_.ui; }
    std::string_view text(TextId id) const { return app_.text.get(id); }
    void drawLogo(float eyeOffset, float y, float scale) const;
    std::string format(TextId id, std::initializer_list<std::string_view> arguments) const {
        return app_.text.format(id, arguments);
    }

    AppContext& app_;
};
