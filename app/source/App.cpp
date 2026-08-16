#include "App.hpp"
#include "BuildConfig.hpp"
#include "ServerConfig.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
constexpr u32 Background = C2D_Color32(238, 248, 240, 255);
constexpr u32 Surface = C2D_Color32(255, 255, 255, 255);
constexpr u32 Ink = C2D_Color32(20, 43, 34, 255);
constexpr u32 Muted = C2D_Color32(86, 111, 99, 255);
constexpr u32 Brand = C2D_Color32(31, 145, 94, 255);
constexpr u32 Accent = C2D_Color32(242, 184, 39, 255);
constexpr u32 Error = C2D_Color32(190, 48, 48, 255);
constexpr u32 GrassLight = C2D_Color32(198, 232, 156, 255);
constexpr u32 GrassMid = C2D_Color32(163, 213, 116, 255);
constexpr u32 HeaderPill = C2D_Color32(246, 244, 224, 250);
constexpr u32 HeaderInk = C2D_Color32(64, 66, 40, 255);
constexpr u32 CountBlock = C2D_Color32(174, 44, 44, 255);
constexpr u32 BankYellow = C2D_Color32(238, 184, 68, 255);
constexpr u32 BankYellowDark = C2D_Color32(202, 148, 40, 255);
constexpr u32 ArrowInk = C2D_Color32(112, 74, 20, 255);
constexpr u32 CursorRed = C2D_Color32(216, 40, 32, 255);
constexpr u32 CursorGreen = C2D_Color32(40, 176, 88, 255);
constexpr u32 SidebarInk = C2D_Color32(50, 96, 40, 255);
constexpr u32 TypeElectric = C2D_Color32(238, 178, 32, 255);
constexpr UiRect EmailField{24.0F, 62.0F, 272.0F, 42.0F};
constexpr UiRect PasswordField{24.0F, 114.0F, 272.0F, 42.0F};
constexpr UiRect SubmitButton{24.0F, 195.0F, 272.0F, 38.0F};
constexpr UiRect BackButton{8.0F, 8.0F, 70.0F, 32.0F};
constexpr UiRect LogoutButton{8.0F, 199.0F, 82.0F, 33.0F};

std::uint8_t pokemonFormatFromCode(const std::string& code) {
    if (code.empty()) {
        return 0;
    }
    for (const auto& game : supportedGames()) {
        if (game.code == code) {
            return static_cast<std::uint8_t>(game.format);
        }
    }
    return 0;
}

struct GameVisual {
    std::string_view label;
    u32 primary;
    u32 secondary;
};

struct MusicGlyph {
    std::size_t offset;
    bool doubleNote;
};

struct PreparedText {
    std::string value;
    std::vector<MusicGlyph> musicGlyphs;
};

PreparedText prepareText(std::string_view value) {
    PreparedText prepared;
    prepared.value.reserve(value.size());
    for (std::size_t index = 0; index < value.size();) {
        const bool unicodeMusicNote = index + 2 < value.size()
            && static_cast<unsigned char>(value[index]) == 0xE2
            && static_cast<unsigned char>(value[index + 1]) == 0x99
            && static_cast<unsigned char>(value[index + 2]) >= 0xA9
            && static_cast<unsigned char>(value[index + 2]) <= 0xAC;
        const bool pokemonMusicNote = index + 2 < value.size()
            && static_cast<unsigned char>(value[index]) == 0xEE
            && static_cast<unsigned char>(value[index + 1]) == 0x82
            && static_cast<unsigned char>(value[index + 2]) == 0x9A;
        const bool musicNote = unicodeMusicNote || pokemonMusicNote;
        if (!musicNote) {
            prepared.value.push_back(value[index++]);
            continue;
        }
        const bool doubleNote = unicodeMusicNote
            && static_cast<unsigned char>(value[index + 2]) >= 0xAB;
        prepared.musicGlyphs.push_back({prepared.value.size(), doubleNote});
        prepared.value.append("  ");
        index += 3;
    }
    return prepared;
}

void parseText(C2D_Text& text, C2D_Font font, C2D_TextBuf buffer, const std::string& value) {
    if (font) {
        C2D_TextFontParse(&text, font, buffer, value.c_str());
    } else {
        C2D_TextParse(&text, buffer, value.c_str());
    }
    C2D_TextOptimize(&text);
}

float textWidth(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size) {
    C2D_Text text;
    const std::string owned(value);
    parseText(text, font, buffer, owned);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    return width;
}

void drawMusicGlyphs(const PreparedText& prepared, C2D_Font font, C2D_TextBuf buffer,
                     float x, float y, float size, u32 color) {
    for (const MusicGlyph& glyph : prepared.musicGlyphs) {
        const float glyphX = x + textWidth(font, buffer,
            std::string_view(prepared.value).substr(0, glyph.offset), size);
        const float headY = y + 19.0F * size;
        const float topY = y + 6.0F * size;
        const float radius = std::max(1.0F, 2.6F * size);
        C2D_DrawCircleSolid(glyphX + 4.0F * size, headY, 0.51F, radius, color);
        C2D_DrawRectSolid(glyphX + 5.5F * size, topY, 0.51F,
                          std::max(1.0F, 1.5F * size), 13.0F * size, color);
        if (glyph.doubleNote) {
            C2D_DrawCircleSolid(glyphX + 12.0F * size, headY - 2.0F * size,
                                0.51F, radius, color);
            C2D_DrawRectSolid(glyphX + 13.5F * size, topY, 0.51F,
                              std::max(1.0F, 1.5F * size), 11.0F * size, color);
            C2D_DrawRectSolid(glyphX + 5.5F * size, topY, 0.51F,
                              9.5F * size, std::max(1.0F, 2.0F * size), color);
        } else {
            C2D_DrawRectSolid(glyphX + 5.5F * size, topY, 0.51F,
                              5.0F * size, std::max(1.0F, 1.5F * size), color);
            C2D_DrawRectSolid(glyphX + 9.0F * size, topY, 0.51F,
                              std::max(1.0F, 1.5F * size), 4.5F * size, color);
        }
    }
}

GameVisual gameVisual(std::string_view code) {
    if (code == "x") return {"X", C2D_Color32(35, 104, 184, 255), C2D_Color32(100, 195, 240, 255)};
    if (code == "y") return {"Y", C2D_Color32(190, 42, 52, 255), C2D_Color32(245, 115, 105, 255)};
    if (code == "omega-ruby") return {"OR", C2D_Color32(190, 42, 45, 255), C2D_Color32(245, 146, 75, 255)};
    if (code == "alpha-sapphire") return {"AS", C2D_Color32(30, 95, 180, 255), C2D_Color32(82, 192, 220, 255)};
    if (code == "sun") return {"S", C2D_Color32(224, 118, 25, 255), C2D_Color32(250, 204, 60, 255)};
    if (code == "moon") return {"M", C2D_Color32(48, 72, 150, 255), C2D_Color32(114, 146, 220, 255)};
    if (code == "ultra-sun") return {"US", C2D_Color32(220, 80, 30, 255), C2D_Color32(250, 190, 45, 255)};
    if (code == "ultra-moon") return {"UM", C2D_Color32(65, 60, 145, 255), C2D_Color32(120, 120, 220, 255)};
    if (code == "diamond") return {"D", C2D_Color32(56, 145, 190, 255), C2D_Color32(150, 225, 240, 255)};
    if (code == "pearl") return {"P", C2D_Color32(190, 95, 145, 255), C2D_Color32(245, 185, 210, 255)};
    if (code == "platinum") return {"Pt", C2D_Color32(75, 82, 88, 255), C2D_Color32(184, 191, 195, 255)};
    if (code == "heartgold") return {"HG", C2D_Color32(181, 125, 28, 255), C2D_Color32(245, 210, 90, 255)};
    if (code == "soulsilver") return {"SS", C2D_Color32(86, 112, 128, 255), C2D_Color32(190, 215, 224, 255)};
    if (code == "black" || code == "black2") {
        return {code == "black2" ? "B2" : "B", C2D_Color32(35, 39, 42, 255), C2D_Color32(120, 128, 132, 255)};
    }
    return {code == "white2" ? "W2" : "W", C2D_Color32(175, 180, 184, 255), C2D_Color32(245, 247, 248, 255)};
}

std::string paddedTrainerId(std::uint32_t trainerId) {
    if (trainerId > 99999) {
        return std::to_string(trainerId);
    }
    std::string value = std::to_string(trainerId);
    return std::string(5 - std::min<std::size_t>(5, value.size()), '0') + value;
}
}

bool UiRect::contains(touchPosition point) const {
    return point.px >= x && point.px <= x + width && point.py >= y && point.py <= y + height;
}

App::App(std::string executablePath, bool homebrew)
    : screen_(Screen::Intro),
    previousScreen_(Screen::Welcome),
      topLeft_(nullptr),
      topRight_(nullptr),
      bottom_(nullptr),
      textBuffer_(nullptr),
            textFont_(nullptr),
    pokemonSprites_(nullptr),
      introStartedAt_(svcGetSystemTick()),
        executablePath_(std::move(executablePath)),
        homebrew_(homebrew),
        updateThread_(nullptr),
        updateState_(UpdateState::Idle),
    authThread_(nullptr),
    authState_(AuthState::Idle),
    authOperation_(AuthOperation::Login),
    loadThread_(nullptr),
    loadState_(LoadState::Idle),
    loadOperation_(LoadOperation::None),
    loadingPhase_(LoadingPhase::Idle),
    loadingStartedAt_(0),
    loadingCatalogIndex_(0),
    loadingCloudBox_(0),
            gameIndex_(0),
            gameSelectionChangedAt_(0),
            gameSelectionDirection_(0),
            localBox_(0),
            cloudBox_(0),
            focusedSlot_(0),
            selectionTool_(SelectionTool::Single),
            storagePane_(StoragePane::Local),
            transferArmed_(false),
            uploadThread_(nullptr),
            uploadState_(UploadState::Idle),
            uploadStartedAt_(0),
            downloadThread_(nullptr),
            downloadState_(DownloadState::Idle),
            downloadStartedAt_(0),
            commitThread_(nullptr),
            commitState_(CommitState::Idle),
            commitStartedAt_(0),
            heldDirection_(0),
            directionRepeatAt_(0),
      running_(true),
      autoLogin_(false),
    authFocus_(AuthFocus::Username),
      authAnimationStartedAt_(svcGetSystemTick()) {
        Logger::instance().initialize();
        Logger::instance().info("Client boot");
        Logger::instance().info("Server " + ServerConfig::baseUrl());
        romfsInit();
        music_.play("romfs:/assets/music.ogg");
    gfxInitDefault();
    gfxSet3D(true);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    topLeft_ = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    topRight_ = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottom_ = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textBuffer_ = C2D_TextBufNew(4096);
    textFont_ = C2D_FontLoadSystem(CFG_REGION_JPN);
    if (!textFont_) {
        Logger::instance().warning("Japanese system font could not be loaded");
    }
    pokemonSprites_ = C2D_SpriteSheetLoad("romfs:/assets/pkm_spritesheet.t3x");
    if (!pokemonSprites_) {
        Logger::instance().error("Pokemon sprite sheet could not be loaded");
    }
    credentials_.init();
    beginUpdate();
}

App::~App() {
    if (updateThread_) {
        threadJoin(updateThread_, U64_MAX);
        threadFree(updateThread_);
    }
    if (loadThread_) {
        threadJoin(loadThread_, U64_MAX);
        threadFree(loadThread_);
    }
    if (commitThread_) {
        threadJoin(commitThread_, U64_MAX);
        threadFree(commitThread_);
    }
    if (downloadThread_) {
        threadJoin(downloadThread_, U64_MAX);
        threadFree(downloadThread_);
    }
    if (uploadThread_) {
        threadJoin(uploadThread_, U64_MAX);
        threadFree(uploadThread_);
    }
    if (authThread_) {
        threadJoin(authThread_, U64_MAX);
        threadFree(authThread_);
    }
    Logger::instance().info("Client shutdown");
    if (pokemonSprites_) {
        C2D_SpriteSheetFree(pokemonSprites_);
    }
    for (auto& profile : availableGames_) {
        if (profile.iconLoaded) {
            C3D_TexDelete(&profile.iconTexture);
        }
    }
    if (textFont_) {
        C2D_FontFree(textFont_);
    }
    C2D_TextBufDelete(textBuffer_);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    music_.stop();
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
    if (keysDown & KEY_START) {
        running_ = false;
        return;
    }

    pollUpdate();
    pollAuth();
    pollLoad();
    pollUpload();
    pollDownload();
    pollCommit();

    if (errorDialogVisible_) {
        const bool dismissTouch = (keysDown & KEY_TOUCH)
            && UiRect{92.0F, 190.0F, 136.0F, 34.0F}.contains(touch);
        if ((keysDown & (KEY_A | KEY_B)) || dismissTouch) {
            errorDialogVisible_ = false;
        }
        return;
    }

    if (isLoading()) {
        return;
    }

    if ((keysDown & KEY_SELECT) && screen_ != Screen::Storage) {
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
        updateIntro();
        if (keysDown & KEY_A) {
            screen_ = Screen::Welcome;
        }
        return;
    }

    const bool touched = (keysDown & KEY_TOUCH) != 0;
    if (screen_ == Screen::Welcome) {
        updateWelcome(keysDown, touch, touched);
    } else if (screen_ == Screen::GameSelect) {
        updateGameSelect(keysDown, touch, touched);
    } else if (screen_ == Screen::Storage) {
        updateStorage(keysDown, keysHeld, circle, touch, touched);
    } else {
        updateForm(keysDown, keysHeld, circle, touch, touched);
    }
}

