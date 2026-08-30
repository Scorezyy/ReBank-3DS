#include "app/App.hpp"
#include "core/FsGuard.hpp"
#include "core/ServerConfig.hpp"
#include "gui/Theme.hpp"

#include <utils/i18n.hpp>

#include <array>
#include <cmath>
#include <cstring>

using namespace Gui;

App::App(std::string executablePath, bool homebrew)
    : screen_(Screen::Intro),
    previousScreen_(Screen::Welcome),
        executablePath_(std::move(executablePath)),
        homebrew_(homebrew),
      running_(true) {
        FsGuard::init();
        Logger::instance().initialize();
        Logger::instance().info("Client boot");
        Logger::instance().info("Server " + ServerConfig::baseUrl());
        romfsInit();
        i18n::init(pksm::Language::ENG);
        music_.play("romfs:/assets/music.ogg");
    resources_.load();
    ui_.setFont(resources_.textFont);
    credentials_.init();
    beginUpdate();
}

App::~App() {
    Logger::instance().info("Client shutdown");
    gameSelectScreen_.reset();
    music_.stop();
    i18n::exit();
    romfsExit();
}

int App::run() {
    while (aptMainLoop() && running_) {
        hidScanInput();
        touchPosition touch{};
        circlePosition circle{};
        hidTouchRead(&touch);
        hidCircleRead(&circle);
        update(hidKeysDown(), hidKeysHeld(), circle, touch);
        render();
    }
    return 0;
}

void App::update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch) {
    Logger::instance().flush();
    const bool canExitOnStart = !isLoading()
        && (screen_ == Screen::Welcome
            || screen_ == Screen::Login
            || screen_ == Screen::Register
            || screen_ == Screen::ResetPassword
            || screen_ == Screen::GameSelect);
    if ((keysDown & KEY_START) && canExitOnStart) {
        running_ = false;
        return;
    }

    pollUpdate();
    pollAuth();
    pollWelcomeBack();
    pollLoad();
    bankScreen_.pollCommit();
    bankScreen_.pollRenameBox();

    if (bankScreen_.errorDialogVisible()) {
        const bool dismissTouch = (keysDown & KEY_TOUCH)
            && UiRect{92.0F, 190.0F, 136.0F, 34.0F}.contains(touch);
        if ((keysDown & (KEY_A | KEY_B)) || dismissTouch) {
            bankScreen_.dismissErrorDialog();
        }
        return;
    }

    if (errorDialog_.visible()) {
        const bool dismissTouch = (keysDown & KEY_TOUCH)
            && UiRect{92.0F, 190.0F, 136.0F, 34.0F}.contains(touch);
        if ((keysDown & (KEY_A | KEY_B)) || dismissTouch) {
            errorDialog_.dismiss();
        }
        return;
    }

    if (isLoading()) {
        return;
    }

    if ((keysDown & KEY_SELECT) && screen_ != Screen::Bank) {
        if (screen_ == Screen::Logs) {
            screen_ = previousScreen_;
        } else {
            previousScreen_ = screen_;
            screen_ = Screen::Logs;
        }
        return;
    }

    if (screen_ == Screen::Logs) {
        return;
    }

    if (screen_ == Screen::Intro) {
        finishIntro();
        return;
    }

    const bool touched = (keysDown & KEY_TOUCH) != 0;
    if (screen_ == Screen::Welcome) {
        welcomeScreen_.update(keysDown, touch, touched);
    } else if (screen_ == Screen::Login) {
        loginScreen_.update(keysDown, keysHeld, circle, touch, touched);
    } else if (screen_ == Screen::Register) {
        registerScreen_.update(keysDown, keysHeld, circle, touch, touched);
    } else if (screen_ == Screen::ResetPassword) {
        resetPasswordScreen_.update(keysDown, keysHeld, circle, touch, touched);
    } else if (screen_ == Screen::GameSelect) {
        gameSelectScreen_.update(keysDown, touch, touched);
    } else if (screen_ == Screen::Bank) {
        bankScreen_.update(keysDown, keysHeld, circle, touch, touched);
    }
}

void App::finishIntro() {
    screen_ = Screen::Welcome;
    std::string refreshToken;
    if (sessionStore_.load(refreshToken, accountUsername_)) {
        bootAutoLoginInProgress_ = true;
        beginAuth(AuthOperation::Refresh, {}, {}, std::move(refreshToken));
        return;
    }
    if (auto stored = credentials_.load()) {
        accountUsername_ = stored->username;
        autoLogin_ = true;
        bootAutoLoginInProgress_ = true;
        beginAuth(AuthOperation::Login, stored->username, {}, stored->password);
    }
}

void App::logout() {
    sessionStore_.clear();
    credentials_.clear();
    session_ = {};
    autoLogin_ = false;
    accountUsername_.clear();
    bootAutoLoginInProgress_ = false;
    welcomeBackPending_ = false;
    cloudBoxCache_ = {};
    cloudBoxNamesCache_.clear();
    loginScreen_.reset();
    registerScreen_.reset();
    resetPasswordScreen_.reset();
    bankScreen_.reset();
    gameSelectScreen_.reset();
    status_ = "Logged out.";
    screen_ = Screen::Welcome;
    Logger::instance().info("User logged out");
}

