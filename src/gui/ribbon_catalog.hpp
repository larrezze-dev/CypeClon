#pragma once

#include "gui/theme_palette.hpp"
#include "model/obra.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::gui {

/**
 * @brief Tipo de botón/elemento en `entomenu.dll` (`ebaboton_nousar_crea_*` / `ebamenu_*`).
 */
enum class RibbonButtonKind : std::uint8_t {
    BotonDlgObra = 0,          ///< ebaboton_nousar_crea_boton_dlg_obra_u
    BotonPanelObra = 1,        ///< ebaboton_nousar_crea_boton_panel_obra_u
    ReceptorOpcion = 2,        ///< ebaboton_nousar_crea_receptor_opcion_u
    BotonInterruptor = 3,      ///< ebaboton_nousar_crea_boton_interruptor_u
    BotonMostrarErrores = 4,   ///< ebaboton_nousar_crea_boton_mostrar_errores
    SubmenuDesplegable = 5,    ///< ebamenu_crea_con_icono_u / ebamenu_append_submenu_desplegable
    SubmenuModeless = 6,       ///< ebamenu_append_submenu_modeless
    Separador = 7,             ///< ebamenu_append_separador
    ExtensorDerecha = 8        ///< ebamenu_append_extensor
};

/**
 * @brief Modo del menú de Uniones en `ed3dlib.dll` (`e3dpermis_con_permisos_uniones_editables`).
 */
enum class UnionesMenuMode : std::uint8_t {
    UnionesEd = 0,          ///< "uniones_ed" (0x1803dc290 — CYPE Connect / StruBIM Steel integrado, v2027 por defecto)
    UnionesE3diClasico = 1  ///< "uniones_e3di" (0x1803da4a0 -> "uniones_auto" + "Placas_Anclaje")
};

struct RibbonCommandItem {
    std::string id;              ///< ID interno en entomenu.dll, p.ej. "Obra_DatosGenerales", "Nudo_Nuevo"
    std::string label;           ///< Etiqueta visible en UI
    std::string icon_resource;   ///< Clave de icono en componen.dll, p.ej. "datobra2", "nudo_new2"
    std::string source_tag;      ///< Tag de fichero fuente original, p.ej. "ed3mnuob.c (ED3DLIB.DLL)"
    RibbonButtonKind kind{RibbonButtonKind::ReceptorOpcion};
    bool enabled{true};
    bool toggled{false};         ///< Estado activo para BotonInterruptor
    bool conditional{false};     ///< Botón condicional (p.ej. niveles/plantas o param_1 en Cimentación)
    std::vector<RibbonCommandItem> children; ///< Subcomandos si es SubmenuDesplegable o SubmenuModeless
};

struct RibbonGroup {
    std::string id;              ///< ID interno del grupo ("obra", "numeracion", "Vistas3D", "menu_cimentacion", etc.)
    std::string title;           ///< Título visible en la base del grupo del Ribbon
    std::string icon_resource;   ///< Icono de grupo cuando se contrae
    bool aligned_right{false};   ///< Precedido por ebamenu_append_extensor
    std::vector<RibbonCommandItem> items;
};

struct RibbonTab {
    std::string id;              ///< ID interno ("obra", "geometria", "propiedades", "carga", "uniones", "calculo", "armaduras", "ventana")
    std::string title;           ///< Etiqueta visible de la pestaña
    std::vector<RibbonGroup> groups;
};

struct ClassicMenuBarDropdown {
    std::string id;
    std::string title;
    std::vector<std::string> command_ids;
};

/**
 * @brief Catálogo completo de Ribbons, menús desplegables y comandos de las solapas
 *        **Estructura** (`"METAL3D_ESTRUCTURA"`, `ed3dlib.dll!0x1800f3120`) y
 *        **Cimentación** (`"ID_CIMENTACION"`, `planocim.dll!0x180097ed0`).
 */
class RibbonCatalog {
public:
    explicit RibbonCatalog(bool admitir_elementos_lineales_cimentacion = true);

    // Solapa Estructura ("METAL3D_ESTRUCTURA" - 8 pestañas Ribbon)
    const std::vector<RibbonTab>& estructura_tabs() const noexcept { return estructura_tabs_; }
    std::size_t active_estructura_tab_index() const noexcept { return active_estructura_tab_; }
    bool set_active_estructura_tab_index(std::size_t idx) noexcept;
    bool set_active_estructura_tab_by_id(std::string_view tab_id) noexcept;
    const RibbonTab& current_estructura_tab() const noexcept { return estructura_tabs_[active_estructura_tab_]; }

    // Modo de Uniones ("uniones_ed" vs "uniones_e3di" + "Placas_Anclaje")
    UnionesMenuMode uniones_mode() const noexcept { return uniones_mode_; }
    void set_uniones_mode(UnionesMenuMode mode);
    const RibbonGroup& uniones_ed_group() const noexcept { return uniones_ed_group_; }
    const std::vector<RibbonGroup>& uniones_e3di_groups() const noexcept { return uniones_e3di_groups_; }

    // Menú auxiliar de exportación ("Menu_Exportacion" en ed3mnuob.c!0x1803daa40)
    const RibbonGroup& menu_exportacion() const noexcept { return menu_exportacion_; }

    // Solapa Cimentación ("ID_CIMENTACION" - Banda única "menu_unico" con aspecto 0x10 y 4 grupos)
    static constexpr std::string_view kCimentacionMenuId = "menu_unico";
    static constexpr std::uint32_t kCimentacionRibbonAspect = 0x10;
    bool admite_elementos_lineales_cimentacion() const noexcept { return admitir_elem_lineales_cim_; }
    const std::vector<RibbonGroup>& cimentacion_groups() const noexcept { return cimentacion_groups_; }

    // Barras de menús clásicas superiores
    const std::vector<ClassicMenuBarDropdown>& classic_menubar(model::SolapaActiva solapa) const noexcept;

    // Búsqueda e inspección de comandos en todo el árbol
    const RibbonCommandItem* find_command(std::string_view command_id) const noexcept;
    RibbonCommandItem* find_command_mut(std::string_view command_id) noexcept;
    std::size_t total_registered_commands() const noexcept;

    // Precarga todos los iconos del catálogo en la caché de ThemePalette (imagen_nousar_carga_recurso)
    void preload_icons(ThemePalette& palette) const;

private:
    void build_estructura_ribbon();
    void build_cimentacion_ribbon();
    void build_classic_menubars();

    static const RibbonCommandItem* find_in_items(const std::vector<RibbonCommandItem>& items, std::string_view id) noexcept;
    static RibbonCommandItem* find_in_items_mut(std::vector<RibbonCommandItem>& items, std::string_view id) noexcept;
    static std::size_t count_items_recursive(const std::vector<RibbonCommandItem>& items) noexcept;
    static void preload_items_icons(const std::vector<RibbonCommandItem>& items, ThemePalette& palette);

    bool admitir_elem_lineales_cim_{true};
    UnionesMenuMode uniones_mode_{UnionesMenuMode::UnionesEd};
    std::size_t active_estructura_tab_{0};

    std::vector<RibbonTab> estructura_tabs_;
    RibbonGroup uniones_ed_group_{};
    std::vector<RibbonGroup> uniones_e3di_groups_;
    RibbonGroup menu_exportacion_{};
    std::vector<RibbonGroup> cimentacion_groups_;

    std::vector<ClassicMenuBarDropdown> classic_menu_estructura_;
    std::vector<ClassicMenuBarDropdown> classic_menu_cimentacion_;
};

} // namespace cypeclon::gui