void App::updateIntro() {
    const double elapsed = static_cast<double>(svcGetSystemTick() - introStartedAt_) / SYSCLOCK_ARM11;
    if (elapsed >= 2.8) {
        screen_ = Screen::Welcome;
    }
}

void App::updateWelcome(u32 keysDown, touchPosition touch, bool touched) {
    if (authState_.load(std::memory_order_acquire) != AuthState::Running) {
        auto tryAutoLogin = [&]() -> bool {
            std::string refreshToken;
            if (sessionStore_.load(refreshToken)) {
                authPassword_ = std::move(refreshToken);
                beginAuth(AuthOperation::Refresh);
                return true;
            }
            if (auto stored = credentials_.load()) {
                username_ = stored->username;
                password_ = stored->password;
                autoLogin_ = true;
                authUsername_ = stored->username;
                authPassword_ = stored->password;
                beginAuth(AuthOperation::Login);
                return true;
            }
            return false;
        };
        if (keysDown & KEY_A) {
            if (!tryAutoLogin()) {
                status_ = "Kein gespeicherter Login gefunden.";
                Logger::instance().info("Auto-login: no stored credentials");
                screen_ = Screen::Login;
                authFocus_ = AuthFocus::Username;
                authAnimationStartedAt_ = svcGetSystemTick();
            }
            return;
        }
    }

    if (!touched) {
        return;
    }

    constexpr UiRect login{24.0F, 86.0F, 272.0F, 46.0F};
    constexpr UiRect registration{24.0F, 144.0F, 272.0F, 46.0F};
    constexpr UiRect reset{70.0F, 202.0F, 180.0F, 28.0F};
    if (login.contains(touch)) {
        Logger::instance().info("Login form opened");
        screen_ = Screen::Login;
        authFocus_ = AuthFocus::Username;
        authAnimationStartedAt_ = svcGetSystemTick();
    } else if (registration.contains(touch)) {
        Logger::instance().info("Registration form opened");
        screen_ = Screen::Register;
        authFocus_ = AuthFocus::Username;
        authAnimationStartedAt_ = svcGetSystemTick();
    } else if (reset.contains(touch)) {
        Logger::instance().info("Password reset form opened");
        screen_ = Screen::ResetPassword;
        authFocus_ = AuthFocus::Email;
        authAnimationStartedAt_ = svcGetSystemTick();
    }
}

void App::updateGameSelect(u32 keysDown, touchPosition touch, bool touched) {
    if (touched && LogoutButton.contains(touch)) {
        logout();
        return;
    }
    if (keysDown & KEY_X) {
        status_ = "Finding save games...";
        refreshGameProfiles();
        status_ = availableGames_.empty() ? "No compatible save game found." : std::string{};
        return;
    }
    if (availableGames_.empty()) {
        return;
    }
    if (keysDown & (KEY_LEFT | KEY_L)) {
        const std::size_t next = gameIndex_ == 0 ? availableGames_.size() - 1 : gameIndex_ - 1;
        selectGameProfile(next, -1);
    }
    if (keysDown & (KEY_RIGHT | KEY_R)) {
        selectGameProfile((gameIndex_ + 1) % availableGames_.size(), 1);
    }
    if ((keysDown & KEY_A) || (touched && UiRect{48.0F, 56.0F, 224.0F, 128.0F}.contains(touch))) {
        openSelectedGame();
        return;
    }
}

void App::refreshGameProfiles() {
    status_ = "Finding save games...";
    beginLoad(LoadOperation::DiscoverGames);
}

void App::selectGameProfile(std::size_t index, int direction) {
    if (index >= availableGames_.size() || index == gameIndex_) {
        return;
    }
    gameIndex_ = index;
    gameSelectionDirection_ = direction;
    gameSelectionChangedAt_ = svcGetSystemTick();
}

bool App::openSelectedGame() {
    if (gameIndex_ >= availableGames_.size()) {
        return false;
    }
    const auto games = supportedGames();
    const GameProfile& profile = availableGames_[gameIndex_];
    if (profile.catalogIndex >= games.size()) {
        return false;
    }
    const GameDescriptor& game = games[profile.catalogIndex];
    Logger::instance().info("Game selected: " + std::string(game.code));
    status_ = "Reading save...";
    loadingCatalogIndex_ = profile.catalogIndex;
    beginLoad(LoadOperation::OpenGame);
    return loadState_.load(std::memory_order_acquire) == LoadState::Running;
}

void App::logout() {
    sessionStore_.clear();
    credentials_.clear();
    session_ = {};
    autoLogin_ = false;
    username_.clear();
    email_.clear();
    password_.clear();
    authUsername_.clear();
    authEmail_.clear();
    authPassword_.clear();
    saveAdapter_.close();
    saveSummary_ = {};
    cloudBoxes_.clear();
    localBaselines_.clear();
    localDrafts_.clear();
    cloudPreview_.fill({});
    for (auto& profile : availableGames_) {
        if (profile.iconLoaded) {
            C3D_TexDelete(&profile.iconTexture);
        }
    }
    availableGames_.clear();
    status_ = "Logged out.";
    screen_ = Screen::Welcome;
    Logger::instance().info("User logged out");
}

void App::updateStorage(
    u32 keysDown,
    u32 keysHeld,
    circlePosition circle,
    touchPosition touch,
    bool touched
) {
    if (uploadState_.load(std::memory_order_acquire) == UploadState::Running
        || downloadState_.load(std::memory_order_acquire) == DownloadState::Running
        || commitState_.load(std::memory_order_acquire) == CommitState::Running
        || (loadState_.load(std::memory_order_acquire) == LoadState::Running
            && loadOperation_ == LoadOperation::CloudBox)) {
        return;
    }
    if (keysDown & KEY_B) {
        if (hand_.active) {
            storageReturnHand();
            return;
        }
        if (hasPendingChanges()) {
            discardPendingChanges();
            return;
        }
        if (storagePane_ == StoragePane::Cloud) {
            storagePane_ = StoragePane::Local;
        } else {
            screen_ = Screen::GameSelect;
        }
        return;
    }

    const std::size_t boxLimit = session_.boxLimit == 0 ? 50 : session_.boxLimit;
    if ((keysDown & KEY_L) || (keysDown & KEY_R)) {
        if (hand_.active) {
            status_ = "Drop the Pokemon first.";
        } else if (keysDown & KEY_L) {
            if (storagePane_ == StoragePane::Cloud) {
                persistCloudDraft();
                cloudBox_ = cloudBox_ == 0 ? boxLimit - 1 : cloudBox_ - 1;
                refreshCloudBox();
            } else {
                persistLocalDraft();
                localBox_ = localBox_ == 0 ? saveAdapter_.boxCount() - 1 : localBox_ - 1;
                loadLocalBox();
            }
        } else {
            if (storagePane_ == StoragePane::Cloud) {
                persistCloudDraft();
                cloudBox_ = (cloudBox_ + 1) % boxLimit;
                refreshCloudBox();
            } else {
                persistLocalDraft();
                localBox_ = (localBox_ + 1) % saveAdapter_.boxCount();
                loadLocalBox();
            }
        }
    }

    if (keysDown & KEY_A) {
        if (hand_.active) {
            storageDrop();
        } else {
            storagePickUp();
        }
    }

    auto move = [this](int direction) {
        if (direction == 0) {
            return;
        }
        std::size_t column = focusedSlot_ % 6;
        std::size_t row = focusedSlot_ / 6;
        if (direction == 1) {
            row = row == 0 ? 0 : row - 1;
        } else if (direction == 2) {
            row = row == 4 ? 4 : row + 1;
        } else if (direction == 3) {
            column = column == 0 ? 5 : column - 1;
        } else if (direction == 4) {
            column = (column + 1) % 6;
        }
        focusedSlot_ = row * 6 + column;
    };

    const std::size_t priorSlot = focusedSlot_;
    const StoragePane priorPane = storagePane_;
    move(storageDirection(keysDown, keysHeld, circle));

    if ((keysDown & KEY_UP)
        && priorPane == StoragePane::Local
        && storagePane_ == StoragePane::Local
        && priorSlot < 6
        && focusedSlot_ == priorSlot) {
        const std::size_t column = focusedSlot_ % 6;
        storagePane_ = StoragePane::Cloud;
        focusedSlot_ = 24 + column;
    } else if ((keysDown & KEY_DOWN)
        && priorPane == StoragePane::Cloud
        && storagePane_ == StoragePane::Cloud
        && priorSlot >= 24
        && focusedSlot_ == priorSlot) {
        const std::size_t column = focusedSlot_ % 6;
        storagePane_ = StoragePane::Local;
        focusedSlot_ = column;
    }

    if (keysDown & KEY_SELECT) {
        if (hand_.active) {
            status_ = "Drop the Pokemon first.";
        } else if (!hasPendingChanges()) {
            status_ = "Nothing to commit.";
        } else {
            beginCommit();
        }
    }

    if (!touched) {
        return;
    }

    constexpr float gridX = 8.0F;
    constexpr float gridY = 60.0F;
    constexpr float pitchX = 32.0F;
    constexpr float pitchY = 25.0F;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float x = gridX + static_cast<float>(slot % 6) * pitchX;
        const float y = gridY + static_cast<float>(slot / 6) * pitchY;
        if (UiRect{x, y, pitchX, pitchY}.contains(touch)) {
            if (focusedSlot_ == slot && storagePane_ == StoragePane::Local) {
                if (hand_.active) {
                    storageDrop();
                } else {
                    storagePickUp();
                }
            } else {
                focusedSlot_ = slot;
                storagePane_ = StoragePane::Local;
            }
            return;
        }
    }
}

int App::storageDirection(u32 keysDown, u32 keysHeld, circlePosition circle) {
    int direction = 0;
    if ((keysHeld & KEY_UP) || circle.dy > 60) {
        direction = 1;
    } else if ((keysHeld & KEY_DOWN) || circle.dy < -60) {
        direction = 2;
    } else if ((keysHeld & KEY_LEFT) || circle.dx < -60) {
        direction = 3;
    } else if ((keysHeld & KEY_RIGHT) || circle.dx > 60) {
        direction = 4;
    }

    if (direction == 0) {
        heldDirection_ = 0;
        directionRepeatAt_ = 0;
        return 0;
    }
    const u64 now = svcGetSystemTick();
    const bool digitalPressed = (direction == 1 && (keysDown & KEY_UP))
        || (direction == 2 && (keysDown & KEY_DOWN))
        || (direction == 3 && (keysDown & KEY_LEFT))
        || (direction == 4 && (keysDown & KEY_RIGHT));
    if (direction != heldDirection_ || digitalPressed) {
        heldDirection_ = direction;
        directionRepeatAt_ = now + static_cast<u64>(SYSCLOCK_ARM11 * 0.28);
        return direction;
    }
    if (now >= directionRepeatAt_) {
        directionRepeatAt_ = now + static_cast<u64>(SYSCLOCK_ARM11 * 0.09);
        return direction;
    }
    return 0;
}

void App::selectFocusedPokemon() {
    // kept as a no-op stub; pickup/drop replaces manual selection.
}

