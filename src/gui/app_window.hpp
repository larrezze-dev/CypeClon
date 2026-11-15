#pragma once

#include "cad/cad_engine.hpp"
#include "cad/results_visualization_engine.hpp"
#include "codes/design_check_engine.hpp"
#include "connections/connections_engine.hpp"
#include "foundation/foundation_engine.hpp"
#include "gui/docking_system.hpp"
#include "gui/ribbon_catalog.hpp"
#include "gui/theme_palette.hpp"
#include "gui/welcome_and_dialogs.hpp"
#include "io/reports_drawings_bim_engine.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::gui {

struct RibbonTabSpec {
    std::string id;
    std::string title;
    std::vector<std::string> groups;
};

enum class ActiveModalDialog : std::uint8_t {
    None = 0,
    NuevaObraClave = 1,      ///< 02_dialog_nueva_obra.png (537x285)
    AsistenteNuevaObra = 2,  ///< 03_dialog_wizard_nueva_obra.png (682x457)
    DatosGenerales = 3,      ///< 04_dialog_step4.png / 14_dialog_datos_generales.png (848x774)
    EstadosLimite = 4        ///< 05_dialog_step5.png (358x438)
};

/**
 * @brief Marco de ventana principal de CypeClon equivalente a `CypeAplicacion` / `CypeMiniSolapas`
 *        documentado en `rea_evidence/ui_menus_and_tabs_spec.md`.
 */
class AppWindow {
public:
    AppWindow();

    // Acceso al modelo de obra (ed3datos_t + plcdatos_t)
    const model::Obra& obra() const noexcept { return obra_; }
    model::Obra& obra_mut() noexcept { return obra_; }

    // Submódulos de Fase 2 (P-03.1, P-03.2, P-03.3)
    ThemePalette& palette() noexcept { return palette_; }
    const ThemePalette& palette() const noexcept { return palette_; }

    DockingSystem& docking() noexcept { return docking_; }
    const DockingSystem& docking() const noexcept { return docking_; }

    RibbonCatalog& ribbon() noexcept { return ribbon_; }
    const RibbonCatalog& ribbon() const noexcept { return ribbon_; }

    WelcomeScreenModel& welcome_screen() noexcept { return welcome_; }
    const WelcomeScreenModel& welcome_screen() const noexcept { return welcome_; }

    DialogoNuevaObra& dlg_nueva_obra() noexcept { return dlg_nueva_obra_; }
    const DialogoNuevaObra& dlg_nueva_obra() const noexcept { return dlg_nueva_obra_; }

    AsistenteNuevaObra& wizard_nueva_obra() noexcept { return wizard_nueva_obra_; }
    const AsistenteNuevaObra& wizard_nueva_obra() const noexcept { return wizard_nueva_obra_; }

    DialogoDatosGenerales& dlg_datos_generales() noexcept { return dlg_datos_generales_; }
    const DialogoDatosGenerales& dlg_datos_generales() const noexcept { return dlg_datos_generales_; }

    // Submódulos de Fase 3: Motor CAD 2D/3D y Gestión de Vistas MDI (P-04.1, P-04.2, P-04.3)
    cad::MdiViewManager& mdi_manager() noexcept { return mdi_manager_; }
    const cad::MdiViewManager& mdi_manager() const noexcept { return mdi_manager_; }

    cad::CadPicker& picker() noexcept { return picker_; }
    const cad::CadPicker& picker() const noexcept { return picker_; }

    cad::SnapEngine& snap_engine() noexcept { return snap_engine_; }
    const cad::SnapEngine& snap_engine() const noexcept { return snap_engine_; }

    cad::DimensionSystem& dimensions() noexcept { return dimensions_; }
    const cad::DimensionSystem& dimensions() const noexcept { return dimensions_; }

    cad::DxfTemplate& dxf_template() noexcept { return dxf_template_; }
    const cad::DxfTemplate& dxf_template() const noexcept { return dxf_template_; }

    cad::SceneTessellator& scene_tessellator() noexcept { return scene_tessellator_; }
    const cad::SceneTessellator& scene_tessellator() const noexcept { return scene_tessellator_; }

    cad::CadCategoryFilter build_category_filter() const noexcept;
    void sync_cad_configs_from_docking() noexcept;

    // Compatibilidad e inspección rápida de especificaciones de Ribbon
    const std::vector<RibbonTabSpec>& estructura_ribbon_tabs() const noexcept { return estructura_tabs_; }
    const std::vector<std::string>& cimentacion_ribbon_groups() const noexcept { return cimentacion_groups_; }

