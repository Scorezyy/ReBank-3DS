#pragma once

#include <3ds.h>

// Serializes filesystem access across threads (save/icon reads, the logger,
// and background music streaming all touch stdio or fs:USER). The lock is a
// plain global initialized once by FsGuard::init() on the main thread before
// any other thread exists: a lazily-initialized function-local static here
// previously raced between threads on first use and corrupted memory instead
// of just serializing it.
class FsGuard {
public:
    static void init();

    FsGuard();
    ~FsGuard();
    FsGuard(const FsGuard&) = delete;
    FsGuard& operator=(const FsGuard&) = delete;

private:
    static LightLock lock_;
};
