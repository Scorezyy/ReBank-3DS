#include "i18n/Translation.hpp"

namespace {
constexpr Translation build() {
    Translation text{};
    text[at(TextId::Tagline)] = "Todos os Pokemon. Todos os jogos. Um banco.";
    text[at(TextId::Login)] = "Entrar";
    text[at(TextId::Register)] = "Registrar";
    text[at(TextId::Username)] = "Usuario";
    text[at(TextId::Email)] = "E-mail";
    text[at(TextId::Password)] = "Senha";
    text[at(TextId::ForgotPassword)] = "Esqueceu a senha?";
    text[at(TextId::Submit)] = "Continuar";
    text[at(TextId::Back)] = "Voltar";
    text[at(TextId::CreateAccount)] = "Criar conta";
    text[at(TextId::ResetPassword)] = "Redefinir senha";
    text[at(TextId::LoadingWait)] = "Por favor aguarde...";
    text[at(TextId::LoadingCheckingUpdates)] = "Verificando atualizacoes...";
    text[at(TextId::LoadingSigningIn)] = "Entrando...";
    text[at(TextId::LoadingSearchingGames)] = "Procurando jogos salvos...";
    text[at(TextId::LoadingReadingIcons)] = "Carregando icones...";
    text[at(TextId::LoadingReadingSave)] = "Carregando save...";
    text[at(TextId::LoadingSearchingPokemon)] = "Procurando Pokemon...";
    text[at(TextId::LoadingBankData)] = "Carregando dados do banco...";
    text[at(TextId::LoadingDetailInitializing)] = "Inicializando...";
    text[at(TextId::LoadingDetailCheckingUpdates)] = "Baixando e verificando novas versoes com seguranca...";
    text[at(TextId::LoadingDetailSigningIn)] = "Verificando servidor e sessao...";
    text[at(TextId::LoadingDetailSearchingGames)] = "Verificando cartucho e jogos instalados...";
    text[at(TextId::LoadingDetailReadingIcons)] = "Lendo icones originais...";
    text[at(TextId::LoadingDetailReadingSave)] = "Abrindo save local...";
    text[at(TextId::LoadingDetailSearchingPokemon)] = "Lendo caixa e Pokemon...";
    text[at(TextId::LoadingDetailLoadingBank)] = "Conectando ao seu banco...";
    text[at(TextId::LoadingProgressLabel)] = "Progresso";
    text[at(TextId::LoadingAutoLoginDetected)] = "Login automatico detectado...";
    text[at(TextId::LoadingDetailAutoLoginDetected)] = "Entrando automaticamente...";
    text[at(TextId::LoadingWelcomeBackPrefix)] = "Bem-vindo de volta, ";
    text[at(TextId::TrashCan)] = "Lixeira";
    text[at(TextId::TrashConfirmMessage)] = "Excluir estes Pokemon?";
    text[at(TextId::Yes)] = "Sim";
    text[at(TextId::No)] = "Nao";
    text[at(TextId::StatusFindingSaveGames)] = "Procurando jogos salvos...";
    text[at(TextId::StatusNoCompatibleSaveGame)] = "Nenhum save compativel encontrado.";
    text[at(TextId::StatusCheckingCartridgeSlot)] = "Verificando slot do cartucho...";
    text[at(TextId::StatusReadingSave)] = "Lendo save...";
    text[at(TextId::NoCompatibleSaveGameTitle)] = "Nenhum save compativel";
    text[at(TextId::InsertCartridgeOrCreateSave)] = "Insira um cartucho ou crie um save primeiro.";
    text[at(TextId::NoCartridgeInserted)] = "Nenhum cartucho inserido";
    text[at(TextId::InsertCartridgeToLoad)] = "Insira um cartucho de jogo para carrega-lo aqui.";
    text[at(TextId::UnknownTrainer)] = "Treinador desconhecido";
    text[at(TextId::IdNoPrefix)] = "N.º Treinador ";
    text[at(TextId::PlayTimePrefix)] = "Tempo de jogo: ";
    text[at(TextId::PokedexPrefix)] = "Pokedex: ";
    text[at(TextId::CartridgeRemovedTitle)] = "Cartucho removido";
    text[at(TextId::CartridgeRemovedMessage)] = "O cartucho do jogo foi removido. Voltando para a selecao de jogos.";
    text[at(TextId::PrivacyConsentPrefix)] = "Aceito a ";
    text[at(TextId::PrivacyConsentLink)] = "politica de privacidade";
    text[at(TextId::PrivacyRequiredTitle)] = "Politica de privacidade";
    text[at(TextId::PrivacyRequiredMessage)] = "Le e aceita a politica de privacidade para criar uma conta.";
    text[at(TextId::PrivacyLoading)] = "A carregar a politica de privacidade...";
    text[at(TextId::PrivacyUnavailableTitle)] = "Politica de privacidade indisponivel";
    text[at(TextId::PrivacyUnavailableMessage)] = "Nao foi possivel carregar a politica de privacidade. Verifica a ligacao e tenta novamente.";
    text[at(TextId::PolicyAccept)] = "Aceitar (A)";
    text[at(TextId::PolicyAgeConfirmation)] = "Ao aceitar confirmas tambem que tens pelo menos 16 anos ou que os teus pais concordam.";
    text[at(TextId::PolicyDecline)] = "Recusar (B)";
    return text;
}

constexpr Translation Text = build();
}

const Translation& Translations::portuguese() {
    return Text;
}
