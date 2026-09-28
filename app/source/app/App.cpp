#include "app/App.hpp"
#include "core/Logger.hpp"
#include "core/ServerConfig.hpp"
#include "gui/Theme.hpp"
#include "gui/elements/ErrorDialog.hpp"

#include <utils/i18n.hpp>

#include <array>
#include <cstring>
#include <utility>

using namespace Gui;

namespace {
constexpr double WelcomeBackSeconds = 1.1;
constexpr float StereoDepth = 2.5F;
}

App::App(std::string executablePath, bool homebrew)
    : executablePath_(std::move(executablePath)), homebrew_(homebrew) {
    Logger::instance().initialize();
    Logger::instance().info("Client boot");
    Logger::instance().info("Server " + ServerConfig::baseUrl());
    romfsInit();
    i18n::init(pksm::Language::ENG);
    music_.start("romfs:/assets/music.ogg", "romfs:/assets/music_vc.ogg");
    resources_.load();
    ui_.setFont(resources_.textFont);
    deviceIdentity_.resolve();
    SessionStore::removeLegacyCredentials();
    beginUpdate();
}

App::~App() {
    Logger::instance().info("Client shutdown");
    music_.stop();
    i18n::exit();
    romfsExit();
}

int App::run() {
    while (aptMainLoop() && running_) {
        update(InputFrame::read());
        render();
    }
    return 0;
}

bool App::onAuthScreen() const {
    return screen_ == ScreenId::Welcome || screen_ == ScreenId::Login || screen_ == ScreenId::Register
        || screen_ == ScreenId::ResetPassword;
}

bool App::canExit() const {
    return !isLoading() && (onAuthScreen() || screen_ == ScreenId::GameSelect);
}

Screen& App::activeScreen() {
    switch (screen_) {
        case ScreenId::Intro:
            return introScreen_;
        case ScreenId::Welcome:
            return welcomeScreen_;
        case ScreenId::Login:
            return loginScreen_;
        case ScreenId::Register:
            return registerScreen_;
        case ScreenId::ResetPassword:
            return resetPasswordScreen_;
        case ScreenId::GameSelect:
            return gameSelectScreen_;
        case ScreenId::Bank:
            return bankScreen_;
        case ScreenId::Logs:
            break;
    }
    return logsScreen_;
}

void App::update(const InputFrame& input) {
    if (input.pressed(KEY_START) && canExit()) {
        running_ = false;
        return;
    }
    pollBackgroundWork();
    if (errors_.visible()) {
        if (input.pressed(KEY_A | KEY_B) || input.tapped(ErrorDialogOkButton)) {
            errors_.dismiss();
        }
        return;
    }
    if (isLoading()) {
        return;
    }
    if (input.pressed(KEY_SELECT) && screen_ != ScreenId::Bank) {
        if (screen_ == ScreenId::Logs) {
            screen_ = previousScreen_;
        } else {
            previousScreen_ = std::exchange(screen_, ScreenId::Logs);
        }
        return;
    }
    activeScreen().update(input);
}

void App::pollBackgroundWork() {
    pollUpdate();
    pollAuth();
    pollWelcomeBack();
    pollSaveLoad();
    pollBankLoad();
    bankScreen_.pollBackgroundWork();
    pollSessionRejected();
    loadingScreen_.tick();
}

void App::finishIntro() {
    screen_ = ScreenId::Welcome;
    std::string refreshToken;
    if (sessionStore_.load(refreshToken, accountUsername_)) {
        autoLogin_ = true;
        bootAutoLoginInProgress_ = true;
        beginAuth(AuthRequest::refresh(std::move(refreshToken)));
    }
}

void App::resetAccountState() {
    sessionStore_.clear();
    session_ = {};
    bootAutoLoginInProgress_ = false;
    welcomeBackPending_ = false;
    cloudBoxCache_ = {};
    cloudBoxNamesCache_.clear();
    loadService_.invalidate();
    bankScreen_.reset();
    gameSelectScreen_.reset();
}

void App::logout() {
    resetAccountState();
    accountUsername_.clear();
    loginScreen_.reset();
    registerScreen_.reset();
    resetPasswordScreen_.reset();
    status_ = localization_.get(TextId::LoggedOut);
    screen_ = ScreenId::Welcome;
    Logger::instance().info("User logged out");
}

