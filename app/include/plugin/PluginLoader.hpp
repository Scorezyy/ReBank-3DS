#pragma once

enum class PluginLoaderState { Enabled, Manual, Missing };

namespace PluginLoader {
PluginLoaderState enable();
}
