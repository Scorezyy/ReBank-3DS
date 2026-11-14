#include "app/AccountFlow.hpp"

#include "platform/Clock.hpp"
#include "platform/Logger.hpp"

#include <utility>

namespace {
constexpr double WelcomeBackSeconds = 1.1;
constexpr int Unauthorized = 401;
}

AccountFlow::AccountFlow(AccountApi& api, AccountState& account, LoadService& loads)
    : account_(account), loads_(loads), auth_(api) {}

void AccountFlow::initialize() {
    identity_.resolve();
    SessionStore::removeLegacyCredentials();
}

std::optional<AuthRequest> AccountFlow::storedSession() {
    std::string refreshToken;
    if (!sessionStore_.load(refreshToken, account_.username)) {
        return std::nullopt;
    }
    account_.autoLogin = true;
    account_.restoringSession = true;
    return AuthRequest::refresh(std::move(refreshToken));
}

bool AccountFlow::begin(AuthRequest request) {
    request.deviceFingerprint = identity_.fingerprint();
    if (auth_.begin(std::move(request))) {
        return true;
    }
    Logger::instance().error("Authentication worker creation failed");
    return false;
}

AuthOutcome AccountFlow::poll() {
    AuthController::Completed completed;
    if (!auth_.poll(completed)) {
        return {};
    }
    return completed.result.success ? signedIn(std::move(completed)) : failed(completed);
}

AuthOutcome AccountFlow::failed(const AuthController::Completed& completed) {
    const AuthResult& result = completed.result;
    Logger::instance().warning("Authentication failed");
    account_.restoringSession = false;
    const bool sessionLost = completed.operation == AuthOperation::Refresh && !result.networkError
        && result.httpStatus == Unauthorized;
    return {AuthOutcome::Kind::Failed, completed.operation, result.message, result.networkError, sessionLost};
}

AuthOutcome AccountFlow::signedIn(AuthController::Completed completed) {
    if (completed.operation == AuthOperation::ResetPassword) {
        Logger::instance().info("Password reset accepted");
        return {AuthOutcome::Kind::PasswordResetAccepted, completed.operation, completed.result.message};
    }
    account_.session = std::move(completed.result.session);
    if (!completed.username.empty()) {
        account_.username = completed.username;
    }
    if (account_.autoLogin) {
        sessionStore_.save(account_.session.refreshToken, account_.username);
    } else {
        sessionStore_.clear();
    }
    Logger::instance().info("Authentication succeeded");

    if (std::exchange(account_.restoringSession, false)) {
        account_.welcomeBackPending = true;
        welcomeBackUntil_ = Clock::after(WelcomeBackSeconds);
    } else {
        loads_.begin(LoadService::Operation::LoadBank);
    }
    return {AuthOutcome::Kind::SignedIn, completed.operation, completed.result.message};
}

void AccountFlow::pollWelcomeBack() {
    if (!account_.welcomeBackPending || !Clock::reached(welcomeBackUntil_)) {
        return;
    }
    account_.welcomeBackPending = false;
    loads_.begin(LoadService::Operation::LoadBank);
}

void AccountFlow::toggleAutoLogin() {
    account_.autoLogin = !account_.autoLogin;
    if (!account_.autoLogin) {
        sessionStore_.clear();
    }
}

void AccountFlow::forget() {
    sessionStore_.clear();
    account_.session = {};
    account_.restoringSession = false;
    account_.welcomeBackPending = false;
    loads_.invalidate();
}

void AccountFlow::signOut() {
    forget();
    account_.username.clear();
    Logger::instance().info("User logged out");
}