    // Conmutación de solapas inferiores ("METAL3D_ESTRUCTURA" <-> "ID_CIMENTACION")
    void switch_solapa(model::SolapaActiva nueva_solapa) noexcept;
    model::SolapaActiva active_solapa() const noexcept { return obra_.solapa_activa(); }
    std::string_view active_solapa_internal_id() const noexcept;

    // Gestión de tema Claro / Oscuro (pnpaleta_set_dark_mode) y estilo Ribbon
    void set_dark_mode(bool dark) noexcept;
    bool is_dark_mode() const noexcept { return palette_.pnpaleta_get_dark_mode(); }

    // Secuencia de diálogos modales y bienvenida
    ActiveModalDialog active_modal_dialog() const noexcept { return active_modal_; }
    void close_active_obra() noexcept;
    void begin_nueva_obra_flow() noexcept;
    bool confirm_dialogo_nueva_obra();
    bool confirm_asistente_nueva_obra();
    bool confirm_dialogo_datos_generales();
    void cancel_modal_dialog() noexcept;

    // Despachador de comandos de Ribbon y menús (entomenu.dll)
    bool execute_command(std::string_view command_id);
    const std::string& active_receptor_id() const noexcept { return active_receptor_id_; }
    const std::string& status_bar_text() const noexcept { return status_bar_text_; }
    const solver::SolverSummary& last_solver_summary() const noexcept { return last_solver_summary_; }
    const codes::ResumenComprobacionObra& last_design_summary() const noexcept { return last_design_summary_; }
    const cad::EscenaDeformada3d& last_deformed_scene() const noexcept { return last_deformed_scene_; }
    const std::vector<cad::DiagramaEsfuerzoBarra3d>& last_bar_diagrams() const noexcept { return last_bar_diagrams_; }
    const cad::MapaIsovaloresLamina3d& last_shell_isovalues() const noexcept { return last_shell_isovalues_; }
    const std::vector<cad::VectorReaccionApoyo3d>& last_support_reactions() const noexcept { return last_support_reactions_; }
    const std::vector<cad::ColorAprovechamientoBarra3d>& last_utilization_colors() const noexcept { return last_utilization_colors_; }
    const std::vector<connections::PlacaAnclaje3d>& last_baseplates() const noexcept { return last_baseplates_; }
    const std::vector<connections::UnionEstructural3d>& last_connections() const noexcept { return last_connections_; }
    const foundation::ModeloCompletoCimentacion& last_foundation_model() const noexcept { return last_foundation_model_; }
    foundation::ModeloCompletoCimentacion& last_foundation_model_mut() noexcept { return last_foundation_model_; }
    const io::DocumentoListadoObra& last_report_document() const noexcept { return last_report_doc_; }
    const std::string& last_exported_payload() const noexcept { return last_exported_payload_; }

    std::string window_title() const;
    void load_default_demo_project();
    int run_headless_self_check() const;
    int run_win32_window();

private:
    void sync_status_bar() noexcept;

    model::Obra obra_{};
    solver::SolverSummary last_solver_summary_{};
    codes::ResumenComprobacionObra last_design_summary_{};
    cad::EscenaDeformada3d last_deformed_scene_{};
    std::vector<cad::DiagramaEsfuerzoBarra3d> last_bar_diagrams_{};
    cad::MapaIsovaloresLamina3d last_shell_isovalues_{};
    std::vector<cad::VectorReaccionApoyo3d> last_support_reactions_{};
    std::vector<cad::ColorAprovechamientoBarra3d> last_utilization_colors_{};
    std::vector<connections::PlacaAnclaje3d> last_baseplates_{};
    std::vector<connections::UnionEstructural3d> last_connections_{};
    foundation::ModeloCompletoCimentacion last_foundation_model_{};
    io::DocumentoListadoObra last_report_doc_{};
    std::string last_exported_payload_{};
    ThemePalette palette_{};
    DockingSystem docking_{};
    RibbonCatalog ribbon_{true};
    WelcomeScreenModel welcome_{};

    DialogoNuevaObra dlg_nueva_obra_{};
    AsistenteNuevaObra wizard_nueva_obra_{};
    DialogoDatosGenerales dlg_datos_generales_{};

    cad::MdiViewManager mdi_manager_{};
    cad::CadPicker picker_{};
    cad::SnapEngine snap_engine_{};
    cad::DimensionSystem dimensions_{};
    cad::DxfTemplate dxf_template_{};
    cad::SceneTessellator scene_tessellator_{};

    ActiveModalDialog active_modal_{ActiveModalDialog::None};
    std::string active_receptor_id_;
    std::string status_bar_text_;

    std::vector<RibbonTabSpec> estructura_tabs_;
    std::vector<std::string> cimentacion_groups_;
};

} // namespace cypeclon::gui
