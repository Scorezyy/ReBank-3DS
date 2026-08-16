#pragma once

#include "ApiClient.hpp"
#include "CredentialStore.hpp"
#include "GameCatalog.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "MusicPlayer.hpp"
#include "SessionStore.hpp"
#include "StorageModel.hpp"

#include <citro2d.h>

#include <atomic>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct UiRect {
    float x;
    float y;
    float width;
    float height;

    bool contains(touchPosition point) const;
};

class App {
public:
    App();
    ~App();
    int run();

private:
    enum class Screen {
        Intro,
        Welcome,
        Login,
        Register,
        ResetPassword,
        GameSelect,
        Storage,
        Logs
    };

    enum class AuthOperation {
        Login,
        Register,
        ResetPassword,
        Refresh
    };

    enum class AuthState {
        Idle,
        Running,
        Completed
    };

    enum class LoadOperation {
        None,
        DiscoverGames,
        OpenGame,
        CloudBox
    };

    enum class LoadState {
        Idle,
        Running,
        Completed
    };

    enum class LoadingPhase {
        Idle,
        Authenticating,
        SearchingGames,
        ReadingIcons,
        ReadingSave,
        SearchingPokemon,
        LoadingBank
    };

    enum class SelectionTool {
        Single,
        Multi
    };

    enum class StoragePane {
        Local,
        Cloud
    };

    enum class UploadState {
        Idle,
        Running,
        Completed
    };

    enum class AuthFocus {
        Username,
        Email,
        Password,
        AutoLogin,
        Submit,
        Back
    };

    enum class DownloadState {
        Idle,
        Running,
        Completed
    };

    struct DownloadJob {
        std::uint16_t cloudBox = 0;
        std::uint8_t cloudSlot = 0;
        std::size_t localBox = 0;
        std::size_t localSlot = 0;
        std::uint8_t saveGeneration = 0;
    };

    enum class CommitState {
        Idle,
        Running,
        Completed
    };

    struct CommitResult {
        bool success = false;
        std::string message;
        std::string problemPokemon;
        std::string problemLocation;
        std::string problemReason;
        std::size_t uploads = 0;
        std::size_t downloads = 0;
        std::size_t deletes = 0;
    };

    struct GameProfile {
        std::size_t catalogIndex = 0;
        SaveSummary save;
        bool cartridge = false;
        C3D_Tex iconTexture{};
        Tex3DS_SubTexture iconSubTexture{};
        bool iconLoaded = false;
    };

    struct DiscoveredGame {
        std::size_t catalogIndex = 0;
        SaveSummary save;
        bool cartridge = false;
        std::unique_ptr<std::array<std::uint16_t, 48 * 48>> iconPixels;
    };

    struct OpenGameResult {
        bool success = false;
        std::string message;
        SaveSummary save;
        std::size_t localBox = 0;
        std::string localBoxName;
        std::array<PokemonSummary, 30> localPokemon{};
        std::array<PokemonPayload, 30> localPayloads{};
        BoxListResult cloudBox;
    };

    enum class HandSource {
        Local,
        Cloud
    };

    struct Hand {
        bool active = false;
        HandSource source = HandSource::Local;
        std::size_t sourceIndex = 0;
        std::uint16_t sourceCloudBox = 0;
        PokemonSummary summary;
        PokemonPayload payload;
        bool payloadKnown = false;
    };

