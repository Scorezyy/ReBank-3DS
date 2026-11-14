#pragma once

#include "app/Navigator.hpp"
#include "audio/MusicPlayer.hpp"
#include "bank/LoadService.hpp"
#include "gui/GfxResources.hpp"
#include "gui/UiRenderer.hpp"
#include "gui/widgets/ErrorNotice.hpp"
#include "i18n/Localization.hpp"
#include "network/ApiTypes.hpp"
#include "network/api/AccountApi.hpp"
#include "network/api/CloudApi.hpp"
#include "network/api/ReleaseApi.hpp"
#include "save/SaveLoadService.hpp"
#include "save/adapter/SaveAdapter.hpp"
#include "update/UpdateInstaller.hpp"

#include <string>
#include <vector>

struct AccountState {
    AccountSession session;
    std::string username;
    bool autoLogin = true;
    bool restoringSession = false;
    bool welcomeBackPending = false;
};

struct UpdateState {
    UpdateProgress progress;
    bool keptCia = false;
    bool homebrew = false;
};

struct CloudCache {
    BoxListResult firstBox;
    std::vector<BoxNameEntry> boxNames;
};

struct AppContext {
    Navigator& navigator;
    UiRenderer& ui;
    const GfxResources& resources;
    const Localization& text;
    ErrorNotice& errors;
    std::string& status;
    AccountState& account;
    const UpdateState& update;
    CloudCache& cloudCache;
    AccountApi& accountApi;
    CloudApi& cloudApi;
    ReleaseApi& releaseApi;
    SaveAdapter& saveAdapter;
    SaveLoadService& saveLoads;
    LoadService& bankLoads;
    MusicPlayer& music;
};
