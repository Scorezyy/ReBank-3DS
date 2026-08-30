#include "i18n/Localization.hpp"

namespace {
using Translation = std::array<std::string_view, static_cast<std::size_t>(TextId::Count)>;

constexpr Translation English{
    "Your Pokemon. One secure home.", "Login", "Register", "Username", "Email", "Password",
    "Forgot password?", "Continue", "Back", "Create account", "Reset password",
    "Please wait...", "Checking updates...", "Signing in...", "Searching save files...",
    "Loading game icons...", "Loading save file...", "Searching Pokemon...", "Loading bank data...",
    "Initializing...", "Downloading and verifying new versions securely...",
    "Checking server and session...", "Checking cartridge and installed games...",
    "Reading original game icons...", "Opening local save file...", "Reading box and Pokemon...",
    "Connecting to your bank...", "Progress",
    "Autologin detected...", "Logging you in automatically...", "Welcome back, ",
    "Trash Can", "Delete these Pokemon?", "Yes", "No"
};

constexpr Translation German{
    "Deine Pokémon. in einem sicheren Zuhause.", "Anmelden", "Registrieren", "Benutzername", "E-Mail", "Passwort",
    "Passwort vergessen?", "Weiter", "Zurück", "Konto erstellen", "Passwort zurücksetzen",
    "Bitte warten...", "Updates prüfen...", "Anmelden...", "Spielstände suchen...",
    "Spielbilder laden...", "Spielstand laden...", "Pokémon suchen...", "Bankdaten laden...",
    "Initialisieren...", "Neue Version prüfen...",
    "Server & Sitzung prüfen...", "Spiele prüfen...",
    "Spielbilder lesen...", "Spielstand öffnen...", "Boxen & Pokémon lesen...",
    "Mit Bank verbinden...", "Fortschritt",
    "Automatischer Login erkannt...", "Du wirst automatisch angemeldet...", "Willkommen zurück, ",
    "Papierkorb", "Pokémon löschen?", "Ja", "Nein"
};

constexpr Translation French{
    "Vos Pokemon. Un espace securise.", "Connexion", "Inscription", "Utilisateur", "E-mail", "Mot de passe",
    "Mot de passe oublie ?", "Continuer", "Retour", "Creer un compte", "Reinitialiser",
    "Veuillez patienter...", "Verification des mises a jour...", "Connexion en cours...",
    "Recherche des sauvegardes...", "Chargement des icones...", "Chargement de la sauvegarde...",
    "Recherche de Pokemon...", "Chargement de la banque...", "Initialisation...",
    "Telechargement et verification securisee des nouvelles versions...",
    "Verification du serveur et de la session...", "Verification de la cartouche et des jeux installes...",
    "Lecture des icones originales...", "Ouverture de la sauvegarde locale...",
    "Lecture de la boite et des Pokemon...", "Connexion a votre banque...", "Progression",
    "Connexion automatique detectee...", "Connexion automatique en cours...", "Content de te revoir, ",
    "Corbeille", "Supprimer ces Pokemon ?", "Oui", "Non"
};

constexpr Translation Spanish{
    "Tus Pokemon. Un hogar seguro.", "Iniciar sesion", "Registrarse", "Usuario", "Correo", "Contrasena",
    "Olvidaste la contrasena?", "Continuar", "Volver", "Crear cuenta", "Restablecer",
    "Por favor espera...", "Buscando actualizaciones...", "Iniciando sesion...",
    "Buscando partidas guardadas...", "Cargando iconos...", "Cargando partida...",
    "Buscando Pokemon...", "Cargando datos del banco...", "Inicializando...",
    "Descargando y verificando nuevas versiones de forma segura...",
    "Verificando servidor y sesion...", "Verificando cartucho y juegos instalados...",
    "Leyendo iconos originales...", "Abriendo partida local...", "Leyendo caja y Pokemon...",
    "Conectando con tu banco...", "Progreso",
    "Inicio de sesion automatico detectado...", "Iniciando sesion automaticamente...", "Bienvenido de nuevo, ",
    "Papelera", "Eliminar estos Pokemon?", "Si", "No"
};

constexpr Translation Italian{
    "I tuoi Pokemon. Una casa sicura.", "Accedi", "Registrati", "Username", "E-mail", "Password",
    "Password dimenticata?", "Continua", "Indietro", "Crea account", "Reimposta password",
    "Attendere prego...", "Controllo aggiornamenti...", "Accesso in corso...",
    "Ricerca salvataggi...", "Caricamento icone...", "Caricamento salvataggio...",
    "Ricerca Pokemon...", "Caricamento dati banca...", "Inizializzazione...",
    "Download e verifica sicura delle nuove versioni...",
    "Verifica server e sessione...", "Verifica cartuccia e giochi installati...",
    "Lettura icone originali...", "Apertura salvataggio locale...", "Lettura box e Pokemon...",
    "Connessione alla tua banca...", "Progresso",
    "Login automatico rilevato...", "Accesso automatico in corso...", "Bentornato, ",
    "Cestino", "Eliminare questi Pokemon?", "Si", "No"
};

constexpr Translation Portuguese{
    "Seus Pokemon. Um lar seguro.", "Entrar", "Registrar", "Usuario", "E-mail", "Senha",
    "Esqueceu a senha?", "Continuar", "Voltar", "Criar conta", "Redefinir senha",
    "Por favor aguarde...", "Verificando atualizacoes...", "Entrando...",
    "Procurando jogos salvos...", "Carregando icones...", "Carregando save...",
    "Procurando Pokemon...", "Carregando dados do banco...", "Inicializando...",
    "Baixando e verificando novas versoes com seguranca...",
    "Verificando servidor e sessao...", "Verificando cartucho e jogos instalados...",
    "Lendo icones originais...", "Abrindo save local...", "Lendo caixa e Pokemon...",
    "Conectando ao seu banco...", "Progresso",
    "Login automatico detectado...", "Entrando automaticamente...", "Bem-vindo de volta, ",
    "Lixeira", "Excluir estes Pokemon?", "Sim", "Nao"
};
}

Localization::Localization() : translation_(&English) {
    u8 language = CFG_LANGUAGE_EN;
    if (R_FAILED(CFGU_GetSystemLanguage(&language))) {
        return;
    }

    switch (language) {
        case CFG_LANGUAGE_DE:
            translation_ = &German;
            break;
        case CFG_LANGUAGE_FR:
            translation_ = &French;
            break;
        case CFG_LANGUAGE_ES:
            translation_ = &Spanish;
            break;
        case CFG_LANGUAGE_IT:
            translation_ = &Italian;
            break;
        case CFG_LANGUAGE_PT:
            translation_ = &Portuguese;
            break;
        default:
            break;
    }
}

std::string_view Localization::get(TextId id) const {
    return (*translation_)[static_cast<std::size_t>(id)];
}