void App::updateForm(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched) {
    if (authState_.load(std::memory_order_acquire) == AuthState::Running) {
        return;
    }

    const bool resetForm = screen_ == Screen::ResetPassword;
    const bool registerForm = screen_ == Screen::Register;
    const auto usernameValid = [&]() {
        return username_.size() >= 3 && username_.size() <= 32
            && std::all_of(username_.begin(), username_.end(), [](unsigned char value) {
                return std::isalnum(value) || value == '_' || value == '-';
            });
    };
    const auto activate = [&]() {
        switch (authFocus_) {
            case AuthFocus::Username:
                if (!resetForm) {
                    requestText(username_, localization_.get(TextId::Username), false);
                }
                break;
            case AuthFocus::Email:
                if (registerForm || resetForm) {
                    requestText(email_, localization_.get(TextId::Email), false);
                }
                break;
            case AuthFocus::Password:
                if (!resetForm) {
                    requestText(password_, localization_.get(TextId::Password), true);
                }
                break;
            case AuthFocus::AutoLogin:
                if (resetForm) {
                    break;
                }
                autoLogin_ = !autoLogin_;
                status_ = autoLogin_
                    ? "Auto-login enabled."
                    : "Auto-login disabled.";
                if (!autoLogin_) {
                    credentials_.clear();
                }
                break;
            case AuthFocus::Submit: {
                if (!resetForm && !usernameValid()) {
                    status_ = "Username must be 3-32 letters, numbers, _ or -.";
                } else if ((registerForm || resetForm)
                           && email_.find('@') == std::string::npos) {
                    status_ = "Please enter a valid email address.";
                } else if (!resetForm && password_.size() < 10) {
                    status_ = "Password must contain at least 10 characters.";
                } else {
                    AuthOperation operation = AuthOperation::Login;
                    if (screen_ == Screen::Register) {
                        operation = AuthOperation::Register;
                    } else if (resetForm) {
                        operation = AuthOperation::ResetPassword;
                    }
                    beginAuth(operation);
                }
                break;
            }
            case AuthFocus::Back:
                password_.clear();
                status_.clear();
                screen_ = Screen::Welcome;
                authFocus_ = resetForm ? AuthFocus::Email : AuthFocus::Username;
                break;
        }
    };

    const auto step = [&](int delta) {
        const std::array<AuthFocus, 6> order{
            AuthFocus::Username, AuthFocus::Email, AuthFocus::Password, AuthFocus::AutoLogin,
            AuthFocus::Submit, AuthFocus::Back
        };
        const auto visible = [&](AuthFocus focus) {
            if (focus == AuthFocus::Username) return !resetForm;
            if (focus == AuthFocus::Email) return registerForm || resetForm;
            if (focus == AuthFocus::Password) return !resetForm;
            if (focus == AuthFocus::AutoLogin) return !resetForm;
            return true;
        };
        int index = 0;
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (order[i] == authFocus_) {
                index = static_cast<int>(i);
            }
        }
        int next = (index + delta) % static_cast<int>(order.size());
        if (next < 0) {
            next += static_cast<int>(order.size());
        }
        while (!visible(order[next])) {
            next = (next + delta) % static_cast<int>(order.size());
            if (next < 0) {
                next += static_cast<int>(order.size());
            }
        }
        authFocus_ = order[next];
    };

    if ((keysDown & KEY_UP) || circle.dy > 60) {
        step(-1);
    } else if ((keysDown & KEY_DOWN) || circle.dy < -60) {
        step(1);
    }
    if (keysDown & KEY_A) {
        activate();
        return;
    }
    if (keysDown & KEY_Y) {
        if (resetForm) {
            return;
        }
        autoLogin_ = !autoLogin_;
        status_ = autoLogin_ ? "Auto-login enabled." : "Auto-login disabled.";
        if (!autoLogin_) {
            credentials_.clear();
        }
    }
    if (keysDown & KEY_B) {
        password_.clear();
        status_.clear();
        screen_ = Screen::Welcome;
        authFocus_ = resetForm ? AuthFocus::Email : AuthFocus::Username;
        return;
    }

    if (!touched) {
        (void)keysHeld;
        return;
    }

    if (BackButton.contains(touch)) {
        password_.clear();
        status_.clear();
        screen_ = Screen::Welcome;
        authFocus_ = resetForm ? AuthFocus::Email : AuthFocus::Username;
    } else if (!resetForm && UiRect{24.0F, registerForm ? 48.0F : 62.0F,
                                    272.0F, registerForm ? 34.0F : 42.0F}.contains(touch)) {
        authFocus_ = AuthFocus::Username;
        requestText(username_, localization_.get(TextId::Username), false);
    } else if ((registerForm || resetForm)
               && UiRect{24.0F, registerForm ? 88.0F : 62.0F,
                         272.0F, registerForm ? 34.0F : 42.0F}.contains(touch)) {
        authFocus_ = AuthFocus::Email;
        requestText(email_, localization_.get(TextId::Email), false);
    } else if (!resetForm && UiRect{24.0F, registerForm ? 128.0F : 114.0F,
                                    272.0F, registerForm ? 34.0F : 42.0F}.contains(touch)) {
        authFocus_ = AuthFocus::Password;
        requestText(password_, localization_.get(TextId::Password), true);
    } else if (!resetForm
               && UiRect{24.0F, registerForm ? 166.0F : 162.0F, 272.0F, 26.0F}.contains(touch)) {
        authFocus_ = AuthFocus::AutoLogin;
        autoLogin_ = !autoLogin_;
        status_ = autoLogin_ ? "Auto-login enabled." : "Auto-login disabled.";
        if (!autoLogin_) {
            credentials_.clear();
        }
    } else if (SubmitButton.contains(touch)) {
        authFocus_ = AuthFocus::Submit;
        activate();
    }
}

void App::beginUpdate() {
    status_ = "Checking for updates...";
    updateState_.store(UpdateState::Running, std::memory_order_release);
    updateThread_ = threadCreate(updateWorker, this, 128 * 1024, 0x30, -2, false);
    if (!updateThread_) {
        updateState_.store(UpdateState::Idle, std::memory_order_release);
        status_ = "Update check could not start.";
        Logger::instance().warning("Update worker creation failed");
    }
}

void App::pollUpdate() {
    if (updateState_.load(std::memory_order_acquire) != UpdateState::Completed) {
        return;
    }
    threadJoin(updateThread_, U64_MAX);
    threadFree(updateThread_);
    updateThread_ = nullptr;
    updateState_.store(UpdateState::Idle, std::memory_order_release);
    status_ = updateResult_.message;
    if (updateResult_.updated) {
        Logger::instance().info(updateResult_.message);
        running_ = false;
    } else if (!updateResult_.success) {
        Logger::instance().warning(updateResult_.message);
    }
}

void App::updateWorker(void* argument) {
    auto* app = static_cast<App*>(argument);
    app->updateResult_ = UpdateInstaller::run(app->api_, app->executablePath_, app->homebrew_);
    app->updateState_.store(UpdateState::Completed, std::memory_order_release);
}

void App::beginAuth(AuthOperation operation) {
    if (authState_.load(std::memory_order_acquire) == AuthState::Running) {
        return;
    }
    if (authThread_) {
        threadJoin(authThread_, U64_MAX);
        threadFree(authThread_);
        authThread_ = nullptr;
    }
    authOperation_ = operation;
    if (operation != AuthOperation::Refresh) {
        authUsername_ = username_;
        authEmail_ = email_;
        authPassword_ = password_;
    }
    status_ = operation == AuthOperation::Refresh ? "Restoring session..." : "Connecting...";
    loadingPhase_.store(LoadingPhase::Authenticating, std::memory_order_release);
    authState_.store(AuthState::Running, std::memory_order_release);
    authThread_ = threadCreate(authWorker, this, 64 * 1024, 0x30, -2, false);
    if (!authThread_) {
        authState_.store(AuthState::Idle, std::memory_order_release);
        status_ = "Could not start the login process.";
        Logger::instance().error("Authentication worker creation failed");
    }
}

void App::pollAuth() {
    if (authState_.load(std::memory_order_acquire) != AuthState::Completed) {
        return;
    }
    threadJoin(authThread_, U64_MAX);
    threadFree(authThread_);
    authThread_ = nullptr;
    authState_.store(AuthState::Idle, std::memory_order_release);
    loadingPhase_.store(LoadingPhase::Idle, std::memory_order_release);

    status_ = authResult_.message;
    if (!authResult_.success) {
        Logger::instance().warning("Authentication failed");
        if (authOperation_ == AuthOperation::Refresh) {
            sessionStore_.clear();
            screen_ = Screen::Welcome;
        } else if (authOperation_ == AuthOperation::Login && autoLogin_) {
            credentials_.clear();
            autoLogin_ = false;
        }
        authUsername_.clear();
        authEmail_.clear();
        authPassword_.clear();
        return;
    }
    if (authOperation_ == AuthOperation::ResetPassword) {
        Logger::instance().info("Password reset accepted");
        authUsername_.clear();
        authEmail_.clear();
        authPassword_.clear();
        screen_ = Screen::Welcome;
        return;
    }

    session_ = std::move(authResult_.session);
    sessionStore_.save(session_.refreshToken);
    if (autoLogin_ && authOperation_ != AuthOperation::Refresh
        && !authUsername_.empty() && !authPassword_.empty()) {
        credentials_.save({authUsername_, authPassword_});
    }
    authUsername_.clear();
    authEmail_.clear();
    authPassword_.clear();
    password_.clear();
    status_ = "Finding save games...";
    refreshGameProfiles();
    Logger::instance().info("Authentication succeeded");
}

void App::authWorker(void* argument) {
    auto* app = static_cast<App*>(argument);
    switch (app->authOperation_) {
        case AuthOperation::Register:
            app->authResult_ = app->api_.registerAccount(
                app->authUsername_, app->authEmail_, app->authPassword_);
            break;
        case AuthOperation::ResetPassword:
            app->authResult_ = app->api_.requestPasswordReset(app->authEmail_);
            break;
        case AuthOperation::Refresh:
            app->authResult_ = app->api_.refresh(app->authPassword_);
            break;
        default:
            app->authResult_ = app->api_.login(app->authUsername_, app->authPassword_);
            break;
    }
    app->authState_.store(AuthState::Completed, std::memory_order_release);
}

void App::beginLoad(LoadOperation operation) {
    if (loadState_.load(std::memory_order_acquire) == LoadState::Running) {
        return;
    }
    if (loadThread_) {
        threadJoin(loadThread_, U64_MAX);
        threadFree(loadThread_);
        loadThread_ = nullptr;
    }
    loadOperation_ = operation;
    loadingStartedAt_ = svcGetSystemTick();
    switch (operation) {
        case LoadOperation::DiscoverGames:
            discoveredGames_.clear();
            loadingPhase_.store(LoadingPhase::SearchingGames, std::memory_order_release);
            break;
        case LoadOperation::OpenGame:
            openGameResult_ = {};
            loadingPhase_.store(LoadingPhase::ReadingSave, std::memory_order_release);
            break;
        case LoadOperation::CloudBox:
            cloudLoadResult_ = {};
            loadingPhase_.store(LoadingPhase::LoadingBank, std::memory_order_release);
            break;
        default:
            return;
    }
    loadState_.store(LoadState::Running, std::memory_order_release);
    loadThread_ = threadCreate(loadWorker, this, 128 * 1024, 0x30, -2, false);
    if (!loadThread_) {
        loadState_.store(LoadState::Idle, std::memory_order_release);
        loadingPhase_.store(LoadingPhase::Idle, std::memory_order_release);
        loadOperation_ = LoadOperation::None;
        status_ = "Could not start loading.";
        Logger::instance().error("Loading worker creation failed");
    }
}

void App::loadWorker(void* argument) {
    auto* app = static_cast<App*>(argument);
    try {
        if (app->loadOperation_ == LoadOperation::DiscoverGames) {
            const auto games = supportedGames();
            for (std::size_t index = 0; index < games.size(); ++index) {
                SaveAdapter candidate;
                std::string error;
                if (!candidate.open(games[index], error)) {
                    continue;
                }
                DiscoveredGame game;
                game.catalogIndex = index;
                game.save = candidate.summary();
                game.cartridge = candidate.isCartridge();
                app->discoveredGames_.push_back(std::move(game));
            }
            app->loadingPhase_.store(LoadingPhase::ReadingIcons, std::memory_order_release);
            for (DiscoveredGame& game : app->discoveredGames_) {
                game.iconPixels.reset(
                    new (std::nothrow) std::array<std::uint16_t, 48 * 48>());
                if (game.iconPixels
                    && !SaveAdapter::readGameIcon(
                        games[game.catalogIndex], game.cartridge, *game.iconPixels)) {
                    game.iconPixels.reset();
                }
            }
        } else if (app->loadOperation_ == LoadOperation::OpenGame) {
            const auto games = supportedGames();
            if (app->loadingCatalogIndex_ >= games.size()) {
                app->openGameResult_.message = "Invalid game selection.";
            } else if (app->saveAdapter_.open(
                           games[app->loadingCatalogIndex_], app->openGameResult_.message)) {
                app->openGameResult_.save = app->saveAdapter_.summary();
                app->openGameResult_.localBox = app->saveAdapter_.currentBox();
                app->openGameResult_.localBoxName =
                    app->saveAdapter_.boxName(app->openGameResult_.localBox);
                app->loadingPhase_.store(LoadingPhase::SearchingPokemon, std::memory_order_release);
                app->openGameResult_.localPokemon =
                    app->saveAdapter_.readBox(app->openGameResult_.localBox);
                for (std::size_t slot = 0; slot < 30; ++slot) {
                    app->openGameResult_.localPayloads[slot] =
                        app->saveAdapter_.readPokemon(app->openGameResult_.localBox, slot);
                }
                app->loadingPhase_.store(LoadingPhase::LoadingBank, std::memory_order_release);
                if (!app->session_.accessToken.empty()) {
                    app->openGameResult_.cloudBox =
                        app->api_.listCloudBox(1, app->session_.accessToken);
                }
                app->openGameResult_.success = true;
            }
        } else if (app->loadOperation_ == LoadOperation::CloudBox) {
            app->cloudLoadResult_ = app->api_.listCloudBox(
                static_cast<std::uint16_t>(app->loadingCloudBox_ + 1),
                app->session_.accessToken);
        }
    } catch (...) {
        if (app->loadOperation_ == LoadOperation::OpenGame) {
            app->openGameResult_.success = false;
            app->openGameResult_.message = "Loading failed unexpectedly.";
        } else if (app->loadOperation_ == LoadOperation::CloudBox) {
            app->cloudLoadResult_.success = false;
            app->cloudLoadResult_.message = "Bank loading failed unexpectedly.";
        }
        Logger::instance().error("Unhandled loading worker exception");
    }
    app->loadState_.store(LoadState::Completed, std::memory_order_release);
}

