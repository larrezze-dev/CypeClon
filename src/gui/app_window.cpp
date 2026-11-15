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
    if (command_id == "Ayuda_MemoriaCalculo" || command_id == "Archivo_Listados") {
        last_report_doc_ = io::ReportsDrawingsBimEngine::generate_project_report(
            obra_, &last_solver_summary_, &last_design_summary_,
            &last_baseplates_, &last_connections_, &last_foundation_model_);
        last_exported_payload_ = io::ReportsDrawingsBimEngine::export_report(
            last_report_doc_, io::FormatoExportacionListado::HTML);
        status_bar_text_ = "Listado y Memoria de Cálculo generados (" +
                           std::to_string(last_report_doc_.capitulos.size()) + " capítulos).";
        return true;
    }
    if (command_id == "Archivo_Planos") {
        const auto sheet = io::ReportsDrawingsBimEngine::compose_drawing_sheet(
            obra_, io::TipoPlanoObra::Estructura3D, io::FormatoHojaPlano::ISO_A1, 100.0,
            &last_baseplates_, &last_connections_, &last_foundation_model_, {"EAC001", "CSZ002"});
        last_exported_payload_ = io::ReportsDrawingsBimEngine::export_drawing_to_dxf(sheet);
        status_bar_text_ = "Plano de obra generado en DXF (" + std::to_string(sheet.lineas.size()) + " líneas).";
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
    if (command_id == "Obra_Vista3D" || command_id == "Obra_Vista3D_vista" ||
        (command_id == "vista_3D" && obra_.solapa_activa() == model::SolapaActiva::Estructura)) {
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
        last_utilization_colors_ = cad::ResultsVisualizationEngine::generar_mapa_colores_aprovechamiento_barras(
            obra_, last_design_summary_);
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

    // Comandos de Fase 10 (Consulta Gráfica de Resultados en Estructura 3D — P-11.1 / T-016)
    if (command_id == "Calculo_Desplazamientos" || command_id == "PO_Desplazamientos" ||
        command_id == "Calculo_ModosVibracion" || command_id == "Calculo_ModosPandeo") {
        if (last_solver_summary_.resultados_hipotesis.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        auto origen = cad::ModoOrigenDeformada::HipotesisSimple;
        if (command_id == "Calculo_ModosVibracion") {
            if (!last_solver_summary_.analisis_modal.has_value()) {
                last_solver_summary_.analisis_modal = solver::SolverEngine::analyze_modal_spectral(
                    obra_, obra_.datos_generales().modos_vibracion_sismo);
            }
            origen = cad::ModoOrigenDeformada::ModoVibracionSismico;
        } else if (command_id == "Calculo_ModosPandeo") {
            if (!last_solver_summary_.analisis_pandeo_global.has_value()) {
                last_solver_summary_.analisis_pandeo_global = solver::SolverEngine::analyze_global_buckling(obra_, {{1, 1.0}});
            }
            origen = cad::ModoOrigenDeformada::ModoPandeoGlobal;
        }
        last_deformed_scene_ = cad::ResultsVisualizationEngine::generar_deformada_3d(
            obra_, last_solver_summary_, origen, 1);
        status_bar_text_ = "Deformada 3D generada (" + last_deformed_scene_.titulo_caso +
                           ", u_max = " + std::to_string(last_deformed_scene_.desplazamiento_maximo_mm) + " mm).";
        return true;
    }
    if (command_id == "Calculo_Reacciones" || command_id == "PO_Reacciones") {
        if (last_solver_summary_.resultados_hipotesis.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        last_support_reactions_ = cad::ResultsVisualizationEngine::generar_vectores_reacciones_apoyos(
            obra_, last_solver_summary_, 1, false);
        status_bar_text_ = "Consulta gráfica de reacciones en apoyos: " +
                           std::to_string(last_support_reactions_.size()) + " apoyos con reacción.";
        return true;
    }
    if (command_id == "Calculo_esfuerzos" || command_id == "PO_Esfuerzos") {
        if (last_solver_summary_.resultados_hipotesis.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        last_bar_diagrams_ = cad::ResultsVisualizationEngine::generar_diagramas_esfuerzos_barras(
            obra_, last_solver_summary_, cad::ComponenteEsfuerzoBarra::Flector_My, true);
        status_bar_text_ = "Diagramas 3D de esfuerzos y envolventes en barras generados (" +
                           std::to_string(last_bar_diagrams_.size()) + " barras).";
        return true;
    }
    if (command_id == "Calculo_esfuerzos3D" || command_id == "PO_esfuerzos3D") {
        if (last_solver_summary_.resultados_hipotesis.empty() &&
            (!obra_.estructura().barras.empty() || !obra_.estructura().laminas.empty())) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        last_deformed_scene_ = cad::ResultsVisualizationEngine::generar_deformada_3d(
            obra_, last_solver_summary_, cad::ModoOrigenDeformada::HipotesisSimple, 1);
        last_shell_isovalues_ = cad::ResultsVisualizationEngine::generar_mapa_isovalores_laminas(
            obra_, last_solver_summary_, cad::MagnitudIsovalorLamina::Tension_VonMises_Max, 1, false, true);
        status_bar_text_ = "Vista 3D de Isovalores y Deformada generada (" +
                           std::to_string(last_shell_isovalues_.triangulos.size()) + " triángulos, " +
                           std::to_string(last_shell_isovalues_.isolineas.size()) + " isolíneas).";
        return true;
    }

    // -------------------------------------------------------------------------
    // Comandos de Fase 11 (P-12.1): Uniones y Placas de Anclaje (`placancl`, `uniones`, `unionbase`)
    // -------------------------------------------------------------------------
    if (command_id == "Placas_Anclaje_generar" || command_id == "Placas_Anclaje_dimensionar" ||
        command_id == "Placas_Anclaje_comprobar" || command_id == "Placas_Anclaje_editar" ||
        command_id == "Placas_Anclaje_igualar" || command_id == "Placas_Anclaje_borrar") {
        if (last_solver_summary_.resultados_hipotesis.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        if (command_id == "Placas_Anclaje_borrar") {
            last_baseplates_.clear();
            status_bar_text_ = "Placas de anclaje borradas.";
            return true;
        }
        if (last_baseplates_.empty() || command_id == "Placas_Anclaje_generar") {
            last_baseplates_ = connections::ConnectionsEngine::generar_todas_las_placas_anclaje(obra_);
        }
        if (command_id == "Placas_Anclaje_igualar" && last_baseplates_.size() >= 2) {
            connections::ConnectionsEngine::igualar_placas_anclaje(last_baseplates_.front(), last_baseplates_, {});
        }
        std::size_t cumplen = 0;
        for (auto& pl : last_baseplates_) {
            const auto r = (command_id == "Placas_Anclaje_dimensionar")
                ? connections::ConnectionsEngine::dimensionar_placa_anclaje(obra_, pl, last_solver_summary_)
                : connections::ConnectionsEngine::comprobar_placa_anclaje(obra_, pl, last_solver_summary_);
            if (r.cumple_global) {
                ++cumplen;
            }
        }
        status_bar_text_ = "Placas de anclaje procesadas: " + std::to_string(cumplen) + "/" +
                           std::to_string(last_baseplates_.size()) + " cumplen todas las comprobaciones ELU.";
        return true;
    }
    if (command_id == "ued_generar" || command_id == "uaut_analizar" || command_id == "uniones_ed_calc" ||
        command_id == "ued_nueva" || command_id == "ued_editar" || command_id == "uaut_consulta_uniones" ||
        command_id == "uaut_consulta_uniones_sin_calculo" || command_id == "ued_analisis_rigidez_rotacional" ||
        command_id == "uaut_analisis_rigidez_rotacional" || command_id == "ued_agrupar" ||
        command_id == "ued_desagrupar" || command_id == "ued_bloquear" || command_id == "ued_desbloquear" ||
        command_id == "uaut_consultar_incidencias" || command_id == "uaut_opcs_calculo") {
        if (last_solver_summary_.resultados_hipotesis.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        if (last_connections_.empty() || command_id == "ued_generar" || command_id == "uaut_analizar" || command_id == "uniones_ed_calc") {
            last_connections_ = connections::ConnectionsEngine::detectar_y_generar_uniones_obra(obra_, last_solver_summary_);
        }
        if (command_id == "ued_analisis_rigidez_rotacional" || command_id == "uaut_analisis_rigidez_rotacional") {
            const auto n_act = connections::ConnectionsEngine::aplicar_rigideces_rotacionales_propuestas_a_obra(obra_, last_connections_);
            status_bar_text_ = "Rigideces rotacionales Sj aplicadas a " + std::to_string(n_act) + " extremos de piezas.";
            return true;
        }
        if (command_id == "ued_agrupar") {
            const auto n_grp = connections::ConnectionsEngine::agrupar_uniones_equivalentes(last_connections_);
            status_bar_text_ = "Uniones agrupadas en " + std::to_string(n_grp) + " tipos equivalentes.";
            return true;
        }
        std::size_t cumplen = 0;
        for (const auto& u : last_connections_) {
            if (u.cumple_global) {
                ++cumplen;
            }
        }
        status_bar_text_ = "Uniones estructurales dimensionadas: " + std::to_string(cumplen) + "/" +
                           std::to_string(last_connections_.size()) + " nudos resueltos satisfactoriamente.";
        return true;
    }

    // -------------------------------------------------------------------------
    // Comandos de Fase 12 (P-13.1 / T-018): Solapa Cimentación (`planocim`, `arranque`, `zptipos`, `encepado`, `vigacim`)
    // -------------------------------------------------------------------------
    if (command_id == "generar_zapatas_y_vigas" || command_id == "eliminar_solapes" ||
        (obra_.solapa_activa() == model::SolapaActiva::Cimentacion &&
         (command_id == "comprobar" || command_id == "dimensionar" || command_id == "vista_3D"))) {
        if (last_solver_summary_.resultados_hipotesis.empty() && !obra_.estructura().barras.empty()) {
            last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
        }
        if (command_id == "generar_zapatas_y_vigas" ||
            (last_foundation_model_.zapatas.empty() && last_foundation_model_.encepados.empty())) {
            last_foundation_model_ = foundation::FoundationEngine::generate_automatic_foundation(
                obra_, &last_solver_summary_, false, true);
            obra_.cimentacion_mut().elementos.clear();
            for (const auto& zp : last_foundation_model_.zapatas) {
                if (!zp.nudos_arranque_ids.empty()) {
                    obra_.add_elemento_cimentacion(
                        model::TipoElementoCimentacion::ZapataHormigonArmado,
                        zp.nudos_arranque_ids.front(),
                        zp.ancho_x_m, zp.ancho_y_m, zp.canto_total_m);
                }
            }
        }
        if (command_id == "eliminar_solapes") {
            const auto unidos = foundation::FoundationEngine::resolve_overlapping_footings(last_foundation_model_);
            foundation::FoundationEngine::check_all_foundation(last_foundation_model_, obra_.datos_generales());
            status_bar_text_ = "Eliminación de solapes en cimentación: " + std::to_string(unidos) +
                               " zapatas combinadas.";
            return true;
        }
        if (command_id == "comprobar") {
            foundation::FoundationEngine::check_all_foundation(last_foundation_model_, obra_.datos_generales());
            status_bar_text_ = "Comprobación de cimentación completada (" +
                               std::to_string(last_foundation_model_.resultados_zapatas.size()) + " zapatas, " +
                               std::to_string(last_foundation_model_.resultados_encepados.size()) + " encepados, " +
                               std::to_string(last_foundation_model_.resultados_vigas.size()) + " vigas).";
            return true;
        }
        if (command_id == "dimensionar") {
            foundation::FoundationEngine::design_all_foundation(obra_, last_foundation_model_);
            status_bar_text_ = "Dimensionamiento completo de cimentación: V_horm = " +
                               std::to_string(last_foundation_model_.medicion.volumen_hormigon_total_m3) +
                               " m3, Acero = " +
                               std::to_string(last_foundation_model_.medicion.peso_acero_total_con_mermas_kg) + " kg.";
            return true;
        }
        if (command_id == "vista_3D") {
            const auto scene3d = foundation::FoundationEngine::build_3d_scene(last_foundation_model_);
            status_bar_text_ = "Vista 3D de cimentación generada (" +
                               std::to_string(scene3d.bloques_hormigon.size()) + " bloques, " +
                               std::to_string(scene3d.barras_armadura_3d.size()) + " barras de armado 3D).";
            return true;
        }
        status_bar_text_ = "Generadas " + std::to_string(last_foundation_model_.zapatas.size()) +
                           " zapatas y " + std::to_string(last_foundation_model_.vigas.size()) + " vigas de cimentación.";
        return true;
    }

    // -------------------------------------------------------------------------
    // Comandos de Fase 13 (P-14.1, P-14.2, P-14.3 / T-019): Listados, Planos e Interoperabilidad BIM/CAD
    // -------------------------------------------------------------------------
    if (command_id == "Exportacion_A_Tekla" || command_id == "Exportacion_A_CIS2" ||
        command_id == "Exportacion_A_CSV" || command_id == "exportar_bim" ||
        command_id == "vincular_bim" || command_id == "Ayuda_MemoriaCalculo") {
        last_report_doc_ = io::ReportsDrawingsBimEngine::generate_project_report(
            obra_, &last_solver_summary_, &last_design_summary_,
            &last_baseplates_, &last_connections_, &last_foundation_model_);
        if (command_id == "Exportacion_A_Tekla") {
            last_exported_payload_ = io::ReportsDrawingsBimEngine::export_to_tekla(
                obra_, &last_connections_, &last_baseplates_);
            status_bar_text_ = "Exportación a Tekla Structures generada (" +
                               std::to_string(last_exported_payload_.size()) + " bytes).";
            return true;
        }
        if (command_id == "Exportacion_A_CIS2") {
            last_exported_payload_ = io::ReportsDrawingsBimEngine::export_to_cis2_step(
                obra_, &last_connections_, &last_baseplates_);
            status_bar_text_ = "Exportación a CIS/2 (STEP ISO-10303-21) generada (" +
                               std::to_string(last_exported_payload_.size()) + " bytes).";
            return true;
        }
        if (command_id == "Exportacion_A_CSV") {
            last_exported_payload_ = io::ReportsDrawingsBimEngine::export_report(
                last_report_doc_, io::FormatoExportacionListado::CSV);
            status_bar_text_ = "Exportación a CSV completada (" +
                               std::to_string(last_exported_payload_.size()) + " bytes).";
            return true;
        }
        if (command_id == "exportar_bim" || command_id == "vincular_bim") {
            last_exported_payload_ = io::ReportsDrawingsBimEngine::export_to_ifc(
                obra_, io::VersionEsquemaIfc::IFC4_ADD2_TC1, &last_foundation_model_, &last_baseplates_);
            status_bar_text_ = "Exportación BIM IFC4 + glTF 2.0 completada (" +
                               std::to_string(last_exported_payload_.size()) + " bytes).";
            return true;
        }
        last_exported_payload_ = io::ReportsDrawingsBimEngine::export_report(
            last_report_doc_, io::FormatoExportacionListado::HTML);
        status_bar_text_ = "Listado y Memoria de Cálculo generados (" +
                           std::to_string(last_report_doc_.capitulos.size()) + " capítulos).";
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

void AppWindow::load_default_demo_project() {
    obra_ = model::Obra{};
    obra_.set_abierta(true);
    obra_.datos_generales_mut().clave = "NaveIndustrial2027";
    obra_.datos_generales_mut().descripcion = "Nave Industrial de 2 Vanos con Puente Grúa, Uniones y Cimentación";
    obra_.datos_generales_mut().con_sismo_dinamico = true;
    obra_.datos_generales_mut().modos_vibracion_sismo = 6;

    // Generar 3 pórticos a dos aguas (Y = 0, 6, 12 m), luz X = 16 m, altura alero Z = 6 m, cumbrera Z = 8 m
    std::array<std::uint32_t, 3> n_base_izq{};
    std::array<std::uint32_t, 3> n_base_der{};
    std::array<std::uint32_t, 3> n_alero_izq{};
    std::array<std::uint32_t, 3> n_alero_der{};
    std::array<std::uint32_t, 3> n_cumb{};

    for (std::size_t idx = 0; idx < 3; ++idx) {
        const double y = static_cast<double>(idx) * 6.0;
        n_base_izq[idx]  = obra_.add_nudo({0.0,  y, 0.0}, model::VinculacionExterior::Empotrado);
        n_base_der[idx]  = obra_.add_nudo({16.0, y, 0.0}, model::VinculacionExterior::Empotrado);
        n_alero_izq[idx] = obra_.add_nudo({0.0,  y, 6.0}, model::VinculacionExterior::Libre);
        n_alero_der[idx] = obra_.add_nudo({16.0, y, 6.0}, model::VinculacionExterior::Libre);
        n_cumb[idx]      = obra_.add_nudo({8.0,  y, 8.0}, model::VinculacionExterior::Libre);

        // Pilares HEB 240 y dinteles IPE 300
        const auto b_pil_i = obra_.add_barra(n_base_izq[idx], n_alero_izq[idx], "HEB 240", "S275");
        const auto b_pil_d = obra_.add_barra(n_base_der[idx], n_alero_der[idx], "HEB 240", "S275");
        const auto b_din_i = obra_.add_barra(n_alero_izq[idx], n_cumb[idx],     "IPE 300", "S275");
        const auto b_din_d = obra_.add_barra(n_cumb[idx],      n_alero_der[idx],"IPE 300", "S275");

        if (auto* bp = obra_.find_barra_mut(b_pil_i)) bp->tipo_estructural = model::TipoBarraEstructural::ColumnaPilar;
        if (auto* bp = obra_.find_barra_mut(b_pil_d)) bp->tipo_estructural = model::TipoBarraEstructural::ColumnaPilar;
        if (auto* bd = obra_.find_barra_mut(b_din_i)) bd->tipo_estructural = model::TipoBarraEstructural::Viga;
        if (auto* bd = obra_.find_barra_mut(b_din_d)) bd->tipo_estructural = model::TipoBarraEstructural::Viga;

        // Cargas en nudos y cargas uniformes sobre dinteles
        obra_.add_carga_puntual_nudo(n_alero_izq[idx], 1, {18.0, 0.0, -45.0});
        obra_.add_carga_puntual_nudo(n_cumb[idx],      1, {0.0,  0.0, -65.0});
        obra_.add_carga_lineal_barra(b_din_i, 1, 8.5, 8.5, {0.0, 0.0, -1.0});
        obra_.add_carga_lineal_barra(b_din_d, 1, 8.5, 8.5, {0.0, 0.0, -1.0});
    }

    // Vigas de atado en aleros y correas ZF 200x2.5 en cumbrera + cruces de San Andrés (Tirantes R 16)
    for (std::size_t idx = 0; idx < 2; ++idx) {
        const auto b_at_i = obra_.add_barra(n_alero_izq[idx], n_alero_izq[idx + 1], "IPE 200", "S275");
        const auto b_at_d = obra_.add_barra(n_alero_der[idx], n_alero_der[idx + 1], "IPE 200", "S275");
        const auto b_cor  = obra_.add_barra(n_cumb[idx],      n_cumb[idx + 1],      "ZF 200x2.5", "S235");
        if (auto* b = obra_.find_barra_mut(b_at_i)) b->tipo_estructural = model::TipoBarraEstructural::Viga;
        if (auto* b = obra_.find_barra_mut(b_at_d)) b->tipo_estructural = model::TipoBarraEstructural::Viga;
        if (auto* b = obra_.find_barra_mut(b_cor))  b->tipo_estructural = model::TipoBarraEstructural::Viga;

        const auto t1 = obra_.add_barra(n_alero_izq[idx],     n_cumb[idx + 1], "R 16", "S275");
        const auto t2 = obra_.add_barra(n_alero_izq[idx + 1], n_cumb[idx],     "R 16", "S275");
        if (auto* t = obra_.find_barra_mut(t1)) {
            t->tipo_estructural = model::TipoBarraEstructural::Tirante;
            t->es_tirante = true;
        }
        if (auto* t = obra_.find_barra_mut(t2)) {
            t->tipo_estructural = model::TipoBarraEstructural::Tirante;
            t->es_tirante = true;
        }
    }

    // Losa de cimentación / solera MEF (Lamina3d) y Paños de cubierta (Panyo3d)
    obra_.add_lamina({{0.0, 0.0, 0.0}, {16.0, 0.0, 0.0}, {16.0, 12.0, 0.0}, {0.0, 12.0, 0.0}}, 0.20, "HA-25");
    const auto p_izq = obra_.add_panyo({{0.0, 0.0, 6.0}, {8.0, 0.0, 8.0}, {8.0, 12.0, 8.0}, {0.0, 12.0, 6.0}}, {1.0, 0.0, 0.25}, 1);
    const auto p_der = obra_.add_panyo({{8.0, 0.0, 8.0}, {16.0, 0.0, 6.0}, {16.0, 12.0, 6.0}, {8.0, 12.0, 8.0}}, {1.0, 0.0, -0.25}, 1);
    obra_.add_carga_superficial_panyo(p_izq, 1, 1.20);
    obra_.add_carga_superficial_panyo(p_der, 1, 1.20);

    // Ejecutar cálculo estructural inicial, comprobación ELU/ELS, uniones, placas y cimentación
    last_solver_summary_ = solver::SolverEngine::analyze_linear_static(obra_);
    last_design_summary_ = codes::MemberDesignEngine::comprobar_todas_las_barras(obra_, last_solver_summary_);
    last_utilization_colors_ = cad::ResultsVisualizationEngine::generar_mapa_colores_aprovechamiento_barras(
        obra_, last_design_summary_);
    last_deformed_scene_ = cad::ResultsVisualizationEngine::generar_deformada_3d(
        obra_, last_solver_summary_, cad::ModoOrigenDeformada::HipotesisSimple, 1);
    last_bar_diagrams_ = cad::ResultsVisualizationEngine::generar_diagramas_esfuerzos_barras(
        obra_, last_solver_summary_, cad::ComponenteEsfuerzoBarra::Flector_My, true);
    last_support_reactions_ = cad::ResultsVisualizationEngine::generar_vectores_reacciones_apoyos(
        obra_, last_solver_summary_, 1, false);
    last_baseplates_ = connections::ConnectionsEngine::generar_todas_las_placas_anclaje(obra_);
    for (auto& bp : last_baseplates_) {
        connections::ConnectionsEngine::dimensionar_placa_anclaje(obra_, bp, last_solver_summary_);
    }
    last_connections_ = connections::ConnectionsEngine::detectar_y_generar_uniones_obra(obra_, last_solver_summary_);
    last_foundation_model_ = foundation::FoundationEngine::generate_automatic_foundation(
        obra_, &last_solver_summary_, false, true);
    obra_.cimentacion_mut().elementos.clear();
    for (const auto& zp : last_foundation_model_.zapatas) {
        if (!zp.nudos_arranque_ids.empty()) {
            obra_.add_elemento_cimentacion(
                model::TipoElementoCimentacion::ZapataHormigonArmado,
                zp.nudos_arranque_ids.front(),
                zp.ancho_x_m, zp.ancho_y_m, zp.canto_total_m);
        }
    }
    last_report_doc_ = io::ReportsDrawingsBimEngine::generate_project_report(
        obra_, &last_solver_summary_, &last_design_summary_,
        &last_baseplates_, &last_connections_, &last_foundation_model_);

    if (auto* av = mdi_manager_.active_view_mut()) {
        av->camera.set_preset_view("Isometric");
        av->camera.zoom_extents(obra_.bounding_box(), av->rect.width, av->rect.height);
    }
    welcome_.add_recent_file("D:\\Proyectos\\NaveIndustrial2027.ed3", obra_.datos_generales().descripcion);
    sync_status_bar();
    status_bar_text_ = "Obra cargada y calculada: NaveIndustrial2027.ed3 | Arrastre con el ratón en la vista 3D para orbitar, rueda para zoom y clic en botones del Ribbon.";
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

struct ClickableRegion {
    RectPx rect{};
    std::string action_id;
    std::string label;
};

struct Win32GuiContext {
    AppWindow* app{nullptr};
    HWND hwnd_main{nullptr};
    HWND hwnd_area_dibujo{nullptr};
    HDC gl_hdc{nullptr};
    HGLRC gl_hrc{nullptr};

    // Fuentes GDI ClearType (Segoe UI)
    HFONT font_ui{nullptr};
    HFONT font_bold{nullptr};
    HFONT font_small{nullptr};
    HFONT font_mono{nullptr};

    // Estado de visualización interactiva 3D
    cad::RenderRepresentationMode render_mode{cad::RenderRepresentationMode::PerfilRealExtruido3d};
    bool show_deformed_3d{false};
    bool show_force_diagrams_3d{false};
    bool show_utilization_colors{true};
    bool show_loads_3d{true};
    bool show_connections_3d{true};
    bool show_report_modal{false};
    std::string report_modal_title;
    std::string report_modal_body;

    // Estado de interacción ratón en viewport OpenGL
    bool dragging_orbit{false};
    bool dragging_pan{false};
    int last_mouse_x{0};
    int last_mouse_y{0};
    std::optional<cad::PickHit> selected_entity{};

    // Regiones interactivas registradas en el último repintado
    std::vector<ClickableRegion> hit_regions;
    int hover_mouse_x{-1};
    int hover_mouse_y{-1};
};

std::array<double, 16> Mat4ToOpenGLColMajor(const core::Mat4d& mat) noexcept {
    std::array<double, 16> out{};
    for (std::size_t col = 0; col < 4; ++col) {
        for (std::size_t row = 0; row < 4; ++row) {
            out[col * 4 + row] = mat.m[row][col];
        }
    }
    return out;
}

void GlColorFromRgbHex(std::uint32_t rgb, float alpha = 1.0f) noexcept {
    const float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(rgb & 0xFF) / 255.0f;
    glColor4f(r, g, b, alpha);
}

void DrawBox3dWireAndSolid(const core::Vec3d& c, double dx, double dy, double dz, std::uint32_t rgb, float alpha = 0.85f) {
    const double hx = 0.5 * dx;
    const double hy = 0.5 * dy;
    const double hz = 0.5 * dz;
    const core::Vec3d v[8] = {
        {c.x - hx, c.y - hy, c.z - hz},
        {c.x + hx, c.y - hy, c.z - hz},
        {c.x + hx, c.y + hy, c.z - hz},
        {c.x - hx, c.y + hy, c.z - hz},
        {c.x - hx, c.y - hy, c.z + hz},
        {c.x + hx, c.y - hy, c.z + hz},
        {c.x + hx, c.y + hy, c.z + hz},
        {c.x - hx, c.y + hy, c.z + hz}
    };
    const int faces[6][4] = {
        {0, 1, 2, 3}, {4, 5, 6, 7},
        {0, 1, 5, 4}, {2, 3, 7, 6},
        {1, 2, 6, 5}, {3, 0, 4, 7}
    };
    const float shade[6] = {0.70f, 1.00f, 0.88f, 0.80f, 0.92f, 0.76f};

    glBegin(GL_QUADS);
    for (int f = 0; f < 6; ++f) {
        const float r = (static_cast<float>((rgb >> 16) & 0xFF) / 255.0f) * shade[f];
        const float g = (static_cast<float>((rgb >> 8) & 0xFF) / 255.0f) * shade[f];
        const float b = (static_cast<float>(rgb & 0xFF) / 255.0f) * shade[f];
        glColor4f(r, g, b, alpha);
        for (int k = 0; k < 4; ++k) {
            const auto& pt = v[faces[f][k]];
            glVertex3d(pt.x, pt.y, pt.z);
        }
    }
    glEnd();

    const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    glColor4f(0.12f, 0.16f, 0.22f, 0.9f);
    glBegin(GL_LINES);
    for (const auto& e : edges) {
        glVertex3d(v[e[0]].x, v[e[0]].y, v[e[0]].z);
        glVertex3d(v[e[1]].x, v[e[1]].y, v[e[1]].z);
    }
    glEnd();
}

void RenderOpenGLViewport(Win32GuiContext* ctx, int width, int height) {
    if (!ctx || !ctx->app || !ctx->gl_hdc || !ctx->gl_hrc || width <= 0 || height <= 0) {
        return;
    }
    wglMakeCurrent(ctx->gl_hdc, ctx->gl_hrc);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto bg = ctx->app->palette().color_fondo_vista_segun_tema();
    glClearColor(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto* av = ctx->app->mdi_manager().active_view_mut();
    if (!av) {
        SwapBuffers(ctx->gl_hdc);
        return;
    }
    av->rect.width = width;
    av->rect.height = height;

    const auto proj_gl = Mat4ToOpenGLColMajor(av->camera.projection_matrix(width, height));
    const auto view_gl = Mat4ToOpenGLColMajor(av->camera.view_matrix());

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(proj_gl.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(view_gl.data());

    const bool dark = ctx->app->is_dark_mode();

    // 1. Rejilla del plano de referencia Z = 0 y triedro de ejes globales (X rojo, Y verde, Z azul)
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    if (dark) {
        glColor4f(0.26f, 0.30f, 0.36f, 0.75f);
    } else {
        glColor4f(0.76f, 0.79f, 0.83f, 0.85f);
    }
    for (int i = -4; i <= 24; i += 2) {
        glVertex3d(static_cast<double>(i), -4.0, 0.0);
        glVertex3d(static_cast<double>(i), 18.0, 0.0);
    }
    for (int j = -4; j <= 18; j += 2) {
        glVertex3d(-4.0, static_cast<double>(j), 0.0);
        glVertex3d(24.0, static_cast<double>(j), 0.0);
    }
    glEnd();

    glLineWidth(2.5f);
    glBegin(GL_LINES);
    // Eje X global (Rojo)
    glColor3f(0.88f, 0.18f, 0.18f);
    glVertex3d(0.0, 0.0, 0.01);
    glVertex3d(2.5, 0.0, 0.01);
    // Eje Y global (Verde)
    glColor3f(0.16f, 0.72f, 0.24f);
    glVertex3d(0.0, 0.0, 0.01);
    glVertex3d(0.0, 2.5, 0.01);
    // Eje Z global (Azul)
    glColor3f(0.18f, 0.42f, 0.92f);
    glVertex3d(0.0, 0.0, 0.01);
    glVertex3d(0.0, 0.0, 2.5);
    glEnd();
    glLineWidth(1.0f);

    const auto& obra = ctx->app->obra();
    const auto cat_filter = ctx->app->build_category_filter();
    const auto& pref = ctx->app->docking().panel_preferencias();

    // Mapa rápido de color por aprovechamiento de barra
    auto find_bar_util_rgb = [&](std::uint32_t bar_id) -> std::optional<std::uint32_t> {
        if (!ctx->show_utilization_colors) {
            return std::nullopt;
        }
        for (const auto& u : ctx->app->last_utilization_colors()) {
            if (u.barra_id == bar_id) {
                return u.color_rgb;
            }
        }
        return std::nullopt;
    };

    // 2. Si estamos en la solapa Cimentación (o si hay cimentación generada), dibujar Zapatas, Encepados, Pilotes y Vigas
    if (ctx->app->active_solapa() == model::SolapaActiva::Cimentacion || !ctx->app->last_foundation_model().zapatas.empty()) {
        const auto& fm = ctx->app->last_foundation_model();
        const float alpha_cim = (ctx->app->active_solapa() == model::SolapaActiva::Cimentacion) ? 0.88f : 0.55f;
        const auto scene3d = foundation::FoundationEngine::build_3d_scene(fm);
        for (const auto& blk : scene3d.bloques_hormigon) {
            DrawBox3dWireAndSolid(
                blk.centro_m,
                blk.dimensiones_m.x, blk.dimensiones_m.y, blk.dimensiones_m.z,
                (blk.color_rgba >> 8) & 0xFFFFFFu, alpha_cim);
        }
        if (ctx->app->active_solapa() == model::SolapaActiva::Cimentacion) {
            // Dibujar barras de armadura 3D de las zapatas y vigas de cimentación
            glLineWidth(1.6f);
            glBegin(GL_LINES);
            for (const auto& rb : scene3d.barras_armadura_3d) {
                GlColorFromRgbHex(0xDC2626, 0.95f);
                for (std::size_t i = 0; i + 1 < rb.polilinea_m.size(); ++i) {
                    glVertex3d(rb.polilinea_m[i].x, rb.polilinea_m[i].y, rb.polilinea_m[i].z);
                    glVertex3d(rb.polilinea_m[i + 1].x, rb.polilinea_m[i + 1].y, rb.polilinea_m[i + 1].z);
                }
            }
            glEnd();
            glLineWidth(1.0f);
        }
    }

    // 3. Renderizar escena 3D de Estructura usando SceneTessellator (Alámbrico, HLR o Perfil Real Extruido 3D)
    const auto tess = ctx->app->scene_tessellator().build_scene(
        obra,
        av->camera,
        width,
        height,
        ctx->render_mode,
        cat_filter,
        pref.mostrar_ejes_locales,
        true,
        &ctx->app->dimensions());

    // Láminas y Paños (superficies)
    if (pref.dibujar_laminas_con_relleno && !tess.surface_triangles.empty()) {
        glBegin(GL_TRIANGLES);
        for (const auto& tri : tess.surface_triangles) {
            GlColorFromRgbHex(tri.color_rgb, 0.35f);
            glVertex3d(tri.v0.x, tri.v0.y, tri.v0.z);
            glVertex3d(tri.v1.x, tri.v1.y, tri.v1.z);
            glVertex3d(tri.v2.x, tri.v2.y, tri.v2.z);
        }
        glEnd();
    }

    // Perfiles reales extruidos en 3D con sombreado direccional por normales
    if (ctx->render_mode == cad::RenderRepresentationMode::PerfilRealExtruido3d) {
        const core::Vec3d light_dir = core::Vec3d{0.45, -0.60, 0.66}.normalized();
        glBegin(GL_TRIANGLES);
        for (const auto& mesh : tess.extruded_bar_meshes) {
            std::uint32_t base_rgb = 0x3B74C4;
            if (auto util_c = find_bar_util_rgb(mesh.barra_id)) {
                base_rgb = *util_c;
            }
            if (ctx->selected_entity && ctx->selected_entity->kind == cad::EntityKind::Barra &&
                ctx->selected_entity->entity_id == mesh.barra_id) {
                base_rgb = 0x00E5FF;
            }
            const float br = static_cast<float>((base_rgb >> 16) & 0xFF) / 255.0f;
            const float bg_c = static_cast<float>((base_rgb >> 8) & 0xFF) / 255.0f;
            const float bb = static_cast<float>(base_rgb & 0xFF) / 255.0f;

            for (std::size_t t = 0; t < mesh.triangle_indices.size(); ++t) {
                const auto& idx = mesh.triangle_indices[t];
                const core::Vec3d n = (t < mesh.normals.size()) ? mesh.normals[t] : core::Vec3d{0.0, 0.0, 1.0};
                const float lambert = static_cast<float>(0.55 + 0.45 * std::abs(n.dot(light_dir)));
                glColor4f(std::min(1.0f, br * lambert),
                          std::min(1.0f, bg_c * lambert),
                          std::min(1.0f, bb * lambert),
                          0.96f);
                glVertex3d(mesh.vertices[idx[0]].x, mesh.vertices[idx[0]].y, mesh.vertices[idx[0]].z);
                glVertex3d(mesh.vertices[idx[1]].x, mesh.vertices[idx[1]].y, mesh.vertices[idx[1]].z);
                glVertex3d(mesh.vertices[idx[2]].x, mesh.vertices[idx[2]].y, mesh.vertices[idx[2]].z);
            }
        }
        glEnd();

        // Aristas de contorno para nitidez CAD
        glColor4f(0.10f, 0.14f, 0.20f, 0.70f);
        glBegin(GL_LINES);
        for (const auto& mesh : tess.extruded_bar_meshes) {
            for (const auto& edge : mesh.contour_edges) {
                glVertex3d(edge.p0.x, edge.p0.y, edge.p0.z);
                glVertex3d(edge.p1.x, edge.p1.y, edge.p1.z);
            }
        }
        glEnd();
    } else if (ctx->render_mode == cad::RenderRepresentationMode::LineaOcultaHlr) {
        glLineWidth(1.6f);
        glBegin(GL_LINES);
        for (const auto& seg : tess.hlr_output.visible_segments) {
            const auto util_c = find_bar_util_rgb(seg.entity_id);
            GlColorFromRgbHex(util_c.value_or(seg.color_rgb), 1.0f);
            glVertex3d(seg.p0.x, seg.p0.y, seg.p0.z);
            glVertex3d(seg.p1.x, seg.p1.y, seg.p1.z);
        }
        glEnd();
        glLineWidth(1.0f);
    }

    // Ejes alámbricos, símbolos de apoyo y ejes locales
    glLineWidth(ctx->render_mode == cad::RenderRepresentationMode::AlambricoUnifilar ? 2.4f : 1.2f);
    glBegin(GL_LINES);
    for (const auto& seg : tess.wireframe_segments) {
        std::uint32_t col = seg.color_rgb;
        if (auto util_c = find_bar_util_rgb(seg.entity_id)) {
            col = *util_c;
        }
        if (ctx->selected_entity && ctx->selected_entity->kind == cad::EntityKind::Barra &&
            ctx->selected_entity->entity_id == seg.entity_id) {
            col = 0x00E5FF;
        }
        GlColorFromRgbHex(col, 1.0f);
        glVertex3d(seg.p0.x, seg.p0.y, seg.p0.z);
        glVertex3d(seg.p1.x, seg.p1.y, seg.p1.z);
    }
    for (const auto& seg : tess.support_symbol_segments) {
        GlColorFromRgbHex(seg.color_rgb, 1.0f);
        glVertex3d(seg.p0.x, seg.p0.y, seg.p0.z);
        glVertex3d(seg.p1.x, seg.p1.y, seg.p1.z);
    }
    for (const auto& seg : tess.local_axes_segments) {
        GlColorFromRgbHex(seg.color_rgb, 1.0f);
        glVertex3d(seg.p0.x, seg.p0.y, seg.p0.z);
        glVertex3d(seg.p1.x, seg.p1.y, seg.p1.z);
    }
    for (const auto& seg : tess.dimension_segments) {
        GlColorFromRgbHex(seg.color_rgb, 1.0f);
        glVertex3d(seg.p0.x, seg.p0.y, seg.p0.z);
        glVertex3d(seg.p1.x, seg.p1.y, seg.p1.z);
    }
    glEnd();
    glLineWidth(1.0f);

    // 4. Nudos visibles como puntos destacados
    if (cat_filter.nudos_visible) {
        glPointSize(6.5f);
        glBegin(GL_POINTS);
        for (const auto& n : obra.estructura().nudos) {
            if (ctx->selected_entity && ctx->selected_entity->kind == cad::EntityKind::Nudo &&
                ctx->selected_entity->entity_id == n.id) {
                glColor3f(0.0f, 0.95f, 1.0f);
            } else if (n.vinc_ext != model::VinculacionExterior::Libre) {
                glColor3f(0.85f, 0.22f, 0.18f);
            } else {
                glColor3f(0.15f, 0.25f, 0.45f);
            }
            glVertex3d(n.pos.x, n.pos.y, n.pos.z);
        }
        glEnd();
        glPointSize(1.0f);
    }

    // 5. Dibujar Placas de Anclaje y Uniones en 3D si están activas
    if (ctx->show_connections_3d) {
        for (const auto& bp : ctx->app->last_baseplates()) {
            if (const auto* n = obra.find_nudo(bp.nudo_id)) {
                DrawBox3dWireAndSolid(
                    {n->pos.x, n->pos.y, n->pos.z + 0.015},
                    bp.ancho_lx_mm * 0.001,
                    bp.largo_ly_mm * 0.001,
                    std::max(0.025, bp.espesor_e_mm * 0.001),
                    0xD97706, 0.92f);
            }
        }
        for (const auto& u : ctx->app->last_connections()) {
            if (const auto* n = obra.find_nudo(u.nudo_id)) {
                DrawBox3dWireAndSolid(
                    n->pos,
                    0.34, 0.34, 0.34,
                    u.cumple_global ? 0x0284C7 : 0xDC2626, 0.70f);
            }
        }
    }

    // 6. Dibujar vectores de Cargas 3D (CargaPuntualNudo y CargaLinealBarra)
    if (ctx->show_loads_3d) {
        glLineWidth(2.0f);
        glColor4f(0.86f, 0.20f, 0.52f, 0.90f);
        glBegin(GL_LINES);
        for (const auto& cn : obra.estructura().cargas_nudos) {
            if (const auto* n = obra.find_nudo(cn.nudo_id)) {
                const core::Vec3d f_vec = cn.fuerzas_kn;
                const double mag = f_vec.norm();
                if (mag > 1e-3) {
                    const core::Vec3d dir = f_vec / mag;
                    const double arrow_len = std::clamp(mag * 0.025, 0.6, 2.0);
                    const core::Vec3d tail = n->pos - dir * arrow_len;
                    glVertex3d(tail.x, tail.y, tail.z);
                    glVertex3d(n->pos.x, n->pos.y, n->pos.z);
                }
            }
        }
        for (const auto& cb : obra.estructura().cargas_lineales) {
            if (const auto* b = obra.find_barra(cb.barra_id)) {
                const auto* ni = obra.find_nudo(b->nudo_inicial_id);
                const auto* nf = obra.find_nudo(b->nudo_final_id);
                if (ni && nf) {
                    for (int k = 1; k <= 5; ++k) {
                        const double t = static_cast<double>(k) / 6.0;
                        const core::Vec3d p = ni->pos + (nf->pos - ni->pos) * t;
                        const core::Vec3d tail = p + core::Vec3d{0.0, 0.0, 0.9};
                        glVertex3d(tail.x, tail.y, tail.z);
                        glVertex3d(p.x, p.y, p.z);
                    }
                }
            }
        }
        glEnd();
        glLineWidth(1.0f);
    }

    // 7. Superponer Deformada 3D si está activa
    if (ctx->show_deformed_3d && !ctx->app->last_deformed_scene().barras_deformadas.empty()) {
        glLineWidth(2.6f);
        glBegin(GL_LINES);
        for (const auto& bd : ctx->app->last_deformed_scene().barras_deformadas) {
            for (std::size_t i = 0; i + 1 < bd.puntos.size(); ++i) {
                GlColorFromRgbHex(bd.puntos[i].color_rgb, 0.95f);
                glVertex3d(bd.puntos[i].pos_deformada_escalada_m.x,
                           bd.puntos[i].pos_deformada_escalada_m.y,
                           bd.puntos[i].pos_deformada_escalada_m.z);
                GlColorFromRgbHex(bd.puntos[i + 1].color_rgb, 0.95f);
                glVertex3d(bd.puntos[i + 1].pos_deformada_escalada_m.x,
                           bd.puntos[i + 1].pos_deformada_escalada_m.y,
                           bd.puntos[i + 1].pos_deformada_escalada_m.z);
            }
        }
        glEnd();
        glLineWidth(1.0f);
    }

    // 8. Superponer Diagramas de Esfuerzos 3D en Barras (My / Vz / N) si están activos
    if (ctx->show_force_diagrams_3d && !ctx->app->last_bar_diagrams().empty()) {
        for (const auto& diag : ctx->app->last_bar_diagrams()) {
            const auto& pts = diag.curva_principal_o_max;
            if (pref.dibujar_cargas_con_relleno && pts.size() >= 2) {
                glBegin(GL_QUADS);
                for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
                    GlColorFromRgbHex(pts[i].color_rgb, 0.38f);
                    glVertex3d(pts[i].punto_directriz_3d.x, pts[i].punto_directriz_3d.y, pts[i].punto_directriz_3d.z);
                    glVertex3d(pts[i].punto_diagrama_3d.x, pts[i].punto_diagrama_3d.y, pts[i].punto_diagrama_3d.z);
                    GlColorFromRgbHex(pts[i + 1].color_rgb, 0.38f);
                    glVertex3d(pts[i + 1].punto_diagrama_3d.x, pts[i + 1].punto_diagrama_3d.y, pts[i + 1].punto_diagrama_3d.z);
                    glVertex3d(pts[i + 1].punto_directriz_3d.x, pts[i + 1].punto_directriz_3d.y, pts[i + 1].punto_directriz_3d.z);
                }
                glEnd();
            }
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
                GlColorFromRgbHex(pts[i].color_rgb, 0.95f);
                glVertex3d(pts[i].punto_diagrama_3d.x, pts[i].punto_diagrama_3d.y, pts[i].punto_diagrama_3d.z);
                GlColorFromRgbHex(pts[i + 1].color_rgb, 0.95f);
                glVertex3d(pts[i + 1].punto_diagrama_3d.x, pts[i + 1].punto_diagrama_3d.y, pts[i + 1].punto_diagrama_3d.z);
            }
            glEnd();
            glLineWidth(1.0f);
        }
    }

    SwapBuffers(ctx->gl_hdc);
}

void FillRectColor(HDC hdc, const RectPx& r, RgbColor c) {
    RECT rc{r.x, r.y, r.right(), r.bottom()};
    HBRUSH br = CreateSolidBrush(c.to_win32_colorref());
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

void DrawBorderRect(HDC hdc, const RectPx& r, RgbColor c) {
    RECT rc{r.x, r.y, r.right(), r.bottom()};
    HBRUSH br = CreateSolidBrush(c.to_win32_colorref());
    FrameRect(hdc, &rc, br);
    DeleteObject(br);
}

void DrawTextUtf8(HDC hdc, const RectPx& r, std::string_view text, RgbColor color, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE, HFONT font = nullptr) {
    HGDIOBJ old_font = nullptr;
    if (font) {
        old_font = SelectObject(hdc, font);
    }
    RECT rc{r.x, r.y, r.right(), r.bottom()};
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color.to_win32_colorref());
    const std::wstring wtext = core::StringUtils::utf8_to_wide(text);
    DrawTextW(hdc, wtext.c_str(), static_cast<int>(wtext.size()), &rc, flags | DT_NOPREFIX);
    if (old_font) {
        SelectObject(hdc, old_font);
    }
}

void DrawProceduralIcon(HDC hdc, const RectPx& icon_rc, std::uint32_t accent_rgb, std::string_view cmd_id) {
    const RgbColor col{
        static_cast<std::uint8_t>((accent_rgb >> 16) & 0xFF),
        static_cast<std::uint8_t>((accent_rgb >> 8) & 0xFF),
        static_cast<std::uint8_t>(accent_rgb & 0xFF)
    };
    FillRectColor(hdc, icon_rc, col);
    DrawBorderRect(hdc, icon_rc, {255, 255, 255});

    // Glifo interior representativo según familia del comando
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    const int cx = icon_rc.x + icon_rc.width / 2;
    const int cy = icon_rc.y + icon_rc.height / 2;
    if (cmd_id.find("Nudo") != std::string_view::npos) {
        Ellipse(hdc, cx - 4, cy - 4, cx + 4, cy + 4);
    } else if (cmd_id.find("Barra") != std::string_view::npos || cmd_id.find("viga") != std::string_view::npos) {
        MoveToEx(hdc, icon_rc.x + 4, icon_rc.bottom() - 4, nullptr);
        LineTo(hdc, icon_rc.right() - 4, icon_rc.y + 4);
    } else if (cmd_id.find("Calculo") != std::string_view::npos || cmd_id.find("comprobar") != std::string_view::npos) {
        MoveToEx(hdc, icon_rc.x + 4, cy, nullptr);
        LineTo(hdc, cx - 1, icon_rc.bottom() - 4);
        LineTo(hdc, icon_rc.right() - 3, icon_rc.y + 4);
    } else {
        MoveToEx(hdc, icon_rc.x + 4, cy, nullptr);
        LineTo(hdc, icon_rc.right() - 4, cy);
        MoveToEx(hdc, cx, icon_rc.y + 4, nullptr);
        LineTo(hdc, cx, icon_rc.bottom() - 4);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void RefreshAllWindows(Win32GuiContext* ctx) {
    if (!ctx) return;
    if (ctx->hwnd_main) {
        const std::wstring wtitle = core::StringUtils::utf8_to_wide(ctx->app->window_title());
        SetWindowTextW(ctx->hwnd_main, wtitle.c_str());
        InvalidateRect(ctx->hwnd_main, nullptr, FALSE);
    }
    if (ctx->hwnd_area_dibujo) {
        const bool show_gl = ctx->app->obra().abierta() &&
                             ctx->app->active_modal_dialog() == ActiveModalDialog::None &&
                             !ctx->show_report_modal;
        ShowWindow(ctx->hwnd_area_dibujo, show_gl ? SW_SHOW : SW_HIDE);
        if (show_gl) {
            InvalidateRect(ctx->hwnd_area_dibujo, nullptr, FALSE);
        }
    }
}

LRESULT CALLBACK CypeAreaDibujoWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    auto* ctx = reinterpret_cast<Win32GuiContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            RenderOpenGLViewport(ctx, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            SetCapture(hwnd);
            const int mx = static_cast<short>(LOWORD(lparam));
            const int my = static_cast<short>(HIWORD(lparam));
            ctx->last_mouse_x = mx;
            ctx->last_mouse_y = my;
            if ((wparam & MK_SHIFT) != 0) {
                ctx->dragging_pan = true;
            } else {
                ctx->dragging_orbit = true;
            }
            if (ctx && ctx->app) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                if (const auto* av = ctx->app->mdi_manager().active_view()) {
                    ctx->app->picker().rebuild_index(ctx->app->obra(), ctx->app->build_category_filter());
                    ctx->selected_entity = ctx->app->picker().pick_entity(
                        ctx->app->obra(), mx, my, av->camera,
                        rc.right - rc.left, rc.bottom - rc.top,
                        14.0, ctx->app->build_category_filter());
                }
                RefreshAllWindows(ctx);
            }
            return 0;
        }
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            SetFocus(hwnd);
            SetCapture(hwnd);
            ctx->last_mouse_x = static_cast<short>(LOWORD(lparam));
            ctx->last_mouse_y = static_cast<short>(HIWORD(lparam));
            ctx->dragging_pan = true;
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            ctx->dragging_orbit = false;
            ctx->dragging_pan = false;
            ReleaseCapture();
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            if (ctx && ctx->app) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                if (auto* av = ctx->app->mdi_manager().active_view_mut()) {
                    av->camera.zoom_extents(ctx->app->obra().bounding_box(), rc.right - rc.left, rc.bottom - rc.top);
                }
                RefreshAllWindows(ctx);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!ctx || !ctx->app) return 0;
            const int mx = static_cast<short>(LOWORD(lparam));
            const int my = static_cast<short>(HIWORD(lparam));
            const int dx = mx - ctx->last_mouse_x;
            const int dy = my - ctx->last_mouse_y;
            ctx->last_mouse_x = mx;
            ctx->last_mouse_y = my;

            if (ctx->dragging_orbit || ctx->dragging_pan) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                if (auto* av = ctx->app->mdi_manager().active_view_mut()) {
                    if (ctx->dragging_orbit) {
                        av->camera.orbit(-dx * 0.008, dy * 0.008);
                    } else if (ctx->dragging_pan) {
                        av->camera.pan_pixels(dx, dy, rc.right - rc.left, rc.bottom - rc.top);
                    }
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (ctx && ctx->app) {
                const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
                if (auto* av = ctx->app->mdi_manager().active_view_mut()) {
                    av->camera.zoom_wheel(delta > 0 ? 1.18 : (1.0 / 1.18));
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

void PaintMainApplicationWindow(Win32GuiContext* ctx, HDC hdc, int win_w, int win_h) {
    if (!ctx || !ctx->app) return;
    ctx->hit_regions.clear();

    const auto& pal = ctx->app->palette().colors();
    const auto& lay = ctx->app->docking().layout();

    // 1. Barra de título principal y barra de herramientas de acceso rápido (QAT)
    FillRectColor(hdc, {0, 0, win_w, 26}, pal.app_barra_titulo);
    DrawTextUtf8(hdc, {14, 0, 560, 26}, ctx->app->window_title(), {255, 255, 255},
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);

    struct QuickBtn { const char* id; const char* label; int w; };
    const QuickBtn qbtns[] = {
        {"Archivo_Nuevo",               "Nuevo...",          68},
        {"QAT_Welcome",                 "Bienvenida",        80},
        {"QAT_DemoNave",                "Demo Nave 3D",      94},
        {"Obra_DatosGenerales",         "Datos Generales",  108},
        {"Acciones_EstadosLimite",      "Estados Límite",   100},
        {"Calculo_calcular",            "Calcular (F5)",     92},
        {"Calculo_comprueba_elementos", "Comprobar ELU",     98},
        {"Ayuda_MemoriaCalculo",        "Listados / HTML",  104},
        {"Archivo_Planos",              "Planos DXF",        82},
        {"exportar_bim",                "Exportar IFC4",     94},
        {"Exportacion_A_Tekla",         "Exportar Tekla",    98},
        {"QAT_ToggleTheme",             ctx->app->is_dark_mode() ? "Tema: Oscuro" : "Tema: Claro", 92}
    };
    int qx = std::max(440, win_w - 1140);
    for (const auto& qb : qbtns) {
        if (qx + qb.w > win_w - 6) break;
        RectPx br{qx, 3, qb.w, 20};
        const bool hov = br.contains(ctx->hover_mouse_x, ctx->hover_mouse_y);
        FillRectColor(hdc, br, hov ? RgbColor{56, 132, 208} : RgbColor{40, 96, 152});
        DrawBorderRect(hdc, br, {140, 180, 220});
        DrawTextUtf8(hdc, br, qb.label, {255, 255, 255}, DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
        ctx->hit_regions.push_back({br, qb.id, qb.label});
        qx += qb.w + 4;
    }

    // 2. Barra de pestañas del Ribbon (Estructura: 8 pestañas | Cimentación: 1 pestaña)
    FillRectColor(hdc, {0, 26, win_w, 25}, pal.ribbon_tab_inactiva_fondo);
    if (ctx->app->active_solapa() == model::SolapaActiva::Estructura) {
        const auto& tabs = ctx->app->ribbon().estructura_tabs();
        for (std::size_t i = 0; i < tabs.size(); ++i) {
            const bool active = (i == ctx->app->ribbon().active_estructura_tab_index());
            RectPx tr{static_cast<int>(10 + i * 96), 27, 92, 24};
            FillRectColor(hdc, tr, active ? pal.ribbon_tab_activa_fondo : pal.ribbon_tab_inactiva_fondo);
            DrawTextUtf8(hdc, tr, tabs[i].title,
                         active ? pal.ribbon_tab_activa_texto : pal.ribbon_tab_inactiva_texto,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE, active ? ctx->font_bold : ctx->font_ui);
            ctx->hit_regions.push_back({tr, "TAB_EST_" + std::to_string(i), tabs[i].title});
        }
    } else {
        RectPx tr{10, 27, 130, 24};
        FillRectColor(hdc, tr, pal.ribbon_tab_activa_fondo);
        DrawTextUtf8(hdc, tr, "Cimentación", pal.ribbon_tab_activa_texto,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);
    }

    const std::string unit_badge = "Unidades: " +
        std::string(units::UnitManager::system_name(ctx->app->obra().datos_generales().sistema_unidades)) +
        "  |  Herramienta: " + (ctx->app->active_receptor_id().empty() ? "Selección 3D" : ctx->app->active_receptor_id());
    DrawTextUtf8(hdc, {win_w - 460, 26, 448, 24}, unit_badge, {220, 235, 250},
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE, ctx->font_small);

    // 3. Banda de grupos y botones del Ribbon (lay.banda_ribbon)
    FillRectColor(hdc, {0, 51, win_w, 92}, pal.ribbon_banda_fondo);
    DrawBorderRect(hdc, {0, 51, win_w, 92}, pal.ribbon_grupo_borde);

    const auto& groups = (ctx->app->active_solapa() == model::SolapaActiva::Estructura)
                             ? ctx->app->ribbon().current_estructura_tab().groups
                             : ctx->app->ribbon().cimentacion_groups();

    int gx = 8;
    for (const auto& grp : groups) {
        if (gx >= win_w - 40) break;
        const int grp_start_x = gx;
        int bx = gx + 4;

        auto render_ribbon_cmd = [&](const RibbonCommandItem& item) {
            if (item.kind == RibbonButtonKind::Separador || item.kind == RibbonButtonKind::ExtensorDerecha) {
                return;
            }
            if (bx + 58 > win_w - 8) return;
            RectPx btn_rc{bx, 55, 56, 68};
            const bool is_active = (ctx->app->active_receptor_id() == item.id) || item.toggled;
            const bool is_hover = btn_rc.contains(ctx->hover_mouse_x, ctx->hover_mouse_y);

            if (is_active) {
                FillRectColor(hdc, btn_rc, RgbColor{194, 222, 250});
                DrawBorderRect(hdc, btn_rc, RgbColor{36, 114, 200});
            } else if (is_hover) {
                FillRectColor(hdc, btn_rc, RgbColor{218, 232, 248});
                DrawBorderRect(hdc, btn_rc, RgbColor{120, 165, 215});
            }

            const auto& icon_desc = ctx->app->palette().imagen_carga_recurso("", item.source_tag, item.icon_resource);
            DrawProceduralIcon(hdc, {bx + 17, 59, 22, 22}, icon_desc.accent_rgb, item.id);
            DrawTextUtf8(hdc, {bx + 2, 83, 52, 38}, item.label, pal.panel_texto_primario,
                         DT_CENTER | DT_TOP | DT_WORDBREAK, ctx->font_small);

            ctx->hit_regions.push_back({btn_rc, "CMD:" + item.id, item.label});
            bx += 58;
        };

        for (const auto& item : grp.items) {
            if ((item.kind == RibbonButtonKind::SubmenuDesplegable || item.kind == RibbonButtonKind::SubmenuModeless) &&
                !item.children.empty()) {
                // Mostrar los primeros subcomandos directamente para acceso con 1 clic
                const std::size_t max_sub = std::min<std::size_t>(item.children.size(), 3);
                for (std::size_t k = 0; k < max_sub; ++k) {
                    render_ribbon_cmd(item.children[k]);
                }
            } else {
                render_ribbon_cmd(item);
            }
        }

        const int grp_w = std::max(72, bx - grp_start_x + 4);
        RectPx grp_title_rc{grp_start_x, 125, grp_w, 16};
        FillRectColor(hdc, {grp_start_x + grp_w, 55, 1, 84}, pal.ribbon_grupo_borde);
        DrawTextUtf8(hdc, grp_title_rc, grp.title, pal.ribbon_grupo_titulo_texto,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
        gx = grp_start_x + grp_w + 6;
    }

    // 4. Barra Superior CAD (Capturas 3D + Modos de Renderizado y Consulta Gráfica de Resultados)
    FillRectColor(hdc, {0, 143, win_w, 37}, pal.panel_dock_fondo);
    DrawBorderRect(hdc, {0, 143, win_w, 37}, pal.panel_borde_control);

    auto& cap = ctx->app->docking().cad_captures(ctx->app->active_solapa());
    struct SnapPill { const char* id; const char* label; bool active; };
    const SnapPill snaps[] = {
        {"SNAP_EXT", "Snap Extremo", cap.extremo},
        {"SNAP_MED", "Punto Medio",  cap.punto_medio},
        {"SNAP_PER", "Perpend.",     cap.perpendicular},
        {"SNAP_INT", "Intersección", cap.interseccion},
        {"SNAP_REJ", "Rejilla",      cap.rejilla}
    };
    int sx = 10;
    for (const auto& sp : snaps) {
        RectPx pr{sx, 149, 86, 24};
        FillRectColor(hdc, pr, sp.active ? RgbColor{36, 114, 200} : pal.solapa_inferior_inactiva);
        DrawBorderRect(hdc, pr, pal.panel_borde_control);
        DrawTextUtf8(hdc, pr, sp.label, sp.active ? RgbColor{255, 255, 255} : pal.panel_texto_primario,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
        ctx->hit_regions.push_back({pr, sp.id, sp.label});
        sx += 90;
    }

    // Selector de modo de representación 3D y superposición de resultados
    struct ModePill { const char* id; const char* label; bool active; };
    const ModePill modes[] = {
        {"VIEW_WIRE",   "1: Alámbrico",     ctx->render_mode == cad::RenderRepresentationMode::AlambricoUnifilar},
        {"VIEW_HLR",    "2: Línea Oculta",  ctx->render_mode == cad::RenderRepresentationMode::LineaOcultaHlr},
        {"VIEW_SOLID",  "3: Perfil Real 3D",ctx->render_mode == cad::RenderRepresentationMode::PerfilRealExtruido3d},
        {"VIEW_DEF",    "Deformada 3D",     ctx->show_deformed_3d},
        {"VIEW_FORCES", "Esfuerzos My",     ctx->show_force_diagrams_3d},
        {"VIEW_UTIL",   "Aprovech. ELU %",  ctx->show_utilization_colors},
        {"VIEW_LOADS",  "Cargas 3D",        ctx->show_loads_3d},
        {"VIEW_CONN",   "Uniones/Placas 3D",ctx->show_connections_3d}
    };
    int mx_pill = std::max(sx + 16, win_w - 820);
    for (const auto& mp : modes) {
        if (mx_pill + 98 > win_w - 6) break;
        RectPx pr{mx_pill, 149, 96, 24};
        FillRectColor(hdc, pr, mp.active ? RgbColor{22, 130, 88} : pal.solapa_inferior_inactiva);
        DrawBorderRect(hdc, pr, pal.panel_borde_control);
        DrawTextUtf8(hdc, pr, mp.label, mp.active ? RgbColor{255, 255, 255} : pal.panel_texto_primario,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
        ctx->hit_regions.push_back({pr, mp.id, mp.label});
        mx_pill += 100;
    }

    // 5. Si no hay obra abierta, mostrar la Pantalla de Bienvenida (01_welcome_main_window.png)
    if (!ctx->app->obra().abierta()) {
        RectPx ws_rc{0, 180, win_w, lay.franja_selector_solapas.y - 180};
        FillRectColor(hdc, ws_rc, pal.bienvenida_fondo);

        // Tarjeta Archivo
        RectPx card_arch{60, 220, 420, 290};
        FillRectColor(hdc, card_arch, pal.bienvenida_tarjeta_fondo);
        FillRectColor(hdc, {60, 220, 420, 34}, pal.bienvenida_tarjeta_cabecera);
        DrawBorderRect(hdc, card_arch, pal.panel_borde_control);
        DrawTextUtf8(hdc, {76, 220, 380, 34}, "Archivo", {24, 26, 30}, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);

        int by = 272;
        for (const auto& btn : ctx->app->welcome_screen().botones_archivo()) {
            RectPx brc{84, by, 372, 54};
            FillRectColor(hdc, brc, {255, 255, 255});
            DrawBorderRect(hdc, brc, {180, 186, 195});
            DrawProceduralIcon(hdc, {96, by + 15, 24, 24}, 0x1C4E80, btn.id);
            DrawTextUtf8(hdc, {132, by, 310, 54}, btn.label + " — Crear o abrir proyecto .ed3", {24, 26, 30},
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);
            ctx->hit_regions.push_back({brc, btn.id == "archivo_nuevo" ? "Archivo_Nuevo" : "QAT_DemoNave", btn.label});
            by += 66;
        }

        // Tarjeta Últimos ficheros (MRU)
        RectPx card_mru{510, 220, 560, 290};
        FillRectColor(hdc, card_mru, pal.bienvenida_tarjeta_fondo);
        FillRectColor(hdc, {510, 220, 560, 34}, pal.bienvenida_tarjeta_cabecera);
        DrawBorderRect(hdc, card_mru, pal.panel_borde_control);
        DrawTextUtf8(hdc, {526, 220, 520, 34}, "Últimos ficheros (Clic para abrir obra 3D)", {24, 26, 30},
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);

        RectPx mru_btn{530, 272, 520, 64};
        FillRectColor(hdc, mru_btn, {235, 244, 254});
        DrawBorderRect(hdc, mru_btn, {36, 114, 200});
        DrawTextUtf8(hdc, {546, 278, 490, 24}, "NaveIndustrial2027.ed3", {20, 65, 130}, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);
        DrawTextUtf8(hdc, {546, 304, 490, 24}, "Nave Industrial de 2 Vanos con Puente Grúa, Uniones y Cimentación", {70, 80, 95}, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_ui);
        ctx->hit_regions.push_back({mru_btn, "QAT_DemoNave", "Abrir NaveIndustrial2027.ed3"});
    } else {
        // 6. Paneles laterales acoplables pndock (visibles en Solapa Estructura)
        if (lay.left_dock_visible) {
            FillRectColor(hdc, lay.sub_dock_izquierdo, pal.panel_dock_fondo);
            DrawBorderRect(hdc, lay.sub_dock_izquierdo, pal.panel_borde_control);

            // Cabecera: Etiquetas y Categorías
            FillRectColor(hdc, {0, lay.panel_etiquetas_categorias.y, 382, 26}, pal.panel_dock_barra_titulo);
            DrawTextUtf8(hdc, {10, lay.panel_etiquetas_categorias.y, 360, 26},
                         "Etiquetas y Categorías (ed3base_pncapas_visualizacion)",
                         pal.panel_texto_primario, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);

            int cy_cat = lay.panel_etiquetas_categorias.y + 34;
            DrawTextUtf8(hdc, {12, cy_cat, 350, 20}, "Categorías estructurales (Clic para mostrar/ocultar):",
                         pal.ribbon_grupo_titulo_texto, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
            cy_cat += 24;

            const auto& cats = ctx->app->docking().panel_capas().categorias;
            for (std::size_t i = 0; i < cats.size(); ++i) {
                RectPx chk_rc{16, cy_cat, 18, 18};
                FillRectColor(hdc, chk_rc, cats[i].visible ? RgbColor{36, 114, 200} : RgbColor{200, 205, 212});
                DrawBorderRect(hdc, chk_rc, pal.panel_borde_control);
                if (cats[i].visible) {
                    DrawTextUtf8(hdc, chk_rc, "✓", {255, 255, 255}, DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);
                }
                RectPx row_rc{16, cy_cat - 2, 340, 22};
                DrawTextUtf8(hdc, {42, cy_cat - 2, 310, 22}, cats[i].nombre, pal.panel_texto_primario,
                             DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_ui);
                ctx->hit_regions.push_back({row_rc, "CAT_VIS_" + std::to_string(i), cats[i].nombre});
                cy_cat += 25;
            }

            // Cabecera: Opciones de visualización e Inspector Estructural
            const int pref_y = cy_cat + 10;
            FillRectColor(hdc, {0, pref_y, 382, 26}, pal.panel_dock_barra_titulo);
            DrawTextUtf8(hdc, {10, pref_y, 360, 26},
                         "Opciones de visualización y Resumen de Cálculo",
                         pal.panel_texto_primario, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);

            int py_opt = pref_y + 32;
            const auto& pref = ctx->app->docking().panel_preferencias();
            struct PrefItem { const char* id; const char* label; bool val; };
            const PrefItem pitems[] = {
                {"PREF_EJES",   "Mostrar ejes locales de barras y láminas", pref.mostrar_ejes_locales},
                {"PREF_LAM",    "Dibujar láminas y paños con relleno",       pref.dibujar_laminas_con_relleno},
                {"PREF_CARGAS", "Dibujar diagramas con relleno translúcido", pref.dibujar_cargas_con_relleno}
            };
            for (const auto& pi : pitems) {
                RectPx chk{16, py_opt, 18, 18};
                FillRectColor(hdc, chk, pi.val ? RgbColor{36, 114, 200} : RgbColor{200, 205, 212});
                DrawBorderRect(hdc, chk, pal.panel_borde_control);
                if (pi.val) {
                    DrawTextUtf8(hdc, chk, "✓", {255, 255, 255}, DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);
                }
                RectPx row{16, py_opt - 2, 350, 22};
                DrawTextUtf8(hdc, {42, py_opt - 2, 325, 22}, pi.label, pal.panel_texto_primario,
                             DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_ui);
                ctx->hit_regions.push_back({row, pi.id, pi.label});
                py_opt += 25;
            }

            // Tarjeta de Resumen de Cálculo e Inspector de Entidad Seleccionada
            py_opt += 8;
            RectPx sum_card{10, py_opt, 362, std::max(120, lay.sub_dock_izquierdo.bottom() - py_opt - 8)};
            FillRectColor(hdc, sum_card, ctx->app->is_dark_mode() ? RgbColor{30, 41, 59} : RgbColor{255, 255, 255});
            DrawBorderRect(hdc, sum_card, pal.panel_borde_control);

            const auto& ob = ctx->app->obra();
            const auto& ss = ctx->app->last_solver_summary();
            const auto& ds = ctx->app->last_design_summary();
            const auto& fm = ctx->app->last_foundation_model();

            int sy = py_opt + 8;
            auto draw_kv = [&](std::string_view k, std::string_view v, bool bold_val = false) {
                if (sy + 18 > sum_card.bottom()) return;
                DrawTextUtf8(hdc, {20, sy, 195, 18}, k, pal.ribbon_grupo_titulo_texto, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
                DrawTextUtf8(hdc, {215, sy, 148, 18}, v, pal.panel_texto_primario, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, bold_val ? ctx->font_bold : ctx->font_small);
                sy += 19;
            };

            draw_kv("Obra activa (.ed3):", ob.datos_generales().clave, true);
            draw_kv("Nudos / Barras / Láminas:",
                    std::to_string(ob.estructura().nudos.size()) + " / " +
                    std::to_string(ob.estructura().barras.size()) + " / " +
                    std::to_string(ob.estructura().laminas.size()));
            draw_kv("GDL / Desplazamiento máx:",
                    std::to_string(ss.dof_count) + " GDL / " +
                    std::to_string(core::round_decimals(ss.max_displacement_m * 1000.0, 2)) + " mm");
            draw_kv("Comprobación ELU/ELS:",
                    std::to_string(ds.barras_que_cumplen) + "/" +
                    std::to_string(ds.comprobaciones_barras.size()) + " cumplen", true);
            draw_kv("Aprovechamiento ELU máx:",
                    std::to_string(core::round_decimals(ds.aprovechamiento_maximo_obra_pct, 1)) + " %", true);
            draw_kv("Uniones / Placas anclaje:",
                    std::to_string(ctx->app->last_connections().size()) + " / " +
                    std::to_string(ctx->app->last_baseplates().size()));
            draw_kv("Zapatas / Vigas de atado:",
                    std::to_string(fm.zapatas.size()) + " / " + std::to_string(fm.vigas.size()));

            if (ctx->selected_entity.has_value()) {
                sy += 6;
                DrawTextUtf8(hdc, {20, sy, 340, 18}, "--- Elemento Seleccionado en Vista 3D ---",
                             RgbColor{36, 114, 200}, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);
                sy += 20;
                if (ctx->selected_entity->kind == cad::EntityKind::Barra) {
                    if (const auto* b = ob.find_barra(ctx->selected_entity->entity_id)) {
                        draw_kv("Barra ID / Perfil:", "B" + std::to_string(b->id) + " (" + b->perfil + ")", true);
                        draw_kv("Material / Longitud:", b->material + " / " + std::to_string(core::round_decimals(ob.longitud_barra_m(b->id), 2)) + " m");
                        for (const auto& cb : ds.comprobaciones_barras) {
                            if (cb.barra_id == b->id) {
                                draw_kv("Aprovechamiento ELU:",
                                        std::to_string(core::round_decimals(cb.aprovechamiento_elu_max_pct, 1)) + " % (" +
                                        (cb.cumple_global ? "CUMPLE" : "NO CUMPLE") + ")", true);
                                break;
                            }
                        }
                    }
                } else if (ctx->selected_entity->kind == cad::EntityKind::Nudo) {
                    if (const auto* n = ob.find_nudo(ctx->selected_entity->entity_id)) {
                        draw_kv("Nudo ID:", "N" + std::to_string(n->id), true);
                        draw_kv("Coordenadas (X,Y,Z):",
                                "(" + std::to_string(core::round_decimals(n->pos.x, 1)) + ", " +
                                std::to_string(core::round_decimals(n->pos.y, 1)) + ", " +
                                std::to_string(core::round_decimals(n->pos.z, 1)) + ") m");
                    }
                }
            }
        }

        // 7. Barra de herramientas vertical 3D izquierda del área de dibujo (lay.barra_vertical_vista_3d)
        const int vx = lay.cype_mdi_frame.x;
        const int vy = lay.cype_area_dibujo.y;
        FillRectColor(hdc, {vx, vy, 24, lay.cype_area_dibujo.height}, pal.panel_dock_barra_titulo);
        struct VTool { const char* id; const char* label; };
        const VTool vtools[] = {
            {"CAM_ISO", "3D"},
            {"CAM_XY",  "XY"},
            {"CAM_XZ",  "XZ"},
            {"CAM_YZ",  "YZ"},
            {"CAM_EXT", "FIT"},
            {"CAM_ZIN", "+"},
            {"CAM_ZOUT","-"},
            {"CAM_PERSP","P"}
        };
        for (int i = 0; i < 8; ++i) {
            RectPx vrc{vx + 1, vy + 4 + i * 28, 22, 24};
            FillRectColor(hdc, vrc, pal.ribbon_banda_fondo);
            DrawBorderRect(hdc, vrc, pal.panel_borde_control);
            DrawTextUtf8(hdc, vrc, vtools[i].label, pal.panel_texto_primario,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
            ctx->hit_regions.push_back({vrc, vtools[i].id, vtools[i].label});
        }

        // Cabecera del marco MDI sobre el viewport OpenGL
        FillRectColor(hdc, {lay.cype_mdi_frame.x, lay.cype_mdi_frame.y, lay.cype_mdi_frame.width, 20}, pal.panel_dock_barra_titulo);
        const std::string mdi_caption = (ctx->app->active_solapa() == model::SolapaActiva::Estructura)
            ? "Vista 3D Interactiva — Estructura [METAL3D_ESTRUCTURA] (Arrastrar: Orbitar | Botón Der./Shift: Encuadrar | Rueda: Zoom | Clic: Seleccionar)"
            : "Vista 3D Interactiva — Cimentación [ID_CIMENTACION] (Zapatas H.A., Vigas de Atado y Armaduras 3D)";
        DrawTextUtf8(hdc, {lay.cype_mdi_frame.x + 8, lay.cype_mdi_frame.y, lay.cype_mdi_frame.width - 16, 20},
                     mdi_caption, pal.panel_texto_primario, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
    }

    // 8. Diálogo Modal Activo (Nueva Obra / Asistente / Datos Generales / Estados Límite / Visor de Listados y BIM)
    if (ctx->app->active_modal_dialog() != ActiveModalDialog::None || ctx->show_report_modal) {
        const int mw = 740;
        const int mh = 480;
        const int mx = std::max(40, (win_w - mw) / 2);
        const int my = std::max(60, (win_h - mh) / 2);
        RectPx mrc{mx, my, mw, mh};
        FillRectColor(hdc, mrc, {245, 246, 248});
        FillRectColor(hdc, {mx, my, mw, 30}, pal.dialogo_barra_titulo_roja);
        DrawBorderRect(hdc, mrc, {90, 20, 20});

        std::string title = ctx->report_modal_title;
        if (ctx->app->active_modal_dialog() == ActiveModalDialog::NuevaObraClave) {
            title = "Nueva obra (ed3obra) — Clave y descripción del proyecto";
        } else if (ctx->app->active_modal_dialog() == ActiveModalDialog::AsistenteNuevaObra) {
            title = "Asistente de Nueva Obra — Selección de generador (Obra vacía / IFC / DXF / Generador de Pórticos)";
        } else if (ctx->app->active_modal_dialog() == ActiveModalDialog::DatosGenerales) {
            title = "Datos generales de la obra — Normativas, Materiales, Acciones y Terreno";
        } else if (ctx->app->active_modal_dialog() == ActiveModalDialog::EstadosLimite) {
            title = "Estados límite (sestlim.dll / combs.dll) — Combinaciones E.L.U. y E.L.S.";
        }
        DrawTextUtf8(hdc, {mx + 12, my, mw - 24, 30}, title, {255, 255, 255},
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);

        RectPx body_rc{mx + 16, my + 42, mw - 32, mh - 96};
        FillRectColor(hdc, body_rc, {255, 255, 255});
        DrawBorderRect(hdc, body_rc, {190, 196, 205});

        std::string content_text = ctx->report_modal_body;
        if (ctx->app->active_modal_dialog() == ActiveModalDialog::NuevaObraClave) {
            content_text =
                "Fichero de obra (.ed3): " + ctx->app->dlg_nueva_obra().full_ed3_path().string() + "\r\n\r\n"
                "Descripción: " + ctx->app->dlg_nueva_obra().descripcion() + "\r\n\r\n"
                "Pulse [Aceptar / Siguiente] para pasar al Asistente de creación (Generador de Pórticos / Obra vacía).";
        } else if (ctx->app->active_modal_dialog() == ActiveModalDialog::AsistenteNuevaObra) {
            content_text =
                "Modo seleccionado: Generador de Pórticos integrado (ed3genport) / Nave Industrial 3D\r\n\r\n"
                "• Generación automática de pórticos biempotrados, correas ZF, cargas de viento CTE/EC1 y nieve.\r\n"
                "• Previsualización vectorial 3D activa (" +
                std::to_string(ctx->app->wizard_nueva_obra().preview_wireframe_3d().size()) + " segmentos).\r\n\r\n"
                "Pulse [Aceptar / Siguiente] para confirmar y revisar los Datos Generales.";
        } else if (ctx->app->active_modal_dialog() == ActiveModalDialog::DatosGenerales ||
                   ctx->app->active_modal_dialog() == ActiveModalDialog::EstadosLimite) {
            const auto& dg = ctx->app->obra().datos_generales();
            content_text =
                "Normativa Hormigón:          " + dg.norma_hormigon + " (" + dg.hormigon_estructural + ")\r\n"
                "Normativa Acero Laminado:    " + dg.norma_acero_laminado + " (" + dg.acero_laminado + ")\r\n"
                "Normativa Acero Conformado:  " + dg.norma_acero_conformado + " (" + dg.acero_conformado + ")\r\n"
                "Normativa Madera / Aluminio: " + dg.norma_madera + " / " + dg.norma_aluminio + "\r\n"
                "Tensión admisible terreno:   " + std::to_string(core::round_decimals(dg.tension_terreno_persistente_mpa, 2)) + " MPa\r\n"
                "Análisis Sísmico Dinámico:   " + std::string(dg.con_sismo_dinamico ? "Activado (Modal Espectral CQC)" : "No") + "\r\n\r\n"
                "Pulse [Aceptar] para aplicar a la obra y abrir el entorno 3D.";
        }

        DrawTextUtf8(hdc, {body_rc.x + 12, body_rc.y + 10, body_rc.width - 24, body_rc.height - 20},
                     content_text, {24, 28, 34}, DT_LEFT | DT_TOP | DT_WORDBREAK, ctx->font_mono);

        RectPx btn_ok{mx + mw - 230, my + mh - 42, 104, 30};
        RectPx btn_cancel{mx + mw - 116, my + mh - 42, 100, 30};
        FillRectColor(hdc, btn_ok, RgbColor{28, 104, 180});
        DrawBorderRect(hdc, btn_ok, RgbColor{18, 70, 130});
        DrawTextUtf8(hdc, btn_ok, "Aceptar", {255, 255, 255}, DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_bold);

        FillRectColor(hdc, btn_cancel, RgbColor{220, 224, 230});
        DrawBorderRect(hdc, btn_cancel, RgbColor{150, 156, 165});
        DrawTextUtf8(hdc, btn_cancel, "Cerrar", {30, 34, 40}, DT_CENTER | DT_VCENTER | DT_SINGLELINE, ctx->font_ui);

        ctx->hit_regions.push_back({btn_ok, "MODAL_OK", "Aceptar"});
        ctx->hit_regions.push_back({btn_cancel, "MODAL_CANCEL", "Cerrar"});
    }

    // 9. Selector inferior de solapas (Estructura / Cimentación) y Barra de Estado
    FillRectColor(hdc, {0, lay.franja_selector_solapas.y, win_w, 20}, pal.solapa_inferior_inactiva);
    const bool is_est = (ctx->app->active_solapa() == model::SolapaActiva::Estructura);
    FillRectColor(hdc, lay.tab_solapa_estructura, is_est ? pal.solapa_inferior_activa : pal.solapa_inferior_inactiva);
    DrawBorderRect(hdc, lay.tab_solapa_estructura, pal.panel_borde_control);
    DrawTextUtf8(hdc, lay.tab_solapa_estructura, "Estructura", pal.panel_texto_primario,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE, is_est ? ctx->font_bold : ctx->font_ui);

    FillRectColor(hdc, lay.tab_solapa_cimentacion, !is_est ? pal.solapa_inferior_activa : pal.solapa_inferior_inactiva);
    DrawBorderRect(hdc, lay.tab_solapa_cimentacion, pal.panel_borde_control);
    DrawTextUtf8(hdc, lay.tab_solapa_cimentacion, "Cimentación", pal.panel_texto_primario,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE, !is_est ? ctx->font_bold : ctx->font_ui);

    FillRectColor(hdc, {0, lay.barra_estado.y, win_w, lay.barra_estado.height}, pal.barra_estado_fondo);
    DrawTextUtf8(hdc, {8, lay.barra_estado.y, win_w - 16, lay.barra_estado.height},
                 ctx->app->status_bar_text(), pal.panel_texto_primario,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE, ctx->font_small);
}

void HandleHitAction(Win32GuiContext* ctx, const std::string& action) {
    if (!ctx || !ctx->app) return;
    auto& app = *ctx->app;

    if (action == "MODAL_CANCEL") {
        ctx->show_report_modal = false;
        app.cancel_modal_dialog();
        RefreshAllWindows(ctx);
        return;
    }
    if (action == "MODAL_OK") {
        if (ctx->show_report_modal) {
            ctx->show_report_modal = false;
        } else if (app.active_modal_dialog() == ActiveModalDialog::NuevaObraClave) {
            app.confirm_dialogo_nueva_obra();
        } else if (app.active_modal_dialog() == ActiveModalDialog::AsistenteNuevaObra) {
            app.wizard_nueva_obra().set_modo_seleccionado(ModoCreacionNuevaObra::GeneradorPorticos);
            app.confirm_asistente_nueva_obra();
        } else if (app.active_modal_dialog() == ActiveModalDialog::DatosGenerales ||
                   app.active_modal_dialog() == ActiveModalDialog::EstadosLimite) {
            app.confirm_dialogo_datos_generales();
            app.execute_command("Calculo_calcular");
            app.execute_command("Calculo_comprueba_elementos");
            if (auto* av = app.mdi_manager().active_view_mut()) {
                av->camera.zoom_extents(app.obra().bounding_box(), av->rect.width, av->rect.height);
            }
        }
        RefreshAllWindows(ctx);
        return;
    }

    if (action == "QAT_Welcome") {
        ctx->show_report_modal = false;
        app.close_active_obra();
        RefreshAllWindows(ctx);
        return;
    }
    if (action == "QAT_DemoNave") {
        ctx->show_report_modal = false;
        app.load_default_demo_project();
        RefreshAllWindows(ctx);
        return;
    }
    if (action == "QAT_ToggleTheme") {
        app.set_dark_mode(!app.is_dark_mode());
        RefreshAllWindows(ctx);
        return;
    }
    if (action.rfind("TAB_EST_", 0) == 0) {
        const std::size_t idx = static_cast<std::size_t>(std::stoul(action.substr(8)));
        app.ribbon().set_active_estructura_tab_index(idx);
        RefreshAllWindows(ctx);
        return;
    }
    if (action.rfind("CAT_VIS_", 0) == 0) {
        const std::size_t idx = static_cast<std::size_t>(std::stoul(action.substr(8)));
        const auto& cats = app.docking().panel_capas().categorias;
        if (idx < cats.size()) {
            app.docking().set_categoria_visible(cats[idx].categoria, !cats[idx].visible);
        }
        RefreshAllWindows(ctx);
        return;
    }
    if (action == "PREF_EJES") {
        auto& p = app.docking().panel_preferencias();
        p.mostrar_ejes_locales = !p.mostrar_ejes_locales;
        RefreshAllWindows(ctx);
        return;
    }
    if (action == "PREF_LAM") {
        auto& p = app.docking().panel_preferencias();
        p.dibujar_laminas_con_relleno = !p.dibujar_laminas_con_relleno;
        RefreshAllWindows(ctx);
        return;
    }
    if (action == "PREF_CARGAS") {
        auto& p = app.docking().panel_preferencias();
        p.dibujar_cargas_con_relleno = !p.dibujar_cargas_con_relleno;
        RefreshAllWindows(ctx);
        return;
    }

    // Barra de capturas Snap
    auto& cap = app.docking().cad_captures_mut(app.active_solapa());
    if (action == "SNAP_EXT") { cap.extremo = !cap.extremo; app.sync_cad_configs_from_docking(); RefreshAllWindows(ctx); return; }
    if (action == "SNAP_MED") { cap.punto_medio = !cap.punto_medio; app.sync_cad_configs_from_docking(); RefreshAllWindows(ctx); return; }
    if (action == "SNAP_PER") { cap.perpendicular = !cap.perpendicular; app.sync_cad_configs_from_docking(); RefreshAllWindows(ctx); return; }
    if (action == "SNAP_INT") { cap.interseccion = !cap.interseccion; app.sync_cad_configs_from_docking(); RefreshAllWindows(ctx); return; }
    if (action == "SNAP_REJ") { cap.rejilla = !cap.rejilla; app.sync_cad_configs_from_docking(); RefreshAllWindows(ctx); return; }

    // Modos de vista 3D y resultados
    if (action == "VIEW_WIRE")   { ctx->render_mode = cad::RenderRepresentationMode::AlambricoUnifilar; RefreshAllWindows(ctx); return; }
    if (action == "VIEW_HLR")    { ctx->render_mode = cad::RenderRepresentationMode::LineaOcultaHlr; RefreshAllWindows(ctx); return; }
    if (action == "VIEW_SOLID")  { ctx->render_mode = cad::RenderRepresentationMode::PerfilRealExtruido3d; RefreshAllWindows(ctx); return; }
    if (action == "VIEW_DEF")    {
        if (app.last_deformed_scene().barras_deformadas.empty()) app.execute_command("Calculo_Desplazamientos");
        ctx->show_deformed_3d = !ctx->show_deformed_3d;
        RefreshAllWindows(ctx);
        return;
    }
    if (action == "VIEW_FORCES") {
        if (app.last_bar_diagrams().empty()) app.execute_command("Calculo_esfuerzos");
        ctx->show_force_diagrams_3d = !ctx->show_force_diagrams_3d;
        RefreshAllWindows(ctx);
        return;
    }
    if (action == "VIEW_UTIL")   { ctx->show_utilization_colors = !ctx->show_utilization_colors; RefreshAllWindows(ctx); return; }
    if (action == "VIEW_LOADS")  { ctx->show_loads_3d = !ctx->show_loads_3d; RefreshAllWindows(ctx); return; }
    if (action == "VIEW_CONN")   { ctx->show_connections_3d = !ctx->show_connections_3d; RefreshAllWindows(ctx); return; }

    // Botones verticales de cámara 3D
    if (auto* av = app.mdi_manager().active_view_mut()) {
        if (action == "CAM_ISO")  { av->camera.set_preset_view("Isometric"); av->camera.zoom_extents(app.obra().bounding_box(), av->rect.width, av->rect.height); RefreshAllWindows(ctx); return; }
        if (action == "CAM_XY")   { av->camera.set_preset_view("XY"); av->camera.zoom_extents(app.obra().bounding_box(), av->rect.width, av->rect.height); RefreshAllWindows(ctx); return; }
        if (action == "CAM_XZ")   { av->camera.set_preset_view("XZ"); av->camera.zoom_extents(app.obra().bounding_box(), av->rect.width, av->rect.height); RefreshAllWindows(ctx); return; }
        if (action == "CAM_YZ")   { av->camera.set_preset_view("YZ"); av->camera.zoom_extents(app.obra().bounding_box(), av->rect.width, av->rect.height); RefreshAllWindows(ctx); return; }
        if (action == "CAM_EXT")  { av->camera.zoom_extents(app.obra().bounding_box(), av->rect.width, av->rect.height); RefreshAllWindows(ctx); return; }
        if (action == "CAM_ZIN")  { av->camera.zoom_wheel(1.25); RefreshAllWindows(ctx); return; }
        if (action == "CAM_ZOUT") { av->camera.zoom_wheel(0.80); RefreshAllWindows(ctx); return; }
        if (action == "CAM_PERSP") {
            av->camera.set_projection_mode(
                av->camera.projection_mode() == cad::ProjectionMode::Perspective
                    ? cad::ProjectionMode::Isometric
                    : cad::ProjectionMode::Perspective);
            RefreshAllWindows(ctx);
            return;
        }
    }

    // Comandos del Ribbon o barra superior
    const std::string cmd_id = (action.rfind("CMD:", 0) == 0) ? action.substr(4) : action;
    app.execute_command(cmd_id);

    // Si es un comando de visualización de deformada o esfuerzos, activar su capa visual 3D
    if (cmd_id == "Calculo_Desplazamientos" || cmd_id == "PO_Desplazamientos" ||
        cmd_id == "Calculo_ModosVibracion" || cmd_id == "Calculo_ModosPandeo" ||
        cmd_id == "Calculo_esfuerzos3D") {
        ctx->show_deformed_3d = true;
    } else if (cmd_id == "Calculo_esfuerzos" || cmd_id == "PO_Esfuerzos") {
        ctx->show_force_diagrams_3d = true;
    } else if (cmd_id == "Ayuda_MemoriaCalculo" || cmd_id == "Archivo_Listados") {
        ctx->show_report_modal = true;
        ctx->report_modal_title = "Listado Completo y Memoria de Cálculo — " + app.obra().datos_generales().clave;
        ctx->report_modal_body = io::ReportsDrawingsBimEngine::export_report(
            app.last_report_document(), io::FormatoExportacionListado::TXT);
    } else if (cmd_id == "Archivo_Planos") {
        ctx->show_report_modal = true;
        ctx->report_modal_title = "Plano Estructural 2D/3D Exportado a DXF (enplanos.dll / undxf.dll)";
        ctx->report_modal_body = app.last_exported_payload().substr(0, std::min<std::size_t>(1800, app.last_exported_payload().size()));
    } else if (cmd_id == "exportar_bim" || cmd_id == "vincular_bim" ||
               cmd_id == "Exportacion_A_Tekla" || cmd_id == "Exportacion_A_CIS2" || cmd_id == "Exportacion_A_CSV") {
        ctx->show_report_modal = true;
        ctx->report_modal_title = "Exportación BIM / Fabricación Generada (" + cmd_id + ")";
        ctx->report_modal_body = app.last_exported_payload().substr(0, std::min<std::size_t>(1800, app.last_exported_payload().size()));
    }

    RefreshAllWindows(ctx);
}

LRESULT CALLBACK CypeAplicacionWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    auto* ctx = reinterpret_cast<Win32GuiContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE: {
            if (ctx && ctx->app) {
                const int w = LOWORD(lparam);
                const int h = HIWORD(lparam);
                if (w > 0 && h > 0) {
                    ctx->app->docking().recalculate_layout(w, h, ctx->app->active_solapa());
                    if (ctx->hwnd_area_dibujo) {
                        const auto& r = ctx->app->docking().layout().cype_area_dibujo;
                        MoveWindow(ctx->hwnd_area_dibujo, r.x, r.y, r.width, r.height, TRUE);
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (ctx) {
                const int px = static_cast<short>(LOWORD(lparam));
                const int py = static_cast<short>(HIWORD(lparam));
                if (py < 180) {
                    ctx->hover_mouse_x = px;
                    ctx->hover_mouse_y = py;
                    RECT top_rc{0, 0, 4000, 180};
                    InvalidateRect(hwnd, &top_rc, FALSE);
                } else if (ctx->hover_mouse_y >= 0 && ctx->hover_mouse_y < 180) {
                    ctx->hover_mouse_x = -1;
                    ctx->hover_mouse_y = -1;
                    RECT top_rc{0, 0, 4000, 180};
                    InvalidateRect(hwnd, &top_rc, FALSE);
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (ctx && ctx->app) {
                const int px = static_cast<short>(LOWORD(lparam));
                const int py = static_cast<short>(HIWORD(lparam));
                const auto& lay = ctx->app->docking().layout();

                // 1. Verificar regiones interactivas registradas (botones QAT, pestañas, Ribbon, snaps, modos, categorías, modal)
                for (auto it = ctx->hit_regions.rbegin(); it != ctx->hit_regions.rend(); ++it) {
                    if (it->rect.contains(px, py)) {
                        const std::string act = it->action_id;
                        HandleHitAction(ctx, act);
                        return 0;
                    }
                }

                // 2. Selector inferior de solapas (Estructura / Cimentación)
                if (lay.tab_solapa_estructura.contains(px, py)) {
                    ctx->app->switch_solapa(model::SolapaActiva::Estructura);
                    const auto& r = ctx->app->docking().layout().cype_area_dibujo;
                    if (ctx->hwnd_area_dibujo) MoveWindow(ctx->hwnd_area_dibujo, r.x, r.y, r.width, r.height, TRUE);
                    RefreshAllWindows(ctx);
                } else if (lay.tab_solapa_cimentacion.contains(px, py)) {
                    ctx->app->switch_solapa(model::SolapaActiva::Cimentacion);
                    const auto& r = ctx->app->docking().layout().cype_area_dibujo;
                    if (ctx->hwnd_area_dibujo) MoveWindow(ctx->hwnd_area_dibujo, r.x, r.y, r.width, r.height, TRUE);
                    RefreshAllWindows(ctx);
                }
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (ctx && ctx->app) {
                if (wparam == VK_F5) {
                    HandleHitAction(ctx, "Calculo_calcular");
                } else if (wparam == VK_ESCAPE) {
                    ctx->show_report_modal = false;
                    ctx->app->cancel_modal_dialog();
                    RefreshAllWindows(ctx);
                }
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc_screen = BeginPaint(hwnd, &ps);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int win_w = std::max(1, static_cast<int>(rc.right - rc.left));
            const int win_h = std::max(1, static_cast<int>(rc.bottom - rc.top));

            // Doble buffer GDI libre de parpadeos
            HDC mem_dc = CreateCompatibleDC(hdc_screen);
            HBITMAP mem_bmp = CreateCompatibleBitmap(hdc_screen, win_w, win_h);
            HGDIOBJ old_bmp = SelectObject(mem_dc, mem_bmp);

            PaintMainApplicationWindow(ctx, mem_dc, win_w, win_h);

            BitBlt(hdc_screen, 0, 0, win_w, win_h, mem_dc, 0, 0, SRCCOPY);
            SelectObject(mem_dc, old_bmp);
            DeleteObject(mem_bmp);
            DeleteDC(mem_dc);

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
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
        wc.lpfnWndProc = (std::wstring_view(cls_name) == L"CypeAreaDibujo")
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
    // Si no se ha cargado ninguna estructura aún, inicializar la nave industrial 3D demostrativa completa
    if (obra_.estructura().barras.empty()) {
        load_default_demo_project();
    }

    HINSTANCE hinst = GetModuleHandleW(nullptr);
    RegisterCypeWindowClasses(hinst);

    Win32GuiContext ctx{};
    ctx.app = this;
    ctx.font_ui    = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    ctx.font_bold  = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    ctx.font_small = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    ctx.font_mono  = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    const std::wstring wtitle = core::StringUtils::utf8_to_wide(window_title());
    HWND hwnd = CreateWindowExW(
        0,
        L"CypeAplicacion",
        wtitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1600, 940,
        nullptr, nullptr, hinst, &ctx
    );
    if (!hwnd) {
        return 1;
    }
    ctx.hwnd_main = hwnd;

    RECT client_rc{};
    GetClientRect(hwnd, &client_rc);
    docking_.recalculate_layout(
        std::max(800, static_cast<int>(client_rc.right - client_rc.left)),
        std::max(600, static_cast<int>(client_rc.bottom - client_rc.top)),
        obra_.solapa_activa());

    const auto& area_rc = docking_.layout().cype_area_dibujo;
    HWND hwnd_gl = CreateWindowExW(
        0,
        L"CypeAreaDibujo",
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        area_rc.x, area_rc.y, area_rc.width, area_rc.height,
        hwnd, nullptr, hinst, &ctx
    );
    ctx.hwnd_area_dibujo = hwnd_gl;

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

    if (auto* av = mdi_manager_.active_view_mut()) {
        av->camera.zoom_extents(obra_.bounding_box(), area_rc.width, area_rc.height);
    }

    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);
    if (hwnd_gl) {
        InvalidateRect(hwnd_gl, nullptr, FALSE);
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
    if (ctx.font_ui) DeleteObject(ctx.font_ui);
    if (ctx.font_bold) DeleteObject(ctx.font_bold);
    if (ctx.font_small) DeleteObject(ctx.font_small);
    if (ctx.font_mono) DeleteObject(ctx.font_mono);
    return static_cast<int>(msg.wParam);
#else
    return run_headless_self_check();
#endif
}

} // namespace cypeclon::gui
