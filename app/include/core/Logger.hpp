#pragma once

#include <3ds.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

enum class LogLevel {
    Info,
    Warning,
    Error
};

struct LogEntry {
    LogLevel level;
    std::uint64_t timeMs;
    std::string message;
};

class Logger {
public:
    static Logger& instance();
    void initialize();
    void shutdown();
    void info(std::string_view message);
    void warning(std::string_view message);
    void error(std::string_view message);
    std::vector<LogEntry> recent(std::size_t count) const;

private:
    Logger();
    void write(LogLevel level, std::string_view message);
    void flush();
    static void flushWorker(void* argument);
    void flushLoop();

    mutable LightLock lock_;
    std::deque<LogEntry> entries_;
    std::size_t unflushed_ = 0;
    Thread flushThread_ = nullptr;
    std::atomic<bool> flushRunning_{false};
};