void App::pollLoad() {
    if (loadState_.load(std::memory_order_acquire) != LoadState::Completed) {
        return;
    }
    threadJoin(loadThread_, U64_MAX);
    threadFree(loadThread_);
    loadThread_ = nullptr;
    const LoadOperation completed = loadOperation_;
    loadState_.store(LoadState::Idle, std::memory_order_release);
    loadingPhase_.store(LoadingPhase::Idle, std::memory_order_release);

    if (completed == LoadOperation::DiscoverGames) {
        for (auto& profile : availableGames_) {
            if (profile.iconLoaded) {
                C3D_TexDelete(&profile.iconTexture);
            }
        }
        availableGames_.clear();
        std::stable_partition(discoveredGames_.begin(), discoveredGames_.end(),
            [](const DiscoveredGame& game) { return game.cartridge; });
        availableGames_.reserve(discoveredGames_.size());
        for (DiscoveredGame& game : discoveredGames_) {
            GameProfile profile;
            profile.catalogIndex = game.catalogIndex;
            profile.save = std::move(game.save);
            profile.cartridge = game.cartridge;
            availableGames_.push_back(std::move(profile));
            GameProfile& stored = availableGames_.back();
            if (!game.iconPixels
                || !C3D_TexInit(&stored.iconTexture, 64, 64, GPU_RGB565)) {
                continue;
            }
            const std::unique_ptr<std::array<std::uint16_t, 64 * 64>> tiled(
                new (std::nothrow) std::array<std::uint16_t, 64 * 64>());
            if (!tiled) {
                C3D_TexDelete(&stored.iconTexture);
                continue;
            }
            for (std::size_t y = 0; y < 48; ++y) {
                for (std::size_t x = 0; x < 48; ++x) {
                    const std::size_t pixel = ((x & 1) | ((y & 1) << 1)
                        | ((x & 2) << 1) | ((y & 2) << 2)
                        | ((x & 4) << 2) | ((y & 4) << 3));
                    const std::size_t destination = ((y / 8) * 8 + x / 8) * 64 + pixel;
                    (*tiled)[destination] = (*game.iconPixels)[y * 48 + x];
                }
            }
            C3D_TexUpload(&stored.iconTexture, tiled->data());
            C3D_TexSetFilter(&stored.iconTexture, GPU_LINEAR, GPU_LINEAR);
            stored.iconSubTexture = {48, 48, 0.0F, 1.0F, 0.75F, 0.25F};
            stored.iconLoaded = true;
        }
        discoveredGames_.clear();
        gameIndex_ = 0;
        gameSelectionChangedAt_ = svcGetSystemTick();
        gameSelectionDirection_ = 0;
        status_ = availableGames_.empty() ? "No compatible save game found." : std::string{};
        screen_ = Screen::GameSelect;
        Logger::instance().info("Detected " + std::to_string(availableGames_.size())
                                + " save games");
    } else if (completed == LoadOperation::OpenGame) {
        if (!openGameResult_.success) {
            status_ = openGameResult_.message;
            screen_ = Screen::GameSelect;
        } else {
            saveSummary_ = std::move(openGameResult_.save);
            localBox_ = openGameResult_.localBox;
            cloudBox_ = 0;
            localBoxName_ = std::move(openGameResult_.localBoxName);
            localBaselines_.clear();
            localDrafts_.clear();
            cloudBoxes_.clear();
            LocalBoxDraft baseline;
            baseline.summaries = std::move(openGameResult_.localPokemon);
            baseline.payloads = std::move(openGameResult_.localPayloads);
            localBaselines_[localBox_] = std::move(baseline);
            storage_.load(localBaselines_[localBox_].summaries);
            localPayloads_ = localBaselines_[localBox_].payloads;
            storagePane_ = StoragePane::Local;
            transferArmed_ = false;
            focusedSlot_ = 0;
            for (std::size_t slot = 0; slot < 30; ++slot) {
                if (storage_.pokemon(slot).species != 0) {
                    focusedSlot_ = slot;
                    break;
                }
            }
            CloudBoxDraft cloud;
            if (openGameResult_.cloudBox.success) {
                cloud.baseline = openGameResult_.cloudBox.pokemon;
                cloud.summaries = openGameResult_.cloudBox.pokemon;
                cloudBoxes_[0] = cloud;
                cloudPreview_ = cloud.summaries;
                status_.clear();
            } else {
                cloudPreview_.fill({});
                status_ = openGameResult_.cloudBox.message;
            }
            pendingUploadPayloads_ = {};
            cachedCloudPayloads_ = {};
            hand_ = Hand{};
            screen_ = Screen::Storage;
        }
    } else if (completed == LoadOperation::CloudBox) {
        const auto boxKey = loadingCloudBox_;
        if (cloudLoadResult_.success) {
            CloudBoxDraft draft;
            draft.baseline = cloudLoadResult_.pokemon;
            draft.summaries = cloudLoadResult_.pokemon;
            cloudBoxes_[boxKey] = std::move(draft);
            if (cloudBox_ == boxKey) {
                cloudPreview_ = cloudBoxes_[boxKey].summaries;
                pendingUploadPayloads_ = cloudBoxes_[boxKey].pending;
                cachedCloudPayloads_ = {};
            }
            status_.clear();
        } else {
            if (cloudBox_ == boxKey) {
                cloudPreview_.fill({});
                pendingUploadPayloads_ = {};
            }
            status_ = cloudLoadResult_.message;
            Logger::instance().warning("Cloud box refresh failed: " + status_);
        }
    }
    loadOperation_ = LoadOperation::None;
}

bool App::isLoading() const {
    return updateState_.load(std::memory_order_acquire) == UpdateState::Running
        || authState_.load(std::memory_order_acquire) == AuthState::Running
        || (loadState_.load(std::memory_order_acquire) == LoadState::Running
            && loadOperation_ != LoadOperation::CloudBox);
}

void App::beginUpload() {
    // Deprecated: uploads now flow through beginCommit().
}

void App::pollUpload() {
    // Deprecated: legacy state transitions are handled by pollCommit().
}

void App::uploadWorker(void* /*argument*/) {
}

void App::beginDownload(
    std::uint16_t /*cloudBox*/,
    std::uint8_t /*cloudSlot*/,
    std::size_t /*localBox*/,
    std::size_t /*localSlot*/
) {
    // Deprecated: downloads now flow through beginCommit().
}

void App::pollDownload() {
    // Deprecated.
}

void App::downloadWorker(void* /*argument*/) {
}

bool App::hasPendingChanges() const {
    for (const auto& [box, draft] : localDrafts_) {
        auto it = localBaselines_.find(box);
        if (it == localBaselines_.end()) {
            return true;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            if (draft.summaries[slot].species != it->second.summaries[slot].species
                || draft.summaries[slot].nickname != it->second.summaries[slot].nickname
                || draft.payloads[slot].data != it->second.payloads[slot].data) {
                return true;
            }
        }
    }
    if (localBox_ < saveAdapter_.boxCount()) {
        auto it = localBaselines_.find(localBox_);
        if (it != localBaselines_.end()) {
            for (std::size_t slot = 0; slot < 30; ++slot) {
                if (storage_.pokemon(slot).species != it->second.summaries[slot].species
                    || storage_.pokemon(slot).nickname != it->second.summaries[slot].nickname
                    || localPayloads_[slot].data != it->second.payloads[slot].data) {
                    return true;
                }
            }
        }
    }
    for (const auto& [box, draft] : cloudBoxes_) {
        for (std::size_t slot = 0; slot < 30; ++slot) {
            if (draft.summaries[slot].species != draft.baseline[slot].species
                || draft.summaries[slot].nickname != draft.baseline[slot].nickname
                || !draft.pending[slot].data.empty()) {
                return true;
            }
        }
    }
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (!pendingUploadPayloads_[slot].data.empty()) {
            return true;
        }
    }
    return false;
}

void App::snapshotBox() {
    // Legacy no-op; baselines are managed per-box in loadLocalBox/refreshCloudBox.
}

void App::storagePickUp() {
    if (hand_.active) {
        return;
    }
    if (storagePane_ == StoragePane::Local) {
        const PokemonSummary& mon = storage_.pokemon(focusedSlot_);
        if (mon.species == 0) {
            status_ = "This slot is empty.";
            return;
        }
        hand_.active = true;
        hand_.source = HandSource::Local;
        hand_.sourceIndex = focusedSlot_;
        hand_.summary = mon;
        hand_.payload = std::move(localPayloads_[focusedSlot_]);
        hand_.payloadKnown = !hand_.payload.data.empty();
        localPayloads_[focusedSlot_] = {};
        storage_.set(focusedSlot_, PokemonSummary{});
        status_ = mon.nickname + " picked up.";
        return;
    }

    const PokemonSummary& mon = cloudPreview_[focusedSlot_];
    if (mon.species == 0) {
        status_ = "This slot is empty.";
        return;
    }
    if (session_.accessToken.empty()) {
        status_ = "Please sign in again.";
        return;
    }
    PokemonPayload payload;
    if (!pendingUploadPayloads_[focusedSlot_].data.empty()) {
        payload = std::move(pendingUploadPayloads_[focusedSlot_]);
        pendingUploadPayloads_[focusedSlot_] = {};
    } else if (!cachedCloudPayloads_[focusedSlot_].data.empty()) {
        payload = cachedCloudPayloads_[focusedSlot_];
    } else {
        status_ = "Fetching " + mon.nickname + "...";
        DownloadResult result = api_.downloadPokemon(
            static_cast<std::uint16_t>(cloudBox_ + 1),
            static_cast<std::uint8_t>(focusedSlot_ + 1),
            session_.accessToken
        );
        if (!result.success) {
            status_ = "Cannot pick up: " + result.message;
            return;
        }
        payload.format = result.pokemon.format;
        payload.data = std::move(result.pokemon.payload);
        cachedCloudPayloads_[focusedSlot_] = payload;
    }
    hand_.active = true;
    hand_.source = HandSource::Cloud;
    hand_.sourceIndex = focusedSlot_;
    hand_.sourceCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
    hand_.summary = mon;
    hand_.payload = std::move(payload);
    hand_.payloadKnown = !hand_.payload.data.empty();
    cloudPreview_[focusedSlot_] = {};
    status_ = mon.nickname + " picked up.";
}

void App::storageDrop() {
    if (!hand_.active) {
        return;
    }
    if (storagePane_ == StoragePane::Local) {
        const std::uint8_t saveGen = saveAdapter_.gameGeneration();
        if (!saveAdapter_.canImportPokemon(hand_.payload.format, hand_.payload.data)) {
            status_ = "Gen " + std::to_string(hand_.payload.format)
                      + " cannot enter Gen " + std::to_string(saveGen) + ".";
            return;
        }
        const bool occupied = storage_.pokemon(focusedSlot_).species != 0;
        if (occupied) {
            PokemonSummary occupantSummary = storage_.pokemon(focusedSlot_);
            PokemonPayload occupantPayload = std::move(localPayloads_[focusedSlot_]);
            storage_.set(focusedSlot_, hand_.summary);
            localPayloads_[focusedSlot_] = hand_.payload;
            hand_.summary = std::move(occupantSummary);
            hand_.payload = std::move(occupantPayload);
            hand_.source = HandSource::Local;
            hand_.sourceIndex = focusedSlot_;
            hand_.payloadKnown = !hand_.payload.data.empty();
            status_ = hand_.summary.nickname + " swapped.";
            return;
        }
        storage_.set(focusedSlot_, hand_.summary);
        localPayloads_[focusedSlot_] = hand_.payload;
        status_ = hand_.summary.nickname + " placed.";
        hand_ = Hand{};
        return;
    }

    if (!hand_.payloadKnown) {
        status_ = "Payload missing.";
        return;
    }
    const bool occupied = cloudPreview_[focusedSlot_].species != 0;
    if (occupied) {
        PokemonPayload occupantPayload;
        if (!pendingUploadPayloads_[focusedSlot_].data.empty()) {
            occupantPayload = std::move(pendingUploadPayloads_[focusedSlot_]);
            pendingUploadPayloads_[focusedSlot_] = {};
        } else if (!cachedCloudPayloads_[focusedSlot_].data.empty()) {
            occupantPayload = cachedCloudPayloads_[focusedSlot_];
        } else if (!session_.accessToken.empty()) {
            status_ = "Fetching occupant...";
            DownloadResult dr = api_.downloadPokemon(
                static_cast<std::uint16_t>(cloudBox_ + 1),
                static_cast<std::uint8_t>(focusedSlot_ + 1),
                session_.accessToken
            );
            if (!dr.success) {
                status_ = "Swap failed: " + dr.message;
                return;
            }
            occupantPayload.format = dr.pokemon.format;
            occupantPayload.data = std::move(dr.pokemon.payload);
        } else {
            status_ = "Please sign in again.";
            return;
        }
        PokemonSummary occupantSummary = cloudPreview_[focusedSlot_];
        cloudPreview_[focusedSlot_] = hand_.summary;
        pendingUploadPayloads_[focusedSlot_] = hand_.payload;
        hand_.summary = std::move(occupantSummary);
        hand_.payload = std::move(occupantPayload);
        hand_.source = HandSource::Cloud;
        hand_.sourceIndex = focusedSlot_;
        hand_.sourceCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
        hand_.payloadKnown = !hand_.payload.data.empty();
        status_ = hand_.summary.nickname + " swapped.";
        return;
    }
    cloudPreview_[focusedSlot_] = hand_.summary;
    PokemonPayload payload = hand_.payload;
    pendingUploadPayloads_[focusedSlot_] = std::move(payload);
    status_ = hand_.summary.nickname + " placed.";
    hand_ = Hand{};
}

void App::storageReturnHand() {
    if (!hand_.active) {
        return;
    }
    if (hand_.source == HandSource::Local) {
        storage_.set(hand_.sourceIndex, hand_.summary);
        localPayloads_[hand_.sourceIndex] = hand_.payload;
    } else {
        cloudPreview_[hand_.sourceIndex] = hand_.summary;
        if (!hand_.payload.data.empty()) {
            cachedCloudPayloads_[hand_.sourceIndex] = hand_.payload;
        }
    }
    status_ = "Returned to slot " + std::to_string(hand_.sourceIndex + 1) + ".";
    hand_ = Hand{};
}

void App::beginCommit() {
    if (commitState_.load(std::memory_order_acquire) == CommitState::Running) {
        return;
    }
    if (session_.accessToken.empty()) {
        status_ = "Please sign in again.";
        return;
    }
    if (commitThread_) {
        threadJoin(commitThread_, U64_MAX);
        threadFree(commitThread_);
        commitThread_ = nullptr;
    }
    persistLocalDraft();
    persistCloudDraft();
    commitStartedAt_ = svcGetSystemTick();
    commitPhase_.store(0, std::memory_order_release);
    commitProgress_.store(0, std::memory_order_release);
    status_ = "Committing changes...";
    commitState_.store(CommitState::Running, std::memory_order_release);
    commitThread_ = threadCreate(commitWorker, this, 512 * 1024, 0x30, -2, false);
    if (!commitThread_) {
        commitState_.store(CommitState::Idle, std::memory_order_release);
        status_ = "Could not start the commit.";
    }
}