    void update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch);
    void updateIntro();
    void updateWelcome(u32 keysDown, touchPosition touch, bool touched);
    void updateForm(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched);
    void updateGameSelect(u32 keysDown, touchPosition touch, bool touched);
    void updateStorage(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched);
    void beginAuth(AuthOperation operation);
    void pollAuth();
    static void authWorker(void* argument);
    void beginLoad(LoadOperation operation);
    void pollLoad();
    static void loadWorker(void* argument);
    bool isLoading() const;
    void beginUpload();
    void pollUpload();
    static void uploadWorker(void* argument);
    void beginDownload(std::uint16_t cloudBox, std::uint8_t cloudSlot, std::size_t localBox, std::size_t localSlot);
    void pollDownload();
    static void downloadWorker(void* argument);
    int storageDirection(u32 keysDown, u32 keysHeld, circlePosition circle);
    void selectFocusedPokemon();
    void render();
    void renderTop(C3D_RenderTarget* target, float eyeOffset);
    void renderBottom();
    void renderGameSelectTop(float eyeOffset);
    void renderGameSelect();
    void renderLoadingTop(float eyeOffset);
    void renderLoadingBottom();
    void renderStorageTop(float eyeOffset);
    void renderStorageBottom();
    void renderErrorDialog();
    void renderLogs();
    void loadLocalBox();
    void refreshCloudBox();
    void snapshotBox();
    void persistLocalDraft();
    void persistCloudDraft();
    void discardPendingChanges();
    void storagePickUp();
    void storageDrop();
    void storageReturnHand();
    void beginCommit();
    void pollCommit();
    static void commitWorker(void* argument);
    bool hasPendingChanges() const;
    void refreshGameProfiles();
    void selectGameProfile(std::size_t index, int direction);
    bool openSelectedGame();
    void logout();
    void drawGameIcon(const GameProfile& profile, float centerX, float centerY, float size, float z);
    void drawPokemonSprite(const PokemonSummary& pokemon, float x, float y, float scale, float eyeOffset);
    void drawText(std::string_view value, float x, float y, float size, u32 color);
    void drawCentered(std::string_view value, float centerX, float y, float size, u32 color);
    void drawButton(const UiRect& rect, std::string_view label, bool primary);
    void drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password);
    void requestText(std::string& destination, std::string_view hint, bool password);

    Localization localization_;
    Screen screen_;
    Screen previousScreen_;
    C3D_RenderTarget* topLeft_;
    C3D_RenderTarget* topRight_;
    C3D_RenderTarget* bottom_;
    C2D_TextBuf textBuffer_;
    C2D_Font textFont_;
    C2D_SpriteSheet pokemonSprites_;
    u64 introStartedAt_;
    std::string username_;
    std::string email_;
    std::string password_;
    std::string status_;
    ApiClient api_;
    SessionStore sessionStore_;
    AccountSession session_;
    Thread authThread_;
    std::atomic<AuthState> authState_;
    AuthOperation authOperation_;
    AuthResult authResult_;
    std::string authUsername_;
    std::string authEmail_;
    std::string authPassword_;
    Thread loadThread_;
    std::atomic<LoadState> loadState_;
    LoadOperation loadOperation_;
    std::atomic<LoadingPhase> loadingPhase_;
    u64 loadingStartedAt_;
    std::size_t loadingCatalogIndex_;
    std::uint16_t loadingCloudBox_;
    std::vector<DiscoveredGame> discoveredGames_;
    OpenGameResult openGameResult_;
    BoxListResult cloudLoadResult_;
    MusicPlayer music_;
    SaveAdapter saveAdapter_;
    SaveSummary saveSummary_;
    StorageModel storage_;
    std::size_t gameIndex_;
    std::vector<GameProfile> availableGames_;
    u64 gameSelectionChangedAt_;
    int gameSelectionDirection_;
    std::size_t localBox_;
    std::size_t cloudBox_;
    std::size_t focusedSlot_;
    std::string localBoxName_;
    std::array<PokemonSummary, 30> cloudPreview_{};
    std::array<PokemonSummary, 30> initialCloud_{};
    std::array<PokemonSummary, 30> initialLocal_{};
    std::array<PokemonPayload, 30> localPayloads_{};
    std::array<PokemonPayload, 30> initialLocalPayloads_{};
    std::array<PokemonPayload, 30> pendingUploadPayloads_{};
    std::array<PokemonPayload, 30> cachedCloudPayloads_{};
    struct LocalBoxDraft {
        std::array<PokemonSummary, 30> summaries{};
        std::array<PokemonPayload, 30> payloads{};
    };
    struct CloudBoxDraft {
        std::array<PokemonSummary, 30> summaries{};
        std::array<PokemonPayload, 30> pending{};
        std::array<PokemonSummary, 30> baseline{};
    };
    std::unordered_map<std::size_t, LocalBoxDraft> localBaselines_;
    std::unordered_map<std::size_t, LocalBoxDraft> localDrafts_;
    std::unordered_map<std::uint16_t, CloudBoxDraft> cloudBoxes_;
    Hand hand_;
    SelectionTool selectionTool_;
    StoragePane storagePane_;
    bool transferArmed_;
    Thread uploadThread_;
    std::atomic<UploadState> uploadState_;
    UploadResult uploadResult_;
    std::vector<UploadPokemon> uploadQueue_;
    u64 uploadStartedAt_;
    Thread downloadThread_;
    std::atomic<DownloadState> downloadState_;
    DownloadResult downloadResult_;
    DeleteResult deleteResult_;
    DownloadJob downloadJob_;
    u64 downloadStartedAt_;
    Thread commitThread_;
    std::atomic<CommitState> commitState_;
    CommitResult commitResult_;
    u64 commitStartedAt_;
    std::atomic<int> commitPhase_{0};
    std::atomic<int> commitProgress_{0};
    int heldDirection_;
    u64 directionRepeatAt_;
    bool errorDialogVisible_ = false;
    std::string errorDialogPokemon_;
    std::string errorDialogLocation_;
    std::string errorDialogMessage_;
    bool running_;
    CredentialStore credentials_;
    bool autoLogin_;
    AuthFocus authFocus_;
    u64 authAnimationStartedAt_;
};