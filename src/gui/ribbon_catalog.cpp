#include "gui/ribbon_catalog.hpp"

namespace cypeclon::gui {

namespace {
RibbonCommandItem make_cmd(
    std::string id,
    std::string label,
    std::string icon,
    std::string source_tag,
    RibbonButtonKind kind = RibbonButtonKind::ReceptorOpcion,
    bool conditional = false
) {
    RibbonCommandItem item{};
    item.id = std::move(id);
    item.label = std::move(label);
    item.icon_resource = std::move(icon);
    item.source_tag = std::move(source_tag);
    item.kind = kind;
    item.conditional = conditional;
    return item;
}

RibbonCommandItem make_separator(std::string source_tag) {
    return make_cmd("__sep__", "-", "", std::move(source_tag), RibbonButtonKind::Separador);
}
} // namespace

RibbonCatalog::RibbonCatalog(bool admitir_elementos_lineales_cimentacion)
    : admitir_elem_lineales_cim_(admitir_elementos_lineales_cimentacion) {
    build_estructura_ribbon();
    build_cimentacion_ribbon();
    build_classic_menubars();
}

bool RibbonCatalog::set_active_estructura_tab_index(std::size_t idx) noexcept {
    if (idx >= estructura_tabs_.size()) {
        return false;
    }
    active_estructura_tab_ = idx;
    return true;
}

bool RibbonCatalog::set_active_estructura_tab_by_id(std::string_view tab_id) noexcept {
    for (std::size_t i = 0; i < estructura_tabs_.size(); ++i) {
        if (estructura_tabs_[i].id == tab_id) {
            active_estructura_tab_ = i;
            return true;
        }
    }
    return false;
}

void RibbonCatalog::set_uniones_mode(UnionesMenuMode mode) {
    uniones_mode_ = mode;
    for (auto& tab : estructura_tabs_) {
        if (tab.id == "uniones") {
            tab.groups.clear();
            if (uniones_mode_ == UnionesMenuMode::UnionesEd) {
                tab.groups.push_back(uniones_ed_group_);
            } else {
                tab.groups = uniones_e3di_groups_;
            }
            break;
        }
    }
}

const std::vector<ClassicMenuBarDropdown>& RibbonCatalog::classic_menubar(model::SolapaActiva solapa) const noexcept {
    return (solapa == model::SolapaActiva::Estructura) ? classic_menu_estructura_ : classic_menu_cimentacion_;
}

const RibbonCommandItem* RibbonCatalog::find_in_items(
    const std::vector<RibbonCommandItem>& items,
    std::string_view id
) noexcept {
    for (const auto& item : items) {
        if (item.id == id && item.kind != RibbonButtonKind::Separador) {
            return &item;
        }
        if (!item.children.empty()) {
            if (const auto* child = find_in_items(item.children, id)) {
                return child;
            }
        }
    }
    return nullptr;
}

RibbonCommandItem* RibbonCatalog::find_in_items_mut(
    std::vector<RibbonCommandItem>& items,
    std::string_view id
) noexcept {
    for (auto& item : items) {
        if (item.id == id && item.kind != RibbonButtonKind::Separador) {
            return &item;
        }
        if (!item.children.empty()) {
            if (auto* child = find_in_items_mut(item.children, id)) {
                return child;
            }
        }
    }
    return nullptr;
}

const RibbonCommandItem* RibbonCatalog::find_command(std::string_view command_id) const noexcept {
    for (const auto& tab : estructura_tabs_) {
        for (const auto& grp : tab.groups) {
            if (const auto* found = find_in_items(grp.items, command_id)) {
                return found;
            }
        }
    }
    if (const auto* found = find_in_items(uniones_ed_group_.items, command_id)) {
        return found;
    }
    for (const auto& grp : uniones_e3di_groups_) {
        if (const auto* found = find_in_items(grp.items, command_id)) {
            return found;
        }
    }
    for (const auto& grp : cimentacion_groups_) {
        if (const auto* found = find_in_items(grp.items, command_id)) {
            return found;
        }
    }
    if (const auto* found = find_in_items(menu_exportacion_.items, command_id)) {
        return found;
    }
    return nullptr;
}

RibbonCommandItem* RibbonCatalog::find_command_mut(std::string_view command_id) noexcept {
    for (auto& tab : estructura_tabs_) {
        for (auto& grp : tab.groups) {
            if (auto* found = find_in_items_mut(grp.items, command_id)) {
                return found;
            }
        }
    }
    if (auto* found = find_in_items_mut(uniones_ed_group_.items, command_id)) {
        return found;
    }
    for (auto& grp : uniones_e3di_groups_) {
        if (auto* found = find_in_items_mut(grp.items, command_id)) {
            return found;
        }
    }
    for (auto& grp : cimentacion_groups_) {
        if (auto* found = find_in_items_mut(grp.items, command_id)) {
            return found;
        }
    }
    if (auto* found = find_in_items_mut(menu_exportacion_.items, command_id)) {
        return found;
    }
    return nullptr;
}

std::size_t RibbonCatalog::count_items_recursive(const std::vector<RibbonCommandItem>& items) noexcept {
    std::size_t count = 0;
    for (const auto& item : items) {
        if (item.kind != RibbonButtonKind::Separador && item.kind != RibbonButtonKind::ExtensorDerecha) {
            ++count;
        }
        count += count_items_recursive(item.children);
    }
    return count;
}

std::size_t RibbonCatalog::total_registered_commands() const noexcept {
    std::size_t total = 0;
    for (const auto& tab : estructura_tabs_) {
        for (const auto& grp : tab.groups) {
            total += count_items_recursive(grp.items);
        }
    }
    for (const auto& grp : uniones_e3di_groups_) {
        total += count_items_recursive(grp.items);
    }
    for (const auto& grp : cimentacion_groups_) {
        total += count_items_recursive(grp.items);
    }
    total += count_items_recursive(menu_exportacion_.items);
    return total;
}

void RibbonCatalog::preload_items_icons(const std::vector<RibbonCommandItem>& items, ThemePalette& palette) {
    for (const auto& item : items) {
        if (!item.icon_resource.empty()) {
            palette.imagen_carga_recurso("", item.source_tag, item.icon_resource);
        }
        if (!item.children.empty()) {
            preload_items_icons(item.children, palette);
        }
    }
}

void RibbonCatalog::preload_icons(ThemePalette& palette) const {
    for (const auto& tab : estructura_tabs_) {
        for (const auto& grp : tab.groups) {
            if (!grp.icon_resource.empty()) {
                palette.imagen_carga_recurso("", "ribbon.dll", grp.icon_resource);
            }
            preload_items_icons(grp.items, palette);
        }
    }
    for (const auto& grp : uniones_e3di_groups_) {
        preload_items_icons(grp.items, palette);
    }
    for (const auto& grp : cimentacion_groups_) {
        if (!grp.icon_resource.empty()) {
            palette.imagen_carga_recurso("", "plcpanel.c (PLANOCIM.DLL)", grp.icon_resource);
        }
        preload_items_icons(grp.items, palette);
    }
    preload_items_icons(menu_exportacion_.items, palette);
}

void RibbonCatalog::build_estructura_ribbon() {
    estructura_tabs_.clear();
    const std::string tag_ob = "ed3mnuobrb.c (ED3DLIB.DLL)";
    const std::string tag_geo = "ed3mnugeo.c (ED3DLIB.DLL)";
    const std::string tag_bar = "ed3mnubar.c (ED3DLIB.DLL)";
    const std::string tag_lam = "ed3mnulamef.c (ED3DLIB.DLL)";
    const std::string tag_car = "ed3mnucar.c (ED3DLIB.DLL)";
    const std::string tag_uni = "ed3mnuob.c (ED3DLIB.DLL)";
    const std::string tag_pi  = "ed3mnupi.c (ED3DLIB.DLL)";
    const std::string tag_pv  = "ed3mnupv.c (ED3DLIB.DLL)";

    // =========================================================================
    // Pestaña 1: Obra (0x18087eaf0 en ed3mnuobrb.c — 06_estructura_tab_obra.png)
    // =========================================================================
    RibbonTab tab_obra{"obra", "Obra", {}};
    {
        RibbonGroup g_obra{"obra", "Obra", "datobra2", false, {
            make_cmd("Obra_DatosGenerales",            "Datos generales",               "datobra2",         tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("Gestion_biblioteca_perfiles",    "Biblioteca de perfiles",        "biblioteca_obra2", tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("biblioteca_catalogos",           "Catálogos de fabricante",       "cat_fab",          tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_BibRotulasPlasticas",    "Biblioteca de rótulas plásticas","bibrotplas",      tag_ob, RibbonButtonKind::BotonPanelObra),
            make_cmd("Calculo_CfgRendANL",             "Rendimiento",                   "cfgrend",          tag_ob, RibbonButtonKind::BotonPanelObra),
            make_cmd("Obra_Unidades_Acotacion",        "Unidades de acotación",         "uacot",            tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("Obra_perfiles_metalicos",        "Perfiles metálicos",            "perf_met2",        tag_ob, RibbonButtonKind::BotonDlgObra, true),
            make_cmd("Obra_perfiles_madera",           "Perfiles de madera",            "perf_mad2",        tag_ob, RibbonButtonKind::BotonDlgObra, true)
        }};

        RibbonGroup g_num{"numeracion", "Numeración", "numel", false, {
            make_cmd("Obra_opcs_numeracion",           "Editar",                        "numel",            tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("Obra_reiniciar_numeracion",      "Reiniciar",                     "numelrec",         tag_ob, RibbonButtonKind::BotonDlgObra)
        }};

        RibbonGroup g_v3d{"Vistas3D", "Vista 3D", "vista3d", false, {
            make_cmd("Obra_Vista3D",                   "Estructura completa",           "v3dcomp",          tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("Obra_Vista3D_vista",             "Ventana activa",                "v3dvntact",        tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("Obra_Vista3DSeleccionPiezas",    "Selección de elementos",        "v3dpzas",          tag_ob, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_UnionesEnVista3D",          "Uniones",                       "unv3d",            tag_ob, RibbonButtonKind::BotonInterruptor)
        }};

        RibbonGroup g_list{"listados", "Listados", "p", false, {
            make_cmd("Obra_ListadoElementosSeleccionados",       "Elementos seleccionados",   "p",          tag_ob, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_ExportacionCSVElementosSeleccionados","CSV de los elementos...",   "expcsv",     tag_ob, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_EtiquetasVigas",                      "Etiquetas de vigas",        "etiq_vigas", tag_ob, RibbonButtonKind::BotonDlgObra)
        }};

        RibbonGroup g_bim{"gestion_bim", "BIMserver.center", "bim_server", true, {
            make_cmd("vincular_bim",                   "Vincular",                      "ifc_vincular",     tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("desvincular_BIM",                "Desvincular",                   "ifc_desvincular",  tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("actualizar_bim",                 "Consultar",                     "bim_actualizar",   tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("exportar_bim",                   "Compartir",                     "bim_exportar",     tag_ob, RibbonButtonKind::BotonDlgObra)
        }};

        tab_obra.groups = {std::move(g_obra), std::move(g_num), std::move(g_v3d), std::move(g_list), std::move(g_bim)};
    }
    estructura_tabs_.push_back(std::move(tab_obra));

    // =========================================================================
    // Pestaña 2: Geometría (0x18087ea00 en ed3mnugeo.c — 07_estructura_tab_geometria.png)
    // =========================================================================
    RibbonTab tab_geo{"geometria", "Geometría", {}};
    {
        RibbonCommandItem sub_acot = make_cmd("acotacion", "Acotación", "k", tag_geo, RibbonButtonKind::SubmenuModeless);
        sub_acot.children = {
            make_cmd("Planos_Cotas_Anadir",  "Añadir",  "k", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Planos_Cotas_Borrar",  "Borrar",  "l", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Planos_Cotas_Mostrar", "Mostrar", "m", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Planos_Cotas_Ocultar", "Ocultar", "n", tag_geo, RibbonButtonKind::ReceptorOpcion)
        };

        RibbonGroup g_planos{"plano", "Planos", "rejilla", false, {
            make_cmd("Planos_Rejilla",        "Rejilla",           "rejilla", tag_geo, RibbonButtonKind::BotonDlgObra),
            make_cmd("Obra_niveles",          "Niveles",           "niveles", tag_geo, RibbonButtonKind::BotonDlgObra),
            std::move(sub_acot),
            make_cmd("Planos_Referencias",    "Referencias",       "0",       tag_geo, RibbonButtonKind::BotonDlgObra),
            make_cmd("Planos_MostrarOcultar", "Mostrar / Ocultar", "s",       tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Planos_Visibilidad",    "Visibilidad",       "t",       tag_geo, RibbonButtonKind::BotonInterruptor),
            make_cmd("Planos_Agrupar",        "Agrupar",           "q",       tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Planos_Desagrupar",     "Desagrupar",        "r",       tag_geo, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonGroup g_nudos{"nudos", "Nudos", "nudo_new2", false, {
            make_cmd("Nudo_Nuevo",  "Nuevo",  "nudo_new2", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Nudo_Borrar", "Borrar", "nudo_bor2", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Nudo_Mover",  "Mover",  "nudo_mov2", tag_geo, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonCommandItem sub_pilares_ed = make_cmd("pilares", "Edición de pilares", "edit", tag_pi, RibbonButtonKind::SubmenuDesplegable);
        sub_pilares_ed.children = {
            make_cmd("pilares_nuevo",          "Nuevo pilar",       "nuec",  tag_pi, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pilares_cantos_forjado", "Cantos de forjado", "edcf",  tag_pi, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pilares_referencias",    "Referencias",       "edref", tag_pi, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pilares_numerar",        "Numerar",           "num",   tag_pi, RibbonButtonKind::ReceptorOpcion)
        };

        RibbonCommandItem sub_vigas_ed = make_cmd("pv_ed", "Edición de vigas", "edit", tag_pv, RibbonButtonKind::SubmenuDesplegable);
        sub_vigas_ed.children = {
            make_cmd("pv_ed_nuevo_portico",            "Nueva viga",                "nuec",   tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_editar_nudos",             "Editar nudos",              "ednu",   tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_editar_forjados",          "Editar forjados",           "edforj", tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_referencias_vigas",        "Referencias de vigas",      "edref",  tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_referencias_porticos_vigas","Referencias de pórticos",  "edrefc", tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_Flecha_Limite",            "Flecha límite",             "flelim", tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_editar_ambiente_hormigon", "Ambiente",                  "amb",    tag_pv, RibbonButtonKind::ReceptorOpcion)
        };

        RibbonCommandItem cmd_div_barras = make_cmd("Barra_ConDivisionBarras", "Generar nudos en cortes entre barras", "f", tag_bar, RibbonButtonKind::BotonInterruptor);
        cmd_div_barras.toggled = true;

        RibbonGroup g_barras{"barra", "Barras", "0", false, {
            make_cmd("Barra_Nueva",                            "Barra",               "0",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_CrearPieza",                       "Pieza",               "2",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pilares_nuevo_top",                      "Pilar",               "nuec",     tag_pi,  RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_nuevo_portico_top",                "Viga",                "nuec",     tag_pv,  RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Borrar",                           "Borrar",              "1",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Describir",                        "Sección",             "5",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_DescribirMaterial",                "Material",            "7",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_DescribirEnrase",                  "Disposición",         "6",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_mover_extremo",                    "Mover extremo",       "movext",   tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_InvertirEjeX",                     "Invertir eje 'x'",    "g",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Agrupar",                          "Agrupar",             "3",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Desagrupar",                       "Desagrupar",          "4",        tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Receptor_Informacion",             "Información",         "info3",    tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_act_des_conex_int_barras_a_laminas","Conexión barras a láminas","conexlam",tag_bar, RibbonButtonKind::ReceptorOpcion),
            std::move(cmd_div_barras),
            std::move(sub_pilares_ed),
            std::move(sub_vigas_ed),
            make_cmd("Barra_AsignarPlanta",                    "Asignar planta",      "asocbapla",tag_bar, RibbonButtonKind::ReceptorOpcion, true)
        }};

        RibbonGroup g_laminas{"laminas", "Láminas", "nue", false, {
            make_cmd("lamef_nueva_lamina",             "Nueva",            "nue",    tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_borrar",                   "Borrar",           "bor",    tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_desc_seccion",             "Sección",          "edit",   tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_material",                 "Material",         "mat",    tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_enrase",                   "Disposición",      "enra",   tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_mover_nudos",              "Mover",            "mov",    tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_unir",                     "Unir",             "unir",   tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_dividir",                  "Dividir",          "div",    tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_invertir_eje_z",           "Invertir eje 'z'", "invz",   tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_set_arista_define_eje_x",  "Definir eje 'x'",  "invx",   tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_ed_angulo_giro_ejes_lamina","Ejes locales",    "giroej", tag_lam, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonCommandItem sub_generar = make_cmd("generacion", "Generar", "gen", tag_geo, RibbonButtonKind::SubmenuDesplegable);
        sub_generar.children = {
            make_cmd("Obra_GenerarPlanos",         "Planos",              "y", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_GenerarNudosYBarras",   "Nudos y barras",      "w", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_GenerMallaTetraedros",  "Malla de tetraedros", "v", tag_geo, RibbonButtonKind::ReceptorOpcion)
        };

        RibbonGroup g_herr{"herramientas", "Herramientas", "gen", false, {
            std::move(sub_generar),
            make_cmd("movimiento_multiple",      "Mover elementos",              "elem_mov2", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("giro_multiple_plano_xy",   "Giro en plano XY",             "gxy",       tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("copia_multiple",           "Copiar elementos",             "copiar",    tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("simetria_multiple_plano_xy","Simetría en plano XY",        "simcpxy",   tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("giro_multiple",            "Giro alrededor de un eje",     "g2p",       tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("giro_multiple_3_ptos",     "Giro mediante 3 puntos",       "g3p",       tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("simetria_multiple",        "Simetría respecto a un plano", "simcopiar", tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_Buscar",              "Buscar",                       "buscarel",  tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_Referencia",          "Referencia",                   "asgref",    tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_RepararGeometria",    "Reparar",                      "repobra",   tag_geo, RibbonButtonKind::BotonDlgObra)
        }};

        RibbonGroup g_imp{"importar", "Importar", "dxfplt2", false, {
            make_cmd("Obra_Importar",       "DXF o DWG",        "dxfplt2",   tag_geo, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Obra_Importar_ascii", "Fichero de texto", "impascii2", tag_geo, RibbonButtonKind::ReceptorOpcion)
        }};

        tab_geo.groups = {
            std::move(g_planos),
            std::move(g_nudos),
            std::move(g_barras),
            std::move(g_laminas),
            std::move(g_herr),
            std::move(g_imp)
        };
    }
    estructura_tabs_.push_back(std::move(tab_geo));

    // =========================================================================
    // Pestaña 3: Propiedades (0x18087ebd0 en ed3mnuobrb.c — 08_estructura_tab_propiedades.png)
    // =========================================================================
    RibbonTab tab_prop{"propiedades", "Propiedades", {}};
    {
        RibbonGroup g_diaf{"diafragmas", "Diafragmas", "diafrags", false, {
            make_cmd("opc_diafragmas",        "Editar",              "diafrags", tag_ob, RibbonButtonKind::BotonDlgObra),
            make_cmd("mod_ed_diafragma_nudo", "Diafragma",           "anudiaf",  tag_ob, RibbonButtonKind::ReceptorOpcion),
            make_cmd("gen_diafrags",          "Generar por niveles", "dgdiafs",  tag_ob, RibbonButtonKind::BotonDlgObra)
        }};

        RibbonGroup g_pnudos{"props_nudos", "Nudos", "1", false, {
            make_cmd("Nudo_Describir_Vinc_Int", "Vinculación interior", "1",         tag_ob, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Nudo_Describir_Vinc_Ext", "Vinculación exterior", "2",         tag_ob, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Nudo_Ligaduras",          "Ligaduras",            "ligaduras", tag_ob, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonCommandItem sub_flecha = make_cmd("grupos_flecha", "Grupos de flecha", "grufle", tag_bar, RibbonButtonKind::SubmenuDesplegable);
        sub_flecha.children = {
            make_cmd("Barra_Crear_grupo_flecha",  "Crear grupo de flecha",  "b", tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Editar_grupo_flecha", "Editar grupo de flecha", "e", tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Flecha_Limite",       "Flecha límite",          "a", tag_bar, RibbonButtonKind::ReceptorOpcion)
        };

        RibbonCommandItem sub_fuego = make_cmd("resistencia_fuego", "Resistencia al fuego", "resfuego2", tag_bar, RibbonButtonKind::SubmenuDesplegable);
        sub_fuego.children = {
            make_cmd("Resistencia_Fuego_Datos_Comprobacion",    "Datos de comprobación",    "inccomp",   tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Resistencia_Fuego_Exposicion_Caras",      "Exposición de caras",      "inccarexp", tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Resistencia_Fuego_Espesor_Revestimiento", "Espesor de revestimiento", "incesp",    tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Resistencia_Fuego_Grupos",                "Grupos",                   "incgru",    tag_bar, RibbonButtonKind::BotonPanelObra)
        };

        RibbonGroup g_pbarras{"props_barras", "Barras", "8", false, {
            make_cmd("Barra_Pandeo",                      "Pandeo",                        "8",       tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Pandeo_Lateral",              "Pandeo lateral",                "9",       tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_EmparejarArriostramientos",   "Parejas",                       "parri",   tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Articular",                   "Articular",                     "i",       tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_Empotramiento_Extremos_Pieza","Empotramiento",                 "d",       tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_rotulas_plasticas",           "Rótulas plásticas",             "rotplas", tag_bar, RibbonButtonKind::ReceptorOpcion),
            std::move(sub_flecha),
            std::move(sub_fuego),
            make_cmd("Barra_anmodal",                     "Análisis modal",                "vib",     tag_bar, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Barra_sist_arriostr",               "Sistemas resistentes a sismo",  "sisre",   tag_bar, RibbonButtonKind::ReceptorOpcion, true)
        }};

        RibbonGroup g_plaminas{"props_laminas", "Láminas", "vincint", false, {
            make_cmd("lamef_vinc_int_aristas",   "Vinculación interior", "vincint", tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_vinc_ext_aristas",   "Vinculación exterior", "vincext", tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_bandas_integracion", "Bandas de integración","bandint", tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_discretizacion",     "Discretización",       "discr",   tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_edfrig",             "Factores de rigidez",  "edfrig",  tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_ed_pandeo",          "Pandeo",               "panlam",  tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_ed_modal",           "Análisis modal",       "vib",     tag_lam, RibbonButtonKind::ReceptorOpcion),
            make_cmd("lamef_asignar_plantas",    "Asignar plantas",      "asocpla", tag_lam, RibbonButtonKind::ReceptorOpcion, true)
        }};

        tab_prop.groups = {std::move(g_diaf), std::move(g_pnudos), std::move(g_pbarras), std::move(g_plaminas)};
    }
    estructura_tabs_.push_back(std::move(tab_prop));

    // =========================================================================
    // Pestaña 4: Carga (0x1803c8630 en ed3mnucar.c — 09_estructura_tab_carga.png)
    // =========================================================================
    RibbonTab tab_carga{"carga", "Carga", {}};
    {
        RibbonGroup g_vis{"cargas_opcs_visualiz", "Visualización", "0", false, {
            make_cmd("Carga_hipotesis_vista", "Hipótesis vista", "0",       tag_car, RibbonButtonKind::BotonDlgObra),
            make_cmd("Carga_escalas",         "Escalas",         "dimens2", tag_car, RibbonButtonKind::BotonDlgObra)
        }};

        RibbonGroup g_cnudos{"cargas_nudos", "Nudos", "4", false, {
            make_cmd("Carga_nudo_desp_y_giros_prescritos", "Desplazamientos prescritos", "7",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Carga_nudo_nueva_carga",             "Nueva",                      "4",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Carga_nudo_editar_carga",            "Editar",                     "5",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Carga_nudo_borrar_carga",            "Borrar",                     "6",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Editar_hipotesis_carga_nudo",        "Hipótesis",                  "chnud", tag_car, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonGroup g_cbarras{"cargas_barras", "Barras", "1", false, {
            make_cmd("Carga_barra_nueva_carga",          "Nueva",       "1",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Carga_barra_editar_carga",         "Editar",      "2",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Carga_barra_borrar_carga",         "Borrar",      "3",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Editar_hipotesis_cargas_barras",   "Hipótesis",   "chbar", tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Carga_barra_activar_desactiva_pp", "Peso propio", "adpp",  tag_car, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonGroup g_claminas{"cargas_laminas", "Láminas", "nuelam", false, {
            make_cmd("nueva_carga_lamina",              "Nueva",       "nuelam",  tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("editar_carga_lamina",             "Editar",      "edlam",   tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("borrar_cargas_laminas",           "Borrar",      "borlam",  tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("editar_hipotesis_cargas_laminas", "Hipótesis",   "edhlam",  tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("activar_desactiva_pp_lamina",     "Peso propio", "adpplam", tag_car, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonGroup g_cpanyos{"cargas_panyos", "Paños", "d", false, {
            make_cmd("nuevo_panyo",              "Nuevo",                "d",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("editar_panyo",             "Editar",               "i",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("borrar_panyo",             "Borrar",               "e",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("mover_panyo",              "Mover",                "m",     tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("dividir_panyo",            "Dividir",              "divpa", tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("seleccionar_direcc_panyo", "Dirección de reparto", "f",     tag_car, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonGroup g_csup{"cargas_superf", "Cargas superficiales", "8", false, {
            make_cmd("nueva_superficie_carga",              "Nueva",     "8",      tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("editar_superficie_carga",             "Editar",    "a",      tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("borrar_superficie_carga",             "Borrar",    "9",      tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Editar_hipotesis_superficie_carga",   "Hipótesis", "chcsup", tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("copiar_superficies_carga",            "Copiar",    "b",      tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("mover_superficie_carga",              "Mover",     "n",      tag_car, RibbonButtonKind::ReceptorOpcion),
            make_cmd("activar_desactivar_cargas_superf_barra","Asignar", "l",      tag_car, RibbonButtonKind::ReceptorOpcion)
        }};

        tab_carga.groups = {
            std::move(g_vis),
            std::move(g_cnudos),
            std::move(g_cbarras),
            std::move(g_claminas),
            std::move(g_cpanyos),
            std::move(g_csup)
        };
    }
    estructura_tabs_.push_back(std::move(tab_carga));

    // =========================================================================
    // Pestaña 5: Uniones (0x1803dc290 "uniones_ed" y 0x1803da4a0 "uniones_e3di" + "Placas_Anclaje")
    // =========================================================================
    uniones_ed_group_ = RibbonGroup{"uniones_ed", "Uniones", "nuunione3d2", false, {
        make_cmd("ued_nueva",                        "Nueva",                   "nuunione3d2",   tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_cmd("ued_borrar",                       "Borrar",                  "bounione3d2",   tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_cmd("ued_editar",                       "Editar",                  "edunione3d2",   tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_separator(tag_uni),
        make_cmd("ued_editar_asignar",               "Asignar",                 "asunione3d2",   tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_cmd("ued_agrupar",                      "Agrupar",                 "agrunione3d2",  tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_cmd("ued_desagrupar",                   "Desagrupar",              "dagrunione3d2", tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_cmd("ued_bloquear",                     "Bloquear",                "blqunione3d2",  tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_cmd("ued_desbloquear",                  "Desbloquear",             "dblqunione3d2", tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_cmd("ued_Buscar",                       "Buscar",                  "busunione3d2",  tag_uni, RibbonButtonKind::ReceptorOpcion),
        make_separator(tag_uni),
        make_cmd("ued_generar",                      "Generar",                 "genunione3d2",  tag_uni, RibbonButtonKind::BotonDlgObra),
        make_cmd("ued_analisis_rigidez_rotacional",  "Rigideces rotacionales",  "rig_rot3",      tag_uni, RibbonButtonKind::ReceptorOpcion)
    }};

    uniones_e3di_groups_ = {
        RibbonGroup{"uniones_auto", "Uniones", "dimunione3d2", false, {
            make_cmd("uaut_opcs_calculo",                "Opciones",                "opc_uniones",   tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("uaut_analizar",                    "Generar",                 "dimunione3d2",  tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("uaut_consulta_uniones",            "Consultar",               "ver_uniones",   tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("uaut_consulta_uniones_sin_calculo","Consultar sin cálculo",   "ver_uniones",   tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("uaut_analisis_rigidez_rotacional", "Rigideces rotacionales",  "rig_rot3",      tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("uaut_consultar_incidencias",       "Listado de incidencias",  "ver_uniones",   tag_uni, RibbonButtonKind::BotonDlgObra)
        }},
        RibbonGroup{"Placas_Anclaje", "Placas de anclaje", "genplaca2", false, {
            make_cmd("Placas_Anclaje_generar",     "Generar",     "genplaca2",  tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Placas_Anclaje_borrar",      "Borrar",      "borplaca2",  tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Placas_Anclaje_editar",      "Editar",      "editplaca2", tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Placas_Anclaje_igualar",     "Igualar",     "igplaca2",   tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Placas_Anclaje_comprobar",   "Comprobar",   "g",          tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Placas_Anclaje_dimensionar", "Dimensionar", "j",          tag_uni, RibbonButtonKind::BotonDlgObra)
        }}
    };

    RibbonTab tab_uniones{"uniones", "Uniones", {uniones_ed_group_}};
    estructura_tabs_.push_back(std::move(tab_uniones));

    // =========================================================================
    // Pestaña 6: Cálculo (0x1803da560 en ed3mnuob.c — 11_estructura_tab_calculo.png)
    // =========================================================================
    RibbonTab tab_calc{"calculo", "Cálculo", {}};
    {
        RibbonCommandItem sub_dim = make_cmd("dimensionar", "Dimensionar", "dimensio2", tag_uni, RibbonButtonKind::SubmenuDesplegable);
        sub_dim.children = {
            make_cmd("dimensionamiento_desde_perfil_actual", "Barras (desde perfil actual)", "d",         tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("dimensionamiento_desde_primer_perfil", "Barras (desde primer perfil)", "b",         tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("pilares_calc",                         "Pilares",                      "pil",       tag_pi,  RibbonButtonKind::BotonDlgObra),
            make_cmd("pv_calc",                              "Vigas",                        "vigas",     tag_pv,  RibbonButtonKind::BotonDlgObra),
            make_cmd("uniones_ed_calc",                      "Uniones",                      "un",        tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_Dimensionar_revestimiento_incendio", "Revestimiento incendio", "dimesprev", tag_uni, RibbonButtonKind::BotonDlgObra)
        };

        RibbonGroup g_ts{"Calculo_ts", "Tensión / Deformación", "calcts", false, {
            make_cmd("Calculo_CombsANL",            "Combinaciones no lineales", "combsanl",       tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_calcular",            "Calcular",                  "calcts",         tag_uni, RibbonButtonKind::BotonDlgObra),
            std::move(sub_dim),
            make_cmd("Calculo_Desplazamientos",     "Desplazamientos",           "3",              tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Calculo_Reacciones",          "Reacciones",                "4",              tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Calculo_esfuerzos",           "Esfuerzos",                 "8",              tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Calculo_esfuerzos3D",         "Isovalores",                "defiso",         tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_Empresillado",        "Empresillado",              "5",              tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Calculo_sismo_dinamico",      "Sismo dinámico",            "6",              tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_comprueba_elementos", "Comprobar elementos",       "comproba2",      tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Calculo_comprobaciones_ELU",  "Comprobaciones E.L.U.",     "comprobeluels2", tag_uni, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonGroup g_pan{"AnalisisPandeo", "Pandeo", "clpand", false, {
            make_cmd("Calculo_GruAnPan",       "Grupos de análisis",   "grupan",     tag_uni, RibbonButtonKind::BotonPanelObra),
            make_cmd("Calculo_CombsPanL",      "Combinaciones",        "combpan",    tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_CalcPan",        "Calcular",             "clpand",     tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_ModosPandeo",    "Modos de pandeo",      "modospan",   tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Calculo_Pan_esfuerzos3D","Isovalores",           "panisoval",  tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_AsignaCoefsPan", "Asignar coeficientes", "asgcoefpan", tag_uni, RibbonButtonKind::BotonDlgObra)
        }};

        RibbonGroup g_mod{"AnalisisModal", "Modal", "vibcalc", false, {
            make_cmd("Calculo_CombsModal",              "Casos de carga",     "vibccarga", tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_CalcModal",               "Calcular",           "vibcalc",   tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("Calculo_ModosVibracion",          "Modos de vibración", "vibmodos",  tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("Calculo_listado_modos_vibracion", "Listado",            "viblst",    tag_uni, RibbonButtonKind::BotonDlgObra)
        }};

        RibbonGroup g_po{"AnalisisPushover", "Pushover", "pocalc", false, {
            make_cmd("PO_Combinaciones",   "Casos de carga",    "poccarga",  tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("PO_SelNuCtrl",       "Nudo de control",   "ponuctrl",  tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("PO_Calcular",        "Calcular",          "pocalc",    tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("PO_Desplazamientos", "Desplazamientos",   "podesps",   tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("PO_Reacciones",      "Reacciones",        "poreaccs",  tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("PO_Esfuerzos",       "Esfuerzos",         "poesfs",    tag_uni, RibbonButtonKind::ReceptorOpcion),
            make_cmd("PO_esfuerzos3D",     "Isovalores",        "poisoval",  tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("PO_curva_capacidad", "Capacidad",         "pocapac",   tag_uni, RibbonButtonKind::BotonDlgObra),
            make_cmd("PO_comp_rotulas",    "Rótulas plásticas", "poesfsrot", tag_uni, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonGroup g_inc{"incidencias", "Incidencias", "erroravi2", true, {
            make_cmd("Calculo_mostrar_errores", "Incidencias", "erroravi2", tag_uni, RibbonButtonKind::BotonMostrarErrores)
        }};

        tab_calc.groups = {std::move(g_ts), std::move(g_pan), std::move(g_mod), std::move(g_po), std::move(g_inc)};
    }
    estructura_tabs_.push_back(std::move(tab_calc));

    // =========================================================================
    // Pestaña 7: Armaduras (0x180886670 en ed3mnuarmrb.c — 12_estructura_tab_armaduras.png)
    // =========================================================================
    RibbonTab tab_arm{"armaduras", "Armaduras", {}};
    {
        RibbonGroup g_pil{"pilares", "Pilares", "edarm", false, {
            make_cmd("pilares_armaduras",   "Editar armaduras", "edarm", tag_pi, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pilares_bloquear",    "Bloquear",         "bloq",  tag_pi, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pilares_desbloquear", "Desbloquear",      "dbloq", tag_pi, RibbonButtonKind::ReceptorOpcion)
        }};

        RibbonGroup g_vig{"pv_ed", "Vigas", "edarm", false, {
            make_cmd("pv_ed_editar_todos_portico",           "Editar armaduras",                "edarm", tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_editar_porticos_ventana_activa", "Editar pórticos ventana activa",  "edarm", tag_pv, RibbonButtonKind::ReceptorOpcion, true),
            make_cmd("pv_ed_asignar",                        "Asignar armaduras",               "igu",   tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_bloquear",                       "Bloquear",                        "bloq",  tag_pv, RibbonButtonKind::ReceptorOpcion),
            make_cmd("pv_ed_desbloquear",                    "Desbloquear",                     "dbloq", tag_pv, RibbonButtonKind::ReceptorOpcion)
        }};

        tab_arm.groups = {std::move(g_pil), std::move(g_vig)};
    }
    estructura_tabs_.push_back(std::move(tab_arm));

    // =========================================================================
    // Pestaña 8: Ventana (Marco MDI de entomenu.dll — 13_estructura_tab_ventana.png)
    // =========================================================================
    RibbonTab tab_vnt{"ventana", "Ventana", {}};
    {
        const std::string tag_vnt = "entomenu.dll";
        RibbonGroup g_vnt{"ventana", "Ventana", "vnt_nueva", false, {
            make_cmd("Ventana_AbrirNueva",        "Abrir Nueva",         "vnt_nueva",  tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_Renombrar",         "Renombrar",           "vnt_renom",  tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_Cerrar",            "Cerrar",              "vnt_cerrar", tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_Anterior",          "Anterior",            "vnt_ant",    tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_Siguiente",         "Siguiente",           "vnt_sig",    tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_Cascada",           "Cascada",             "vnt_casc",   tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_MosaicoHorizontal", "Mosaico horizontal",  "vnt_mosh",   tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_MosaicoVertical",   "Mosaico vertical",    "vnt_mosv",   tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_OrdenarVentanas",   "Ordenar ventanas",    "vnt_ord",    tag_vnt, RibbonButtonKind::BotonDlgObra),
            make_cmd("Ventana_SeleccionarVentana","Seleccionar ventana", "vnt_sel",    tag_vnt, RibbonButtonKind::SubmenuDesplegable)
        }};
        tab_vnt.groups = {std::move(g_vnt)};
    }
    estructura_tabs_.push_back(std::move(tab_vnt));

    // =========================================================================
    // Menú Auxiliar de Exportación (0x1803daa40 en ed3mnuob.c)
    // =========================================================================
    menu_exportacion_ = RibbonGroup{"Menu_Exportacion", "Exportación", "tekla", false, {
        make_cmd("Exportacion_A_Tekla", "Exportar a Tekla Structures", "tekla",  tag_uni, RibbonButtonKind::BotonDlgObra),
        make_cmd("Exportacion_A_CIS2",  "Exportar a CIS/2",            "cis2",   tag_uni, RibbonButtonKind::BotonDlgObra),
        make_cmd("Exportacion_A_CSV",   "Exportar a CSV",              "expcsv", tag_uni, RibbonButtonKind::BotonDlgObra)
    }};
}

void RibbonCatalog::build_cimentacion_ribbon() {
    cimentacion_groups_.clear();
    const std::string tag_plc = "plcpanel.c (PLANOCIM.DLL)";

    // =========================================================================
    // Grupo 1: Obra (0x18009a200 — "menu_obra", x = 1..216, y = 51..142)
    // =========================================================================
    RibbonCommandItem sub_opciones = make_cmd("menu_opciones", "Opciones", "opdimens2", tag_plc, RibbonButtonKind::SubmenuDesplegable);
    sub_opciones.children = {
        make_cmd("opciones_recubrimientos",          "Recubrimientos",             "recub_cim",   tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("opciones_mermas",                  "Mermas de acero",            "tantp1002",   tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("opciones_hormigon_limpieza",       "Hormigón de limpieza",       "horm_limp",   tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("opciones_comprobacion_fisuracion_u","Comprobación de fisuración","fisuracion2", tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("opciones_elemento_aislado",        "Zapatas aisladas",           "cimen2",      tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("opciones_elemento_lineal",         "Zapatas corridas / lineales","cim_lin",     tag_plc, RibbonButtonKind::BotonDlgObra, !admitir_elem_lineales_cim_),
        make_cmd("opciones_encepados",               "Encepados",                  "encepado2",   tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("opciones_vigas_centradoras",       "Vigas centradoras",          "vig_cen_op",  tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("opciones_vigas_atado",             "Vigas de atado",             "vig_ata_op",  tag_plc, RibbonButtonKind::BotonDlgObra)
    };

    RibbonCommandItem sub_tablas = make_cmd("submenu_tablas_armado", "Tablas de armado", "opctabla2", tag_plc, RibbonButtonKind::SubmenuDesplegable);
    sub_tablas.children = {
        make_cmd("tablas_armado_zapatas_aisladas", "Zapatas aisladas",  "tbarmzp2", tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("tablas_armado_zapatas_lineales", "Zapatas lineales",  "tbarmzl2", tag_plc, RibbonButtonKind::BotonDlgObra, !admitir_elem_lineales_cim_),
        make_cmd("tablas_armado_vig_cen",          "Vigas centradoras", "tbarmvc2", tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("tablas_armado_vig_ata",          "Vigas de atado",    "tbarmva2", tag_plc, RibbonButtonKind::BotonDlgObra)
    };

    RibbonGroup g_obra{"menu_obra", "Obra", "datobra3", false, {
        make_cmd("datos_generales", "Datos generales", "datobra3", tag_plc, RibbonButtonKind::BotonDlgObra),
        std::move(sub_opciones),
        std::move(sub_tablas),
        make_separator(tag_plc),
        make_cmd("vista_3D",        "Vista 3D",        "vista3d2", tag_plc, RibbonButtonKind::BotonDlgObra)
    }};

    // =========================================================================
    // Grupo 2: Elementos de cimentación (0x180098bc0 — "menu_cimentacion", x = 223..803)
    // =========================================================================
    RibbonCommandItem sub_nuevo = make_cmd("nuevo_elemento", "Nuevo", "nuevocim", tag_plc, RibbonButtonKind::SubmenuModeless);
    sub_nuevo.children = {
        make_cmd("nuevo_zapata_horm_arm",            "Zapata de hormigón armado", "zp_ha",    tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("nuevo_zapata_horm_masa",           "Zapata de hormigón en masa","zp_hm",    tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("nuevo_encepado",                   "Encepado",                  "encep_nw", tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("nuevo_elemento_lineal_por_objeto", "Elemento lineal",           "el_lin",   tag_plc, RibbonButtonKind::ReceptorOpcion, !admitir_elem_lineales_cim_),
        make_cmd("nueva_viga_atado",                 "Viga de atado",             "vig_ata",  tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("nueva_viga_centradora",            "Viga centradora",           "vig_cen",  tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("nueva_viga_automatica",            "Viga automática",           "vig_aut",  tag_plc, RibbonButtonKind::ReceptorOpcion)
    };

    RibbonCommandItem sub_rotar = make_cmd("rotar_elementos_aislados", "Rotar", "rotzapat2", tag_plc, RibbonButtonKind::SubmenuModeless);
    sub_rotar.children = {
        make_cmd("introduccion_manual",     "Introducción manual",      "rotcalc", tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("introduccion_dos_puntos", "Introducción dos puntos",  "rotpun",  tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("angulo_del_pilar",        "Ángulo del pilar",         "rotpil",  tag_plc, RibbonButtonKind::ReceptorOpcion)
    };

    RibbonGroup g_cim{"menu_cimentacion", "Elementos de cimentación", "nuevocim", false, {
        std::move(sub_nuevo),
        make_cmd("borrar",                 "Borrar",           "cancelcim", tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("editar",                 "Editar",           "edicim",    tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("igualar_elemento",       "Igualar",          "igualcim",  tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("informacion",            "Información",      "infocim",   tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_separator(tag_plc),
        make_cmd("mover_elemento",         "Mover",            "movelcim2", tag_plc, RibbonButtonKind::ReceptorOpcion),
        std::move(sub_rotar),
        make_cmd("unir_elemento",          "Unir",             "unirzp2",   tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_separator(tag_plc),
        make_cmd("ajustar_viga",           "Ajustar",          "ajustvig",  tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("prolongar_viga",         "Prolongar",        "prolvig",   tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("desplazar_viga",         "Desplazar",        "despvig",   tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("centrar_extremos_viga",  "Centrar extremos", "centvig2",  tag_plc, RibbonButtonKind::ReceptorOpcion)
    }};

    // =========================================================================
    // Grupo 3: Límites (0x180099520 — "limites", x = 810..1029)
    // =========================================================================
    RibbonCommandItem sub_nuevo_lim = make_cmd("nuevo_limite", "Nuevo", "intlim2", tag_plc, RibbonButtonKind::SubmenuModeless);
    sub_nuevo_lim.children = {
        make_cmd("crear_limite_por_puntos",        "Por puntos",        "intlimpt", tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("crear_limite_con_polilinea_dxf", "Con polilínea DXF", "intlimpl", tag_plc, RibbonButtonKind::ReceptorOpcion)
    };

    RibbonGroup g_lim{"limites", "Límites", "intlim2", false, {
        std::move(sub_nuevo_lim),
        make_cmd("borrar_limite",          "Borrar",          "borrlim2", tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("mover_limite",           "Mover",           "movlim2",  tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("insertar_punto_limite",  "Nuevo vértice",   "insvert2", tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("borrar_tramo",           "Borrar un tramo", "bortram2", tag_plc, RibbonButtonKind::ReceptorOpcion)
    }};

    // =========================================================================
    // Grupo 4: Cálculo (0x180098780 — "menu_calculo", x = 1036..1390)
    // =========================================================================
    RibbonGroup g_calc{"menu_calculo", "Cálculo", "dimensio2", false, {
        make_cmd("generar_zapatas_y_vigas", "Generar zapatas y vigas", "genzpvig",  tag_plc, RibbonButtonKind::BotonDlgObra),
        make_cmd("eliminar_solapes",        "Eliminar solapes",        "elimsolap", tag_plc, RibbonButtonKind::BotonDlgObra),
        make_separator(tag_plc),
        make_cmd("comprobar",               "Comprobar",               "comproba2", tag_plc, RibbonButtonKind::ReceptorOpcion),
        make_cmd("dimensionar",             "Dimensionar",             "dimensio2", tag_plc, RibbonButtonKind::BotonDlgObra),
        make_separator(tag_plc),
        make_cmd("Calculo_MostrarErrores",  "Incidencias",             "erroravi2", tag_plc, RibbonButtonKind::BotonMostrarErrores)
    }};

    cimentacion_groups_ = {std::move(g_obra), std::move(g_cim), std::move(g_lim), std::move(g_calc)};
}

void RibbonCatalog::build_classic_menubars() {
    // Barra de menús clásica en solapa Estructura:
    // Archivo, Obra, Nudos, Barras, Láminas, Paños, Cargas, Uniones, Placas de anclaje, Cálculo, Ventana, Ayuda
    classic_menu_estructura_ = {
        {"menu_archivo", "Archivo", {"Archivo_Nuevo", "Archivo_GestionArchivos", "Archivo_Guardar", "Exportacion_A_Tekla", "Exportacion_A_CIS2", "Exportacion_A_CSV"}},
        {"menu_obra",    "Obra",    {"Obra_DatosGenerales", "Gestion_biblioteca_perfiles", "biblioteca_catalogos", "Obra_Vista3D", "vincular_bim", "exportar_bim"}},
        {"menu_nudos",   "Nudos",   {"Nudo_Nuevo", "Nudo_Borrar", "Nudo_Mover", "Nudo_Describir_Vinc_Int", "Nudo_Describir_Vinc_Ext", "Nudo_Ligaduras"}},
        {"menu_barras",  "Barras",  {"Barra_Nueva", "Barra_CrearPieza", "Barra_Borrar", "Barra_Describir", "Barra_DescribirMaterial", "Barra_DescribirEnrase", "Barra_Pandeo", "Barra_Pandeo_Lateral", "Barra_Flecha_Limite"}},
        {"menu_laminas", "Láminas", {"lamef_nueva_lamina", "lamef_borrar", "lamef_desc_seccion", "lamef_material", "lamef_vinc_int_aristas", "lamef_vinc_ext_aristas"}},
        {"menu_panyos",  "Paños",   {"nuevo_panyo", "editar_panyo", "borrar_panyo", "mover_panyo", "dividir_panyo", "seleccionar_direcc_panyo"}},
        {"menu_cargas",  "Cargas",  {"Carga_hipotesis_vista", "Carga_nudo_nueva_carga", "Carga_barra_nueva_carga", "nueva_carga_lamina", "nueva_superficie_carga"}},
        {"menu_uniones", "Uniones", {"ued_nueva", "ued_borrar", "ued_editar", "ued_generar", "ued_analisis_rigidez_rotacional"}},
        {"menu_placas",  "Placas de anclaje", {"Placas_Anclaje_generar", "Placas_Anclaje_borrar", "Placas_Anclaje_editar", "Placas_Anclaje_igualar", "Placas_Anclaje_comprobar", "Placas_Anclaje_dimensionar"}},
        {"menu_calculo", "Cálculo", {"Calculo_calcular", "dimensionar", "Calculo_Desplazamientos", "Calculo_Reacciones", "Calculo_esfuerzos", "Calculo_esfuerzos3D", "Calculo_comprueba_elementos", "Calculo_mostrar_errores"}},
        {"menu_ventana", "Ventana", {"Ventana_AbrirNueva", "Ventana_Renombrar", "Ventana_Cerrar", "Ventana_Cascada", "Ventana_MosaicoHorizontal", "Ventana_MosaicoVertical"}},
        {"menu_ayuda",   "Ayuda",   {"Ayuda_Documentacion", "Ayuda_MemoriaCalculo", "Ayuda_AcercaDe"}}
    };

    // Barra de menús clásica en solapa Cimentación:
    // Archivo, Obra, Cimentación, Cálculo, Ventana, Ayuda
    classic_menu_cimentacion_ = {
        {"menu_archivo",     "Archivo",     {"Archivo_Nuevo", "Archivo_GestionArchivos", "Archivo_Guardar"}},
        {"menu_obra",        "Obra",        {"datos_generales", "menu_opciones", "submenu_tablas_armado", "vista_3D"}},
        {"menu_cimentacion", "Cimentación", {"nuevo_elemento", "borrar", "editar", "igualar_elemento", "informacion", "mover_elemento", "rotar_elementos_aislados", "unir_elemento", "nuevo_limite"}},
        {"menu_calculo",     "Cálculo",     {"generar_zapatas_y_vigas", "eliminar_solapes", "comprobar", "dimensionar", "Calculo_MostrarErrores"}},
        {"menu_ventana",     "Ventana",     {"Ventana_AbrirNueva", "Ventana_Cascada", "Ventana_MosaicoHorizontal", "Ventana_MosaicoVertical"}},
        {"menu_ayuda",       "Ayuda",       {"Ayuda_Documentacion", "Ayuda_MemoriaCalculo", "Ayuda_AcercaDe"}}
    };
}

} // namespace cypeclon::gui

