#pragma once

#include "network/ApiClient.hpp"

#include <string>

namespace SignedFile {
enum class Verdict { Valid, Corrupt, Unsigned };

Verdict verify(const std::string& path, const UpdateAsset& asset);
}