void App::beginUpdate() {
    status_ = "Checking for updates...";
    if (!updateController_.begin(api_, executablePath_, homebrew_)) {
        status_ = "Update check could not start.";
        Logger::instance().warning("Update worker creation failed");
    }
}

void App::pollUpdate() {
    UpdateInstallResult result;
    if (!updateController_.poll(result)) {
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
    status_ = autoLogin_ ? "Auto-login enabled." : "Auto-login disabled.";
    if (!autoLogin_) {
        credentials_.clear();
    }
}

void App::beginAuth(AuthOperation operation, std::string authUsername, std::string authEmail, std::string authPassword) {
    if (authController_.isRunning()) {
        return;
    }
    status_ = operation == AuthOperation::Refresh ? "Restoring session..." : "Connecting...";
    if (!authController_.begin(api_, operation, std::move(authUsername), std::move(authEmail), std::move(authPassword))) {
        status_ = "Could not start the login process.";
        Logger::instance().error("Authentication worker creation failed");
    }
}

void App::pollAuth() {
    AuthController::Completed completed;
    if (!authController_.poll(completed)) {
        return;
    }

    if (!completed.result.success) {
        Logger::instance().warning("Authentication failed");
        bootAutoLoginInProgress_ = false;
        const bool sessionRejected = completed.operation == AuthOperation::Refresh
            && !completed.result.networkError
            && completed.result.httpStatus == 401;
        const bool credentialsRejected = completed.operation == AuthOperation::Login
            && !completed.result.networkError
            && completed.result.httpStatus == 401;
        if (sessionRejected) {
            sessionStore_.clear();
            screen_ = Screen::Welcome;
        } else if (credentialsRejected && autoLogin_) {
            credentials_.clear();
            autoLogin_ = false;
        }
        std::string title = "REQUEST FAILED";
        if (completed.operation == AuthOperation::Login) {
            title = "LOGIN FAILED";
        } else if (completed.operation == AuthOperation::Register) {
            title = "REGISTRATION FAILED";
        } else if (completed.operation == AuthOperation::ResetPassword) {
            title = "RESET FAILED";
        } else if (completed.operation == AuthOperation::Refresh) {
            title = "SESSION RESTORE FAILED";
        }
        const std::string message = completed.result.networkError
            ? "Could not reach the ReBank server. Check your connection and try again."
            : completed.result.message;
        showError(title, message);
        return;
    }

    status_ = completed.result.message;
    if (completed.operation == AuthOperation::ResetPassword) {
        Logger::instance().info("Password reset accepted");
        screen_ = Screen::Welcome;
        return;
    }

    session_ = std::move(completed.result.session);
    if (!completed.username.empty()) {
        accountUsername_ = completed.username;
    }
    sessionStore_.save(session_.refreshToken, accountUsername_);
    if (autoLogin_ && completed.operation != AuthOperation::Refresh
        && !completed.username.empty() && !completed.password.empty()) {
        credentials_.save({completed.username, completed.password});
    }
    if (completed.operation == AuthOperation::Login) {
        loginScreen_.reset();
    } else if (completed.operation == AuthOperation::Register) {
        registerScreen_.reset();
    }
    Logger::instance().info("Authentication succeeded");

    if (bootAutoLoginInProgress_) {
        bootAutoLoginInProgress_ = false;
        welcomeBackPending_ = true;
        welcomeBackUntil_ = svcGetSystemTick() + static_cast<u64>(1.1 * SYSCLOCK_ARM11);
        return;
    }
    loadService_.begin(LoadService::Operation::LoadBank);
}

void App::pollWelcomeBack() {
    if (!welcomeBackPending_ || svcGetSystemTick() < welcomeBackUntil_) {
        return;
    }
    welcomeBackPending_ = false;
    loadService_.begin(LoadService::Operation::LoadBank);
}

void App::pollLoad() {
    const SaveLoadService::Operation completedSave = saveLoadService_.poll();
    switch (completedSave) {
        case SaveLoadService::Operation::DiscoverGames:
        case SaveLoadService::Operation::RescanCartridge:
            gameSelectScreen_.populateFromDiscovered(saveLoadService_.discoveredGames);
            gameSelectScreen_.refreshCartridgeSummary();
            break;
        case SaveLoadService::Operation::OpenGame:
            if (!saveLoadService_.openGameResult.success) {
                status_ = saveLoadService_.openGameResult.message;
                screen_ = Screen::GameSelect;
            } else {
                bankScreen_.onGameOpened();
            }
            break;
        case SaveLoadService::Operation::CartridgeSummary:
            gameSelectScreen_.applyCartridgeSummary(saveLoadService_.cartridgeSummary);
            break;
        default:
            break;
    }
    switch (loadService_.poll()) {
        case LoadService::Operation::LoadBank:
            cloudBoxCache_ = loadService_.cloudBoxResult;
            cloudBoxNamesCache_ = loadService_.pendingBoxNames;
            loadService_.pendingBoxNames.clear();
            status_ = "Finding save games...";
            gameSelectScreen_.refresh();
            break;
        case LoadService::Operation::CloudBox:
            bankScreen_.onCloudBoxLoaded();
            break;
        case LoadService::Operation::PickupCloud:
            bankScreen_.onCloudPickupCompleted();
            break;
        case LoadService::Operation::SwapCloud:
            bankScreen_.onCloudSwapCompleted();
            break;
        default:
            break;
    }
}

bool App::isLoading() const {
    return updateController_.isRunning()
        || authController_.isRunning()
        || loadService_.blocksUi()
        || saveLoadService_.blocksUi()
        || welcomeBackPending_;
}

void App::render() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    const float slider = osGet3DSliderState();

    ui_.setActiveBuffer(resources_.textBufferTopA);
    C2D_TextBufClear(resources_.textBufferTopA);
    renderTop(resources_.topLeft, -slider * 2.5F);

    ui_.setActiveBuffer(resources_.textBufferTopB);
    C2D_TextBufClear(resources_.textBufferTopB);
    renderTop(resources_.topRight, slider * 2.5F);

    ui_.setActiveBuffer(resources_.textBuffer);
    C2D_TextBufClear(resources_.textBuffer);
    renderBottom();

    C3D_FrameEnd(0);
}

