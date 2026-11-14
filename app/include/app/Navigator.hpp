#pragma once

#include "account/AuthController.hpp"
#include "app/ScreenId.hpp"
#include "i18n/TextId.hpp"

#include <cstddef>
#include <string>
#include <string_view>

class Navigator {
public:
    virtual ~Navigator() = default;

    virtual void show(ScreenId screen) = 0;
    virtual ScreenId current() const = 0;
    virtual void showError(TextId title, TextId message) = 0;
    virtual void showError(std::string title, std::string message) = 0;
    virtual void requestText(std::string& destination, std::string_view hint, bool password,
                             std::size_t maxLength = 256) = 0;
    virtual void beginAuth(AuthRequest request) = 0;
    virtual bool authRunning() const = 0;
    virtual bool updateRunning() const = 0;
    virtual bool loadingWork() const = 0;
    virtual void toggleAutoLogin() = 0;
    virtual void logout() = 0;
    virtual void finishIntro() = 0;
};
