#pragma once

class CartridgeSlotMonitor {
public:
    enum class Change { None, Inserted, Removed };

    Change poll();
    void forget() { known_ = false; }

private:
    bool known_ = false;
    bool inserted_ = false;
    bool firstResultLogged_ = false;
};
