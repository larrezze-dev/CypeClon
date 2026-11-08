#include "gui/app_window.hpp"
#include "core/string_utils.hpp"
#include "units/units.hpp"

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#endif

namespace cypeclon::gui {

AppWindow::AppWindow() {
    // Precargar todos los iconos de ed3dlib.dll y planocim.dll en la caché de componen.dll
    ribbon_.preload_icons(palette_);

    // Poblar especificaciones resumen de compatibilidad desde el catálogo completo
    for (const auto& tab : ribbon_.estructura_tabs()) {
        RibbonTabSpec spec{};
        spec.id = tab.id;
        spec.title = tab.title;
        for (const auto& grp : tab.groups) {
            spec.groups.push_back(grp.title);
        }
        estructura_tabs_.push_back(std::move(spec));
    }

    for (const auto& grp : ribbon_.cimentacion_groups()) {
        cimentacion_groups_.push_back(grp.title);
    }

    docking_.recalculate_layout(1920, 1032, obra_.solapa_activa());
    const auto& mf = docking_.layout().cype_mdi_frame;
    mdi_manager_.set_mdi_frame_rect({mf.x, mf.y, mf.width, mf.height});
    sync_cad_configs_from_docking();
    dlg_datos_generales_.load_from_obra(obra_);
    sync_status_bar();
}

void AppWindow::switch_solapa(model::SolapaActiva nueva_solapa) noexcept {
    obra_.set_solapa_activa(nueva_solapa);
    active_receptor_id_.clear();
    const auto& cur = docking_.layout().cype_aplicacion;
    docking_.recalculate_layout(cur.width, cur.height, nueva_solapa);
    const auto& mf = docking_.layout().cype_mdi_frame;
    mdi_manager_.set_mdi_frame_rect({mf.x, mf.y, mf.width, mf.height});
    sync_status_bar();
}

cad::CadCategoryFilter AppWindow::build_category_filter() const noexcept {
    cad::CadCategoryFilter f{};
    for (const auto& c : docking_.panel_capas().categorias) {
        switch (c.categoria) {
            case CategoriaEstructural::Nudos:
                f.nudos_visible = c.visible;
                f.nudos_bloqueada = c.bloqueada;
                break;
            case CategoriaEstructural::Barras:
                f.barras_visible = c.visible;
                f.barras_bloqueada = c.bloqueada;
                break;
            case CategoriaEstructural::Tirantes:
                f.tirantes_visible = c.visible;
                f.tirantes_bloqueada = c.bloqueada;
                break;
            case CategoriaEstructural::Pilares:
                f.pilares_visible = c.visible;
                f.pilares_bloqueada = c.bloqueada;
                break;
            case CategoriaEstructural::Vigas:
                f.vigas_visible = c.visible;
                f.vigas_bloqueada = c.bloqueada;
                break;
            case CategoriaEstructural::Panyos:
                f.panyos_visible = c.visible;
                f.panyos_bloqueada = c.bloqueada;
                break;
            case CategoriaEstructural::Laminas:
                f.laminas_visible = c.visible;
                f.laminas_bloqueada = c.bloqueada;
                break;
        }
    }
    return f;
}

void AppWindow::sync_cad_configs_from_docking() noexcept {
    const auto& cb = docking_.cad_captures(obra_.solapa_activa());
    auto& sc = snap_engine_.config();
    sc.enabled = cb.activa;
    sc.snap_extremo = cb.extremo;
    sc.snap_punto_medio = cb.punto_medio;
    sc.snap_perpendicular = cb.perpendicular;
    sc.snap_interseccion = cb.interseccion;
    sc.snap_mas_cercano = cb.mas_cercano;
    sc.snap_rastreo_polar = cb.rastreo_polar;
    sc.snap_rejilla = cb.rejilla;
    sc.snap_plantilla_dxf = cb.plantilla_dxf;
}

std::string_view AppWindow::active_solapa_internal_id() const noexcept {
    return (obra_.solapa_activa() == model::SolapaActiva::Estructura)
               ? "METAL3D_ESTRUCTURA"
               : "ID_CIMENTACION";
}

void AppWindow::set_dark_mode(bool dark) noexcept {
    palette_.pnpaleta_set_dark_mode(dark);
}

void AppWindow::close_active_obra() noexcept {
    obra_.set_abierta(false);
    active_modal_ = ActiveModalDialog::None;
    active_receptor_id_.clear();
    sync_status_bar();
}

void AppWindow::begin_nueva_obra_flow() noexcept {
    active_modal_ = ActiveModalDialog::NuevaObraClave;
    status_bar_text_ = "Nueva obra: especifique el directorio, clave del fichero .ed3 y descripción.";
}

bool AppWindow::confirm_dialogo_nueva_obra() {
    if (active_modal_ != ActiveModalDialog::NuevaObraClave) {
        return false;
    }
    if (!dlg_nueva_obra_.accept(obra_)) {
        return false;
    }
    active_modal_ = ActiveModalDialog::AsistenteNuevaObra;
    status_bar_text_ = "Asistente Nueva obra: seleccione Obra vacía, Introducción automática IFC, Importación DXF/DWG o Generador de Pórticos.";
    return true;
}

bool AppWindow::confirm_asistente_nueva_obra() {
    if (active_modal_ != ActiveModalDialog::AsistenteNuevaObra) {
        return false;
    }
    if (!wizard_nueva_obra_.apply_creation_mode(obra_)) {
        return false;
    }
    dlg_datos_generales_.set_modo_asistente_inicial(true);
    dlg_datos_generales_.load_from_obra(obra_);
    active_modal_ = ActiveModalDialog::DatosGenerales;
    status_bar_text_ = "Datos generales de la obra: configure normas, materiales, acciones y terreno de cimentación.";
    return true;
}

bool AppWindow::confirm_dialogo_datos_generales() {
    if (active_modal_ != ActiveModalDialog::DatosGenerales &&
        active_modal_ != ActiveModalDialog::EstadosLimite) {
        return false;
    }
    dlg_datos_generales_.apply_to_obra(obra_);
    obra_.set_abierta(true);
    welcome_.add_recent_file(dlg_nueva_obra_.full_ed3_path().string(), obra_.datos_generales().descripcion);
    active_modal_ = ActiveModalDialog::None;
    sync_status_bar();
    return true;
}

void AppWindow::cancel_modal_dialog() noexcept {
    if (active_modal_ == ActiveModalDialog::EstadosLimite) {
        active_modal_ = ActiveModalDialog::DatosGenerales;
    } else {
        active_modal_ = ActiveModalDialog::None;
    }
    sync_status_bar();
}

bool AppWindow::execute_command(std::string_view command_id) {
    if (command_id == "Archivo_Nuevo" || command_id == "archivo_nuevo") {
        begin_nueva_obra_flow();
        return true;
    }
    if (command_id == "Archivo_Cerrar") {
        close_active_obra();
        return true;
    }
    if (command_id == "Obra_DatosGenerales" || command_id == "datos_generales") {
        dlg_datos_generales_.set_modo_asistente_inicial(false);
        dlg_datos_generales_.load_from_obra(obra_);
        active_modal_ = ActiveModalDialog::DatosGenerales;
        status_bar_text_ = "Datos generales de la obra (" + obra_.datos_generales().clave + ".ed3)";
        return true;
    }
    if (command_id == "Acciones_EstadosLimite") {
        dlg_datos_generales_.dialogo_estados_limite().load_from_obra(obra_);
        active_modal_ = ActiveModalDialog::EstadosLimite;
        status_bar_text_ = "Estados límite (combinaciones E.L.U. / E.L.S.)";
        return true;
    }

    auto* item = ribbon_.find_command_mut(command_id);
    if (!item || !item->enabled) {
        return false;
    }

    if (item->kind == RibbonButtonKind::BotonInterruptor) {
        item->toggled = !item->toggled;
        status_bar_text_ = item->label + (item->toggled ? ": Activado" : ": Desactivado");
        return true;
    }

    if (item->kind == RibbonButtonKind::ReceptorOpcion) {
        active_receptor_id_ = item->id;
    }

    if (command_id == "generar_zapatas_y_vigas") {
        // En Cimentación: genera zapatas de hormigón armado en todos los nudos con vinculación exterior
        std::size_t creadas = 0;
        for (const auto& n : obra_.estructura().nudos) {
            if (n.vinc_ext != model::VinculacionExterior::Libre) {
                obra_.add_elemento_cimentacion(model::TipoElementoCimentacion::ZapataHormigonArmado, n.id);
                ++creadas;
            }
        }
        status_bar_text_ = "Generar zapatas y vigas: " + std::to_string(creadas) + " elementos generados.";
        return true;
    }

    // Comandos de Gestión de Ventanas MDI (Pestaña Ventana — P-04.1 / dispoMDI)
    if (command_id == "Ventana_AbrirNueva") {
        const auto id = mdi_manager_.create_view_3d("Vista 3D (" + std::to_string(mdi_manager_.views().size() + 1) + ")");
        status_bar_text_ = "Nueva vista MDI abierta: ID " + std::to_string(id);
        return true;
    }
    if (command_id == "Ventana_Cerrar") {
        const bool closed = mdi_manager_.close_view(mdi_manager_.active_view_id());
        status_bar_text_ = closed ? "Vista activa cerrada." : "No se puede cerrar la única vista MDI activa.";
        return true;
    }
    if (command_id == "Ventana_Anterior") {
        mdi_manager_.activate_previous();
        if (const auto* av = mdi_manager_.active_view()) {
            status_bar_text_ = "Vista activa: " + av->title;
        }
        return true;
    }
    if (command_id == "Ventana_Siguiente") {
        mdi_manager_.activate_next();
        if (const auto* av = mdi_manager_.active_view()) {
            status_bar_text_ = "Vista activa: " + av->title;
        }
        return true;
    }
    if (command_id == "Ventana_Cascada") {
        mdi_manager_.arrange_cascade();
        status_bar_text_ = "Disposición MDI: Cascada (" + std::to_string(mdi_manager_.views().size()) + " ventanas)";
        return true;
    }
    if (command_id == "Ventana_MosaicoHorizontal") {
        mdi_manager_.arrange_tile_horizontal();
        status_bar_text_ = "Disposición MDI: Mosaico horizontal (" + std::to_string(mdi_manager_.views().size()) + " ventanas)";
        return true;
    }
    if (command_id == "Ventana_MosaicoVertical") {
        mdi_manager_.arrange_tile_vertical();
        status_bar_text_ = "Disposición MDI: Mosaico vertical (" + std::to_string(mdi_manager_.views().size()) + " ventanas)";
        return true;
    }
    if (command_id == "Ventana_OrdenarVentanas") {
        mdi_manager_.arrange_order_grid();
        status_bar_text_ = "Disposición MDI: Ordenar ventanas (" + std::to_string(mdi_manager_.views().size()) + " ventanas)";
        return true;
    }

    // Comandos de Vista 3D y Planos/Niveles/Cotas (P-04.1 / P-04.2)
    if (command_id == "Obra_Vista3D" || command_id == "Obra_Vista3D_vista" || command_id == "vista_3D") {
        if (auto* av = mdi_manager_.active_view_mut()) {
            av->camera.zoom_extents(obra_.bounding_box(), av->rect.width, av->rect.height);
        }
        status_bar_text_ = "Vista 3D de la estructura actualizada.";
        return true;
    }
    if (command_id == "Obra_niveles") {
        const double next_z = static_cast<double>(obra_.estructura().niveles.size() + 1) * 3.0;
        const auto niv_id = obra_.add_nivel_planta("Planta " + std::to_string(obra_.estructura().niveles.size() + 1), next_z, 3.0);
        if (const auto& nivs = obra_.estructura().niveles; !nivs.empty()) {
            mdi_manager_.create_view_planta_nivel(nivs.back());
        }
        status_bar_text_ = "Nivel de planta creado (ID " + std::to_string(niv_id) + ") a cota Z = " + std::to_string(next_z) + " m.";
        return true;
    }
    if (command_id == "Planos_Cotas_Mostrar") {
        dimensions_.show_all();
        status_bar_text_ = "Acotación: todas las cotas visibles.";
        return true;
    }
    if (command_id == "Planos_Cotas_Ocultar") {
        dimensions_.hide_all();
        status_bar_text_ = "Acotación: todas las cotas ocultas.";
        return true;
    }

    // Comandos de Fase 5 (Nudos, Barras, Diafragmas, Pandeo, Biblioteca de Perfiles y Rótulas Plásticas)
    if (command_id == "Obra_reiniciar_numeracion") {
        obra_.reiniciar_numeracion_nudos_y_barras();
        status_bar_text_ = "Numeración de nudos y barras reiniciada.";
        return true;
    }
    if (command_id == "gen_diafrags") {
        const auto creados = obra_.generar_diafragmas_por_niveles();
        status_bar_text_ = "Diafragmas rígidos generados por niveles: " + std::to_string(creados);
        return true;
    }
    if (command_id == "Obra_RepararGeometria") {
        const auto cruces = obra_.generar_nudos_y_dividir_barras_en_cruces();
        status_bar_text_ = "Reparación geométrica: " + std::to_string(cruces) + " intersecciones resueltas.";
        return true;
    }
    if (command_id == "Calculo_AsignaCoefsPan") {
        const auto actualizadas = obra_.calcular_pandeo_automatico_estructura(false, false);
        status_bar_text_ = "Cálculo aproximado de longitudes de pandeo: " + std::to_string(actualizadas) + " barras actualizadas.";
        return true;
    }
    if (command_id == "Gestion_biblioteca_perfiles") {
        const auto total_perf = obra_.estructura().biblioteca_perfiles.catalog().size();
        status_bar_text_ = "Biblioteca de perfiles activa: " + std::to_string(total_perf) + " perfiles disponibles.";
        return true;
    }
    if (command_id == "Calculo_BibRotulasPlasticas") {
        const auto total_rot = obra_.estructura().catalogo_rotulas_plasticas.size();
        status_bar_text_ = "Biblioteca de rótulas plásticas (FEMA 356 / ASCE 41): " + std::to_string(total_rot) + " tipos definidos.";
        return true;
    }

    // Comandos de Fase 6 y Fase 7 (Láminas, Paños y Cargas)
    if (command_id == "Carga_hipotesis_vista") {
        auto& vis = obra_.estructura_mut().config_vis_cargas;
        const auto& hips = obra_.estructura().hipotesis;
        if (!hips.empty()) {
            vis.hipotesis_activa_vista_id = (vis.hipotesis_activa_vista_id % static_cast<std::uint32_t>(hips.size())) + 1;
        }
        status_bar_text_ = "Hipótesis de carga activa en vista: ID " + std::to_string(vis.hipotesis_activa_vista_id);
        return true;
    }
    if (command_id == "Carga_escalas") {
        const auto& vis = obra_.estructura().config_vis_cargas;
        status_bar_text_ = "Escalas de dibujo de cargas (Puntual=" + std::to_string(vis.escala_cargas_puntuales) +
                           ", Lineal=" + std::to_string(vis.escala_cargas_lineales) +
                           ", Superficial=" + std::to_string(vis.escala_cargas_superficiales) + ")";
        return true;
    }

    // Comandos de Fase 8 (Cálculo Matricial 3D, MEF, Estabilidad Global y Pushover — P-09.1 / P-09.2)
    if (command_id == "Calculo_calcular" || command_id == "Calculo_Calcular") {
        last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        if (obra_.datos_generales().con_sismo_dinamico) {
            last_solver_summary_.analisis_modal = solver::SolverEngine::analyze_modal_spectral(
                obra_, obra_.datos_generales().modos_vibracion_sismo);
        }
        status_bar_text_ = "Cálculo completado: " + std::to_string(last_solver_summary_.dof_count) +
                           " GDL, u_max = " + std::to_string(last_solver_summary_.max_displacement_m * 1000.0) + " mm.";
        return true;
    }
    if (command_id == "Calculo_CalcModal" || command_id == "Calculo_sismo_dinamico") {
        last_solver_summary_.analisis_modal = solver::SolverEngine::analyze_modal_spectral(
            obra_, obra_.datos_generales().modos_vibracion_sismo);
        status_bar_text_ = "Análisis modal espectral completado: " +
                           std::to_string(last_solver_summary_.analisis_modal->modos.size()) + " modos.";
        return true;
    }
    if (command_id == "Calculo_CalcPan") {
        last_solver_summary_.analisis_pandeo_global = solver::SolverEngine::analyze_global_buckling(obra_, {{1, 1.0}});
        const double lcr = last_solver_summary_.analisis_pandeo_global->lambda_cr_minimo;
        status_bar_text_ = "Análisis de estabilidad global completado (lambda_cr = " + std::to_string(lcr) + ").";
        return true;
    }
    if (command_id == "PO_Calcular") {
        last_solver_summary_.analisis_pushover = solver::SolverEngine::analyze_pushover(obra_);
        status_bar_text_ = "Análisis sísmico estático no lineal Pushover completado (" +
                           std::to_string(last_solver_summary_.analisis_pushover->curva_capacidad.size()) + " pasos).";
        return true;
    }

    // Comandos de Fase 9 (Comprobación ELU/ELS/Fuego, Dimensionamiento Óptimo y Editores H.A. — P-10.1 / P-10.2)
    if (command_id == "Calculo_comprueba_elementos" || command_id == "Calculo_comprobaciones_ELU") {
        if (last_solver_summary_.envolventes_barras.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        last_design_summary_ = codes::MemberDesignEngine::comprobar_todas_las_barras(obra_, last_solver_summary_);
        status_bar_text_ = "Comprobación de barras completada: " +
                           std::to_string(last_design_summary_.barras_que_cumplen) + "/" +
                           std::to_string(last_design_summary_.comprobaciones_barras.size()) +
                           " cumplen (aprov. máx = " +
                           std::to_string(last_design_summary_.aprovechamiento_maximo_obra_pct) + "%).";
        return true;
    }
    if (command_id == "dimensionamiento_desde_primer_perfil" || command_id == "dimensionamiento_desde_perfil_actual") {
        const auto modo = (command_id == "dimensionamiento_desde_primer_perfil")
                              ? codes::ModoDimensionamientoSerie::DesdePrimerPerfilSerie
                              : codes::ModoDimensionamientoSerie::DesdePerfilActual;
        last_design_summary_ = codes::MemberDesignEngine::dimensionar_estructura_completa(
            obra_, last_solver_summary_, modo, true);
        std::size_t modificados = 0;
        for (const auto& dim : last_design_summary_.dimensionamientos_barras) {
            if (dim.perfil_modificado) {
                ++modificados;
            }
        }
        status_bar_text_ = "Dimensionamiento óptimo completado: " +
                           std::to_string(modificados) +
                           " perfiles actualizados.";
        return true;
    }
    if (command_id == "Calculo_Dimensionar_revestimiento_incendio") {
        if (last_solver_summary_.envolventes_barras.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        last_design_summary_ = codes::MemberDesignEngine::comprobar_todas_las_barras(obra_, last_solver_summary_);
        status_bar_text_ = "Dimensionamiento de revestimiento de protección al fuego completado.";
        return true;
    }
    if (command_id == "pilares_armaduras") {
        if (last_solver_summary_.envolventes_barras.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        std::size_t redim = 0;
        for (auto& p : obra_.estructura_mut().pilares) {
            if (!p.bloqueado_armado) {
                const auto tramos_res = codes::ReinforcedConcreteEditorEngine::dimensionar_pilar_completo(
                    obra_, p, last_solver_summary_);
                redim += tramos_res.size();
            }
        }
        status_bar_text_ = "Editor de armaduras de pilares H.A.: " + std::to_string(redim) + " tramos dimensionados.";
        return true;
    }
    if (command_id == "pilares_bloquear" || command_id == "pilares_desbloquear") {
        const bool bloq = (command_id == "pilares_bloquear");
        for (auto& p : obra_.estructura_mut().pilares) {
            p.bloqueado_armado = bloq;
        }
        status_bar_text_ = bloq ? "Armaduras de pilares bloqueadas." : "Armaduras de pilares desbloqueadas.";
        return true;
    }
    if (command_id == "pv_ed_editar_todos_portico") {
        if (last_solver_summary_.envolventes_barras.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        std::size_t redim = 0;
        for (auto& port : obra_.estructura_mut().porticos_vigas) {
            if (!port.bloqueado_armado) {
                const auto vanos_res = codes::ReinforcedConcreteEditorEngine::dimensionar_portico_vigas(
                    obra_, port, last_solver_summary_);
                redim += vanos_res.size();
            }
        }
        status_bar_text_ = "Editor de pórticos de vigas H.A.: " + std::to_string(redim) + " vanos dimensionados.";
        return true;
    }
    if (command_id == "pv_ed_bloquear" || command_id == "pv_ed_desbloquear") {
        const bool bloq = (command_id == "pv_ed_bloquear");
        for (auto& port : obra_.estructura_mut().porticos_vigas) {
            port.bloqueado_armado = bloq;
        }
        status_bar_text_ = bloq ? "Armaduras de pórticos de vigas bloqueadas."
                                : "Armaduras de pórticos de vigas desbloqueadas.";
        return true;
    }

    if (item->kind == RibbonButtonKind::ReceptorOpcion) {
        status_bar_text_ = "Herramienta activa: " + item->label + " [" + item->id + "]";
    } else {
        status_bar_text_ = "Ejecutado comando: " + item->label + " (" + item->id + ")";
    }
    return true;
}

void AppWindow::sync_status_bar() noexcept {
    if (!obra_.abierta()) {
        status_bar_text_ = std::string(WelcomeScreenModel::kNoWorkStatusMessage);
        return;
    }
    const std::string solapa_str = (obra_.solapa_activa() == model::SolapaActiva::Estructura)
                                       ? "Estructura [METAL3D_ESTRUCTURA]"
                                       : "Cimentación [ID_CIMENTACION]";
    status_bar_text_ = "Solapa: " + solapa_str + " | Unidades: " +
                       std::string(units::UnitManager::system_name(obra_.datos_generales().sistema_unidades)) +
                       " | Nudos: " + std::to_string(obra_.estructura().nudos.size()) +
                       " | Barras: " + std::to_string(obra_.estructura().barras.size()) +
                       " | Zapatas: " + std::to_string(obra_.cimentacion().elementos.size());
}

std::string AppWindow::window_title() const {
    if (!obra_.abierta()) {
        return "CYPE 3D v2027.a (CypeClon C++20)";
    }
    return "CYPE 3D v2027.a (CypeClon C++20) - " + obra_.datos_generales().clave + ".ed3";
}

int AppWindow::run_headless_self_check() const {
    std::cout << "[CypeClon] Ventana principal: " << window_title() << "\n";
    std::cout << "[CypeClon] Sistema de unidades activo: "
              << units::UnitManager::system_name(obra_.datos_generales().sistema_unidades) << "\n";
    std::cout << "[CypeClon] Solapa Estructura (METAL3D_ESTRUCTURA): "
              << ribbon_.estructura_tabs().size() << " pestanas Ribbon registradas.\n";
    std::cout << "[CypeClon] Solapa Cimentacion (ID_CIMENTACION): "
              << ribbon_.cimentacion_groups().size() << " grupos de banda registrados.\n";
    std::cout << "[CypeClon] Total comandos registrados en entomenu/ribbon: "
              << ribbon_.total_registered_commands() << " comandos, "
              << palette_.cached_icon_count() << " iconos en cache componen.dll.\n";
    return 0;
}

#ifdef _WIN32
namespace {

struct Win32GuiContext {
    AppWindow* app{nullptr};
    HWND hwnd_main{nullptr};
    HWND hwnd_area_dibujo{nullptr};
    HDC gl_hdc{nullptr};
    HGLRC gl_hrc{nullptr};
};

void RenderOpenGLViewport(Win32GuiContext* ctx, int width, int height) {
    if (!ctx || !ctx->gl_hdc || !ctx->gl_hrc || width <= 0 || height <= 0) {
        return;
    }
    wglMakeCurrent(ctx->gl_hdc, ctx->gl_hrc);
    glViewport(0, 0, width, height);

    const auto bg = ctx->app->palette().color_fondo_vista_segun_tema();
    glClearColor(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-15.0, 15.0, -10.0, 10.0, -100.0, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Dibujar rejilla de referencia y triedro de ejes globales (X rojo, Y verde, Z azul)
    glBegin(GL_LINES);
    glColor3f(0.75f, 0.77f, 0.80f);
    for (int i = -12; i <= 12; i += 2) {
        glVertex3f(static_cast<float>(i), -8.0f, 0.0f);
        glVertex3f(static_cast<float>(i), 8.0f, 0.0f);
    }
    glColor3f(0.85f, 0.15f, 0.15f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(3.0f, 0.0f, 0.0f);

    glColor3f(0.15f, 0.70f, 0.20f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 3.0f, 0.0f);

    glColor3f(0.15f, 0.35f, 0.85f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 3.0f);

    // Dibujar barras de la obra activa
    glColor3f(0.10f, 0.30f, 0.65f);
    for (const auto& b : ctx->app->obra().estructura().barras) {
        const auto* ni = ctx->app->obra().find_nudo(b.nudo_inicial_id);
        const auto* nf = ctx->app->obra().find_nudo(b.nudo_final_id);
        if (ni && nf) {
            glVertex3d(ni->pos.x - 6.0, ni->pos.z - 3.0, ni->pos.y);
            glVertex3d(nf->pos.x - 6.0, nf->pos.z - 3.0, nf->pos.y);
        }
    }
    glEnd();

    SwapBuffers(ctx->gl_hdc);
}

LRESULT CALLBACK CypeAreaDibujoWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* ctx = reinterpret_cast<Win32GuiContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            RenderOpenGLViewport(ctx, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

void FillRectColor(HDC hdc, const RectPx& r, RgbColor c) {
    RECT rc{r.x, r.y, r.right(), r.bottom()};
    HBRUSH br = CreateSolidBrush(c.to_win32_colorref());
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

void DrawTextUtf8(HDC hdc, const RectPx& r, std::string_view text, RgbColor color, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    RECT rc{r.x, r.y, r.right(), r.bottom()};
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color.to_win32_colorref());
    const std::wstring wtext = core::StringUtils::utf8_to_wide(text);
    DrawTextW(hdc, wtext.c_str(), static_cast<int>(wtext.size()), &rc, flags);
}

LRESULT CALLBACK CypeAplicacionWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* ctx = reinterpret_cast<Win32GuiContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_SIZE: {
            if (ctx && ctx->app) {
                const int w = LOWORD(lparam);
                const int h = HIWORD(lparam);
                ctx->app->docking().recalculate_layout(w, h, ctx->app->active_solapa());
                if (ctx->hwnd_area_dibujo) {
                    const auto& r = ctx->app->docking().layout().cype_area_dibujo;
                    MoveWindow(ctx->hwnd_area_dibujo, r.x, r.y, r.width, r.height, TRUE);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (ctx && ctx->app) {
                const int px = LOWORD(lparam);
                const int py = HIWORD(lparam);
                const auto& lay = ctx->app->docking().layout();
                if (lay.tab_solapa_estructura.contains(px, py)) {
                    ctx->app->switch_solapa(model::SolapaActiva::Estructura);
                    const auto& r = ctx->app->docking().layout().cype_area_dibujo;
                    if (ctx->hwnd_area_dibujo) MoveWindow(ctx->hwnd_area_dibujo, r.x, r.y, r.width, r.height, TRUE);
                    InvalidateRect(hwnd, nullptr, TRUE);
                } else if (lay.tab_solapa_cimentacion.contains(px, py)) {
                    ctx->app->switch_solapa(model::SolapaActiva::Cimentacion);
                    const auto& r = ctx->app->docking().layout().cype_area_dibujo;
                    if (ctx->hwnd_area_dibujo) MoveWindow(ctx->hwnd_area_dibujo, r.x, r.y, r.width, r.height, TRUE);
                    InvalidateRect(hwnd, nullptr, TRUE);
                } else if (lay.barra_tabs_ribbon.contains(px, py) &&
                           ctx->app->active_solapa() == model::SolapaActiva::Estructura) {
                    const int tab_idx = (px - 12) / 96;
                    if (tab_idx >= 0 && static_cast<std::size_t>(tab_idx) < ctx->app->ribbon().estructura_tabs().size()) {
                        ctx->app->ribbon().set_active_estructura_tab_index(static_cast<std::size_t>(tab_idx));
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            if (ctx && ctx->app) {
                const auto& pal = ctx->app->palette().colors();
                const auto& lay = ctx->app->docking().layout();

                // Barra de título y pestañas Ribbon
                FillRectColor(hdc, lay.barra_titulo, pal.app_barra_titulo);
                DrawTextUtf8(hdc, {16, 0, 800, 26}, ctx->app->window_title(), {255, 255, 255});

                FillRectColor(hdc, lay.barra_tabs_ribbon, pal.ribbon_tab_inactiva_fondo);
                if (ctx->app->active_solapa() == model::SolapaActiva::Estructura) {
                    const auto& tabs = ctx->app->ribbon().estructura_tabs();
                    for (std::size_t i = 0; i < tabs.size(); ++i) {
                        const bool active = (i == ctx->app->ribbon().active_estructura_tab_index());
                        RectPx tr{static_cast<int>(12 + i * 96), 26, 92, 24};
                        FillRectColor(hdc, tr, active ? pal.ribbon_tab_activa_fondo : pal.ribbon_tab_inactiva_fondo);
                        DrawTextUtf8(hdc, tr, tabs[i].title,
                                     active ? pal.ribbon_tab_activa_texto : pal.ribbon_tab_inactiva_texto,
                                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                } else {
                    RectPx tr{12, 26, 120, 24};
                    FillRectColor(hdc, tr, pal.ribbon_tab_activa_fondo);
                    DrawTextUtf8(hdc, tr, "Cimentación", pal.ribbon_tab_activa_texto, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }

                // Banda Ribbon y barra CAD
                FillRectColor(hdc, lay.banda_ribbon, pal.ribbon_banda_fondo);
                FillRectColor(hdc, lay.barra_superior_cad, pal.panel_dock_fondo);

                // Paneles laterales pndock en Estructura
                if (lay.left_dock_visible) {
                    FillRectColor(hdc, lay.sub_dock_izquierdo, pal.panel_dock_fondo);
                    FillRectColor(hdc, {0, lay.panel_etiquetas_categorias.y, 382, 24}, pal.panel_dock_barra_titulo);
                    DrawTextUtf8(hdc, {8, lay.panel_etiquetas_categorias.y, 360, 24}, "Etiquetas y Categorías", pal.panel_texto_primario);

                    FillRectColor(hdc, {0, lay.panel_opciones_visualizacion.y, 382, 24}, pal.panel_dock_barra_titulo);
                    DrawTextUtf8(hdc, {8, lay.panel_opciones_visualizacion.y, 360, 24}, "Opciones de visualización", pal.panel_texto_primario);
                }

                // Selector inferior de solapas (Estructura / Cimentación) y barra de estado
                FillRectColor(hdc, lay.franja_selector_solapas, pal.solapa_inferior_inactiva);
                const bool is_est = (ctx->app->active_solapa() == model::SolapaActiva::Estructura);
                FillRectColor(hdc, lay.tab_solapa_estructura, is_est ? pal.solapa_inferior_activa : pal.solapa_inferior_inactiva);
                DrawTextUtf8(hdc, lay.tab_solapa_estructura, "Estructura", pal.panel_texto_primario, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                FillRectColor(hdc, lay.tab_solapa_cimentacion, !is_est ? pal.solapa_inferior_activa : pal.solapa_inferior_inactiva);
                DrawTextUtf8(hdc, lay.tab_solapa_cimentacion, "Cimentación", pal.panel_texto_primario, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                FillRectColor(hdc, lay.barra_estado, pal.barra_estado_fondo);
                DrawTextUtf8(hdc, {8, lay.barra_estado.y, lay.barra_estado.width - 16, lay.barra_estado.height},
                             ctx->app->status_bar_text(), pal.panel_texto_primario);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

void RegisterCypeWindowClasses(HINSTANCE hinst) {
    const wchar_t* classes[] = {
        L"CypeAplicacion",
        L"CypeDialogo",
        L"CypeMiniSolapas",
        L"CypeMdiFrame",
        L"CypeDialogoInterior",
        L"CypeAreaDibujo"
    };
    for (const wchar_t* cls_name : classes) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = ( std::wstring_view(cls_name) == L"CypeAreaDibujo" )
                             ? CypeAreaDibujoWndProc
                             : CypeAplicacionWndProc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = cls_name;
        RegisterClassExW(&wc);
    }
}

} // namespace
#endif

int AppWindow::run_win32_window() {
#ifdef _WIN32
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    RegisterCypeWindowClasses(hinst);

    Win32GuiContext ctx{};
    ctx.app = this;

    const std::wstring wtitle = core::StringUtils::utf8_to_wide(window_title());
    HWND hwnd = CreateWindowExW(
        0,
        L"CypeAplicacion",
        wtitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1032,
        nullptr, nullptr, hinst, nullptr
    );
    if (!hwnd) {
        return 1;
    }
    ctx.hwnd_main = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ctx));

    const auto& area_rc = docking_.layout().cype_area_dibujo;
    HWND hwnd_gl = CreateWindowExW(
        0,
        L"CypeAreaDibujo",
        L"",
        WS_CHILD | WS_VISIBLE,
        area_rc.x, area_rc.y, area_rc.width, area_rc.height,
        hwnd, nullptr, hinst, nullptr
    );
    ctx.hwnd_area_dibujo = hwnd_gl;
    SetWindowLongPtrW(hwnd_gl, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ctx));

    ctx.gl_hdc = GetDC(hwnd_gl);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    const int pf = ChoosePixelFormat(ctx.gl_hdc, &pfd);
    if (pf != 0) {
        SetPixelFormat(ctx.gl_hdc, pf, &pfd);
        ctx.gl_hrc = wglCreateContext(ctx.gl_hdc);
        wglMakeCurrent(ctx.gl_hdc, ctx.gl_hrc);
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (ctx.gl_hrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(ctx.gl_hrc);
    }
    if (ctx.gl_hdc && hwnd_gl) {
        ReleaseDC(hwnd_gl, ctx.gl_hdc);
    }
    return static_cast<int>(msg.wParam);
#else
    return run_headless_self_check();
#endif
}

} // namespace cypeclon::gui
