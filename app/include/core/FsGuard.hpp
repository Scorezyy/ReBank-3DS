#pragma once

#include <3ds.h>

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
