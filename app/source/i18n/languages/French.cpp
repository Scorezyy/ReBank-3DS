#include "i18n/Translation.hpp"

namespace {
constexpr Translation build() {
    Translation text{};
    text[at(TextId::Tagline)] = "Tous les Pokemon. Tous les jeux. Une banque.";
    text[at(TextId::Login)] = "Connexion";
    text[at(TextId::Register)] = "Inscription";
    text[at(TextId::Username)] = "Utilisateur";
    text[at(TextId::Email)] = "E-mail";
    text[at(TextId::Password)] = "Mot de passe";
    text[at(TextId::ForgotPassword)] = "Mot de passe oublie ?";
    text[at(TextId::Submit)] = "Continuer";
    text[at(TextId::Back)] = "Retour";
    text[at(TextId::CreateAccount)] = "Creer un compte";
    text[at(TextId::ResetPassword)] = "Reinitialiser";
    text[at(TextId::LoadingWait)] = "Veuillez patienter...";
    text[at(TextId::LoadingCheckingUpdates)] = "Verification des mises a jour...";
    text[at(TextId::LoadingSigningIn)] = "Connexion en cours...";
    text[at(TextId::LoadingSearchingGames)] = "Recherche des sauvegardes...";
    text[at(TextId::LoadingReadingIcons)] = "Chargement des icones...";
    text[at(TextId::LoadingReadingSave)] = "Chargement de la sauvegarde...";
    text[at(TextId::LoadingSearchingPokemon)] = "Recherche de Pokemon...";
    text[at(TextId::LoadingBankData)] = "Chargement de la banque...";
    text[at(TextId::LoadingDetailInitializing)] = "Initialisation...";
    text[at(TextId::LoadingDetailCheckingUpdates)] = "Telechargement et verification securisee des nouvelles versions...";
    text[at(TextId::LoadingDetailSigningIn)] = "Verification du serveur et de la session...";
    text[at(TextId::LoadingDetailSearchingGames)] = "Verification de la cartouche et des jeux installes...";
    text[at(TextId::LoadingDetailReadingIcons)] = "Lecture des icones originales...";
    text[at(TextId::LoadingDetailReadingSave)] = "Ouverture de la sauvegarde locale...";
    text[at(TextId::LoadingDetailSearchingPokemon)] = "Lecture de la boite et des Pokemon...";
    text[at(TextId::LoadingDetailLoadingBank)] = "Connexion a votre banque...";
    text[at(TextId::LoadingProgressLabel)] = "Progression";
    text[at(TextId::LoadingAutoLoginDetected)] = "Connexion automatique detectee...";
    text[at(TextId::LoadingDetailAutoLoginDetected)] = "Connexion automatique en cours...";
    text[at(TextId::LoadingWelcomeBackPrefix)] = "Content de te revoir, ";
    text[at(TextId::TrashCan)] = "Corbeille";
    text[at(TextId::TrashConfirmMessage)] = "Supprimer ces Pokemon ?";
    text[at(TextId::Yes)] = "Oui";
    text[at(TextId::No)] = "Non";
    text[at(TextId::StatusFindingSaveGames)] = "Recherche des sauvegardes...";
    text[at(TextId::StatusNoCompatibleSaveGame)] = "Aucune sauvegarde compatible trouvee.";
    text[at(TextId::StatusCheckingCartridgeSlot)] = "Verification du lecteur de cartouche...";
    text[at(TextId::StatusReadingSave)] = "Lecture de la sauvegarde...";
    text[at(TextId::NoCompatibleSaveGameTitle)] = "Aucune sauvegarde compatible";
    text[at(TextId::InsertCartridgeOrCreateSave)] = "Insere une cartouche ou cree d'abord une sauvegarde.";
    text[at(TextId::NoCartridgeInserted)] = "Aucune cartouche inseree";
    text[at(TextId::InsertCartridgeToLoad)] = "Insere une cartouche de jeu pour la charger ici.";
    text[at(TextId::UnknownTrainer)] = "Dresseur inconnu";
    text[at(TextId::IdNoPrefix)] = "N Dresseur ";
    text[at(TextId::PlayTimePrefix)] = "Temps de jeu : ";
    text[at(TextId::PokedexPrefix)] = "Pokedex : ";
    text[at(TextId::CartridgeRemovedTitle)] = "Cartouche retiree";
    text[at(TextId::CartridgeRemovedMessage)] = "La cartouche de jeu a ete retiree. Retour a la selection des jeux.";
    text[at(TextId::PrivacyConsentPrefix)] = "J'accepte la ";
    text[at(TextId::PrivacyConsentLink)] = "politique de confidentialite";
    text[at(TextId::PrivacyRequiredTitle)] = "Politique de confidentialite";
    text[at(TextId::PrivacyRequiredMessage)] = "Lis et accepte la politique de confidentialite pour creer un compte.";
    text[at(TextId::PrivacyLoading)] = "Chargement de la politique de confidentialite...";
    text[at(TextId::PrivacyUnavailableTitle)] = "Politique de confidentialite indisponible";
    text[at(TextId::PrivacyUnavailableMessage)] = "La politique de confidentialite n'a pas pu etre chargee. Verifie ta connexion et reessaie.";
    text[at(TextId::PolicyAccept)] = "Accepter (A)";
    text[at(TextId::PolicyAgeConfirmation)] = "En acceptant, tu confirmes aussi avoir au moins 16 ans ou l'accord de tes parents.";
    text[at(TextId::PolicyDecline)] = "Refuser (B)";
    return text;
}

constexpr Translation Text = build();
}

const Translation& Translations::french() {
    return Text;
}
