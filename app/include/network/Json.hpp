#pragma once

#include <jansson.h>

#include <string>
#include <utility>

namespace Json {
std::string string(json_t* object, const char* key);
bool flag(json_t* object, const char* key);

template <typename T>
T integer(json_t* object, const char* key, T fallback) {
    json_t* value = json_object_get(object, key);
    return json_is_integer(value) ? static_cast<T>(json_integer_value(value)) : fallback;
}

class Document {
public:
    Document() = default;
    explicit Document(json_t* owned) : root_(owned) {}
    Document(Document&& other) noexcept : root_(std::exchange(other.root_, nullptr)) {}
    Document& operator=(Document&& other) noexcept {
        std::swap(root_, other.root_);
        return *this;
    }
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    ~Document() { json_decref(root_); }

    static Document parse(const std::string& text);
    static Document object() { return Document(json_object()); }

    bool isObject() const { return json_is_object(root_); }
    json_t* root() const { return root_; }
    json_t* field(const char* key) const { return json_object_get(root_, key); }
    std::string string(const char* key) const { return Json::string(root_, key); }
    bool flag(const char* key) const { return Json::flag(root_, key); }
    template <typename T>
    T integer(const char* key, T fallback) const { return Json::integer<T>(root_, key, fallback); }

    Document& set(const char* key, json_t* value) {
        json_object_set_new(root_, key, value);
        return *this;
    }
    std::string dump() const;

private:
    json_t* root_ = nullptr;
};
}
