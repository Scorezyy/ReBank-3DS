#include "core/AppPaths.hpp"

#include <sys/stat.h>

namespace AppPaths {
void ensureDirectory(std::string_view relative) {
    ::mkdir("sdmc:/3ds", 0777);
    std::string path(Root);
    ::mkdir(path.c_str(), 0777);
    std::size_t start = 0;
    while (start < relative.size()) {
        std::size_t end = relative.find('/', start);
        if (end == std::string_view::npos) {
            end = relative.size();
        }
        path += "/" + std::string(relative.substr(start, end - start));
        ::mkdir(path.c_str(), 0777);
        start = end + 1;
    }
}
}
