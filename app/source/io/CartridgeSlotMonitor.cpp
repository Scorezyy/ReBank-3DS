#include "io/CartridgeSlotMonitor.hpp"
#include "core/Logger.hpp"

#include <3ds.h>

#include <string>

CartridgeSlotMonitor::Change CartridgeSlotMonitor::poll() {
    bool inserted = false;
    const Result result = FSUSER_CardSlotIsInserted(&inserted);
    if (!firstResultLogged_) {
        firstResultLogged_ = true;
        Logger::instance().info("FSUSER_CardSlotIsInserted: result=" + std::to_string(result)
                                + " inserted=" + std::to_string(inserted));
    }
    if (R_FAILED(result)) {
        return Change::None;
    }
    if (!known_ || inserted == inserted_) {
        known_ = true;
        inserted_ = inserted;
        return Change::None;
    }
    inserted_ = inserted;
    Logger::instance().info(std::string("Cartridge slot state changed: ") + (inserted ? "inserted" : "removed"));
    return inserted ? Change::Inserted : Change::Removed;
}
