#pragma once

#include "../TaskManagerInclude.hpp"

#include <string>

namespace TaskManager {

    // Starts the app when the user logs in. On Linux this is an XDG autostart entry, on Windows a "Run" registry value.
    // The entry points at the executable that is currently running.
    bool isAutostartEnabled(const std::string& appName);

    // Returns false if the entry could not be written or removed
    bool setAutostartEnabled(const std::string& appName, bool enabled);

}
