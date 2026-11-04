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
                   std::optional<UiRect> autoLoginBounds, std::optional<UiRect> consentBounds = std::nullopt);

    virtual void submit() = 0;
    virtual void openConsentDocument() {}
    const std::string& value(std::size_t field) const { return fields_[field].value; }
    bool consentAccepted() const { return consentAccepted_; }
    void setConsentAccepted(bool accepted) { consentAccepted_ = accepted; }
    bool rejectIf(bool invalid, TextId title, TextId message);

private:
    enum class Item { Field, AutoLogin, Consent, Submit, Back };

    Item itemAt(std::size_t index) const;
    std::size_t itemCount() const;
    std::size_t indexOf(Item item) const;
    void focus(Item item) { focus_ = indexOf(item); }
    void activate();
    void edit(std::size_t field);
    void back();
    UiRect consentLinkBounds() const;
    float consentTextSize() const;
    void renderConsent(float pulse);

    ScreenId id_;
    TextId title_;
    std::vector<Field> fields_;
    std::optional<UiRect> autoLoginBounds_;
    std::optional<UiRect> consentBounds_;
    bool consentAccepted_ = false;
    std::size_t focus_ = 0;
    u64 animationStartedAt_ = 0;
};
