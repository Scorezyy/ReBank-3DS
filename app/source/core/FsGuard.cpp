#include "core/FsGuard.hpp"

namespace {
RecursiveLock& fileSystemLock() {
    static RecursiveLock lock = [] {
        RecursiveLock created;
        RecursiveLock_Init(&created);
        return created;
    }();
    return lock;
}
}

FsGuard::FsGuard() {
    RecursiveLock_Lock(&fileSystemLock());
}

FsGuard::~FsGuard() {
    RecursiveLock_Unlock(&fileSystemLock());
}