void App::commitWorker(void* argument) {
    auto* app = static_cast<App*>(argument);
    CommitResult result;

    std::vector<UploadPokemon> uploads;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> deletes;
    for (const auto& [boxKey, draft] : app->cloudBoxes_) {
        const auto boxPosition = static_cast<std::uint16_t>(boxKey + 1);
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool initHas = draft.baseline[slot].species != 0;
            const bool nowHas = draft.summaries[slot].species != 0;
            const bool same = draft.summaries[slot].species == draft.baseline[slot].species
                && draft.summaries[slot].nickname == draft.baseline[slot].nickname;
            if (nowHas && (!same || !draft.pending[slot].data.empty())) {
                const auto& payload = draft.pending[slot];
                if (payload.data.empty()) {
                    result.message = "A staged Pokemon is missing its payload.";
                    app->commitResult_ = result;
                    app->commitState_.store(CommitState::Completed, std::memory_order_release);
                    return;
                }
                uploads.push_back(UploadPokemon{
                    boxPosition,
                    static_cast<std::uint8_t>(slot + 1),
                    payload.format,
                    payload.data,
                    draft.summaries[slot].species,
                    draft.summaries[slot].nickname,
                    draft.summaries[slot].trainerName,
                    draft.summaries[slot].level,
                    draft.summaries[slot].gameCode
                });
            }
            if (initHas && !nowHas) {
                deletes.emplace_back(boxPosition, static_cast<std::uint8_t>(slot + 1));
            }
        }
    }

    std::size_t localChangeCount = 0;
    for (const auto& [boxKey, draft] : app->localDrafts_) {
        const auto baselineIt = app->localBaselines_.find(boxKey);
        if (baselineIt == app->localBaselines_.end()) {
            continue;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool sameSummary = draft.summaries[slot].species
                    == baselineIt->second.summaries[slot].species
                && draft.summaries[slot].nickname
                    == baselineIt->second.summaries[slot].nickname;
            const bool samePayload = draft.payloads[slot].data
                == baselineIt->second.payloads[slot].data;
            if (!sameSummary || !samePayload) {
                ++localChangeCount;
            }
        }
    }
    const std::size_t uploadBatchCount = (uploads.size() + 29) / 30;
    const std::size_t totalSteps = localChangeCount + (localChangeCount > 0 ? 1 : 0)
        + deletes.size() + uploadBatchCount;
    std::size_t completedSteps = 0;
    const auto advanceProgress = [&]() {
        ++completedSteps;
        const int percent = totalSteps == 0
            ? 100
            : static_cast<int>((completedSteps * 100) / totalSteps);
        app->commitProgress_.store(percent, std::memory_order_release);
    };

    while (!uploads.empty()) {
        app->commitPhase_.store(2, std::memory_order_release);
        const std::size_t batchSize = std::min<std::size_t>(uploads.size(), 30);
        std::vector<UploadPokemon> batch(uploads.begin(), uploads.begin() + batchSize);
        uploads.erase(uploads.begin(), uploads.begin() + batchSize);
        UploadResult ur = app->api_.uploadPokemon(batch, app->session_.accessToken);
        if (!ur.success) {
            result.message = "Upload failed: " + ur.message;
            result.problemReason = ur.message;
            std::uint16_t problemBank = 0;
            const std::size_t bankMarker = ur.message.find("Bank ");
            if (bankMarker != std::string::npos) {
                std::size_t begin = bankMarker + 5;
                std::size_t end = begin;
                while (end < ur.message.size() && ur.message[end] >= '0'
                       && ur.message[end] <= '9') {
                    problemBank = static_cast<std::uint16_t>(
                        problemBank * 10 + (ur.message[end] - '0'));
                    ++end;
                }
            }
            const std::size_t marker = ur.message.find("Slot ");
            if (marker != std::string::npos) {
                std::size_t begin = marker + 5;
                std::size_t end = begin;
                std::uint8_t problemSlot = 0;
                while (end < ur.message.size() && ur.message[end] >= '0'
                       && ur.message[end] <= '9') {
                    problemSlot = static_cast<std::uint8_t>(
                        problemSlot * 10 + (ur.message[end] - '0'));
                    ++end;
                }
                if (end > begin) {
                    const auto problem = std::find_if(batch.begin(), batch.end(),
                        [problemBank, problemSlot](const UploadPokemon& upload) {
                            return upload.slot == problemSlot
                                && (problemBank == 0 || upload.boxPosition == problemBank);
                        });
                    if (problem != batch.end()) {
                        result.problemPokemon = problem->nickname.empty()
                            ? "Pokemon #" + std::to_string(problem->species)
                            : problem->nickname;
                        result.problemLocation = "Bank "
                            + std::to_string(problem->boxPosition) + "  |  Slot "
                            + std::to_string(problem->slot);
                    }
                    const std::size_t reason = ur.message.find(':', end);
                    if (reason != std::string::npos && reason + 1 < ur.message.size()) {
                        result.problemReason = ur.message.substr(reason + 1);
                        while (!result.problemReason.empty()
                               && result.problemReason.front() == ' ') {
                            result.problemReason.erase(result.problemReason.begin());
                        }
                    }
                }
            }
            app->commitResult_ = result;
            app->commitState_.store(CommitState::Completed, std::memory_order_release);
            return;
        }
        result.uploads += ur.storedCount;
        advanceProgress();
    }

    bool anyLocalWrite = false;
    struct LocalWriteVerification {
        std::size_t box;
        std::size_t slot;
        std::uint16_t species;
    };
    std::vector<LocalWriteVerification> localWriteVerifications;
    for (const auto& [boxKey, draft] : app->localDrafts_) {
        auto baselineIt = app->localBaselines_.find(boxKey);
        if (baselineIt == app->localBaselines_.end()) {
            continue;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool sameSummary = draft.summaries[slot].species == baselineIt->second.summaries[slot].species
                && draft.summaries[slot].nickname == baselineIt->second.summaries[slot].nickname;
            const bool samePayload = draft.payloads[slot].data == baselineIt->second.payloads[slot].data;
            if (sameSummary && samePayload) {
                continue;
            }
            if (draft.summaries[slot].species == 0) {
                if (!app->saveAdapter_.clearSlot(boxKey, slot)) {
                    result.message = "Could not clear local slot " + std::to_string(slot + 1) + ".";
                    app->commitResult_ = result;
                    app->commitState_.store(CommitState::Completed, std::memory_order_release);
                    return;
                }
            } else if (!draft.payloads[slot].data.empty()) {
                if (!app->saveAdapter_.writePokemon(boxKey, slot, draft.payloads[slot].format, draft.payloads[slot].data)) {
                    result.message = "Local write failed for slot " + std::to_string(slot + 1) + " (incompatible generation).";
                    app->commitResult_ = result;
                    app->commitState_.store(CommitState::Completed, std::memory_order_release);
                    return;
                }
                localWriteVerifications.push_back({
                    boxKey, slot, draft.summaries[slot].species
                });
                ++result.downloads;
            }
            anyLocalWrite = true;
            advanceProgress();
        }
    }
    if (anyLocalWrite) {
        app->commitPhase_.store(3, std::memory_order_release);
        std::string saveError;
        if (!app->saveAdapter_.writeSave(saveError)) {
            result.message = "Save write failed: " + saveError;
            app->commitResult_ = result;
            app->commitState_.store(CommitState::Completed, std::memory_order_release);
            return;
        }
        advanceProgress();
        for (const auto& expected : localWriteVerifications) {
            const auto savedBox = app->saveAdapter_.readBox(expected.box);
            if (expected.slot >= savedBox.size()
                || savedBox[expected.slot].species != expected.species) {
                result.message = "Saved Pokemon verification failed; cloud copy retained.";
                Logger::instance().error("Post-save verification failed for box "
                                         + std::to_string(expected.box + 1) + " slot "
                                         + std::to_string(expected.slot + 1));
                app->commitResult_ = result;
                app->commitState_.store(CommitState::Completed, std::memory_order_release);
                return;
            }
        }
    }

    for (const auto& deletion : deletes) {
        app->commitPhase_.store(1, std::memory_order_release);
        DeleteResult dr = app->api_.deleteCloudPokemon(deletion.first, deletion.second, app->session_.accessToken);
        if (!dr.success) {
            result.message = "Delete failed: " + dr.message;
            app->commitResult_ = result;
            app->commitState_.store(CommitState::Completed, std::memory_order_release);
            return;
        }
        ++result.deletes;
        advanceProgress();
    }

    result.success = true;
    app->commitProgress_.store(100, std::memory_order_release);
    result.message = "Uploaded " + std::to_string(result.uploads)
                     + ", removed " + std::to_string(result.deletes)
                     + ", saved locally.";
    app->commitResult_ = result;
    app->commitState_.store(CommitState::Completed, std::memory_order_release);
}

void App::pollCommit() {
    if (commitState_.load(std::memory_order_acquire) != CommitState::Completed) {
        return;
    }
    threadJoin(commitThread_, U64_MAX);
    threadFree(commitThread_);
    commitThread_ = nullptr;
    commitState_.store(CommitState::Idle, std::memory_order_release);
    if (commitResult_.success) {
        status_ = commitResult_.message;
        localBaselines_.clear();
        localDrafts_.clear();
        cloudBoxes_.clear();
        loadLocalBox();
        refreshCloudBox();
    } else {
        Logger::instance().warning("Commit failed: " + commitResult_.message);
        const std::string failure = commitResult_.message;
        discardPendingChanges();
        status_ = failure + " Changes reloaded.";
        errorDialogPokemon_ = commitResult_.problemPokemon.empty()
            ? "Transfer failed"
            : commitResult_.problemPokemon;
        errorDialogLocation_ = commitResult_.problemLocation;
        errorDialogMessage_ = commitResult_.problemReason.empty()
            ? failure
            : commitResult_.problemReason;
        errorDialogVisible_ = true;
    }
}

void App::render() {
    C2D_TextBufClear(textBuffer_);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    const float slider = osGet3DSliderState();
    renderTop(topLeft_, -slider * 2.5F);
    renderTop(topRight_, slider * 2.5F);
    renderBottom();
    C3D_FrameEnd(0);
}

void App::renderLoadingTop(float eyeOffset) {
    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float angle = static_cast<float>(seconds) * 4.5F;
    drawCentered("ReBank", 200.0F + eyeOffset, 30.0F, 1.05F, Ink);
    for (int index = 0; index < 10; ++index) {
        const float phase = angle + static_cast<float>(index) * 0.6283185F;
        const float x = 200.0F + eyeOffset + std::cos(phase) * 30.0F;
        const float y = 105.0F + std::sin(phase) * 30.0F;
        const std::uint8_t alpha = static_cast<std::uint8_t>(70 + index * 18);
        C2D_DrawCircleSolid(x, y, 0.3F, 4.5F, C2D_Color32(31, 145, 94, alpha));
    }

    std::string_view message = "Bitte warten...";
    if (updateState_.load(std::memory_order_acquire) == UpdateState::Running) {
        message = "Pruefe Updates...";
    } else if (authState_.load(std::memory_order_acquire) == AuthState::Running) {
        message = "Anmelden...";
    } else {
        switch (loadingPhase_.load(std::memory_order_acquire)) {
            case LoadingPhase::SearchingGames: message = "Suche Spielstaende..."; break;
            case LoadingPhase::ReadingIcons: message = "Lade Spielbilder..."; break;
            case LoadingPhase::ReadingSave: message = "Lade Spielstand..."; break;
            case LoadingPhase::SearchingPokemon: message = "Suche Pokemon..."; break;
            case LoadingPhase::LoadingBank: message = "Lade Bank-Daten..."; break;
            default: break;
        }
    }
    drawCentered(message, 200.0F + eyeOffset, 164.0F, 0.68F, Ink);
}

void App::renderLoadingBottom() {
    std::string_view detail = "Initialisiere...";
    if (updateState_.load(std::memory_order_acquire) == UpdateState::Running) {
        detail = "Lade und verifiziere neue Versionen sicher...";
    } else if (authState_.load(std::memory_order_acquire) == AuthState::Running) {
        detail = "Pruefe Server und Sitzung...";
    } else {
        switch (loadingPhase_.load(std::memory_order_acquire)) {
            case LoadingPhase::SearchingGames:
                detail = "Pruefe Cartridge und installierte Spiele...";
                break;
            case LoadingPhase::ReadingIcons: detail = "Lese originale Spielbilder..."; break;
            case LoadingPhase::ReadingSave: detail = "Oeffne lokalen Spielstand..."; break;
            case LoadingPhase::SearchingPokemon: detail = "Lese Box und Pokemon..."; break;
            case LoadingPhase::LoadingBank: detail = "Verbinde mit deiner Bank..."; break;
            default: break;
        }
    }
    drawCentered(detail, 160.0F, 76.0F, 0.50F, Ink);

    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float cycle = std::fmod(static_cast<float>(seconds) * 0.65F, 1.0F);
    C2D_DrawRectSolid(36.0F, 122.0F, 0.2F, 248.0F, 8.0F, C2D_Color32(205, 220, 211, 255));
    C2D_DrawRectSolid(36.0F + cycle * 188.0F, 122.0F, 0.3F, 60.0F, 8.0F, Brand);
    for (int index = 0; index < 3; ++index) {
        const float pulse = 0.5F + 0.5F * std::sin(
            static_cast<float>(seconds) * 5.0F + static_cast<float>(index) * 1.4F);
        C2D_DrawCircleSolid(140.0F + index * 20.0F, 172.0F, 0.3F,
                            3.0F + pulse * 2.0F, Accent);
    }
}

