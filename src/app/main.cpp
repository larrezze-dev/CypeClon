#include "core/memory.hpp"
#include "gui/app_window.hpp"

#include <string_view>

int main(int argc, char* argv[]) {
    cypeclon::core::MemoryTracker::inicia();

    bool headless = true;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--gui") {
            headless = false;
        } else if (arg == "--headless-check" || arg == "--version") {
            headless = true;
        }
    }

    cypeclon::gui::AppWindow app;
    const int exit_code = headless ? app.run_headless_self_check() : app.run_win32_window();

    cypeclon::core::MemoryTracker::fin();
    return exit_code;
}

