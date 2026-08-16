#pragma once

#include <3ds.h>

#include <array>
#include <cstddef>
#include <string_view>

enum class TextId : std::size_t {
    Tagline,
    Login,
    Register,
    Username,
    Email,
    Password,
    ForgotPassword,
    Submit,
    Back,
    CreateAccount,
    ResetPassword,
    Count
};

class Localization {
public:
    Localization();
    std::string_view get(TextId id) const;

private:
    using Translation = std::array<std::string_view, static_cast<std::size_t>(TextId::Count)>;
    const Translation* translation_;
};