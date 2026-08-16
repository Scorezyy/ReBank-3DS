#include "Logger.hpp"

#include <3ds.h>

#include <cstdio>
#include <sys/stat.h>

namespace {
constexpr std::size_t MaximumEntries = 40;

const char* label(LogLevel level) {
    switch (level) {
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "INFO";
    }
}
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    LightLock_Init(&lock_);
}

void Logger::initialize() {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ReBank", 0777);
    if (FILE* file = std::fopen("sdmc:/3ds/ReBank/rebank.log", "w")) {
        std::fclose(file);
    }
    info("Logger initialized");
}

void Logger::info(std::string_view message) {
    write(LogLevel::Info, message);
}

void Logger::warning(std::string_view message) {
    write(LogLevel::Warning, message);
}

void Logger::error(std::string_view message) {
    write(LogLevel::Error, message);
}

std::deque<LogEntry> Logger::entries() const {
    LightLock_Lock(&lock_);
    const auto copy = entries_;
    LightLock_Unlock(&lock_);
    return copy;
}

void Logger::write(LogLevel level, std::string_view message) {
    LightLock_Lock(&lock_);
    entries_.push_back({level, std::string(message)});
    if (entries_.size() > MaximumEntries) {
        entries_.pop_front();
    }

    FILE* file = std::fopen("sdmc:/3ds/ReBank/rebank.log", "a");
    if (!file) {
        LightLock_Unlock(&lock_);
        return;
    }
    std::fprintf(
        file,
        "%llu [%s] %.*s\n",
        static_cast<unsigned long long>(osGetTime()),
        label(level),
        static_cast<int>(message.size()),
        message.data()
    );
    std::fclose(file);
    LightLock_Unlock(&lock_);
}