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
    // Writes queued entries to the SD card. Main thread only: fprintf from a
    // background thread was the cause of a reproducible ARM11 data abort.
    void flush();

private:
    Logger();
    void write(LogLevel level, std::string_view message);

    mutable LightLock lock_;
    std::deque<LogEntry> entries_;
    std::deque<LogEntry> pendingWrites_;
};