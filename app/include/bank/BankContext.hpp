#pragma once

#include "core/ErrorNotice.hpp"
#include "i18n/Localization.hpp"
#include "network/ApiClient.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

class LoadService;

struct BankContext {
    static constexpr std::size_t DefaultBoxLimit = 50;

    ApiClient& api;
    LoadService& loads;
    const AccountSession& account;
    std::string& status;
    ErrorNotice& errors;
    const Localization& text;
    std::function<void()> leaveBank;
    std::function<void(std::string&, std::string_view, std::size_t)> requestText;

    std::size_t cloudBoxLimit() const { return account.boxLimit == 0 ? DefaultBoxLimit : account.boxLimit; }
    bool signedIn() const { return !account.accessToken.empty(); }
};
