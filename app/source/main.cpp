#include "App.hpp"

#include <3ds.h>

int main(int argc, char* argv[]) {
    const std::string executablePath = argc > 0 && argv[0] ? argv[0] : "";
    App app(executablePath, envIsHomebrew());
    return app.run();
}