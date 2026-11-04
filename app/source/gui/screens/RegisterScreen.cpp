#include "gui/screens/RegisterScreen.hpp"
#include "app/App.hpp"
#include "core/InputValidation.hpp"

#include <3ds.h>

namespace {
enum RegisterField : std::size_t { Username, Email, Password };
}

RegisterScreen::RegisterScreen(App& app)
    : AuthFormScreen(app, ScreenId::Register, TextId::CreateAccount,
                     {{TextId::Username, {24.0F, 48.0F, 272.0F, 34.0F}, false, 1.0F, {}},
                      {TextId::Email, {24.0F, 88.0F, 272.0F, 34.0F}, false, -1.0F, {}},
                      {TextId::Password, {24.0F, 128.0F, 272.0F, 34.0F}, true, 1.0F, {}}},
                     std::nullopt, UiRect{24.0F, 166.0F, 272.0F, 26.0F}) {}

void RegisterScreen::update(const InputFrame& input) {
    Policy policy;
    if (policyTask_.poll(policy)) {
        app_.status_.clear();
        if (policy) {
            policy->insert(policy->begin(), PolicyBlock{PolicyBlock::Kind::Subheading, std::string(text(TextId::PolicyAgeConfirmation)), {}});
            reader_.open(std::move(*policy));
        } else {
            app_.showError(TextId::PrivacyUnavailableTitle, TextId::PrivacyUnavailableMessage);
        }
        return;
    }
    if (reader_.isOpen()) {
        const PolicyReader::Outcome outcome = reader_.update(input);
        if (outcome != PolicyReader::Outcome::None) {
            setConsentAccepted(outcome == PolicyReader::Outcome::Accepted);
        }
        return;
    }
    if (policyTask_.running()) {
        return;
    }
    AuthFormScreen::update(input);
}

void RegisterScreen::render() {
    if (reader_.isOpen()) {
        reader_.render(ui(), text(TextId::PrivacyRequiredTitle), text(TextId::PolicyDecline), text(TextId::PolicyAccept));
        return;
    }
    AuthFormScreen::render();
}

void RegisterScreen::openConsentDocument() {
    if (policyTask_.running() || reader_.isOpen()) {
        return;
    }
    u8 language = CFG_LANGUAGE_EN;
    const std::string code = R_SUCCEEDED(CFGU_GetSystemLanguage(&language)) && language == CFG_LANGUAGE_DE ? "de" : "en";
    app_.status_ = text(TextId::PrivacyLoading);
    if (!policyTask_.start([this, code]() { return app_.api_.privacyPolicy(code); })) {
        app_.status_.clear();
        app_.showError(TextId::PrivacyUnavailableTitle, TextId::PrivacyUnavailableMessage);
    }
}

void RegisterScreen::submit() {
    if (rejectIf(!isValidUsername(value(Username)), TextId::InvalidUsernameTitle, TextId::InvalidUsernameMessage)
        || rejectIf(!isValidEmail(value(Email)), TextId::InvalidEmailTitle, TextId::InvalidEmailMessage)
        || rejectIf(!isValidPassword(value(Password)), TextId::InvalidPasswordTitle, TextId::InvalidPasswordMessage)
        || rejectIf(!consentAccepted(), TextId::PrivacyRequiredTitle, TextId::PrivacyRequiredMessage)) {
        return;
    }
    app_.beginAuth(AuthRequest::registration(value(Username), value(Email), value(Password)));
}