void App::renderTop(C3D_RenderTarget* target, float eyeOffset) {
    C2D_TargetClear(target, Background);
    C2D_SceneBegin(target);
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 400.0F, 240.0F, Background);

    if (isLoading()) {
        loadingScreen_.renderTop(eyeOffset);
        return;
    }

    if (screen_ == Screen::Bank) {
        bankScreen_.renderTop(eyeOffset);
        return;
    }
    if (screen_ == Screen::GameSelect) {
        gameSelectScreen_.renderTop(eyeOffset);
        return;
    }

    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float drift = std::sin(static_cast<float>(seconds) * 1.7F) * 4.0F;
    C2D_DrawCircleSolid(200.0F + eyeOffset + drift, 91.0F, 0.0F, 62.0F, Brand);
    C2D_DrawCircleSolid(200.0F + eyeOffset - drift * 0.4F, 91.0F, 0.0F, 43.0F, Accent);
    ui_.drawCentered("ReBank", 200.0F + eyeOffset, 71.0F, 1.7F, Ink);
    ui_.drawCentered(localization_.get(TextId::Tagline), 200.0F, 171.0F, 0.62F, Muted);
}

void App::renderBottom() {
    C2D_TargetClear(resources_.bottom, Background);
    C2D_SceneBegin(resources_.bottom);
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 320.0F, 240.0F, Background);

    if (errorDialog_.visible()) {
        errorDialog_.render(ui_);
        return;
    }

    if (isLoading()) {
        loadingScreen_.render();
        return;
    }

    if (screen_ == Screen::Logs) {
        logsScreen_.render();
        return;
    }
    if (screen_ == Screen::GameSelect) {
        gameSelectScreen_.render();
        return;
    }
    if (screen_ == Screen::Bank) {
        bankScreen_.render();
        return;
    }
    if (screen_ == Screen::Intro) {
        ui_.drawCentered("ReBank", 160.0F, 94.0F, 1.15F, Ink);
        ui_.drawCentered("A", 160.0F, 154.0F, 0.55F, Muted);
        return;
    }

    if (screen_ == Screen::Welcome) {
        welcomeScreen_.render();
        return;
    }
    if (screen_ == Screen::Login) {
        loginScreen_.render();
        return;
    }
    if (screen_ == Screen::Register) {
        registerScreen_.render();
        return;
    }
    if (screen_ == Screen::ResetPassword) {
        resetPasswordScreen_.render();
    }
}

void App::drawText(std::string_view value, float x, float y, float size, u32 color) {
    ui_.drawText(value, x, y, size, color);
}

void App::drawCentered(std::string_view value, float centerX, float y, float size, u32 color) {
    ui_.drawCentered(value, centerX, y, size, color);
}

float App::textWidth(std::string_view value, float size) {
    return ui_.textWidth(value, size);
}

void App::drawRight(std::string_view value, float rightX, float y, float size, u32 color) {
    ui_.drawRight(value, rightX, y, size, color);
}

void App::drawButton(const UiRect& rect, std::string_view label, bool primary) {
    ui_.drawButton(rect, label, primary);
}

void App::drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password) {
    ui_.drawField(rect, label, value, password);
}

void App::requestText(std::string& destination, std::string_view hint, bool password, std::size_t maxLength) {
    SwkbdState keyboard;
    std::array<char, 257> buffer{};
    std::strncpy(buffer.data(), destination.c_str(), buffer.size() - 1);
    swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, static_cast<int>(maxLength));
    std::string ownedHint(hint);
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
    errorDialog_.show(std::move(title), std::move(message));
    status_.clear();
}