void App::beginUpdate() {
    status_ = localization_.get(TextId::CheckingForUpdates);
    const bool started = updateTask_.start([this]() {
        return UpdateInstaller::run(api_, executablePath_, homebrew_);
    });
    if (!started) {
        status_ = localization_.get(TextId::UpdateStartFailed);
        Logger::instance().warning("Update worker creation failed");
    }
}

void App::pollUpdate() {
    UpdateInstallResult result;
    if (!updateTask_.poll(result)) {
        return;
    }
    status_ = result.message;
    if (result.updated) {
        Logger::instance().info(result.message);
        running_ = false;
    } else if (!result.success) {
        Logger::instance().warning(result.message);
    }
}

void App::toggleAutoLogin() {
    autoLogin_ = !autoLogin_;
    status_ = localization_.get(autoLogin_ ? TextId::AutoLoginEnabled : TextId::AutoLoginDisabled);
    if (!autoLogin_) {
        sessionStore_.clear();
    }
}

void App::beginAuth(AuthRequest request) {
    if (authController_.isRunning()) {
        return;
    }
    status_ = localization_.get(request.operation == AuthOperation::Refresh ? TextId::RestoringSession
                                                                            : TextId::Connecting);
    request.deviceFingerprint = deviceIdentity_.fingerprint();
    if (!authController_.begin(std::move(request))) {
        status_ = localization_.get(TextId::AuthStartFailed);
        Logger::instance().error("Authentication worker creation failed");
    }
}

void App::pollSessionRejected() {
    if (isLoading() || bankScreen_.backgroundWorkRunning() || !api_.consumeSessionRejected() || onAuthScreen()) {
        return;
    }
    Logger::instance().warning("Session rejected by the server - signing out");
    resetAccountState();
    screen_ = ScreenId::Welcome;
    showError(TextId::SignedOutTitle, TextId::SignedOutMessage);
}

namespace {
TextId authFailureTitle(AuthOperation operation) {
    switch (operation) {
        case AuthOperation::Login:
            return TextId::LoginFailedTitle;
        case AuthOperation::Register:
            return TextId::RegistrationFailedTitle;
        case AuthOperation::ResetPassword:
            return TextId::ResetFailedTitle;
        case AuthOperation::Refresh:
            return TextId::SessionRestoreFailedTitle;
    }
    return TextId::RequestFailedTitle;
}
}

void App::pollAuth() {
    AuthController::Completed completed;
    if (!authController_.poll(completed)) {
        return;
    }
    if (completed.result.success) {
        onAuthSucceeded(std::move(completed));
    } else {
        onAuthFailed(completed);
    }
}

void App::onAuthFailed(const AuthController::Completed& completed) {
    const AuthResult& result = completed.result;
    Logger::instance().warning("Authentication failed");
    bootAutoLoginInProgress_ = false;
    if (completed.operation == AuthOperation::Refresh && !result.networkError && result.httpStatus == 401) {
        resetAccountState();
        screen_ = ScreenId::Welcome;
    }
    showError(std::string(localization_.get(authFailureTitle(completed.operation))),
              result.networkError ? std::string(localization_.get(TextId::ServerUnreachable)) : result.message);
}

void App::onAuthSucceeded(AuthController::Completed completed) {
    status_ = completed.result.message;
    if (completed.operation == AuthOperation::ResetPassword) {
        Logger::instance().info("Password reset accepted");
        screen_ = ScreenId::Welcome;
        return;
    }
    session_ = std::move(completed.result.session);
    if (!completed.username.empty()) {
        accountUsername_ = completed.username;
    }
    if (autoLogin_) {
        sessionStore_.save(session_.refreshToken, accountUsername_);
    } else {
        sessionStore_.clear();
    }
    if (completed.operation == AuthOperation::Login) {
        loginScreen_.reset();
    } else if (completed.operation == AuthOperation::Register) {
        registerScreen_.reset();
    }
    Logger::instance().info("Authentication succeeded");

    if (!std::exchange(bootAutoLoginInProgress_, false)) {
        loadService_.begin(LoadService::Operation::LoadBank);
        return;
    }
    welcomeBackPending_ = true;
    welcomeBackUntil_ = svcGetSystemTick() + static_cast<u64>(WelcomeBackSeconds * SYSCLOCK_ARM11);
}

void App::pollWelcomeBack() {
    if (!welcomeBackPending_ || svcGetSystemTick() < welcomeBackUntil_) {
        return;
    }
    welcomeBackPending_ = false;
    loadService_.begin(LoadService::Operation::LoadBank);
}

