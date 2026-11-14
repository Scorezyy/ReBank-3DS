#include "app/App.hpp"

#include "config/ServerConfig.hpp"
#include "gui/Theme.hpp"
#include "gui/widgets/ErrorDialog.hpp"
#include "platform/Logger.hpp"

#include <utils/i18n.hpp>

#include <array>
#include <cstring>
#include <utility>

using namespace Gui;

namespace {
constexpr float StereoDepth = 2.5F;

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

App::App(std::string executablePath, bool homebrew)
    : updateFlow_(releaseApi_, std::move(executablePath), update_) {
    update_.homebrew = homebrew;
    Logger::instance().initialize();
    Logger::instance().info("Client boot");
    Logger::instance().info("Server " + ServerConfig::baseUrl());
    romfsInit();
    i18n::init(pksm::Language::ENG);
    music_.start("romfs:/assets/music.ogg", "romfs:/assets/music_vc.ogg");
    resources_.load();
    ui_.setFont(resources_.textFont);
    accountFlow_.initialize();
    status_ = localization_.get(updateFlow_.start() ? TextId::CheckingForUpdates : TextId::UpdateStartFailed);
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

void App::show(ScreenId screen) {
    screen_ = screen;
    activeScreen().enter();
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
        case ScreenId::Plugin:
            return pluginScreen_;
        case ScreenId::Logs:
            break;
    }
    return logsScreen_;
}

bool App::onAuthScreen() const {
    return screen_ == ScreenId::Welcome || screen_ == ScreenId::Login || screen_ == ScreenId::Register
        || screen_ == ScreenId::ResetPassword;
}

bool App::canExit() const {
    return !isLoading() && (onAuthScreen() || screen_ == ScreenId::GameSelect);
}

bool App::isLoading() const {
    return loadingWork() || loadingScreen_.finishing();
}

bool App::loadingWork() const {
    return updateRunning() || accountFlow_.running() || loadService_.blocksUi() || saveLoadService_.blocksUi()
        || account_.welcomeBackPending;
}

void App::update(const InputFrame& input) {
    if (input.pressed(KEY_START) && canExit()) {
        running_ = false;
        return;
    }
    pollBackgroundWork();
    switch (updateFlow_.pollNotice()) {
        case UpdateFlow::Notice::Ended:
            running_ = !updateFlow_.installed();
            return;
        case UpdateFlow::Notice::Showing:
            return;
        case UpdateFlow::Notice::None:
            break;
    }
    if (errors_.visible()) {
        if (input.pressed(KEY_A | KEY_B) || input.tapped(ErrorDialogOkButton)) {
            errors_.dismiss();
        }
        return;
    }
    if (isLoading()) {
        return;
    }
    if (input.pressed(KEY_SELECT) && screen_ != ScreenId::Bank && screen_ != ScreenId::Plugin) {
        toggleLogs();
        return;
    }
    activeScreen().update(input);
}

void App::toggleLogs() {
    if (screen_ == ScreenId::Logs) {
        screen_ = previousScreen_;
        return;
    }
    previousScreen_ = std::exchange(screen_, ScreenId::Logs);
}

void App::pollBackgroundWork() {
    pollUpdate();
    pollAuth();
    accountFlow_.pollWelcomeBack();
    pollSaveLoad();
    pollBankLoad();
    bankScreen_.pollBackgroundWork();
    pollSessionRejected();
    loadingScreen_.tick();
}

void App::finishIntro() {
    screen_ = ScreenId::Welcome;
    if (std::optional<AuthRequest> stored = accountFlow_.storedSession()) {
        beginAuth(std::move(*stored));
    }
}

void App::resetAccountState() {
    accountFlow_.forget();
    cloudCache_ = {};
    bankScreen_.reset();
    gameSelectScreen_.reset();
}

void App::logout() {
    resetAccountState();
    accountFlow_.signOut();
    loginScreen_.reset();
    registerScreen_.reset();
    resetPasswordScreen_.reset();
    status_ = localization_.get(TextId::LoggedOut);
    screen_ = ScreenId::Welcome;
}

void App::toggleAutoLogin() {
    accountFlow_.toggleAutoLogin();
    status_ = localization_.get(account_.autoLogin ? TextId::AutoLoginEnabled : TextId::AutoLoginDisabled);
}

void App::pollUpdate() {
    if (const std::optional<UpdateInstallResult> result = updateFlow_.poll()) {
        status_ = result->message;
    }
}

void App::beginAuth(AuthRequest request) {
    if (accountFlow_.running()) {
        return;
    }
    const bool refresh = request.operation == AuthOperation::Refresh;
    status_ = localization_.get(refresh ? TextId::RestoringSession : TextId::Connecting);
    if (!accountFlow_.begin(std::move(request))) {
        status_ = localization_.get(TextId::AuthStartFailed);
    }
}

void App::pollAuth() {
    const AuthOutcome outcome = accountFlow_.poll();
    switch (outcome.kind) {
        case AuthOutcome::Kind::None:
            return;
        case AuthOutcome::Kind::Failed:
            onAuthFailed(outcome);
            return;
        case AuthOutcome::Kind::PasswordResetAccepted:
            status_ = outcome.message;
            screen_ = ScreenId::Welcome;
            return;
        case AuthOutcome::Kind::SignedIn:
            status_ = outcome.message;
            onSignedIn(outcome.operation);
            return;
    }
}

void App::onAuthFailed(const AuthOutcome& outcome) {
    if (outcome.sessionLost) {
        resetAccountState();
        screen_ = ScreenId::Welcome;
    }
    const std::string message = outcome.networkError ? std::string(localization_.get(TextId::ServerUnreachable))
                                                     : outcome.message;
    showError(std::string(localization_.get(authFailureTitle(outcome.operation))), message);
}

void App::onSignedIn(AuthOperation operation) {
    if (operation == AuthOperation::Login) {
        loginScreen_.reset();
    } else if (operation == AuthOperation::Register) {
        registerScreen_.reset();
    }
}

void App::pollSessionRejected() {
    if (isLoading() || bankScreen_.backgroundWorkRunning() || !http_.consumeSessionRejected() || onAuthScreen()) {
        return;
    }
    Logger::instance().warning("Session rejected by the server - signing out");
    resetAccountState();
    screen_ = ScreenId::Welcome;
    showError(TextId::SignedOutTitle, TextId::SignedOutMessage);
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
                return;
            }
            status_ = saveLoadService_.openGameResult.message;
            screen_ = ScreenId::GameSelect;
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
            cloudCache_.firstBox = loadService_.cloudBoxResult;
            cloudCache_.boxNames = std::move(loadService_.pendingBoxNames);
            loadService_.pendingBoxNames.clear();
            status_ = localization_.get(TextId::StatusFindingSaveGames);
            gameSelectScreen_.refresh(false);
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
