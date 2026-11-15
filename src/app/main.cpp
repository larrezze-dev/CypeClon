#include "core/memory.hpp"
#include "gui/app_window.hpp"

#include <cstdio>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
    cypeclon::core::MemoryTracker::inicia();

    // Por defecto iniciar la ventana gráfica nativa Win32/OpenGL de CYPE 3D (CypeClon.exe)
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--gui") {
            headless = false;
        } else if (arg == "--headless-check" || arg == "--version") {
            headless = true;
        }
    }

#ifdef _WIN32
    if (headless) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* fp_out = nullptr;
            FILE* fp_err = nullptr;
            freopen_s(&fp_out, "CONOUT$", "w", stdout);
            freopen_s(&fp_err, "CONOUT$", "w", stderr);
        }
    }
#endif

    cypeclon::gui::AppWindow app;
    const int exit_code = headless ? app.run_headless_self_check() : app.run_win32_window();

    cypeclon::core::MemoryTracker::fin();
    return exit_code;
}

