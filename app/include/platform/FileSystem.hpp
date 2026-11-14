#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace FileSystem {
bool exists(const std::string& path);
std::optional<std::size_t> size(const std::string& path);
bool readAt(const std::string& path, std::size_t offset, std::span<std::uint8_t> destination);
std::optional<std::vector<std::uint8_t>> read(const std::string& path, std::size_t maximumSize);
std::vector<std::string> readLines(const std::string& path);
bool write(const std::string& path, std::span<const std::uint8_t> data);
bool write(const std::string& path, std::string_view text);
bool replace(const std::string& path, std::span<const std::uint8_t> data);
bool copy(const std::string& source, const std::string& destination);
void createDirectories(std::string_view path);
}