void App::renderTop(C3D_RenderTarget* target, float eyeOffset) {
    C2D_TargetClear(target, Background);
    C2D_SceneBegin(target);
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 400.0F, 240.0F, Background);

    if (isLoading()) {
        renderLoadingTop(eyeOffset);
        return;
    }

    if (screen_ == Screen::Storage) {
        renderStorageTop(eyeOffset);
        return;
    }
    if (screen_ == Screen::GameSelect) {
        renderGameSelectTop(eyeOffset);
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
    C2D_TargetClear(bottom_, Background);
    C2D_SceneBegin(bottom_);
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 320.0F, 240.0F, Background);

    if (isLoading()) {
        renderLoadingBottom();
        return;
    }

    if (screen_ == Screen::Logs) {
        renderLogs();
        return;
    }
    if (screen_ == Screen::GameSelect) {
        renderGameSelect();
        return;
    }
    if (screen_ == Screen::Storage) {
        if (errorDialogVisible_) {
            renderErrorDialog();
        } else {
            renderStorageBottom();
        }
        return;
    }
    if (screen_ == Screen::Intro) {
        drawCentered("ReBank", 160.0F, 94.0F, 1.15F, Ink);
        drawCentered("A", 160.0F, 154.0F, 0.55F, Muted);
        return;
    }

    if (screen_ == Screen::Welcome) {
        drawCentered("ReBank", 160.0F, 30.0F, 1.05F, Ink);
        drawButton({24.0F, 86.0F, 272.0F, 46.0F}, localization_.get(TextId::Login), true);
        drawButton({24.0F, 144.0F, 272.0F, 46.0F}, localization_.get(TextId::Register), false);
        drawCentered(localization_.get(TextId::ForgotPassword), 160.0F, 207.0F, 0.52F, Brand);
        if (!status_.empty()) {
            drawCentered(status_, 160.0F, 226.0F, 0.36F, Error);
        }
        return;
    }

    drawButton(BackButton, localization_.get(TextId::Back), false);
    TextId heading = TextId::Login;
    if (screen_ == Screen::Register) {
        heading = TextId::CreateAccount;
    } else if (screen_ == Screen::ResetPassword) {
        heading = TextId::ResetPassword;
    }

    const double t = static_cast<double>(svcGetSystemTick() - authAnimationStartedAt_) / SYSCLOCK_ARM11;
    const float slide = std::max(0.0F, 20.0F - static_cast<float>(t) * 90.0F);
    const float pulse = 0.5F + 0.5F * std::sin(static_cast<float>(t) * 3.0F);
    const float accentPulse = 60.0F + pulse * 40.0F;

    for (int i = 0; i < 3; ++i) {
        const float bandY = 40.0F + i * 62.0F + std::sin(static_cast<float>(t) + i) * 2.0F;
        C2D_DrawRectSolid(0.0F, bandY, 0.02F, 320.0F, 2.0F,
                          C2D_Color32(210, 220, 240, 60));
    }

    drawCentered(localization_.get(heading), 160.0F, 18.0F, 0.80F, Ink);
    const u32 underline = C2D_Color32(70, 132, 200, static_cast<u8>(accentPulse * 3.5F));
    C2D_DrawRectSolid(112.0F, 38.0F, 0.05F, 96.0F, 2.0F, underline);

    const auto focusRing = [&](const UiRect& rect, AuthFocus focus) {
        if (authFocus_ != focus) {
            return;
        }
        const u8 alpha = static_cast<u8>(140 + pulse * 100);
        const u32 ring = C2D_Color32(70, 132, 200, alpha);
        C2D_DrawRectSolid(rect.x - 3.0F, rect.y - 3.0F, 0.08F, rect.width + 6.0F, 3.0F, ring);
        C2D_DrawRectSolid(rect.x - 3.0F, rect.y + rect.height, 0.08F, rect.width + 6.0F, 3.0F, ring);
        C2D_DrawRectSolid(rect.x - 3.0F, rect.y - 3.0F, 0.08F, 3.0F, rect.height + 6.0F, ring);
        C2D_DrawRectSolid(rect.x + rect.width, rect.y - 3.0F, 0.08F, 3.0F, rect.height + 6.0F, ring);
    };

    const bool registerForm = screen_ == Screen::Register;
    const bool resetForm = screen_ == Screen::ResetPassword;
    if (!resetForm) {
        const UiRect usernameField{24.0F + slide, registerForm ? 48.0F : 62.0F,
                                   272.0F, registerForm ? 34.0F : 42.0F};
        focusRing(usernameField, AuthFocus::Username);
        drawField(usernameField, localization_.get(TextId::Username), username_, false);
    }
    if (registerForm || resetForm) {
        const UiRect emailField{24.0F - slide, registerForm ? 88.0F : 62.0F,
                                272.0F, registerForm ? 34.0F : 42.0F};
        focusRing(emailField, AuthFocus::Email);
        drawField(emailField, localization_.get(TextId::Email), email_, false);
    }
    if (!resetForm) {
        const UiRect pwField{24.0F + slide, registerForm ? 128.0F : 114.0F,
                             272.0F, registerForm ? 34.0F : 42.0F};
        focusRing(pwField, AuthFocus::Password);
        drawField(pwField, localization_.get(TextId::Password), password_, true);
    }

    if (!resetForm) {
        const UiRect autoRect{24.0F, registerForm ? 166.0F : 162.0F, 272.0F, 26.0F};
        focusRing(autoRect, AuthFocus::AutoLogin);
        C2D_DrawRectSolid(autoRect.x, autoRect.y, 0.1F, autoRect.width, autoRect.height,
                          C2D_Color32(250, 250, 254, 220));
        const float boxX = autoRect.x + 8.0F;
        const float boxY = autoRect.y + 5.0F;
        C2D_DrawRectSolid(boxX, boxY, 0.12F, 16.0F, 16.0F,
                          autoLogin_ ? C2D_Color32(40, 176, 88, 255) : C2D_Color32(210, 214, 224, 255));
        C2D_DrawRectSolid(boxX + 2.0F, boxY + 2.0F, 0.13F, 12.0F, 12.0F,
                          autoLogin_ ? C2D_Color32(80, 200, 120, 255) : C2D_Color32(240, 244, 252, 255));
        if (autoLogin_) {
            C2D_DrawRectSolid(boxX + 4.0F, boxY + 8.0F, 0.14F, 3.0F, 4.0F, C2D_Color32(255, 255, 255, 255));
            C2D_DrawRectSolid(boxX + 5.0F, boxY + 10.0F, 0.14F, 8.0F, 3.0F, C2D_Color32(255, 255, 255, 255));
        }
        drawText("Auto-Login (Y)", boxX + 26.0F, autoRect.y + 7.0F, 0.44F, Ink);
    }

    const UiRect submitRect{SubmitButton.x, SubmitButton.y, SubmitButton.width, SubmitButton.height};
    focusRing(submitRect, AuthFocus::Submit);
    drawButton(submitRect, localization_.get(TextId::Submit), true);

    if (authState_.load(std::memory_order_acquire) == AuthState::Running) {
        for (int i = 0; i < 3; ++i) {
            const float phase = static_cast<float>(t) * 4.0F + i * 0.6F;
            const float px = 140.0F + i * 14.0F;
            const float py = 226.0F - std::abs(std::sin(phase)) * 6.0F;
            C2D_DrawCircleSolid(px, py, 0.4F, 4.0F, C2D_Color32(70, 132, 200, 255));
        }
    } else if (!status_.empty()) {
        drawCentered(status_, 160.0F, 222.0F, 0.42F, Error);
    }
}

void App::drawGameIcon(const GameProfile& profile, float centerX, float centerY, float size, float z) {
    const GameDescriptor& game = supportedGames()[profile.catalogIndex];
    const GameVisual visual = gameVisual(game.code);
    const float x = centerX - size * 0.5F;
    const float y = centerY - size * 0.5F;
    C2D_DrawRectSolid(x - 4.0F, y - 4.0F, z, size + 8.0F, size + 8.0F, Ink);
    if (profile.iconLoaded) {
        const C2D_Image image{const_cast<C3D_Tex*>(&profile.iconTexture), &profile.iconSubTexture};
        C2D_DrawImageAt(image, x, y, z + 0.01F, nullptr, size / 48.0F, size / 48.0F);
        return;
    }
    C2D_DrawRectSolid(x, y, z + 0.01F, size, size, visual.primary);
    C2D_DrawRectSolid(x + 6.0F, y + 6.0F, z + 0.02F, size - 12.0F, size - 12.0F, visual.secondary);
    C2D_DrawCircleSolid(centerX, centerY, z + 0.03F, size * 0.25F, Surface);
    C2D_DrawRectSolid(centerX - size * 0.25F, centerY - 2.0F, z + 0.04F,
                      size * 0.5F, 4.0F, Ink);
}

void App::renderGameSelectTop(float eyeOffset) {
    const std::string buildLabel = BuildConfig::label();
    const float buildSize = 0.30F;
    drawText(buildLabel, 394.0F - textWidth(textFont_, textBuffer_, buildLabel, buildSize),
             225.0F, buildSize, Muted);
    if (availableGames_.empty()) {
        drawCentered("No compatible save game", 200.0F, 92.0F, 0.78F, Error);
        drawCentered("Insert a cartridge or create a save first.", 200.0F, 130.0F, 0.48F, Muted);
        return;
    }
    const auto games = supportedGames();
    const GameProfile& profile = availableGames_[gameIndex_];
    const GameDescriptor& game = games[profile.catalogIndex];
    const double elapsed = static_cast<double>(svcGetSystemTick() - gameSelectionChangedAt_)
        / SYSCLOCK_ARM11;
    const float progress = std::min(1.0F, static_cast<float>(elapsed) * 5.0F);
    const float eased = 1.0F - (1.0F - progress) * (1.0F - progress);
    const float slide = static_cast<float>(gameSelectionDirection_) * (1.0F - eased) * 90.0F;

    for (float x = 0.0F; x < 400.0F; x += 20.0F) {
        C2D_DrawRectSolid(x, 0.0F, 0.01F, 1.0F, 240.0F, C2D_Color32(105, 180, 116, 35));
    }
    drawGameIcon(profile, 200.0F + eyeOffset + slide, 54.0F, 62.0F, 0.12F);
    if (profile.cartridge) {
        C2D_DrawRectSolid(252.0F + slide, 22.0F, 0.2F, 88.0F, 18.0F, CursorGreen);
        drawCentered("CARTRIDGE", 296.0F + slide, 26.0F, 0.34F, Surface);
    }
    C2D_DrawRectSolid(0.0F, 91.0F, 0.1F, 400.0F, 33.0F, gameVisual(game.code).primary);
    drawCentered(game.name, 200.0F + slide, 99.0F, 0.68F, Surface);

    drawText(profile.save.trainerName.empty() ? "Unknown Trainer" : profile.save.trainerName,
             28.0F, 144.0F, 0.56F, Ink);
    drawText("ID No. " + paddedTrainerId(profile.save.trainerId), 222.0F, 144.0F, 0.56F, Ink);
    const std::uint32_t hours = profile.save.playTimeMinutes / 60;
    const std::uint32_t minutes = profile.save.playTimeMinutes % 60;
    drawText("Play time: " + std::to_string(hours) + ":"
             + (minutes < 10 ? "0" : "") + std::to_string(minutes),
             28.0F, 188.0F, 0.54F, Ink);
    drawText("Pokedex: " + std::to_string(profile.save.pokedexCount),
             222.0F, 188.0F, 0.54F, Ink);
}

void App::renderGameSelect() {
    drawCentered("Choose the Pokemon title to use", 160.0F, 18.0F, 0.62F, Ink);
    drawButton(LogoutButton, "Logout", false);
    if (availableGames_.empty()) {
        drawCentered(status_, 160.0F, 106.0F, 0.44F, Error);
        return;
    }
    const auto games = supportedGames();
    const std::size_t count = availableGames_.size();
    const std::size_t previous = gameIndex_ == 0 ? count - 1 : gameIndex_ - 1;
    const std::size_t next = (gameIndex_ + 1) % count;
    if (count > 1) {
        drawGameIcon(availableGames_[previous], 56.0F, 94.0F, 42.0F, 0.1F);
        drawGameIcon(availableGames_[next], 264.0F, 94.0F, 42.0F, 0.1F);
        drawText("<", 18.0F, 86.0F, 0.72F, Muted);
        drawText(">", 294.0F, 86.0F, 0.72F, Muted);
    }
    const GameDescriptor& selected = games[availableGames_[gameIndex_].catalogIndex];
    drawGameIcon(availableGames_[gameIndex_], 160.0F, 104.0F, 82.0F, 0.2F);
    drawCentered(selected.name, 160.0F, 157.0F, 0.54F, Ink);
    drawCentered(std::to_string(gameIndex_ + 1) + " / " + std::to_string(count),
                 160.0F, 181.0F, 0.38F, Muted);
    drawCentered("A Select   L/R Browse   X Rescan", 205.0F, 211.0F, 0.31F, Muted);
}

