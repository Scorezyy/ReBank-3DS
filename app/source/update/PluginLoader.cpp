#include "update/PluginLoader.hpp"

#include "core/Logger.hpp"

#include <3ds.h>

namespace {
constexpr const char* ServiceName = "plg:ldr";
constexpr u32 IsEnabledCommand = 2;
constexpr u32 SetStateCommand = 3;

bool isEnabled(Handle service, bool& enabled) {
    u32* command = getThreadCommandBuffer();
    command[0] = IPC_MakeHeader(IsEnabledCommand, 0, 0);
    if (R_FAILED(svcSendSyncRequest(service)) || R_FAILED(static_cast<Result>(command[1]))) {
        return false;
    }
    enabled = (command[2] & 0xFF) != 0;
    return true;
}

bool setState(Handle service, bool enabled) {
    u32* command = getThreadCommandBuffer();
    command[0] = IPC_MakeHeader(SetStateCommand, 1, 0);
    command[1] = enabled ? 1 : 0;
    return R_SUCCEEDED(svcSendSyncRequest(service)) && R_SUCCEEDED(static_cast<Result>(command[1]));
}
}

bool PluginLoader::enable() {
    Handle service = 0;
    if (R_FAILED(srvGetServiceHandle(&service, ServiceName))) {
        Logger::instance().warning("Plugin loader: the Luma plugin loader service is not available");
        return false;
    }
    bool enabled = false;
    const bool known = isEnabled(service, enabled);
    if (known && !enabled) {
        enabled = setState(service, true) && isEnabled(service, enabled) && enabled;
        Logger::instance().info(enabled ? "Plugin loader: switched on" : "Plugin loader: could not be switched on");
    }
    svcCloseHandle(service);
    return known && enabled;
}
