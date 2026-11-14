#pragma once

#include "account/AuthController.hpp"
#include "account/DeviceIdentity.hpp"
#include "account/SessionStore.hpp"
#include "app/AppContext.hpp"
#include "bank/LoadService.hpp"

#include <cstdint>
#include <optional>
#include <string>

struct AuthOutcome {
    enum class Kind { None, SignedIn, PasswordResetAccepted, Failed };

    Kind kind = Kind::None;
    AuthOperation operation = AuthOperation::Login;
    std::string message;
    bool networkError = false;
    bool sessionLost = false;
};

class AccountFlow {
public:
    AccountFlow(AccountApi& api, AccountState& account, LoadService& loads);

    void initialize();
    std::optional<AuthRequest> storedSession();
    bool begin(AuthRequest request);
    bool running() const { return auth_.isRunning(); }
    AuthOutcome poll();
    void pollWelcomeBack();
    void toggleAutoLogin();
    void forget();
    void signOut();

private:
    AuthOutcome signedIn(AuthController::Completed completed);
    AuthOutcome failed(const AuthController::Completed& completed);

    AccountState& account_;
    LoadService& loads_;
    DeviceIdentity identity_;
    SessionStore sessionStore_{identity_};
    AuthController auth_;
    std::uint64_t welcomeBackUntil_ = 0;
};
