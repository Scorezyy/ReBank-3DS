#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace SaveFiles {
std::shared_ptr<std::uint8_t[]> read(const std::string& path, std::size_t& size);
std::shared_ptr<std::uint8_t[]> readExport(std::string_view code, std::size_t& size, std::string& path);
bool exportExists(std::string_view code);
bool write(const std::string& path, const std::uint8_t* data, std::size_t size);
}
