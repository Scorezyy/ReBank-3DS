#pragma once

#include "network/ApiTypes.hpp"

#include <string>

namespace SignedFile {
enum class Verdict { Valid, Corrupt, Unsigned };

Verdict verify(const std::string& path, const UpdateAsset& asset);
}
