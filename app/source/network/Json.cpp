#include "network/Json.hpp"

#include <cstdlib>

namespace Json {
std::string string(json_t* object, const char* key) {
    json_t* value = json_object_get(object, key);
    return json_is_string(value) ? json_string_value(value) : "";
}

bool flag(json_t* object, const char* key) {
    return json_is_true(json_object_get(object, key));
}

Document Document::parse(const std::string& text) {
    json_error_t error{};
    return Document(json_loadb(text.data(), text.size(), JSON_REJECT_DUPLICATES, &error));
}

std::string Document::dump() const {
    char* encoded = json_dumps(root_, JSON_COMPACT);
    if (!encoded) {
        return {};
    }
    std::string result(encoded);
    std::free(encoded);
    return result;
}
}
