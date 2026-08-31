#pragma once

#include <3ds.h>

#include <deque>
#include <string>
#include <string_view>

enum class LogLevel {
    Info,
    Warning,
    Error
};

struct LogEntry {
    LogLevel level;
    std::string message;
};

class Logger {
public:
    static Logger& instance();
    void initialize();
    void info(std::string_view message);
    void warning(std::string_view message);
    void error(std::string_view message);
    std::deque<LogEntry> entries() const;

private:
    Logger();
    void write(LogLevel level, std::string_view message);
    void flush();
    static void flushWorker(void* argument);
    void flushLoop();

    mutable LightLock lock_;
    std::deque<LogEntry> entries_;
    std::deque<LogEntry> pendingWrites_;
    Thread flushThread_ = nullptr;
};