void App::pollSaveLoad() {
    switch (saveLoadService_.poll()) {
        case SaveLoadService::Operation::DiscoverGames:
        case SaveLoadService::Operation::RescanCartridge:
            gameSelectScreen_.populateFromDiscovered(saveLoadService_.discoveredGames);
            return;
        case SaveLoadService::Operation::OpenGame:
            if (saveLoadService_.openGameResult.success) {
                bankScreen_.onGameOpened();
            } else {
                status_ = saveLoadService_.openGameResult.message;
                screen_ = ScreenId::GameSelect;
            }
            return;
        case SaveLoadService::Operation::Summary:
            gameSelectScreen_.applySummary(saveLoadService_.catalogIndex, saveLoadService_.summaryCartridge,
                                           saveLoadService_.summaryResult);
            return;
        case SaveLoadService::Operation::None:
            return;
    }
}

void App::pollBankLoad() {
    switch (loadService_.poll()) {
        case LoadService::Operation::LoadBank:
            cloudBoxCache_ = loadService_.cloudBoxResult;
            cloudBoxNamesCache_ = std::move(loadService_.pendingBoxNames);
            loadService_.pendingBoxNames.clear();
            status_ = localization_.get(TextId::StatusFindingSaveGames);
            gameSelectScreen_.refresh();
            return;
        case LoadService::Operation::CloudBox:
            bankScreen_.onCloudBoxLoaded();
            return;
        case LoadService::Operation::PickupCloud:
            bankScreen_.onCloudPickupCompleted();
            return;
        case LoadService::Operation::SwapCloud:
            bankScreen_.onCloudSwapCompleted();
            return;
        case LoadService::Operation::None:
            return;
    }
}

bool App::isLoading() const {
    return isLoadingWork() || loadingScreen_.finishing();
}

bool App::isLoadingWork() const {
    return updateTask_.running()
        || authController_.isRunning()
        || loadService_.blocksUi()
        || saveLoadService_.blocksUi()
        || welcomeBackPending_;
}

void App::render() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    const float depth = osGet3DSliderState() * StereoDepth;
    renderTop(resources_.topLeft, resources_.textBufferTopA, -depth);
    renderTop(resources_.topRight, resources_.textBufferTopB, depth);
    renderBottom();
    C3D_FrameEnd(0);
}

void App::beginScene(C3D_RenderTarget* target, C2D_TextBuf buffer, float width) {
    ui_.setActiveBuffer(buffer);
    C2D_TextBufClear(buffer);
    C2D_TargetClear(target, Background);
    C2D_SceneBegin(target);
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, width, 240.0F, Background);
}

void App::renderTop(C3D_RenderTarget* target, C2D_TextBuf buffer, float eyeOffset) {
    beginScene(target, buffer, 400.0F);
    if (isLoading()) {
        loadingScreen_.renderTop(eyeOffset);
    } else {
        activeScreen().renderTop(eyeOffset);
    }
}

void App::renderBottom() {
    beginScene(resources_.bottom, resources_.textBuffer, 320.0F);
    if (errors_.visible()) {
        drawErrorDialog(ui_, errors_, localization_.get(TextId::UnexpectedError), localization_.get(TextId::Ok));
    } else if (isLoading()) {
        loadingScreen_.render();
    } else {
        activeScreen().render();
    }
}

void App::requestText(std::string& destination, std::string_view hint, bool password, std::size_t maxLength) {
    SwkbdState keyboard;
    std::array<char, 257> buffer{};
    std::strncpy(buffer.data(), destination.c_str(), buffer.size() - 1);
    swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, static_cast<int>(maxLength));
    const std::string ownedHint(hint);
    swkbdSetHintText(&keyboard, ownedHint.c_str());
    swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    if (password) {
        swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY);
        swkbdSetPasswordMode(&keyboard, SWKBD_PASSWORD_HIDE_DELAY);
    }
    if (swkbdInputText(&keyboard, buffer.data(), buffer.size()) == SWKBD_BUTTON_CONFIRM) {
        destination = buffer.data();
        status_.clear();
    }
}

void App::showError(std::string title, std::string message) {
    errors_.show(std::move(title), std::move(message));
    status_.clear();
}

void App::showError(TextId title, TextId message) {
    showError(std::string(localization_.get(title)), std::string(localization_.get(message)));
}
