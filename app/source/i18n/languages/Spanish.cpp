#include "i18n/Translation.hpp"

namespace {
constexpr Translation build() {
    Translation text{};
    text[at(TextId::Tagline)] = "Todos los Pokemon. Todos los juegos. Un banco.";
    text[at(TextId::Login)] = "Iniciar sesion";
    text[at(TextId::Register)] = "Registrarse";
    text[at(TextId::Username)] = "Usuario";
    text[at(TextId::Email)] = "Correo";
    text[at(TextId::Password)] = "Contrasena";
    text[at(TextId::ForgotPassword)] = "Olvidaste la contrasena?";
    text[at(TextId::Submit)] = "Continuar";
    text[at(TextId::Back)] = "Volver";
    text[at(TextId::CreateAccount)] = "Crear cuenta";
    text[at(TextId::ResetPassword)] = "Restablecer";
    text[at(TextId::LoadingWait)] = "Por favor espera...";
    text[at(TextId::LoadingCheckingUpdates)] = "Buscando actualizaciones...";
    text[at(TextId::LoadingSigningIn)] = "Iniciando sesion...";
    text[at(TextId::LoadingSearchingGames)] = "Buscando partidas guardadas...";
    text[at(TextId::LoadingReadingIcons)] = "Cargando iconos...";
    text[at(TextId::LoadingReadingSave)] = "Cargando partida...";
    text[at(TextId::LoadingSearchingPokemon)] = "Buscando Pokemon...";
    text[at(TextId::LoadingBankData)] = "Cargando datos del banco...";
    text[at(TextId::LoadingDetailInitializing)] = "Inicializando...";
    text[at(TextId::LoadingDetailCheckingUpdates)] = "Descargando y verificando nuevas versiones de forma segura...";
    text[at(TextId::LoadingDetailSigningIn)] = "Verificando servidor y sesion...";
    text[at(TextId::LoadingDetailSearchingGames)] = "Verificando cartucho y juegos instalados...";
    text[at(TextId::LoadingDetailReadingIcons)] = "Leyendo iconos originales...";
    text[at(TextId::LoadingDetailReadingSave)] = "Abriendo partida local...";
    text[at(TextId::LoadingDetailSearchingPokemon)] = "Leyendo caja y Pokemon...";
    text[at(TextId::LoadingDetailLoadingBank)] = "Conectando con tu banco...";
    text[at(TextId::LoadingProgressLabel)] = "Progreso";
    text[at(TextId::LoadingAutoLoginDetected)] = "Inicio de sesion automatico detectado...";
    text[at(TextId::LoadingDetailAutoLoginDetected)] = "Iniciando sesion automaticamente...";
    text[at(TextId::LoadingWelcomeBackPrefix)] = "Bienvenido de nuevo, ";
    text[at(TextId::TrashCan)] = "Papelera";
    text[at(TextId::TrashConfirmMessage)] = "Eliminar estos Pokemon?";
    text[at(TextId::Yes)] = "Si";
    text[at(TextId::No)] = "No";
    text[at(TextId::StatusFindingSaveGames)] = "Buscando partidas guardadas...";
    text[at(TextId::StatusNoCompatibleSaveGame)] = "No se encontro ninguna partida compatible.";
    text[at(TextId::StatusCheckingCartridgeSlot)] = "Verificando ranura del cartucho...";
    text[at(TextId::StatusReadingSave)] = "Leyendo partida...";
    text[at(TextId::NoCompatibleSaveGameTitle)] = "Ninguna partida compatible";
    text[at(TextId::InsertCartridgeOrCreateSave)] = "Inserta un cartucho o crea primero una partida.";
    text[at(TextId::NoCartridgeInserted)] = "Ningun cartucho insertado";
    text[at(TextId::InsertCartridgeToLoad)] = "Inserta un cartucho de juego para cargarlo aqui.";
    text[at(TextId::UnknownTrainer)] = "Entrenador desconocido";
    text[at(TextId::IdNoPrefix)] = "N.º entrenador ";
    text[at(TextId::PlayTimePrefix)] = "Tiempo de juego: ";
    text[at(TextId::PokedexPrefix)] = "Pokedex: ";
    text[at(TextId::CartridgeRemovedTitle)] = "Cartucho retirado";
    text[at(TextId::CartridgeRemovedMessage)] = "Se retiro el cartucho del juego. Volviendo a la seleccion de juegos.";
    text[at(TextId::PrivacyConsentPrefix)] = "Acepto la ";
    text[at(TextId::PrivacyConsentLink)] = "politica de privacidad";
    text[at(TextId::PrivacyRequiredTitle)] = "Politica de privacidad";
    text[at(TextId::PrivacyRequiredMessage)] = "Lee y acepta la politica de privacidad para crear una cuenta.";
    text[at(TextId::PrivacyLoading)] = "Cargando la politica de privacidad...";
    text[at(TextId::PrivacyUnavailableTitle)] = "Politica de privacidad no disponible";
    text[at(TextId::PrivacyUnavailableMessage)] = "No se pudo cargar la politica de privacidad. Revisa tu conexion e intentalo de nuevo.";
    text[at(TextId::PolicyAccept)] = "Aceptar (A)";
    text[at(TextId::PolicyAgeConfirmation)] = "Al aceptar confirmas tambien que tienes al menos 16 anos o que tus padres estan de acuerdo.";
    text[at(TextId::PolicyDecline)] = "Rechazar (B)";
    return text;
}

constexpr Translation Text = build();
}

const Translation& Translations::spanish() {
    return Text;
}