namespace {
void drawPill(float x, float y, float w, float h, float z, u32 color) {
    const float r = h * 0.5F;
    C2D_DrawCircleSolid(x + r, y + r, z, r, color);
    C2D_DrawCircleSolid(x + w - r, y + r, z, r, color);
    C2D_DrawRectSolid(x + r, y, z, w - 2.0F * r, h, color);
}

void drawGrass(float w, float h) {
    for (float y = 0.0F; y < h; y += 4.0F) {
        const u32 c = (static_cast<int>(y / 4.0F) & 1) ? GrassMid : GrassLight;
        C2D_DrawRectSolid(0.0F, y, 0.0F, w, 4.0F, c);
    }
}

void drawPlusMark(float cx, float cy, u32 color) {
    C2D_DrawRectSolid(cx - 4.0F, cy - 0.5F, 0.12F, 8.0F, 1.5F, color);
    C2D_DrawRectSolid(cx - 0.5F, cy - 4.0F, 0.12F, 1.5F, 8.0F, color);
}

void drawDownArrow(float cx, float topY, float size, u32 color) {
    for (int i = 0; i < 6; ++i) {
        const float w = size * (1.0F - static_cast<float>(i) / 6.0F);
        C2D_DrawRectSolid(cx - w * 0.5F, topY + i * (size / 6.0F + 0.5F), 0.5F,
                          w, size / 6.0F + 1.0F, color);
    }
}
}

void App::renderStorageTop(float eyeOffset) {
    drawGrass(400.0F, 240.0F);

    drawPill(8.0F, 6.0F, 320.0F, 28.0F, 0.1F, HeaderPill);
    C2D_DrawRectSolid(330.0F, 6.0F, 0.1F, 62.0F, 28.0F, CountBlock);
    for (int i = 0; i < 4; ++i) {
        C2D_DrawRectSolid(332.0F + i * 15.0F, 8.0F, 0.11F, 3.0F, 24.0F,
                          C2D_Color32(210, 24, 24, 255));
    }
    drawText("Group: ReBank", 22.0F, 12.0F, 0.55F, HeaderInk);
    drawCentered(std::to_string(storage_.selectedCount()), 361.0F, 12.0F, 0.60F,
                 C2D_Color32(255, 255, 255, 255));

    const std::size_t boxLimit = session_.boxLimit == 0 ? 50 : session_.boxLimit;
    drawPill(60.0F, 44.0F, 280.0F, 30.0F, 0.14F, BankYellowDark);
    drawPill(62.0F, 45.0F, 276.0F, 27.0F, 0.15F, BankYellow);
    drawCentered("Bank " + std::to_string(cloudBox_ + 1), 200.0F, 51.0F, 0.58F, HeaderInk);
    if (loadState_.load(std::memory_order_acquire) == LoadState::Running
        && loadOperation_ == LoadOperation::CloudBox) {
        const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
        const float pulse = 0.45F + 0.55F * std::sin(static_cast<float>(seconds) * 6.0F);
        C2D_DrawCircleSolid(274.0F, 59.0F, 0.4F, 3.0F + pulse * 2.0F, HeaderInk);
        drawText("Loading", 284.0F, 53.0F, 0.34F, HeaderInk);
    }
    drawText("<", 74.0F, 50.0F, 0.7F, ArrowInk);
    drawText(">", 318.0F, 50.0F, 0.7F, ArrowInk);
    (void)boxLimit;

    constexpr float pitchX = 55.0F;
    constexpr float pitchY = 30.0F;
    constexpr float gridLeft = 400.0F * 0.5F - pitchX * 3.0F;
    constexpr float gridTop = 84.0F;
    drawPlusMark(gridLeft - 6.0F, gridTop - 4.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 6.0F, gridTop - 4.0F, HeaderInk);
    drawPlusMark(gridLeft - 6.0F, gridTop + pitchY * 5.0F + 4.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 6.0F, gridTop + pitchY * 5.0F + 4.0F, HeaderInk);

    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float cx = gridLeft + (static_cast<float>(slot % 6) + 0.5F) * pitchX;
        const float cy = gridTop + (static_cast<float>(slot / 6) + 0.5F) * pitchY;
        const float slotX = cx - pitchX * 0.5F + 2.0F;
        const float slotY = cy - pitchY * 0.5F + 2.0F;
        const float slotW = pitchX - 4.0F;
        const float slotH = pitchY - 4.0F;
        C2D_DrawRectSolid(slotX, slotY, 0.18F, slotW, slotH, C2D_Color32(255, 255, 255, 60));
        C2D_DrawRectSolid(slotX, slotY, 0.19F, slotW, 1.0F, C2D_Color32(30, 30, 30, 120));
        C2D_DrawRectSolid(slotX, slotY + slotH - 1.0F, 0.19F, slotW, 1.0F, C2D_Color32(30, 30, 30, 120));
        C2D_DrawRectSolid(slotX, slotY, 0.19F, 1.0F, slotH, C2D_Color32(30, 30, 30, 120));
        C2D_DrawRectSolid(slotX + slotW - 1.0F, slotY, 0.19F, 1.0F, slotH, C2D_Color32(30, 30, 30, 120));
        const PokemonSummary& pokemon = cloudPreview_[slot];
        if (pokemonSprites_ && pokemon.species != 0) {
            const C2D_Image image = C2D_SpriteSheetGetImage(pokemonSprites_, pokemon.species);
            const float scale = std::min(42.0F / image.subtex->width, 28.0F / image.subtex->height);
            const float w = image.subtex->width * scale;
            const float h = image.subtex->height * scale;
            const float spriteX = cx - w * 0.5F + eyeOffset * 0.2F;
            const float spriteY = cy - h * 0.5F;
            const std::uint8_t saveGen = saveAdapter_.gameGeneration();
            const std::uint8_t monFormat = pokemon.format != 0
                ? pokemon.format
                : pokemonFormatFromCode(pokemon.gameCode);
            bool incompatible = false;
            if (saveGen != 0 && monFormat != 0 && monFormat != saveGen) {
                if (monFormat < saveGen) {
                    incompatible = false;
                } else {
                    const PokemonPayload* payload = nullptr;
                    if (!pendingUploadPayloads_[slot].data.empty()) {
                        payload = &pendingUploadPayloads_[slot];
                    } else if (!cachedCloudPayloads_[slot].data.empty()) {
                        payload = &cachedCloudPayloads_[slot];
                    }
                    incompatible = payload
                        ? !saveAdapter_.canImportPokemon(payload->format, payload->data)
                        : true;
                }
            }
            C2D_ImageTint tint{};
            C2D_PlainImageTint(&tint, C2D_Color32(72, 72, 72, 255), 0.82F);
            C2D_DrawImageAt(image, spriteX, spriteY, 0.3F,
                            incompatible ? &tint : nullptr, scale, scale);
        }
        if (storagePane_ == StoragePane::Cloud && slot == focusedSlot_) {
            const double t = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
            const float bounce = std::sin(static_cast<float>(t) * 6.0F) * 3.5F;
            const u32 arrowColor = hand_.active ? CursorGreen : CursorRed;
            drawDownArrow(cx, cy - 22.0F + bounce, 12.0F, arrowColor);

            if (hand_.active && hand_.summary.species != 0 && pokemonSprites_) {
                const C2D_Image image = C2D_SpriteSheetGetImage(pokemonSprites_, hand_.summary.species);
                const float scale = std::min(42.0F / image.subtex->width,
                                             28.0F / image.subtex->height);
                const float w = image.subtex->width * scale;
                const float h = image.subtex->height * scale;
                C2D_DrawImageAt(image,
                                cx - w * 0.5F,
                                cy - h * 0.5F,
                                0.6F, nullptr, scale, scale);
            }
        }
    }

    if (uploadState_.load(std::memory_order_acquire) == UploadState::Running) {
        C2D_DrawRectSolid(0.0F, 0.0F, 0.85F, 400.0F, 240.0F, C2D_Color32(0, 0, 0, 150));
        drawCentered("PKHeX Legitimacy Check", 200.0F, 44.0F, 0.72F,
                     C2D_Color32(255, 255, 255, 255));
        if (!uploadQueue_.empty() && pokemonSprites_) {
            const double seconds = static_cast<double>(svcGetSystemTick() - uploadStartedAt_)
                                   / SYSCLOCK_ARM11;
            const float phase = std::fmod(static_cast<float>(seconds) * 0.55F, 1.0F);
            const std::size_t index = static_cast<std::size_t>(seconds * 2.0)
                                      % uploadQueue_.size();
            const auto& pokemon = uploadQueue_[index];
            const C2D_Image image = C2D_SpriteSheetGetImage(pokemonSprites_, pokemon.species);
            const float scale = std::min(60.0F / image.subtex->width,
                                         48.0F / image.subtex->height);
            const float jump = std::abs(std::sin(phase * 4.0F * 3.14159265F)) * 14.0F;
            const float x = 40.0F + phase * 300.0F;
            C2D_DrawEllipseSolid(x - 6.0F, 158.0F, 0.88F, 40.0F, 6.0F,
                                 C2D_Color32(0, 0, 0, 90));
            C2D_DrawImageAt(image, x, 118.0F - jump, 0.9F, nullptr, scale, scale);
        }
        const double elapsed = static_cast<double>(svcGetSystemTick() - uploadStartedAt_)
                               / SYSCLOCK_ARM11;
        const float progress = std::min(0.92F, 0.08F + static_cast<float>(elapsed) * 0.045F);
        C2D_DrawRectSolid(60.0F, 200.0F, 0.9F, 280.0F, 10.0F,
                          C2D_Color32(40, 40, 40, 220));
        C2D_DrawRectSolid(60.0F, 200.0F, 0.92F, 280.0F * progress, 10.0F, CursorGreen);
    }
}

