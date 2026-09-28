#pragma once

#include <3ds.h>

class FsGuard {
public:
    FsGuard();
    ~FsGuard();
    FsGuard(const FsGuard&) = delete;
    FsGuard& operator=(const FsGuard&) = delete;
};
