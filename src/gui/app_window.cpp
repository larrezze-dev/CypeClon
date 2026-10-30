#include "gui/app_window.hpp"
#include "core/string_utils.hpp"
#include "units/units.hpp"

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cypeclon::gui {

AppWindow::AppWindow() {
    // 8 Pestañas Ribbon de la solapa Estructura (METAL3D_ESTRUCTURA en ed3dlib.dll!0x1800f3120)
    estructura_tabs_ = {
        {"obra",        "Obra",        {"Obra", "Numeración", "Vista 3D", "Listados", "BIMserver.center"}},
        {"geometria",   "Geometría",   {"Planos", "Nudos", "Barras", "Láminas", "Herramientas", "Importar"}},
        {"propiedades", "Propiedades", {"Diafragmas", "Nudos", "Barras", "Láminas"}},
        {"carga",       "Carga",       {"Visualización", "Nudos", "Barras", "Láminas", "Paños", "Cargas superficiales"}},
        {"uniones",     "Uniones",     {"Uniones"}},
        {"calculo",     "Cálculo",     {"Tensión / Deformación", "Pandeo", "Modal", "Pushover", "Incidencias"}},
        {"armaduras",   "Armaduras",   {"Pilares", "Vigas"}},
        {"ventana",     "Ventana",     {"Ventana"}}
    };

    // 4 Grupos de banda única en la solapa Cimentación (ID_CIMENTACION en planocim.dll!0x180097ed0)
    cimentacion_groups_ = {
        "Obra",
        "Elementos de cimentación",
        "Límites",
        "Cálculo"
    };
}

std::string AppWindow::window_title() const {
    return "CYPE 3D v2027.a (CypeClon C++20) - " + obra_.datos_generales().clave + ".ed3";
}

int AppWindow::run_headless_self_check() const {
    std::cout << "[CypeClon] Ventana principal: " << window_title() << "\n";
    std::cout << "[CypeClon] Sistema de unidades activo: "
              << units::UnitManager::system_name(obra_.datos_generales().sistema_unidades) << "\n";
    std::cout << "[CypeClon] Solapa Estructura (METAL3D_ESTRUCTURA): "
              << estructura_tabs_.size() << " pestanas Ribbon registradas.\n";
    std::cout << "[CypeClon] Solapa Cimentacion (ID_CIMENTACION): "
              << cimentacion_groups_.size() << " grupos de banda registrados.\n";
    return 0;
}

#ifdef _WIN32
namespace {
LRESULT CALLBACK CypeAplicacionWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}
} // namespace
#endif

int AppWindow::run_win32_window() {
#ifdef _WIN32
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = CypeAplicacionWndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_APPWORKSPACE + 1);
    wc.lpszClassName = L"CypeAplicacion";
    RegisterClassExW(&wc);

    const std::wstring wtitle = core::StringUtils::utf8_to_wide(window_title());
    HWND hwnd = CreateWindowExW(
        0,
        L"CypeAplicacion",
        wtitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, hinst, nullptr
    );
    if (!hwnd) {
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
#else
    return run_headless_self_check();
#endif
}

} // namespace cypeclon::gui

