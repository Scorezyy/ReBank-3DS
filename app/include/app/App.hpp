#pragma once

#include "account/AuthController.hpp"
#include "account/DeviceIdentity.hpp"
#include "account/SessionStore.hpp"
#include "audio/MusicPlayer.hpp"
#include "bank/LoadService.hpp"
#include "core/AsyncTask.hpp"
#include "core/ErrorNotice.hpp"
#include "gui/GfxResources.hpp"
#include "gui/UiRenderer.hpp"
#include "gui/screens/BankScreen.hpp"
#include "gui/screens/GameSelectScreen.hpp"
#include "gui/screens/IntroScreen.hpp"
#include "gui/screens/LoadingScreen.hpp"
#include "gui/screens/LoginScreen.hpp"
#include "gui/screens/LogsScreen.hpp"
#include "gui/screens/RegisterScreen.hpp"
#include "gui/screens/ResetPasswordScreen.hpp"
#include "gui/screens/WelcomeScreen.hpp"
#include "i18n/Localization.hpp"
#include "network/ApiClient.hpp"
#include "save/SaveLoadService.hpp"
#include "save/adapter/SaveAdapter.hpp"
#include "update/UpdateInstaller.hpp"

#include <citro2d.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class App {
public:
    App(std::string executablePath, bool homebrew);
    ~App();
    int run();

private:
    friend class Screen;
    friend class IntroScreen;
    friend class WelcomeScreen;
    friend class AuthFormScreen;
    friend class LoginScreen;
    friend class RegisterScreen;
    friend class ResetPasswordScreen;
    friend class GameSelectScreen;
    friend class LogsScreen;
    friend class BankScreen;
    friend class LoadingScreen;

    void update(const InputFrame& input);
    void pollBackgroundWork();
    bool onAuthScreen() const;
    bool canExit() const;
    Screen& activeScreen();

    void finishIntro();
    void beginAuth(AuthRequest request);
    void pollAuth();
    void onAuthFailed(const AuthController::Completed& completed);
    void onAuthSucceeded(AuthController::Completed completed);
    void pollSessionRejected();
    void pollWelcomeBack();
    void beginUpdate();
    void pollUpdate();
    void pollSaveLoad();
    void pollBankLoad();
    void resetAccountState();
    void logout();
    void toggleAutoLogin();
    bool isLoading() const;
    bool isLoadingWork() const;

    void render();
    void beginScene(C3D_RenderTarget* target, C2D_TextBuf buffer, float width);
    void renderTop(C3D_RenderTarget* target, C2D_TextBuf buffer, float eyeOffset);
    void renderBottom();

    void requestText(std::string& destination, std::string_view hint, bool password, std::size_t maxLength = 256);
    void showError(std::string title, std::string message);
    void showError(TextId title, TextId message);

    Localization localization_;
    ScreenId screen_ = ScreenId::Intro;
    ScreenId previousScreen_ = ScreenId::Welcome;
    GfxResources resources_;
    UiRenderer ui_;
    ErrorNotice errors_;
    std::string status_;
    DeviceIdentity deviceIdentity_;
    ApiClient api_;
    std::string executablePath_;
    bool homebrew_;
    AsyncTask<UpdateInstallResult> updateTask_;
    SessionStore sessionStore_{deviceIdentity_};
    AccountSession session_;
    AuthController authController_{api_};
    LoadService loadService_{api_, session_};
    SaveAdapter saveAdapter_;
    SaveLoadService saveLoadService_{saveAdapter_};
    MusicPlayer music_;
    bool running_ = true;
    bool autoLogin_ = true;
    std::string accountUsername_;
    bool bootAutoLoginInProgress_ = false;
    bool welcomeBackPending_ = false;
    u64 welcomeBackUntil_ = 0;
    BoxListResult cloudBoxCache_;
    std::vector<BoxNameEntry> cloudBoxNamesCache_;
    IntroScreen introScreen_{*this};
    WelcomeScreen welcomeScreen_{*this};
    LoginScreen loginScreen_{*this};
    RegisterScreen registerScreen_{*this};
    ResetPasswordScreen resetPasswordScreen_{*this};
    GameSelectScreen gameSelectScreen_{*this};
    BankScreen bankScreen_{*this};
    LogsScreen logsScreen_{*this};
    LoadingScreen loadingScreen_{*this};
};
