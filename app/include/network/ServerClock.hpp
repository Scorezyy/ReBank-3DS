#pragma once

#include <3ds.h>

#include <cstdint>
#include <optional>

class ServerClock {
public:
    ServerClock() { LightLock_Init(&lock_); }

    bool claimSync(bool force);
    void record(std::int64_t localMsBeforeRequest, std::int64_t serverUnixSeconds);
    std::uint64_t unixSeconds();

private:
    LightLock lock_;
    std::optional<std::int64_t> deltaMs_;
    std::uint64_t lastAttemptMs_ = 0;
};
