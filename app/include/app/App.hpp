#pragma once

#include "network/ApiClient.hpp"
#include "network/AuthController.hpp"
#include "network/CredentialStore.hpp"
#include "network/LoadService.hpp"
#include "save/catalog/GameCatalog.hpp"
#include "save/adapter/SaveAdapter.hpp"
#include "save/SaveLoadService.hpp"
#include "gui/GfxResources.hpp"
#include "gui/Theme.hpp"
#include "gui/UiRenderer.hpp"
#include "gui/elements/ErrorDialog.hpp"
#include "gui/bankscreen/BankScreen.hpp"
#include "gui/gameselectscreen/GameSelectScreen.hpp"
#include "gui/loadingscreen/LoadingScreen.hpp"
#include "gui/loginscreen/LoginScreen.hpp"
#include "gui/logsscreen/LogsScreen.hpp"
#include "gui/registerscreen/RegisterScreen.hpp"
#include "gui/resetpasswordscreen/ResetPasswordScreen.hpp"
#include "gui/welcomescreen/WelcomeScreen.hpp"
#include "i18n/Localization.hpp"
#include "core/Logger.hpp"
#include "audio/MusicPlayer.hpp"
#include "network/SessionStore.hpp"
#include "network/UpdateController.hpp"

#include <citro2d.h>

#include <atomic>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class App {
public:
    App(std::string executablePath, bool homebrew);
    ~App();
    int run();

    UiRenderer& ui() { return ui_; }

private:
    friend class WelcomeScreen;
    friend class LoginScreen;
    friend class RegisterScreen;
    friend class ResetPasswordScreen;
    friend class GameSelectScreen;
    friend class LogsScreen;
    friend class BankScreen;
    friend class StorageController;
    friend class CommitService;
    friend class CloudSyncController;
    friend class BankInputController;
    friend class LoadService;
    friend class SaveLoadService;
    friend class LoadingScreen;

    enum class Screen {
        Intro,
        Welcome,
        Login,
        Register,
        ResetPassword,
        GameSelect,
        Bank,
        Logs
    };

    void update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch);
    void finishIntro();
    void beginAuth(AuthOperation operation, std::string authUsername, std::string authEmail, std::string authPassword);
    void pollAuth();
    void pollWelcomeBack();
    void beginUpdate();
    void pollUpdate();
    void pollLoad();
    bool isLoading() const;
    void render();
    void renderTop(C3D_RenderTarget* target, float eyeOffset);
    void renderBottom();
    void logout();
    void toggleAutoLogin();
    void drawText(std::string_view value, float x, float y, float size, u32 color);
    void drawCentered(std::string_view value, float centerX, float y, float size, u32 color);
    void drawRight(std::string_view value, float rightX, float y, float size, u32 color);
    float textWidth(std::string_view value, float size);
    void drawButton(const UiRect& rect, std::string_view label, bool primary);
    void drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password);
    void requestText(std::string& destination, std::string_view hint, bool password, std::size_t maxLength = 256);
    void showError(std::string title, std::string message);

    Localization localization_;
    Screen screen_;
    Screen previousScreen_;
    GfxResources resources_;
    UiRenderer ui_;
    ErrorDialog errorDialog_;
    std::string status_;
    ApiClient api_;
    std::string executablePath_;
    bool homebrew_;
    UpdateController updateController_;
    SessionStore sessionStore_;
    AccountSession session_;
    AuthController authController_;
    LoadService loadService_{*this};
    SaveLoadService saveLoadService_{*this};
    MusicPlayer music_;
    bool running_;
    CredentialStore credentials_;
    bool autoLogin_ = false;
    std::string accountUsername_;
    bool bootAutoLoginInProgress_ = false;
    bool welcomeBackPending_ = false;
    u64 welcomeBackUntil_ = 0;
    BoxListResult cloudBoxCache_;
    std::vector<BoxNameEntry> cloudBoxNamesCache_;
    WelcomeScreen welcomeScreen_{*this};
    LoginScreen loginScreen_{*this};
    RegisterScreen registerScreen_{*this};
    ResetPasswordScreen resetPasswordScreen_{*this};
    GameSelectScreen gameSelectScreen_{*this};
    BankScreen bankScreen_{*this};
    LogsScreen logsScreen_{*this};
    LoadingScreen loadingScreen_{*this};
};