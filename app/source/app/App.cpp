#include "app/App.hpp"
#include "core/FsGuard.hpp"
#include "core/ServerConfig.hpp"
#include "gui/elements/TextMetrics.hpp"
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

    status_ = completed.result.message;
    if (!completed.result.success) {
        Logger::instance().warning("Authentication failed");
        bootAutoLoginInProgress_ = false;
        if (completed.operation == AuthOperation::Refresh) {
            sessionStore_.clear();
            screen_ = Screen::Welcome;
        } else if (completed.operation == AuthOperation::Login && autoLogin_) {
            credentials_.clear();
            autoLogin_ = false;
        }
        return;
    }
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
    switch (loadService_.poll()) {
        case LoadService::Operation::DiscoverGames:
            gameSelectScreen_.populateFromDiscovered(loadService_.discoveredGames);
            break;
        case LoadService::Operation::OpenGame:
            if (!loadService_.openGameResult.success) {
                status_ = loadService_.openGameResult.message;
                screen_ = Screen::GameSelect;
            } else {
                bankScreen_.onGameOpened();
            }
            break;
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
        || welcomeBackPending_;
}

void App::render() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    const float slider = osGet3DSliderState();

    activeTextBuffer_ = resources_.textBufferTopA;
    C2D_TextBufClear(activeTextBuffer_);
    renderTop(resources_.topLeft, -slider * 2.5F);

    activeTextBuffer_ = resources_.textBufferTopB;
    C2D_TextBufClear(activeTextBuffer_);
    renderTop(resources_.topRight, slider * 2.5F);

    activeTextBuffer_ = resources_.textBuffer;
    C2D_TextBufClear(activeTextBuffer_);
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
    drawCentered("ReBank", 200.0F + eyeOffset, 71.0F, 1.7F, Ink);
    drawCentered(localization_.get(TextId::Tagline), 200.0F, 171.0F, 0.62F, Muted);
}

void App::renderBottom() {
    C2D_TargetClear(resources_.bottom, Background);
    C2D_SceneBegin(resources_.bottom);
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 320.0F, 240.0F, Background);

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
        drawCentered("ReBank", 160.0F, 94.0F, 1.15F, Ink);
        drawCentered("A", 160.0F, 154.0F, 0.55F, Muted);
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

namespace {
    
void primeTextMode() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0);
}
}

void App::drawText(std::string_view value, float x, float y, float size, u32 color) {
    primeTextMode();
    C2D_Text text;
    const PreparedText prepared = prepareText(value, resources_.textFont);
    parseText(text, resources_.textFont, activeTextBuffer_, prepared.value);
    const float snappedX = std::round(x);
    const float snappedY = std::round(y);
    C2D_DrawText(&text, C2D_WithColor, snappedX, snappedY, 0.85F, size, size, color);
    drawMusicGlyphs(prepared, resources_.textFont, activeTextBuffer_, snappedX, snappedY, size, color);
}

void App::drawCentered(std::string_view value, float centerX, float y, float size, u32 color) {
    primeTextMode();
    C2D_Text text;
    const PreparedText prepared = prepareText(value, resources_.textFont);
    parseText(text, resources_.textFont, activeTextBuffer_, prepared.value);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    const float snappedX = std::round(centerX - width * 0.5F);
    const float snappedY = std::round(y);
    C2D_DrawText(&text, C2D_WithColor, snappedX, snappedY, 0.85F, size, size, color);
    drawMusicGlyphs(prepared, resources_.textFont, activeTextBuffer_, snappedX, snappedY, size, color);
}

float App::textWidth(std::string_view value, float size) {
    primeTextMode();
    C2D_Text text;
    const PreparedText prepared = prepareText(value, resources_.textFont);
    parseText(text, resources_.textFont, activeTextBuffer_, prepared.value);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    return width;
}

void App::drawRight(std::string_view value, float rightX, float y, float size, u32 color) {
    primeTextMode();
    C2D_Text text;
    const PreparedText prepared = prepareText(value, resources_.textFont);
    parseText(text, resources_.textFont, activeTextBuffer_, prepared.value);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    const float snappedX = std::round(rightX - width);
    const float snappedY = std::round(y);
    C2D_DrawText(&text, C2D_WithColor, snappedX, snappedY, 0.85F, size, size, color);
    drawMusicGlyphs(prepared, resources_.textFont, activeTextBuffer_, snappedX, snappedY, size, color);
}

void App::drawButton(const UiRect& rect, std::string_view label, bool primary) {
    const u32 fill = primary ? Brand : Surface;
    const u32 textColor = primary ? Surface : Ink;
    C2D_DrawRectSolid(rect.x, rect.y, 0.1F, rect.width, rect.height, fill);
    drawCentered(label, rect.x + rect.width * 0.5F, rect.y + 11.0F, 0.58F, textColor);
}

void App::drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password) {
    C2D_DrawRectSolid(rect.x, rect.y, 0.1F, rect.width, rect.height, Surface);
    drawText(label, rect.x + 10.0F, rect.y + 5.0F, 0.42F, Muted);
    std::string displayed = value;
    if (password && !value.empty()) {
        displayed.assign(value.size(), '*');
    }
    drawText(displayed, rect.x + 10.0F, rect.y + 21.0F, 0.52F, Ink);
}

void App::requestText(std::string& destination, std::string_view hint, bool password) {
    SwkbdState keyboard;
    std::array<char, 257> buffer{};
    std::strncpy(buffer.data(), destination.c_str(), buffer.size() - 1);
    swkbdInit(&keyboard, password ? SWKBD_TYPE_QWERTY : SWKBD_TYPE_NORMAL, 2, 256);
    std::string ownedHint(hint);
    swkbdSetHintText(&keyboard, ownedHint.c_str());
    swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    if (password) {
        swkbdSetPasswordMode(&keyboard, SWKBD_PASSWORD_HIDE_DELAY);
    }
    if (swkbdInputText(&keyboard, buffer.data(), buffer.size()) == SWKBD_BUTTON_CONFIRM) {
        destination = buffer.data();
        status_.clear();
    }
}

