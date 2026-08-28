#pragma once

#include <string>

// Derives a stable per-console fingerprint from hardware-backed identifiers:
// PS_GetDeviceId (fixed at the factory, never resettable) and the keyY stored
// in nand:/private/movable.sed (only changes on a System Format or System
// Transfer, unlike the local friend code seed, which the system can regenerate
// on its own). Returns an empty string when either value is unavailable, which
// the caller treats as "this console could not be verified".
class DeviceIdentity {
public:
    std::string fingerprint();

private:
    bool resolved_ = false;
    std::string fingerprint_;
};
