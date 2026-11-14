#include "i18n/Translation.hpp"

namespace {
constexpr Translation build() {
    Translation text{};
    text[at(TextId::Tagline)] = "Tutti i Pokemon. Tutti i giochi. Una banca.";
    text[at(TextId::Login)] = "Accedi";
    text[at(TextId::Register)] = "Registrati";
    text[at(TextId::Username)] = "Username";
    text[at(TextId::Email)] = "E-mail";
    text[at(TextId::Password)] = "Password";
    text[at(TextId::ForgotPassword)] = "Password dimenticata?";
    text[at(TextId::Submit)] = "Continua";
    text[at(TextId::Back)] = "Indietro";
    text[at(TextId::CreateAccount)] = "Crea account";
    text[at(TextId::ResetPassword)] = "Reimposta password";
    text[at(TextId::LoadingWait)] = "Attendere prego...";
    text[at(TextId::LoadingCheckingUpdates)] = "Controllo aggiornamenti...";
    text[at(TextId::LoadingSigningIn)] = "Accesso in corso...";
    text[at(TextId::LoadingSearchingGames)] = "Ricerca salvataggi...";
    text[at(TextId::LoadingReadingIcons)] = "Caricamento icone...";
    text[at(TextId::LoadingReadingSave)] = "Caricamento salvataggio...";
    text[at(TextId::LoadingSearchingPokemon)] = "Ricerca Pokemon...";
    text[at(TextId::LoadingBankData)] = "Caricamento dati banca...";
    text[at(TextId::LoadingDetailInitializing)] = "Inizializzazione...";
    text[at(TextId::LoadingDetailCheckingUpdates)] = "Download e verifica sicura delle nuove versioni...";
    text[at(TextId::LoadingDetailSigningIn)] = "Verifica server e sessione...";
    text[at(TextId::LoadingDetailSearchingGames)] = "Verifica cartuccia e giochi installati...";
    text[at(TextId::LoadingDetailReadingIcons)] = "Lettura icone originali...";
    text[at(TextId::LoadingDetailReadingSave)] = "Apertura salvataggio locale...";
    text[at(TextId::LoadingDetailSearchingPokemon)] = "Lettura box e Pokemon...";
    text[at(TextId::LoadingDetailLoadingBank)] = "Connessione alla tua banca...";
    text[at(TextId::LoadingProgressLabel)] = "Progresso";
    text[at(TextId::LoadingAutoLoginDetected)] = "Login automatico rilevato...";
    text[at(TextId::LoadingDetailAutoLoginDetected)] = "Accesso automatico in corso...";
    text[at(TextId::LoadingWelcomeBackPrefix)] = "Bentornato, ";
    text[at(TextId::TrashCan)] = "Cestino";
    text[at(TextId::TrashConfirmMessage)] = "Eliminare questi Pokemon?";
    text[at(TextId::Yes)] = "Si";
    text[at(TextId::No)] = "No";
    text[at(TextId::StatusFindingSaveGames)] = "Ricerca salvataggi...";
    text[at(TextId::StatusNoCompatibleSaveGame)] = "Nessun salvataggio compatibile trovato.";
    text[at(TextId::StatusCheckingCartridgeSlot)] = "Controllo slot cartuccia...";
    text[at(TextId::StatusReadingSave)] = "Lettura salvataggio...";
    text[at(TextId::NoCompatibleSaveGameTitle)] = "Nessun salvataggio compatibile";
    text[at(TextId::InsertCartridgeOrCreateSave)] = "Inserisci una cartuccia o crea prima un salvataggio.";
    text[at(TextId::NoCartridgeInserted)] = "Nessuna cartuccia inserita";
    text[at(TextId::InsertCartridgeToLoad)] = "Inserisci una cartuccia di gioco per caricarla qui.";
    text[at(TextId::UnknownTrainer)] = "Allenatore sconosciuto";
    text[at(TextId::IdNoPrefix)] = "N. Allenatore ";
    text[at(TextId::PlayTimePrefix)] = "Tempo di gioco: ";
    text[at(TextId::PokedexPrefix)] = "Pokedex: ";
    text[at(TextId::CartridgeRemovedTitle)] = "Cartuccia rimossa";
    text[at(TextId::CartridgeRemovedMessage)] = "La cartuccia di gioco e stata rimossa. Ritorno alla selezione dei giochi.";
    text[at(TextId::PrivacyConsentPrefix)] = "Accetto l'";
    text[at(TextId::PrivacyConsentLink)] = "informativa privacy";
    text[at(TextId::PrivacyRequiredTitle)] = "Informativa privacy";
    text[at(TextId::PrivacyRequiredMessage)] = "Leggi e accetta l'informativa privacy per creare un account.";
    text[at(TextId::PrivacyLoading)] = "Caricamento dell'informativa privacy...";
    text[at(TextId::PrivacyUnavailableTitle)] = "Informativa privacy non disponibile";
    text[at(TextId::PrivacyUnavailableMessage)] = "Impossibile caricare l'informativa privacy. Controlla la connessione e riprova.";
    text[at(TextId::PolicyAccept)] = "Accetta (A)";
    text[at(TextId::PolicyAgeConfirmation)] = "Accettando confermi anche di avere almeno 16 anni o il consenso dei tuoi genitori.";
    text[at(TextId::PolicyDecline)] = "Rifiuta (B)";
    return text;
}

constexpr Translation Text = build();
}

const Translation& Translations::italian() {
    return Text;
}
