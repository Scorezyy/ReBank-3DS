#pragma once

#include "save/catalog/GameCatalog.hpp"
#include "save/storage/SaveSource.hpp"

#include <memory>
#include <string>

namespace pksm {
class Sav;
}

namespace SaveParser {
std::unique_ptr<pksm::Sav> parse(const GameDescriptor& game, const SaveImage& image, std::string& error);
}
