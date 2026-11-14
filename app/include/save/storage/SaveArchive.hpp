#pragma once

#include <3ds.h>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace SaveArchive {
std::shared_ptr<std::uint8_t[]> read(std::uint64_t titleId, FS_MediaType mediaType, std::size_t& size, Result& result);
bool hasSave(std::uint64_t titleId, FS_MediaType mediaType);
bool write(std::uint64_t titleId, FS_MediaType mediaType, const std::uint8_t* data, std::size_t size);
}
