#pragma once

#include <3ds.h>

#include <array>
#include <cstddef>
#include <string_view>

enum class TextId : std::size_t {
    Tagline,
    Login,
    Register,
    Username,
    Email,
    Password,
    ForgotPassword,
    Submit,
    Back,
    CreateAccount,
    ResetPassword,
    LoadingWait,
    LoadingCheckingUpdates,
    LoadingSigningIn,
    LoadingSearchingGames,
    LoadingReadingIcons,
    LoadingReadingSave,
    LoadingSearchingPokemon,
    LoadingBankData,
    LoadingDetailInitializing,
    LoadingDetailCheckingUpdates,
    LoadingDetailSigningIn,
    LoadingDetailSearchingGames,
    LoadingDetailReadingIcons,
    LoadingDetailReadingSave,
    LoadingDetailSearchingPokemon,
    LoadingDetailLoadingBank,
    LoadingProgressLabel,
    LoadingAutoLoginDetected,
    LoadingDetailAutoLoginDetected,
    LoadingWelcomeBackPrefix,
    TrashCan,
    TrashConfirmMessage,
    Yes,
    No,
    Count
};

class Localization {
public:
    Localization();
    std::string_view get(TextId id) const;

private:
    using Translation = std::array<std::string_view, static_cast<std::size_t>(TextId::Count)>;
    const Translation* translation_;
};