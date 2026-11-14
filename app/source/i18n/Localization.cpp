#include "i18n/Localization.hpp"

#include <3ds.h>

Localization::Localization() : translation_(&Translations::english()) {
    u8 language = CFG_LANGUAGE_EN;
    if (R_FAILED(CFGU_GetSystemLanguage(&language))) {
        return;
    }
    switch (language) {
        case CFG_LANGUAGE_DE:
            translation_ = &Translations::german();
            break;
        case CFG_LANGUAGE_FR:
            translation_ = &Translations::french();
            break;
        case CFG_LANGUAGE_ES:
            translation_ = &Translations::spanish();
            break;
        case CFG_LANGUAGE_IT:
            translation_ = &Translations::italian();
            break;
        case CFG_LANGUAGE_PT:
            translation_ = &Translations::portuguese();
            break;
        default:
            break;
    }
}

std::string_view Localization::get(TextId id) const {
    const std::string_view text = (*translation_)[at(id)];
    return text.empty() ? Translations::english()[at(id)] : text;
}

std::string Localization::format(TextId id, std::initializer_list<std::string_view> arguments) const {
    const std::string_view pattern = get(id);
    std::string text;
    text.reserve(pattern.size() + 16);
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        const bool placeholder = pattern[index] == '{' && index + 2 < pattern.size()
            && pattern[index + 1] >= '0' && pattern[index + 1] <= '9' && pattern[index + 2] == '}';
        if (!placeholder) {
            text.push_back(pattern[index]);
            continue;
        }
        const auto argument = static_cast<std::size_t>(pattern[index + 1] - '0');
        if (argument < arguments.size()) {
            text.append(*(arguments.begin() + argument));
        }
        index += 2;
    }
    return text;
}
