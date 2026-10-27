#include <Pch.hpp>

#include "Autostart.hpp"

#include <VelyraUtils/PlatformDetection.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#if defined(VL_PLATFORM_WINDOWS)
#include <windows.h>
#endif

namespace TaskManager {

    namespace fs = std::filesystem;

#if defined(VL_PLATFORM_LINUX)

    namespace {

        fs::path getAutostartFile(const std::string& appName) {
            fs::path base;
            if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
                base = xdg;
            } else if (const char* home = std::getenv("HOME"); home != nullptr) {
                base = fs::path(home) / ".config";
            }
            return base / "autostart" / (appName + ".desktop");
        }

    }

    bool isAutostartEnabled(const std::string& appName) {
        std::error_code ec;
        return fs::exists(getAutostartFile(appName), ec);
    }

    bool setAutostartEnabled(const std::string& appName, const bool enabled) {
        const fs::path file = getAutostartFile(appName);
        std::error_code ec;
        if (!enabled) {
            fs::remove(file, ec);
            return !ec;
        }

        const fs::path executable = fs::read_symlink("/proc/self/exe", ec);
        if (ec) {
            return false;
        }
        fs::create_directories(file.parent_path(), ec);
        std::ofstream out(file, std::ios::trunc);
        if (!out) {
            return false;
        }
        out << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=" << appName << "\n"
            << "Comment=Manage your tasks and TODOs\n"
            << "Exec=\"" << executable.string() << "\"\n"
            << "Terminal=false\n"
            << "X-GNOME-Autostart-enabled=true\n";
        return static_cast<bool>(out);
    }

#elif defined(VL_PLATFORM_WINDOWS)

    namespace {

        constexpr const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

        std::wstring toWide(const std::string& text) {
            return fs::path(text).wstring();
        }

    }

    bool isAutostartEnabled(const std::string& appName) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &key) != ERROR_SUCCESS) {
            return false;
        }
        const LONG result = RegQueryValueExW(key, toWide(appName).c_str(), nullptr, nullptr, nullptr, nullptr);
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }

    bool setAutostartEnabled(const std::string& appName, const bool enabled) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
            return false;
        }
        LONG result;
        if (enabled) {
            wchar_t executable[MAX_PATH];
            GetModuleFileNameW(nullptr, executable, MAX_PATH);
            const std::wstring command = L"\"" + std::wstring(executable) + L"\"";
            result = RegSetValueExW(key, toWide(appName).c_str(), 0, REG_SZ,
                                    reinterpret_cast<const BYTE*>(command.c_str()),
                                    static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        } else {
            result = RegDeleteValueW(key, toWide(appName).c_str());
        }
        RegCloseKey(key);
        return result == ERROR_SUCCESS || (!enabled && result == ERROR_FILE_NOT_FOUND);
    }

#else

    bool isAutostartEnabled(const std::string&) {
        return false;
    }

    bool setAutostartEnabled(const std::string&, bool) {
        return false;
    }

#endif

}
