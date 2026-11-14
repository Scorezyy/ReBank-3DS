#pragma once

#include "i18n/Translation.hpp"

#include <initializer_list>
#include <string>
#include <string_view>

class Localization {
public:
    Localization();
    std::string_view get(TextId id) const;
    std::string format(TextId id, std::initializer_list<std::string_view> arguments) const;

private:
    const Translation* translation_;
};
