#pragma once

#include "app/AccountFlow.hpp"
#include "app/AppContext.hpp"
#include "app/Navigator.hpp"
#include "app/UpdateFlow.hpp"
#include "gui/screens/auth/LoginScreen.hpp"
#include "gui/screens/auth/RegisterScreen.hpp"
#include "gui/screens/auth/ResetPasswordScreen.hpp"
#include "gui/screens/auth/WelcomeScreen.hpp"
#include "gui/screens/bank/BankScreen.hpp"
#include "gui/screens/games/GameSelectScreen.hpp"
#include "gui/screens/plugins/PluginScreen.hpp"
#include "gui/screens/system/IntroScreen.hpp"
#include "gui/screens/system/LoadingScreen.hpp"
#include "gui/screens/system/LogsScreen.hpp"
#include "network/HttpClient.hpp"

#include <citro2d.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

class App final : private Navigator {
public:
    App(std::string executablePath, bool homebrew);
    ~App() override;
    int run();

private:
    void show(ScreenId screen) override;
    ScreenId current() const override { return screen_; }
    void showError(TextId title, TextId message) override;
    void showError(std::string title, std::string message) override;
    void requestText(std::string& destination, std::string_view hint, bool password, std::size_t maxLength) override;
    void beginAuth(AuthRequest request) override;
    bool authRunning() const override { return accountFlow_.running(); }
    bool updateRunning() const override { return updateFlow_.running(); }
    bool loadingWork() const override;
    void toggleAutoLogin() override;
    void logout() override;
    void finishIntro() override;

    void update(const InputFrame& input);
    void toggleLogs();
    void pollBackgroundWork();
    bool onAuthScreen() const;
    bool canExit() const;
    bool isLoading() const;
    Screen& activeScreen();

    void pollUpdate();
    void pollAuth();
    void onAuthFailed(const AuthOutcome& outcome);
    void onSignedIn(AuthOperation operation);
    void pollSessionRejected();
    void pollSaveLoad();
    void pollBankLoad();
    void resetAccountState();

    void render();
    void beginScene(C3D_RenderTarget* target, C2D_TextBuf buffer, float width);
    void renderTop(C3D_RenderTarget* target, C2D_TextBuf buffer, float eyeOffset);
    void renderBottom();

    Localization localization_;
    ScreenId screen_ = ScreenId::Intro;
    ScreenId previousScreen_ = ScreenId::Welcome;
    GfxResources resources_;
    UiRenderer ui_;
    ErrorNotice errors_;
    std::string status_;
    HttpClient http_;
    AccountApi accountApi_{http_};
    CloudApi cloudApi_{http_};
    ReleaseApi releaseApi_{http_};
    UpdateState update_;
    UpdateFlow updateFlow_;
    AccountState account_;
    LoadService loadService_{cloudApi_, account_.session};
    AccountFlow accountFlow_{accountApi_, account_, loadService_};
    SaveAdapter saveAdapter_;
    SaveLoadService saveLoadService_{saveAdapter_};
    MusicPlayer music_;
    CloudCache cloudCache_;
    bool running_ = true;
    AppContext context_{*this,       ui_,          resources_,       localization_, errors_,
                        status_,     account_,     update_,          cloudCache_,   accountApi_,
                        cloudApi_,   releaseApi_,  saveAdapter_,     saveLoadService_, loadService_,
                        music_};
    IntroScreen introScreen_{context_};
    WelcomeScreen welcomeScreen_{context_};
    LoginScreen loginScreen_{context_};
    RegisterScreen registerScreen_{context_};
    ResetPasswordScreen resetPasswordScreen_{context_};
    GameSelectScreen gameSelectScreen_{context_};
    BankScreen bankScreen_{context_};
    PluginScreen pluginScreen_{context_};
    LogsScreen logsScreen_{context_};
    LoadingScreen loadingScreen_{context_};
};
