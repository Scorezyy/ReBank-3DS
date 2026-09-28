#include "core/InputValidation.hpp"

#include <algorithm>
#include <cctype>

namespace {
constexpr std::size_t MinimumUsernameLength = 3;
constexpr std::size_t MaximumUsernameLength = 32;
constexpr std::size_t MinimumPasswordLength = 10;
}

bool isValidUsername(const std::string& value) {
    return value.size() >= MinimumUsernameLength && value.size() <= MaximumUsernameLength
        && std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isalnum(character) || character == '_' || character == '-';
        });
}

bool isValidEmail(const std::string& value) {
    return value.find('@') != std::string::npos;
}

bool isValidPassword(const std::string& value) {
    return value.size() >= MinimumPasswordLength;
}
