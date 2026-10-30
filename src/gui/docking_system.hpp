#pragma once

#include "model/obra.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::gui {

struct RectPx {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    constexpr int right() const noexcept { return x + width; }
    constexpr int bottom() const noexcept { return y + height; }
    constexpr bool contains(int px, int py) const noexcept {
        return px >= x && px < right() && py >= y && py < bottom();
    }
};

/**
 * @brief Dirección de división en `ribbon.dll!pndock_crea(id, dir)`.
 */
enum class DockSplitDirection : std::uint8_t {
    Vertical = 0,   ///< Sub-dock vertical (arriba / abajo)
    Horizontal = 1  ///< Split horizontal (izquierda / derecha)
};

/**
 * @brief Categorías estructurales del panel superior "ed3base_pncapas_visualizacion" (`06_estructura_tab_obra.png`).
 */
enum class CategoriaEstructural : std::uint8_t {
    Nudos = 0,
    Barras = 1,
    Tirantes = 2,
    Pilares = 3,
    Vigas = 4,
    Panyos = 5,
    Laminas = 6
};

struct CategoriaVisualizacionItem {
    CategoriaEstructural categoria{CategoriaEstructural::Nudos};
    std::string nombre;
    bool visible{true};
    bool bloqueada{false};
};

/**
 * @brief Estado del panel superior izquierdo `"ed3base_pncapas_visualizacion"` (`Etiquetas` y `Categorías`).
 */
struct PanelCapasVisualizacionState {
    std::string panel_id{"ed3base_pncapas_visualizacion"};
    std::string titulo_etiquetas{"Etiquetas"};
    std::string titulo_categorias{"Categorías"};
    bool desplegado{true};
    std::uint32_t capa_activa_id{1};
    bool representar_usando_colores_capas{false};
    std::string elementos_considerados_analisis{"Tensión / Deformación"};
    std::vector<CategoriaVisualizacionItem> categorias;
};

/**
 * @brief Estado del panel inferior izquierdo `"ed3base_pncapas_preferencias_dibujo"` (`Opciones de visualización`).
 */
struct PanelPreferenciasDibujoState {
    std::string panel_id{"ed3base_pncapas_preferencias_dibujo"};
    std::string titulo{"Opciones de visualización"};
    bool desplegado{true};

    bool mostrar_informacion_elementos{false};
    bool mostrar_textos_elementos{true};
    bool mostrar_ejes_locales{false};
    bool dibujar_laminas_con_relleno{true};
    bool dibujar_cargas_con_relleno{true};

    double dimensiones_nudos_cm{15.0};
    double tamano_textos_pct{100.0};
    std::string posicion_tooltips{"En la esquina superior derecha"};
};

/**
 * @brief Botón de la barra vertical izquierda (`24x24` px) dentro de `CypeDialogoInterior`.
 */
struct ViewportToolButton {
    std::string id;
    std::string tooltip;
    std::string icon_resource;
    bool active{false};
};

/**
 * @brief Configuración de la barra superior de capturas CAD (`ebarcbca_crea_configuracion_barra_capturas`).
 */
struct CadCaptureBarConfig {
    bool activa{true};
    bool extremo{true};
    bool punto_medio{true};
    bool perpendicular{true};
    bool interseccion{true};
    bool mas_cercano{false};
    bool rastreo_polar{true};
    bool rejilla{false};
    bool plantilla_dxf{true};
};

/**
 * @brief Geometría calculada de todas las clases de ventana Win32 documentadas en la sección 2 de `ui_menus_and_tabs_spec.md`.
 */
struct WindowLayoutGeometry {
    RectPx cype_aplicacion{0, 0, 1920, 1032};
    RectPx barra_titulo{0, 0, 1920, 26};
    RectPx barra_qat{8, 2, 260, 22};
    RectPx barra_tabs_ribbon{0, 26, 1920, 24};
    RectPx banda_ribbon{0, 51, 1920, 92};
    RectPx barra_superior_cad{0, 143, 1920, 37};
    RectPx cype_mini_solapas{0, 50, 1920, 964};

    // Paneles laterales pndock (visibles solo en Estructura)
    bool left_dock_visible{true};
    RectPx sub_dock_izquierdo{0, 180, 382, 814};
    RectPx panel_etiquetas_categorias{0, 180, 382, 460};
    RectPx panel_opciones_visualizacion{0, 640, 382, 354};

    // Área de trabajo / MDI / OpenGL
    RectPx cype_mdi_frame{390, 180, 1530, 814};
    RectPx cype_dialogo_interior{390, 180, 1530, 814};
    RectPx barra_vertical_vista_3d{390, 200, 24, 240};
    RectPx cype_area_dibujo{414, 200, 1506, 794};

    // Selector inferior de solapas y barra de estado
    RectPx franja_selector_solapas{0, 994, 1920, 18};
    RectPx tab_solapa_estructura{4, 994, 92, 18};
    RectPx tab_solapa_cimentacion{98, 994, 100, 18};
    RectPx barra_estado{0, 1014, 1920, 18};
};

/**
 * @brief Sistema de paneles acoplables `pndock` (`ribbon.dll` / `entoento.dll`) y conmutador de solapas
 *        `CypeMiniSolapas` (**Estructura** `"METAL3D_ESTRUCTURA"` y **Cimentación** `"ID_CIMENTACION"`).
 */
class DockingSystem {
public:
    DockingSystem();

    const std::string& root_dock_id() const noexcept { return root_dock_id_; }
    DockSplitDirection root_split_direction() const noexcept { return root_split_dir_; }

    PanelCapasVisualizacionState& panel_capas() noexcept { return panel_capas_; }
    const PanelCapasVisualizacionState& panel_capas() const noexcept { return panel_capas_; }

    PanelPreferenciasDibujoState& panel_preferencias() noexcept { return panel_preferencias_; }
    const PanelPreferenciasDibujoState& panel_preferencias() const noexcept { return panel_preferencias_; }

    const std::vector<ViewportToolButton>& viewport_toolbar() const noexcept { return viewport_toolbar_; }
    bool toggle_viewport_tool(std::string_view tool_id) noexcept;

    const CadCaptureBarConfig& cad_captures(model::SolapaActiva solapa) const noexcept;
    CadCaptureBarConfig& cad_captures_mut(model::SolapaActiva solapa) noexcept;

    bool set_categoria_visible(CategoriaEstructural cat, bool visible) noexcept;
    bool set_categoria_bloqueada(CategoriaEstructural cat, bool bloqueada) noexcept;
    const CategoriaVisualizacionItem* find_categoria(CategoriaEstructural cat) const noexcept;

    void recalculate_layout(int client_width, int client_height, model::SolapaActiva solapa) noexcept;
    const WindowLayoutGeometry& layout() const noexcept { return layout_; }

    const std::string& active_mdi_view_title() const noexcept { return active_mdi_view_title_; }
    void set_active_mdi_view_title(std::string title) noexcept { active_mdi_view_title_ = std::move(title); }

private:
    std::string root_dock_id_{"ed3base_dock"};
    DockSplitDirection root_split_dir_{DockSplitDirection::Horizontal};

    PanelCapasVisualizacionState panel_capas_{};
    PanelPreferenciasDibujoState panel_preferencias_{};
    std::vector<ViewportToolButton> viewport_toolbar_;
    CadCaptureBarConfig captures_estructura_{};
    CadCaptureBarConfig captures_cimentacion_{};
    WindowLayoutGeometry layout_{};
    std::string active_mdi_view_title_{"3D"};
};

} // namespace cypeclon::gui

