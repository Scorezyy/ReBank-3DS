#include "FsGuard.hpp"

LightLock FsGuard::lock_;

void FsGuard::init() {
    LightLock_Init(&lock_);
}

FsGuard::FsGuard() {
    LightLock_Lock(&lock_);
}

FsGuard::~FsGuard() {
    LightLock_Unlock(&lock_);
}
