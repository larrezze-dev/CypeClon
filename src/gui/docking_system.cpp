#include "gui/docking_system.hpp"

#include <algorithm>

namespace cypeclon::gui {

DockingSystem::DockingSystem() {
    // 7 Categorías estructurales exactas del panel "ed3base_pncapas_visualizacion" (06_estructura_tab_obra.png)
    panel_capas_.categorias = {
        {CategoriaEstructural::Nudos,    "Nudos",    true, false},
        {CategoriaEstructural::Barras,   "Barras",   true, false},
        {CategoriaEstructural::Tirantes, "Tirantes", true, false},
        {CategoriaEstructural::Pilares,  "Pilares",  true, false},
        {CategoriaEstructural::Vigas,    "Vigas",    true, false},
        {CategoriaEstructural::Panyos,   "Paños",    true, false},
        {CategoriaEstructural::Laminas,  "Láminas",  true, false}
    };

    // 10 Iconos verticales (24x24 px) de CypeDialogoInterior en el Área de trabajo (06_estructura_tab_obra.png)
    viewport_toolbar_ = {
        {"v3d_unifilar",          "Modelo alámbrico / unifilar",                "v3d_unifilar",  true},
        {"v3d_perfil_medio",      "Perfil real con ejes medios",                "v3d_perfmed",   false},
        {"v3d_perfil_extruido",   "Perfil real extruido con espesor",           "v3d_perfext",   false},
        {"v3d_con_panyos",        "Mostrar / ocultar paños de carga",           "v3d_panyos",    true},
        {"v3d_con_laminas",       "Mostrar / ocultar discretización láminas",   "v3d_laminas",   true},
        {"v3d_apoyos",            "Mostrar / ocultar vinculaciones exteriores", "v3d_apoyos",    true},
        {"v3d_perspectiva",       "Conmutar proyección cónica / isométrica",    "v3d_persp",     false},
        {"v3d_orbita",            "Órbita libre 3D",                            "v3d_orbita",    false},
        {"v3d_zoom_ventana",      "Zoom ventana",                               "v3d_zoomv",     false},
        {"v3d_zoom_completo",     "Ver todo / extensión completa",              "v3d_zoomext",   false}
    };

    // Configuración de barra de capturas en Cimentación (plcpanel.c!0x180097250)
    captures_cimentacion_.plantilla_dxf = true;
    captures_cimentacion_.rejilla = true;

    recalculate_layout(1920, 1032, model::SolapaActiva::Estructura);
}

bool DockingSystem::toggle_viewport_tool(std::string_view tool_id) noexcept {
    for (auto& btn : viewport_toolbar_) {
        if (btn.id == tool_id) {
            btn.active = !btn.active;
            return true;
        }
    }
    return false;
}

const CadCaptureBarConfig& DockingSystem::cad_captures(model::SolapaActiva solapa) const noexcept {
    return (solapa == model::SolapaActiva::Estructura) ? captures_estructura_ : captures_cimentacion_;
}

CadCaptureBarConfig& DockingSystem::cad_captures_mut(model::SolapaActiva solapa) noexcept {
    return (solapa == model::SolapaActiva::Estructura) ? captures_estructura_ : captures_cimentacion_;
}

bool DockingSystem::set_categoria_visible(CategoriaEstructural cat, bool visible) noexcept {
    for (auto& item : panel_capas_.categorias) {
        if (item.categoria == cat) {
            item.visible = visible;
            return true;
        }
    }
    return false;
}

bool DockingSystem::set_categoria_bloqueada(CategoriaEstructural cat, bool bloqueada) noexcept {
    for (auto& item : panel_capas_.categorias) {
        if (item.categoria == cat) {
            item.bloqueada = bloqueada;
            return true;
        }
    }
    return false;
}

const CategoriaVisualizacionItem* DockingSystem::find_categoria(CategoriaEstructural cat) const noexcept {
    for (const auto& item : panel_capas_.categorias) {
        if (item.categoria == cat) {
            return &item;
        }
    }
    return nullptr;
}

void DockingSystem::recalculate_layout(int client_width, int client_height, model::SolapaActiva solapa) noexcept {
    const int w = std::max(client_width, 640);
    const int h = std::max(client_height, 480);

    layout_.cype_aplicacion = {0, 0, w, h};
    layout_.barra_titulo = {0, 0, w, 26};
    layout_.barra_qat = {8, 2, 260, 22};
    layout_.barra_tabs_ribbon = {0, 26, w, 24};
    layout_.banda_ribbon = {0, 51, w, 92};
    layout_.barra_superior_cad = {0, 143, w, 37};

    const int status_h = 18;
    const int tabstrip_h = 18;
    const int status_y = h - status_h;
    const int tabstrip_y = status_y - 2 - tabstrip_h;

    layout_.cype_mini_solapas = {0, 50, w, status_y - 50};
    layout_.franja_selector_solapas = {0, tabstrip_y, w, tabstrip_h};
    layout_.tab_solapa_estructura = {4, tabstrip_y, 92, tabstrip_h};
    layout_.tab_solapa_cimentacion = {98, tabstrip_y, 100, tabstrip_h};
    layout_.barra_estado = {0, status_y, w, status_h};

    if (solapa == model::SolapaActiva::Estructura) {
        // En Solapa Estructura: Sub-dock izquierdo vertical (382 px) + Área de trabajo derecha (ed3base_crea_solapa!0x1800f2e10)
        layout_.left_dock_visible = panel_capas_.desplegado || panel_preferencias_.desplegado;
        const int dock_w = layout_.left_dock_visible ? 382 : 0;
        const int work_top = 180;
        const int work_h = std::max(100, tabstrip_y - work_top);

        layout_.sub_dock_izquierdo = {0, work_top, dock_w, work_h};
        const int upper_h = (work_h * 56) / 100;
        layout_.panel_etiquetas_categorias = {0, work_top, dock_w, upper_h};
        layout_.panel_opciones_visualizacion = {0, work_top + upper_h, dock_w, work_h - upper_h};

        const int mdi_x = layout_.left_dock_visible ? 390 : 0;
        const int mdi_w = std::max(100, w - mdi_x);
        layout_.cype_mdi_frame = {mdi_x, work_top, mdi_w, work_h};
        layout_.cype_dialogo_interior = {mdi_x, work_top, mdi_w, work_h};
        layout_.barra_vertical_vista_3d = {mdi_x, work_top + 20, 24, std::min(240, work_h - 20)};
        layout_.cype_area_dibujo = {mdi_x + 24, work_top + 20, std::max(60, mdi_w - 24), std::max(60, work_h - 20)};
    } else {
        // En Solapa Cimentación: Sin panel lateral de docking (panel_dock = NULL en plcpanel_crea_solapa_cimentacion)
        // CypeAreaDibujo se expande al ancho completo (0, 142, 1920x852 en resolución 1920x1032)
        layout_.left_dock_visible = false;
        layout_.sub_dock_izquierdo = {0, 142, 0, 0};
        layout_.panel_etiquetas_categorias = {0, 142, 0, 0};
        layout_.panel_opciones_visualizacion = {0, 142, 0, 0};

        const int cim_top = 142;
        const int cim_h = std::max(100, tabstrip_y - cim_top);
        layout_.cype_mdi_frame = {0, cim_top, w, cim_h};
        layout_.cype_dialogo_interior = {0, cim_top, w, cim_h};
        layout_.barra_vertical_vista_3d = {0, cim_top, 0, 0};
        layout_.cype_area_dibujo = {0, cim_top, w, cim_h};
    }
}

} // namespace cypeclon::gui

