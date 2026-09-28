#include "core/Logger.hpp"
#include "core/AppPaths.hpp"
#include "core/FsGuard.hpp"

#include <3ds.h>

#include <algorithm>
#include <cstdio>

namespace {
constexpr std::size_t MaximumEntries = 1000;

const std::string& logPath() {
    static const std::string path = AppPaths::file("rebank.log");
    return path;
}

const char* label(LogLevel level) {
    switch (level) {
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Info:
            break;
    }
    return "INFO";
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
    {
        const FsGuard guard;
        AppPaths::ensureDirectory();
        if (FILE* file = std::fopen(logPath().c_str(), "w")) {
            std::fclose(file);
        }
    }
    info("Logger initialized");
    if (!flushThread_) {
        flushRunning_.store(true, std::memory_order_release);
        flushThread_ = threadCreate(&Logger::flushWorker, this, 16 * 1024, 0x3F, -2, false);
    }
}

void Logger::shutdown() {
    if (!flushThread_) {
        return;
    }
    flushRunning_.store(false, std::memory_order_release);
    threadJoin(flushThread_, U64_MAX);
    threadFree(flushThread_);
    flushThread_ = nullptr;
    flush();
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

std::vector<LogEntry> Logger::recent(std::size_t count) const {
    LightLock_Lock(&lock_);
    const std::size_t first = entries_.size() > count ? entries_.size() - count : 0;
    std::vector<LogEntry> copy(entries_.begin() + static_cast<std::ptrdiff_t>(first), entries_.end());
    LightLock_Unlock(&lock_);
    return copy;
}

void Logger::write(LogLevel level, std::string_view message) {
    const std::uint64_t now = osGetTime();
    LightLock_Lock(&lock_);
    entries_.push_back({level, now, std::string(message)});
    if (entries_.size() > MaximumEntries) {
        entries_.pop_front();
    }
    unflushed_ = std::min(unflushed_ + 1, entries_.size());
    LightLock_Unlock(&lock_);
}

void Logger::flushWorker(void* argument) {
    static_cast<Logger*>(argument)->flushLoop();
}

void Logger::flushLoop() {
    while (flushRunning_.load(std::memory_order_acquire)) {
        svcSleepThread(150'000'000LL);
        flush();
    }
}

void Logger::flush() {
    LightLock_Lock(&lock_);
    std::vector<LogEntry> pending(entries_.end() - static_cast<std::ptrdiff_t>(unflushed_), entries_.end());
    unflushed_ = 0;
    LightLock_Unlock(&lock_);
    if (pending.empty()) {
        return;
    }

    const FsGuard guard;
    FILE* file = std::fopen(logPath().c_str(), "a");
    if (!file) {
        return;
    }
    for (const LogEntry& entry : pending) {
        std::fprintf(
            file,
            "%llu [%s] %.*s\n",
            static_cast<unsigned long long>(entry.timeMs),
            label(entry.level),
            static_cast<int>(entry.message.size()),
            entry.message.data()
        );
    }
    std::fclose(file);
}
