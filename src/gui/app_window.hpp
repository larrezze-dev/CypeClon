#pragma once

#include "model/obra.hpp"

#include <string>
#include <vector>

namespace cypeclon::gui {

struct RibbonTabSpec {
    std::string id;
    std::string title;
    std::vector<std::string> groups;
};

/**
 * @brief Marco de ventana principal de CypeClon equivalente a CypeAplicacion / CypeMiniSolapas
 *        documentado en rea_evidence/ui_menus_and_tabs_spec.md.
 */
class AppWindow {
public:
    AppWindow();

    const model::Obra& obra() const noexcept { return obra_; }
    model::Obra& obra_mut() noexcept { return obra_; }

    const std::vector<RibbonTabSpec>& estructura_ribbon_tabs() const noexcept { return estructura_tabs_; }
    const std::vector<std::string>& cimentacion_ribbon_groups() const noexcept { return cimentacion_groups_; }

    std::string window_title() const;
    int run_headless_self_check() const;
    int run_win32_window();

private:
    model::Obra obra_{};
    std::vector<RibbonTabSpec> estructura_tabs_;
    std::vector<std::string> cimentacion_groups_;
};

} // namespace cypeclon::gui

