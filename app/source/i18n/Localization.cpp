#include "i18n/Localization.hpp"

namespace {
using Translation = std::array<std::string_view, static_cast<std::size_t>(TextId::Count)>;

constexpr Translation English{
    "Your Pokemon. One secure home.", "Login", "Register", "Username", "Email", "Password",
    "Forgot password?", "Continue", "Back", "Create account", "Reset password"
};

constexpr Translation German{
    "Deine Pokemon. Ein sicheres Zuhause.", "Anmelden", "Registrieren", "Benutzername", "E-Mail", "Passwort",
    "Passwort vergessen?", "Weiter", "Zurueck", "Konto erstellen", "Passwort zuruecksetzen"
};

constexpr Translation French{
    "Vos Pokemon. Un espace securise.", "Connexion", "Inscription", "Utilisateur", "E-mail", "Mot de passe",
    "Mot de passe oublie ?", "Continuer", "Retour", "Creer un compte", "Reinitialiser"
};

constexpr Translation Spanish{
    "Tus Pokemon. Un hogar seguro.", "Iniciar sesion", "Registrarse", "Usuario", "Correo", "Contrasena",
    "Olvidaste la contrasena?", "Continuar", "Volver", "Crear cuenta", "Restablecer"
};

constexpr Translation Italian{
    "I tuoi Pokemon. Una casa sicura.", "Accedi", "Registrati", "Username", "E-mail", "Password",
    "Password dimenticata?", "Continua", "Indietro", "Crea account", "Reimposta password"
};

constexpr Translation Portuguese{
    "Seus Pokemon. Um lar seguro.", "Entrar", "Registrar", "Usuario", "E-mail", "Senha",
    "Esqueceu a senha?", "Continuar", "Voltar", "Criar conta", "Redefinir senha"
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