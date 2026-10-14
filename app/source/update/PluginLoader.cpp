#include "update/PluginLoader.hpp"

#include "core/Logger.hpp"

#include <3ds.h>

#include <atomic>
#include <memory>
#include <string>

namespace {
constexpr const char* PortName = "plg:ldr";
constexpr Result PortNotFound = static_cast<Result>(0xD88007FA);
constexpr u32 IsEnabledCommand = 2;
constexpr u32 SetStateCommand = 3;
constexpr s64 TimeoutNanoseconds = 3'000'000'000LL;
constexpr std::size_t StackSize = 8 * 1024;
constexpr s32 WorkerPriority = 0x30;

struct Attempt {
    LightEvent done;
    std::atomic<PluginLoaderState> state{PluginLoaderState::Manual};
};

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

PluginLoaderState switchOn() {
    Handle port = 0;
    const Result opened = svcConnectToPort(&port, PortName);
    if (opened == PortNotFound) {
        Logger::instance().warning("Plugin loader: this Luma has no plugin loader (plg:ldr missing)");
        return PluginLoaderState::Missing;
    }
    if (R_FAILED(opened)) {
        Logger::instance().warning("Plugin loader: plg:ldr could not be opened (" + std::to_string(static_cast<u32>(opened)) + ")");
        return PluginLoaderState::Manual;
    }
    bool enabled = false;
    const bool known = isEnabled(port, enabled);
    if (known && !enabled) {
        enabled = setState(port, true) && isEnabled(port, enabled) && enabled;
        Logger::instance().info(enabled ? "Plugin loader: switched on" : "Plugin loader: could not be switched on");
    }
    svcCloseHandle(port);
    return known && enabled ? PluginLoaderState::Enabled : PluginLoaderState::Manual;
}

void worker(void* argument) {
    std::unique_ptr<std::shared_ptr<Attempt>> owner(static_cast<std::shared_ptr<Attempt>*>(argument));
    const std::shared_ptr<Attempt> attempt = *owner;
    attempt->state.store(switchOn());
    LightEvent_Signal(&attempt->done);
}
}

PluginLoaderState PluginLoader::enable() {
    const auto attempt = std::make_shared<Attempt>();
    LightEvent_Init(&attempt->done, RESET_ONESHOT);
    auto* handover = new std::shared_ptr<Attempt>(attempt);
    if (!threadCreate(worker, handover, StackSize, WorkerPriority, -2, true)) {
        delete handover;
        Logger::instance().warning("Plugin loader: worker creation failed");
        return PluginLoaderState::Manual;
    }
    if (LightEvent_WaitTimeout(&attempt->done, TimeoutNanoseconds) != 0) {
        Logger::instance().warning("Plugin loader: no answer from the Luma plugin loader");
        return PluginLoaderState::Manual;
    }
    return attempt->state.load();
}
