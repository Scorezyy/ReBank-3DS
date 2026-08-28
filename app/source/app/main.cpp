#include "app/App.hpp"

#include <3ds.h>

extern "C" u32 __stacksize__ = 1 * 1024 * 1024;

int main(int argc, char* argv[]) {
    const bool cfgReady = R_SUCCEEDED(cfguInit());
    const std::string executablePath = argc > 0 && argv[0] ? argv[0] : "";
    App app(executablePath, envIsHomebrew());
    const int result = app.run();
    if (cfgReady) {
        cfguExit();
    }
    return result;
}
