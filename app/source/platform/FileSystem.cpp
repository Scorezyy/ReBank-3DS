#include "platform/FileSystem.hpp"

#include <cstdio>
#include <memory>
#include <sys/stat.h>

namespace {
constexpr std::size_t CopyChunkSize = 32 * 1024;

struct FileCloser {
    void operator()(std::FILE* file) const { std::fclose(file); }
};

using FileHandle = std::unique_ptr<std::FILE, FileCloser>;

FileHandle open(const std::string& path, const char* mode) {
    return FileHandle(std::fopen(path.c_str(), mode));
}

bool closeChecked(FileHandle& file) {
    const bool flushed = std::fflush(file.get()) == 0;
    return std::fclose(file.release()) == 0 && flushed;
}
}

namespace FileSystem {
bool exists(const std::string& path) {
    struct stat info{};
    return ::stat(path.c_str(), &info) == 0;
}

std::optional<std::size_t> size(const std::string& path) {
    struct stat info{};
    if (::stat(path.c_str(), &info) != 0 || info.st_size < 0) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(info.st_size);
}

bool readAt(const std::string& path, std::size_t offset, std::span<std::uint8_t> destination) {
    FileHandle file = open(path, "rb");
    if (!file || std::fseek(file.get(), static_cast<long>(offset), SEEK_SET) != 0) {
        return false;
    }
    return std::fread(destination.data(), 1, destination.size(), file.get()) == destination.size();
}

std::optional<std::vector<std::uint8_t>> read(const std::string& path, std::size_t maximumSize) {
    const std::optional<std::size_t> fileSize = size(path);
    if (!fileSize || *fileSize == 0 || *fileSize > maximumSize) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> content(*fileSize);
    if (!readAt(path, 0, content)) {
        return std::nullopt;
    }
    return content;
}

std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    FileHandle file = open(path, "r");
    if (!file) {
        return lines;
    }
    char line[512];
    while (std::fgets(line, sizeof(line), file.get())) {
        std::string_view text(line);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
            text.remove_suffix(1);
        }
        lines.emplace_back(text);
    }
    return lines;
}

bool write(const std::string& path, std::span<const std::uint8_t> data) {
    FileHandle file = open(path, "wb");
    if (!file) {
        return false;
    }
    const bool written = std::fwrite(data.data(), 1, data.size(), file.get()) == data.size();
    const bool closed = closeChecked(file);
    if (!written || !closed) {
        std::remove(path.c_str());
        return false;
    }
    return true;
}

bool write(const std::string& path, std::string_view text) {
    return write(path, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

bool replace(const std::string& path, std::span<const std::uint8_t> data) {
    const std::string staging = path + ".tmp";
    if (!write(staging, data)) {
        return false;
    }
    std::remove(path.c_str());
    return std::rename(staging.c_str(), path.c_str()) == 0;
}

bool copy(const std::string& source, const std::string& destination) {
    FileHandle input = open(source, "rb");
    FileHandle output = input ? open(destination, "wb") : nullptr;
    if (!output) {
        return false;
    }
    std::vector<std::uint8_t> buffer(CopyChunkSize);
    bool written = true;
    while (const std::size_t count = std::fread(buffer.data(), 1, buffer.size(), input.get())) {
        if (std::fwrite(buffer.data(), 1, count, output.get()) != count) {
            written = false;
            break;
        }
    }
    const bool readFailed = std::ferror(input.get()) != 0;
    return closeChecked(output) && written && !readFailed;
}

void createDirectories(std::string_view path) {
    const std::size_t root = path.find(":/");
    std::size_t separator = path.find('/', root == std::string_view::npos ? 1 : root + 2);
    while (separator != std::string_view::npos) {
        ::mkdir(std::string(path.substr(0, separator)).c_str(), 0777);
        separator = path.find('/', separator + 1);
    }
    ::mkdir(std::string(path).c_str(), 0777);
}
}
