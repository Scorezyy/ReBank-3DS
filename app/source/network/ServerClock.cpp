#include "network/ServerClock.hpp"

#include "platform/Logger.hpp"

#include <string>

namespace {
constexpr std::uint64_t RetryIntervalMs = 30'000;
constexpr std::uint64_t ResyncIntervalMs = 60'000;
constexpr std::uint64_t NtpToUnixEpochOffsetSeconds = 2208988800ULL;
constexpr u32 ConsoleTimeOffsetBlock = 0x00030001;

class ScopedLock {
public:
    explicit ScopedLock(LightLock& lock) : lock_(lock) { LightLock_Lock(&lock_); }
    ~ScopedLock() { LightLock_Unlock(&lock_); }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

private:
    LightLock& lock_;
};
}

bool ServerClock::claimSync(bool force) {
    const std::uint64_t now = osGetTime();
    const ScopedLock lock(lock_);
    const std::uint64_t interval = deltaMs_ ? ResyncIntervalMs : RetryIntervalMs;
    const bool recent = lastAttemptMs_ != 0 && now >= lastAttemptMs_ && now - lastAttemptMs_ < interval;
    if (!force && recent) {
        return false;
    }
    lastAttemptMs_ = now;
    return true;
}

void ServerClock::record(std::int64_t localMsBeforeRequest, std::int64_t serverUnixSeconds) {
    const std::int64_t delta = serverUnixSeconds * 1000 - localMsBeforeRequest;
    {
        const ScopedLock lock(lock_);
        deltaMs_ = delta;
    }
    Logger::instance().info("Clock synced with server, delta=" + std::to_string(delta) + "ms");
}

std::uint64_t ServerClock::unixSeconds() {
    const std::int64_t localMs = static_cast<std::int64_t>(osGetTime());
    std::optional<std::int64_t> delta;
    {
        const ScopedLock lock(lock_);
        delta = deltaMs_;
    }
    if (delta) {
        return static_cast<std::uint64_t>((localMs + *delta) / 1000);
    }
    std::int64_t timeOffsetMs = 0;
    CFGU_GetConfigInfoBlk2(sizeof(timeOffsetMs), ConsoleTimeOffsetBlock, &timeOffsetMs);
    return static_cast<std::uint64_t>(localMs - timeOffsetMs) / 1000ULL - NtpToUnixEpochOffsetSeconds;
}