void App::renderStorageBottom() {
    drawGrass(320.0F, 240.0F);

    C2D_DrawRectSolid(0.0F, 0.0F, 0.05F, 320.0F, 20.0F, C2D_Color32(215, 232, 224, 235));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.1F, 7.0F, C2D_Color32(210, 40, 40, 255));
    C2D_DrawRectSolid(7.0F, 9.0F, 0.15F, 14.0F, 2.0F, C2D_Color32(30, 30, 30, 255));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.2F, 2.5F, C2D_Color32(240, 240, 240, 255));
    drawText(hand_.active ? "HOLDING" : "READY",
             30.0F, 6.0F, 0.34F, hand_.active ? CursorGreen : HeaderInk);
    if (hasPendingChanges()) {
        drawText("PENDING", 90.0F, 6.0F, 0.34F, CursorGreen);
    }
    C2D_DrawRectSolid(252.0F, 2.0F, 0.1F, 60.0F, 16.0F, C2D_Color32(58, 58, 58, 255));
    drawCentered("START", 282.0F, 5.0F, 0.4F, C2D_Color32(240, 240, 240, 255));

    drawPill(6.0F, 26.0F, 200.0F, 26.0F, 0.14F, BankYellowDark);
    drawPill(8.0F, 27.0F, 196.0F, 23.0F, 0.15F, BankYellow);
    const std::string boxLabel = localBoxName_.empty()
        ? "BOX " + std::to_string(localBox_ + 1)
        : localBoxName_;
    drawCentered(boxLabel, 106.0F, 32.0F, 0.55F, HeaderInk);
    drawText("<", 20.0F, 32.0F, 0.6F, ArrowInk);
    drawText(">", 186.0F, 32.0F, 0.6F, ArrowInk);

    constexpr float pitchX = 32.0F;
    constexpr float pitchY = 25.0F;
    constexpr float gridLeft = 8.0F;
    constexpr float gridTop = 60.0F;
    drawPlusMark(gridLeft - 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft - 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);

    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float cx = gridLeft + (static_cast<float>(slot % 6) + 0.5F) * pitchX;
        const float cy = gridTop + (static_cast<float>(slot / 6) + 0.5F) * pitchY;
        const PokemonSummary& pokemon = storage_.pokemon(slot);
        const bool isSelected = storage_.selected(slot);
        if (pokemonSprites_ && pokemon.species != 0) {
            const C2D_Image image = C2D_SpriteSheetGetImage(pokemonSprites_, pokemon.species);
            const float scale = std::min(28.0F / image.subtex->width,
                                         22.0F / image.subtex->height);
            const float w = image.subtex->width * scale;
            const float h = image.subtex->height * scale;
            C2D_DrawImageAt(image, cx - w * 0.5F, cy - h * 0.5F,
                            0.3F, nullptr, scale, scale);
            if (isSelected) {
                const float half = 13.0F;
                C2D_DrawRectSolid(cx - half, cy - half, 0.35F, half * 2.0F, half * 2.0F,
                                  C2D_Color32(255, 255, 255, 160));
            }
        }
        if (storagePane_ == StoragePane::Local && slot == focusedSlot_) {
            const double t = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
            const float bounce = std::sin(static_cast<float>(t) * 6.0F) * 3.0F;
            const u32 arrowColor = hand_.active ? CursorGreen : CursorRed;
            drawDownArrow(cx, cy - 20.0F + bounce, 10.0F, arrowColor);

            if (hand_.active && hand_.summary.species != 0 && pokemonSprites_) {
                const C2D_Image image = C2D_SpriteSheetGetImage(pokemonSprites_, hand_.summary.species);
                const float scale = std::min(22.0F / image.subtex->width,
                                             18.0F / image.subtex->height);
                const float w = image.subtex->width * scale;
                const float h = image.subtex->height * scale;
                C2D_DrawImageAt(image,
                                cx - w * 0.5F,
                                cy - h * 0.5F,
                                0.6F, nullptr, scale, scale);
            }
        }
    }

    constexpr float sidebarX = 202.0F;
    const PokemonSummary& focused = storagePane_ == StoragePane::Cloud
        ? cloudPreview_[focusedSlot_]
        : storage_.pokemon(focusedSlot_);
    if (focused.species != 0) {
        drawText(focused.nickname, sidebarX + 4.0F, 26.0F, 0.58F, HeaderInk);
        drawText("Lv. " + std::to_string(focused.level), sidebarX + 12.0F, 48.0F, 0.5F, SidebarInk);
        for (int i = 0; i < 6; ++i) {
            C2D_DrawCircleSolid(sidebarX + 8.0F + i * 14.0F, 74.0F, 0.2F, 2.5F, SidebarInk);
        }
        drawText(focused.nickname, sidebarX + 4.0F, 86.0F, 0.44F, SidebarInk);
        drawPill(sidebarX + 4.0F, 106.0F, 108.0F, 18.0F, 0.22F, TypeElectric);
        drawCentered("TYPE", sidebarX + 58.0F, 110.0F, 0.42F, C2D_Color32(255, 255, 255, 255));
        drawText("DEX NO.", sidebarX + 4.0F, 132.0F, 0.44F, SidebarInk);
        drawText(std::to_string(focused.species), sidebarX + 32.0F, 152.0F, 0.62F, SidebarInk);
        drawText(focused.trainerName, sidebarX + 4.0F, 180.0F, 0.44F, SidebarInk);
        drawText("ID. " + std::to_string(saveSummary_.trainerId % 100000),
                 sidebarX + 4.0F, 198.0F, 0.44F, SidebarInk);
    } else {
        drawText(storagePane_ == StoragePane::Cloud ? "Empty cloud slot" : "Empty slot",
                 sidebarX + 4.0F, 110.0F, 0.42F, HeaderInk);
    }

    if (uploadState_.load(std::memory_order_acquire) == UploadState::Running
        || downloadState_.load(std::memory_order_acquire) == DownloadState::Running
        || commitState_.load(std::memory_order_acquire) == CommitState::Running) {
        const CommitState cs = commitState_.load(std::memory_order_acquire);
        const bool isCommit = cs == CommitState::Running;
        const bool isUpload = uploadState_.load(std::memory_order_acquire) == UploadState::Running;
        C2D_DrawRectSolid(0.0F, 205.0F, 0.7F, 200.0F, 35.0F, C2D_Color32(0, 0, 0, 170));
        std::string label = "Syncing";
        int progress = 0;
        if (isCommit) {
            const int phase = commitPhase_.load(std::memory_order_acquire);
            label = phase == 3 ? "Writing save..."
                  : phase == 2 ? "Uploading cloud..."
                  : phase == 1 ? "Removing cloud..."
                  : "Preparing...";
            progress = commitProgress_.load(std::memory_order_acquire);
        } else if (isUpload) {
            label = "PKHeX check";
        } else {
            label = "Downloading";
        }
        label += " " + std::to_string(progress) + "%";
        drawCentered(label,
                     100.0F, 210.0F, 0.42F, C2D_Color32(255, 255, 255, 255));
        C2D_DrawRectSolid(10.0F, 228.0F, 0.9F, 180.0F, 6.0F, C2D_Color32(50, 50, 50, 220));
        const float fill = 180.0F * static_cast<float>(progress) / 100.0F;
        C2D_DrawRectSolid(10.0F, 228.0F, 0.92F, fill, 6.0F, CursorGreen);
    } else {
        const bool cloudPane = storagePane_ == StoragePane::Cloud;
        const bool held = hand_.active;
        const bool pending = hasPendingChanges();

        const u32 aCircle = held ? CursorGreen : C2D_Color32(200, 40, 40, 255);
        C2D_DrawCircleSolid(20.0F, 220.0F, 0.2F, 10.0F, aCircle);
        drawCentered("A", 20.0F, 214.0F, 0.55F, C2D_Color32(255, 255, 255, 255));
        drawText(held ? "DROP" : "PICK", 36.0F, 214.0F, 0.5F, HeaderInk);

        const u32 bCircle = held ? C2D_Color32(120, 60, 160, 255)
                                 : C2D_Color32(90, 90, 90, 255);
        C2D_DrawCircleSolid(96.0F, 220.0F, 0.2F, 10.0F, bCircle);
        drawCentered("B", 96.0F, 214.0F, 0.55F, C2D_Color32(255, 255, 255, 255));
        drawText(held ? "RETURN" : "BACK", 112.0F, 214.0F, 0.5F, HeaderInk);

        const u32 selCircle = pending ? CursorGreen : C2D_Color32(90, 90, 90, 255);
        C2D_DrawCircleSolid(190.0F, 220.0F, 0.2F, 10.0F, selCircle);
        drawCentered("SEL", 190.0F, 216.0F, 0.36F, C2D_Color32(255, 255, 255, 255));
        drawText(pending ? "COMMIT" : (cloudPane ? "LOCAL" : "CLOUD"),
                 205.0F, 214.0F, 0.5F, pending ? CursorGreen : HeaderInk);
        if (!status_.empty()) {
            constexpr std::size_t lineLength = 48;
            drawText(status_.substr(0, lineLength), 6.0F, 188.0F, 0.29F, HeaderInk);
            if (status_.size() > lineLength) {
                drawText(status_.substr(lineLength, lineLength), 6.0F, 198.0F, 0.29F, HeaderInk);
            }
        }
    }
}

void App::renderErrorDialog() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.35F, 320.0F, 240.0F, C2D_Color32(12, 24, 19, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.40F, 284.0F, 208.0F, C2D_Color32(250, 247, 238, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.45F, 7.0F, 208.0F, Error);
    C2D_DrawRectSolid(25.0F, 20.0F, 0.45F, 277.0F, 36.0F, C2D_Color32(255, 225, 214, 255));

    drawText("TRANSFER BLOCKED", 36.0F, 30.0F, 0.52F, Error);
    drawText(errorDialogPokemon_.empty() ? "Unknown Pokemon" : errorDialogPokemon_,
             36.0F, 68.0F, 0.62F, HeaderInk);
    if (!errorDialogLocation_.empty()) {
        drawText(errorDialogLocation_, 36.0F, 90.0F, 0.38F, Muted);
    }

    std::vector<std::string> lines;
    std::string remaining = errorDialogMessage_.empty()
        ? "The transfer was rejected. Check rebank.log for details."
        : errorDialogMessage_;
    constexpr std::size_t MaxLineLength = 42;
    while (!remaining.empty() && lines.size() < 4) {
        if (remaining.size() <= MaxLineLength) {
            lines.push_back(remaining);
            break;
        }
        std::size_t split = remaining.rfind(' ', MaxLineLength);
        if (split == std::string::npos || split == 0) {
            split = MaxLineLength;
        }
        lines.push_back(remaining.substr(0, split));
        remaining.erase(0, split);
        while (!remaining.empty() && remaining.front() == ' ') {
            remaining.erase(remaining.begin());
        }
    }
    if (!remaining.empty() && !lines.empty()) {
        std::string& last = lines.back();
        if (last.size() > MaxLineLength - 3) {
            last.resize(MaxLineLength - 3);
        }
        last += "...";
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        drawText(lines[index], 36.0F, 116.0F + static_cast<float>(index) * 15.0F,
                 0.36F, HeaderInk);
    }

    const UiRect okButton{92.0F, 190.0F, 136.0F, 34.0F};
    C2D_DrawRectSolid(okButton.x, okButton.y, 0.46F, okButton.width, okButton.height, Brand);
    C2D_DrawRectSolid(okButton.x + 2.0F, okButton.y + 2.0F, 0.47F,
                      okButton.width - 4.0F, okButton.height - 4.0F, CursorGreen);
    drawCentered("OK", 160.0F, 199.0F, 0.52F, C2D_Color32(255, 255, 255, 255));
    drawCentered("A / B", 268.0F, 201.0F, 0.30F, Muted);
}

void App::loadLocalBox() {
    localBoxName_ = saveAdapter_.boxName(localBox_);
    storagePane_ = StoragePane::Local;
    transferArmed_ = false;
    focusedSlot_ = 0;

    if (localBaselines_.find(localBox_) == localBaselines_.end()) {
        LocalBoxDraft baseline;
        baseline.summaries = saveAdapter_.readBox(localBox_);
        for (std::size_t slot = 0; slot < 30; ++slot) {
            baseline.payloads[slot] = saveAdapter_.readPokemon(localBox_, slot);
        }
        localBaselines_[localBox_] = std::move(baseline);
    }

    auto draftIt = localDrafts_.find(localBox_);
    if (draftIt != localDrafts_.end()) {
        storage_.load(draftIt->second.summaries);
        localPayloads_ = draftIt->second.payloads;
    } else {
        const LocalBoxDraft& baseline = localBaselines_[localBox_];
        storage_.load(baseline.summaries);
        localPayloads_ = baseline.payloads;
    }
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (storage_.pokemon(slot).species != 0) {
            focusedSlot_ = slot;
            break;
        }
    }
    Logger::instance().info("Local box loaded: " + std::to_string(localBox_ + 1));
}

void App::persistLocalDraft() {
    auto baselineIt = localBaselines_.find(localBox_);
    if (baselineIt == localBaselines_.end()) {
        return;
    }
    LocalBoxDraft current;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        current.summaries[slot] = storage_.pokemon(slot);
        current.payloads[slot] = localPayloads_[slot];
    }
    bool differs = false;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (current.summaries[slot].species != baselineIt->second.summaries[slot].species
            || current.summaries[slot].nickname != baselineIt->second.summaries[slot].nickname
            || current.payloads[slot].data != baselineIt->second.payloads[slot].data) {
            differs = true;
            break;
        }
    }
    if (differs) {
        localDrafts_[localBox_] = std::move(current);
    } else {
        localDrafts_.erase(localBox_);
    }
}

void App::refreshCloudBox() {
    const auto boxKey = static_cast<std::uint16_t>(cloudBox_);
    if (session_.accessToken.empty()) {
        cloudPreview_.fill({});
        pendingUploadPayloads_ = {};
        return;
    }

    auto it = cloudBoxes_.find(boxKey);
    if (it == cloudBoxes_.end()) {
        cloudPreview_.fill({});
        pendingUploadPayloads_ = {};
        cachedCloudPayloads_ = {};
        status_.clear();
        loadingCloudBox_ = boxKey;
        beginLoad(LoadOperation::CloudBox);
        return;
    }
    cloudPreview_ = it->second.summaries;
    pendingUploadPayloads_ = it->second.pending;
    cachedCloudPayloads_ = {};
    std::size_t occupied = 0;
    for (const auto& mon : cloudPreview_) {
        if (mon.species != 0) {
            ++occupied;
        }
    }
    Logger::instance().info("Cloud box " + std::to_string(cloudBox_ + 1)
                            + " loaded (" + std::to_string(occupied) + " occupied)");
}

void App::persistCloudDraft() {
    const auto boxKey = static_cast<std::uint16_t>(cloudBox_);
    auto& draft = cloudBoxes_[boxKey];
    draft.summaries = cloudPreview_;
    draft.pending = pendingUploadPayloads_;
}

void App::discardPendingChanges() {
    const StoragePane previousPane = storagePane_;
    hand_ = Hand{};
    localDrafts_.clear();
    localBaselines_.clear();
    cloudBoxes_.clear();
    pendingUploadPayloads_ = {};
    cachedCloudPayloads_ = {};
    loadLocalBox();
    storagePane_ = previousPane;
    refreshCloudBox();
    status_ = "Pending changes discarded.";
    Logger::instance().info("Pending storage changes discarded");
}

void App::drawPokemonSprite(
    const PokemonSummary& pokemon,
    float x,
    float y,
    float scale,
    float eyeOffset
) {
    if (!pokemonSprites_ || pokemon.species == 0) {
        return;
    }
    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float bob = std::sin(static_cast<float>(seconds) * 3.2F) * 4.0F;
    const C2D_Image image = C2D_SpriteSheetGetImage(pokemonSprites_, pokemon.species);
    C2D_DrawEllipseSolid(x - 8.0F, y + 76.0F, 0.2F, 64.0F, 12.0F, C2D_Color32(42, 94, 65, 70));
    C2D_DrawImageAt(image, x + eyeOffset, y + bob, 0.4F, nullptr, scale, scale);
}

void App::drawText(std::string_view value, float x, float y, float size, u32 color) {
    C2D_Text text;
    const PreparedText prepared = prepareText(value);
    parseText(text, textFont_, textBuffer_, prepared.value);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5F, size, size, color);
    drawMusicGlyphs(prepared, textFont_, textBuffer_, x, y, size, color);
}

void App::drawCentered(std::string_view value, float centerX, float y, float size, u32 color) {
    C2D_Text text;
    const PreparedText prepared = prepareText(value);
    parseText(text, textFont_, textBuffer_, prepared.value);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    const float x = centerX - width * 0.5F;
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5F, size, size, color);
    drawMusicGlyphs(prepared, textFont_, textBuffer_, x, y, size, color);
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

void App::renderLogs() {
    drawText("ReBank Log", 12.0F, 10.0F, 0.62F, Ink);
    drawText("SELECT", 253.0F, 13.0F, 0.38F, Brand);
    const auto& entries = Logger::instance().entries();
    const std::size_t visible = 10;
    const std::size_t first = entries.size() > visible ? entries.size() - visible : 0;
    for (std::size_t index = first; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        u32 color = Ink;
        if (entry.level == LogLevel::Warning) {
            color = Accent;
        } else if (entry.level == LogLevel::Error) {
            color = Error;
        }
        const std::string line = entry.message.substr(0, 52);
        drawText(line, 12.0F, 39.0F + static_cast<float>(index - first) * 18.0F, 0.38F, color);
    }
}