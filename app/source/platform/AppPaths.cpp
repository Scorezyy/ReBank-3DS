#include "platform/AppPaths.hpp"

#include "platform/FileSystem.hpp"

namespace AppPaths {
void ensureDirectory(std::string_view relative) {
    FileSystem::createDirectories(relative.empty() ? std::string(Root) : file(relative));
}
}
