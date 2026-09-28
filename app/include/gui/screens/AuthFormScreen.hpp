#pragma once

#include "gui/Screen.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class AuthFormScreen : public Screen {
public:
    void open();
    void reset();
    void update(const InputFrame& input) override;
    void render() override;

protected:
    struct Field {
        TextId label;
        UiRect bounds;
        bool secret = false;
        float slideDirection = 0.0F;
        std::string value;
    };

    AuthFormScreen(App& app, ScreenId id, TextId title, std::vector<Field> fields,
                   std::optional<UiRect> autoLoginBounds);

    virtual void submit() = 0;
    const std::string& value(std::size_t field) const { return fields_[field].value; }
    bool rejectIf(bool invalid, TextId title, TextId message);

private:
    enum class Item { Field, AutoLogin, Submit, Back };

    Item itemAt(std::size_t index) const;
    std::size_t itemCount() const;
    std::size_t indexOf(Item item) const;
    void focus(Item item) { focus_ = indexOf(item); }
    void activate();
    void edit(std::size_t field);
    void back();

    ScreenId id_;
    TextId title_;
    std::vector<Field> fields_;
    std::optional<UiRect> autoLoginBounds_;
    std::size_t focus_ = 0;
    u64 animationStartedAt_ = 0;
};
