#include "core/Logger.hpp"
#include "core/FsGuard.hpp"

#include <3ds.h>

#include <cstdio>
#include <sys/stat.h>

namespace {
constexpr std::size_t MaximumEntries = 4000;

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
    {
        const FsGuard guard;
        mkdir("sdmc:/3ds", 0777);
        mkdir("sdmc:/3ds/ReBank", 0777);
        if (FILE* file = std::fopen("sdmc:/3ds/ReBank/rebank.log", "w")) {
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
    pendingWrites_.push_back({level, std::string(message)});
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
    std::deque<LogEntry> pending;
    pending.swap(pendingWrites_);
    LightLock_Unlock(&lock_);
    if (pending.empty()) {
        return;
    }

    const FsGuard guard;
    FILE* file = std::fopen("sdmc:/3ds/ReBank/rebank.log", "a");
    if (!file) {
        return;
    }
    for (const LogEntry& entry : pending) {
        std::fprintf(
            file,
            "%llu [%s] %.*s\n",
            static_cast<unsigned long long>(osGetTime()),
            label(entry.level),
            static_cast<int>(entry.message.size()),
            entry.message.data()
        );
    }
    std::fclose(file);
}