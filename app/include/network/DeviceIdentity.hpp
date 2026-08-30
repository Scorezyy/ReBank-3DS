#pragma once

#include <string>

class DeviceIdentity {
public:
    std::string fingerprint();

private:
    bool resolved_ = false;
    std::string fingerprint_;
};
