#pragma once

#include <3ds.h>

#include <cstdint>

namespace Clock {
inline std::uint64_t now() {
    return svcGetSystemTick();
}

inline double seconds() {
    return static_cast<double>(now()) / SYSCLOCK_ARM11;
}

inline double secondsSince(std::uint64_t tick) {
    return static_cast<double>(now() - tick) / SYSCLOCK_ARM11;
}

inline std::uint64_t millisecondsSince(std::uint64_t tick) {
    return (now() - tick) * 1000 / SYSCLOCK_ARM11;
}

inline std::uint64_t after(double seconds) {
    return now() + static_cast<std::uint64_t>(seconds * SYSCLOCK_ARM11);
}

inline bool reached(std::uint64_t deadline) {
    return now() >= deadline;
}
}
