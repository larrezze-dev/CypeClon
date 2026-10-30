#include "gui/app_window.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void check(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define TEST_ASSERT(expr) check((expr), #expr, __FILE__, __LINE__)

} // namespace

void run_gui_phase2_tests() {
    using namespace cypeclon;
    using namespace cypeclon::gui;

    AppWindow app;

    // =========================================================================
    // 1. P-03.1: Paleta, Temas Claro/Oscuro, Estilo Ribbon, Docking y Solapas
    // =========================================================================
    TEST_ASSERT(!app.is_dark_mode());
    TEST_ASSERT(app.palette().ribbon_style() == RibbonStyle::RibbonAzul);
    TEST_ASSERT(app.palette().color_barra_titulo_dialogo() == (RgbColor{168, 32, 26}));
    TEST_ASSERT(app.palette().colors().bienvenida_fondo == (RgbColor{115, 115, 115}));
    TEST_ASSERT(app.palette().cached_icon_count() >= 90);
    TEST_ASSERT(app.palette().has_cached_icon("datobra2"));
    TEST_ASSERT(app.palette().has_cached_icon("nudo_new2"));
    TEST_ASSERT(app.palette().has_cached_icon("nuevocim"));

    const RgbColor light_canvas = app.palette().color_fondo_vista_segun_tema();
    app.set_dark_mode(true);
    TEST_ASSERT(app.is_dark_mode());
    const RgbColor dark_canvas = app.palette().color_fondo_vista_segun_tema();
    TEST_ASSERT(!(light_canvas == dark_canvas));
    app.set_dark_mode(false);

    // Verificar layout inicial en Solapa Estructura (1920x1032)
    TEST_ASSERT(app.active_solapa() == model::SolapaActiva::Estructura);
    TEST_ASSERT(app.active_solapa_internal_id() == "METAL3D_ESTRUCTURA");
    const auto& lay_est = app.docking().layout();
    TEST_ASSERT(lay_est.left_dock_visible);
    TEST_ASSERT(lay_est.sub_dock_izquierdo.width == 382);
    TEST_ASSERT(lay_est.cype_mdi_frame.x == 390 && lay_est.cype_mdi_frame.y == 180);
    TEST_ASSERT(lay_est.cype_mdi_frame.width == 1530 && lay_est.cype_mdi_frame.height == 814);
    TEST_ASSERT(lay_est.cype_area_dibujo.x == 414 && lay_est.cype_area_dibujo.y == 200);
    TEST_ASSERT(lay_est.cype_area_dibujo.width == 1506 && lay_est.cype_area_dibujo.height == 794);
    TEST_ASSERT(app.docking().viewport_toolbar().size() == 10);

    // Verificar las 7 Categorías estructurales y Opciones de visualización en pndock
    TEST_ASSERT(app.docking().panel_capas().categorias.size() == 7);
    TEST_ASSERT(app.docking().panel_capas().categorias[0].nombre == "Nudos");
    TEST_ASSERT(app.docking().panel_capas().categorias[6].nombre == "Láminas");
    TEST_ASSERT(app.docking().set_categoria_visible(CategoriaEstructural::Panyos, false));
    TEST_ASSERT(!app.docking().find_categoria(CategoriaEstructural::Panyos)->visible);
    TEST_ASSERT(app.docking().set_categoria_bloqueada(CategoriaEstructural::Pilares, true));
    TEST_ASSERT(app.docking().find_categoria(CategoriaEstructural::Pilares)->bloqueada);

    const auto& pref = app.docking().panel_preferencias();
    TEST_ASSERT(std::abs(pref.dimensiones_nudos_cm - 15.0) < 1e-9);
    TEST_ASSERT(std::abs(pref.tamano_textos_pct - 100.0) < 1e-9);
    TEST_ASSERT(pref.posicion_tooltips == "En la esquina superior derecha");

    // Conmutar a Solapa Cimentación ("ID_CIMENTACION") y verificar expansión a ancho completo
    app.switch_solapa(model::SolapaActiva::Cimentacion);
    TEST_ASSERT(app.active_solapa() == model::SolapaActiva::Cimentacion);
    TEST_ASSERT(app.active_solapa_internal_id() == "ID_CIMENTACION");
    const auto& lay_cim = app.docking().layout();
    TEST_ASSERT(!lay_cim.left_dock_visible);
    TEST_ASSERT(lay_cim.cype_area_dibujo.x == 0 && lay_cim.cype_area_dibujo.y == 142);
    TEST_ASSERT(lay_cim.cype_area_dibujo.width == 1920 && lay_cim.cype_area_dibujo.height == 852);

    // Volver a Solapa Estructura
    app.switch_solapa(model::SolapaActiva::Estructura);
    TEST_ASSERT(app.docking().layout().left_dock_visible);

    // =========================================================================
    // 2. P-03.2: Pantalla de Bienvenida, Asistente de Nueva Obra y Datos Generales
    // =========================================================================
    app.close_active_obra();
    TEST_ASSERT(!app.obra().abierta());
    TEST_ASSERT(app.status_bar_text() == WelcomeScreenModel::kNoWorkStatusMessage);
    TEST_ASSERT(app.welcome_screen().botones_archivo().size() == 3);
    TEST_ASSERT(app.welcome_screen().botones_archivo()[0].label == "Nuevo...");
    TEST_ASSERT(app.welcome_screen().enlaces_ayuda().size() == 11);
    TEST_ASSERT(app.welcome_screen().boton_gestion_licencias().rect.width == 136);

    // Secuencia de diálogos modales:
    // Paso 1: Diálogo Nueva obra (537x285)
    TEST_ASSERT(app.execute_command("Archivo_Nuevo"));
    TEST_ASSERT(app.active_modal_dialog() == ActiveModalDialog::NuevaObraClave);
    TEST_ASSERT(app.dlg_nueva_obra().dialog_rect().width == 537 && app.dlg_nueva_obra().dialog_rect().height == 285);
    app.dlg_nueva_obra().set_nombre_fichero("NavePortico2027.ed3");
    TEST_ASSERT(app.dlg_nueva_obra().nombre_fichero() == "NavePortico2027");
    app.dlg_nueva_obra().set_descripcion("Nave industrial generada con asistente");
    TEST_ASSERT(app.confirm_dialogo_nueva_obra());

    // Paso 2: Asistente Nueva obra (682x457) con previsualización vectorial 3D y Generador de Pórticos
    TEST_ASSERT(app.active_modal_dialog() == ActiveModalDialog::AsistenteNuevaObra);
    TEST_ASSERT(app.wizard_nueva_obra().dialog_rect().width == 682 && app.wizard_nueva_obra().dialog_rect().height == 457);
    TEST_ASSERT(app.wizard_nueva_obra().preview_canvas_rect().width == 672 && app.wizard_nueva_obra().preview_canvas_rect().height == 320);
    TEST_ASSERT(!app.wizard_nueva_obra().preview_wireframe_3d().empty());
    app.wizard_nueva_obra().set_modo_seleccionado(ModoCreacionNuevaObra::GeneradorPorticos);
    TEST_ASSERT(app.confirm_asistente_nueva_obra());
    TEST_ASSERT(app.obra().estructura().nudos.size() == 5);
    TEST_ASSERT(app.obra().estructura().barras.size() == 4);

    // Paso 3: Diálogo Obra vacía / Datos generales (848x774) y subdiálogo Estados límite (358x438)
    TEST_ASSERT(app.active_modal_dialog() == ActiveModalDialog::DatosGenerales);
    TEST_ASSERT(app.dlg_datos_generales().dialog_rect().width == 848 && app.dlg_datos_generales().dialog_rect().height == 774);
    TEST_ASSERT(app.dlg_datos_generales().modo_asistente_inicial());

    app.dlg_datos_generales().draft_mut().acero_laminado = "S355 (UNE-EN 10025-2)";
    app.dlg_datos_generales().draft_mut().hormigon_cimentacion = "C30/37";
    app.dlg_datos_generales().draft_mut().con_sismo_dinamico = true;
    app.dlg_datos_generales().draft_mut().tension_terreno_persistente_mpa = 0.250;
    app.dlg_datos_generales().set_pestana_terreno_activa(PestanaPilotesTerreno::Micropilotes);
    TEST_ASSERT(app.dlg_datos_generales().pestana_terreno_activa() == PestanaPilotesTerreno::Micropilotes);

    // Abrir subdiálogo Estados límite (05_dialog_step5.png)
    TEST_ASSERT(app.execute_command("Acciones_EstadosLimite"));
    TEST_ASSERT(app.active_modal_dialog() == ActiveModalDialog::EstadosLimite);
    TEST_ASSERT(app.dlg_datos_generales().dialogo_estados_limite().dialog_rect().width == 358);
    app.dlg_datos_generales().dialogo_estados_limite().config_mut().categoria_uso_nieve = "Altitud > 1000 m";

    // Confirmar Datos generales y verificar sincronización con model::Obra
    TEST_ASSERT(app.confirm_dialogo_datos_generales());
    TEST_ASSERT(app.active_modal_dialog() == ActiveModalDialog::None);
    TEST_ASSERT(app.obra().abierta());
    TEST_ASSERT(app.obra().datos_generales().clave == "NavePortico2027");
    TEST_ASSERT(app.obra().datos_generales().acero_laminado == "S355 (UNE-EN 10025-2)");
    TEST_ASSERT(app.obra().datos_generales().hormigon_cimentacion == "C30/37");
    TEST_ASSERT(app.obra().datos_generales().con_sismo_dinamico);
    TEST_ASSERT(std::abs(app.obra().datos_generales().tension_terreno_persistente_mpa - 0.250) < 1e-9);
    TEST_ASSERT(app.obra().datos_generales().estados_limite.categoria_uso_nieve == "Altitud > 1000 m");

    // =========================================================================
    // 3. P-03.3: Catálogo Completo de Ribbons, Menús y Comandos (Estructura y Cimentación)
    // =========================================================================
    const auto& est_tabs = app.ribbon().estructura_tabs();
    TEST_ASSERT(est_tabs.size() == 8);
    TEST_ASSERT(est_tabs[0].id == "obra"        && est_tabs[0].groups.size() == 5);
    TEST_ASSERT(est_tabs[1].id == "geometria"   && est_tabs[1].groups.size() == 6);
    TEST_ASSERT(est_tabs[2].id == "propiedades" && est_tabs[2].groups.size() == 4);
    TEST_ASSERT(est_tabs[3].id == "carga"       && est_tabs[3].groups.size() == 6);
    TEST_ASSERT(est_tabs[4].id == "uniones"     && est_tabs[4].groups.size() == 1);
    TEST_ASSERT(est_tabs[5].id == "calculo"     && est_tabs[5].groups.size() == 5);
    TEST_ASSERT(est_tabs[6].id == "armaduras"   && est_tabs[6].groups.size() == 2);
    TEST_ASSERT(est_tabs[7].id == "ventana"     && est_tabs[7].groups.size() == 1);

    // Conmutar modo de Uniones a clásico ("uniones_e3di" + "Placas_Anclaje") y volver a "uniones_ed"
    app.ribbon().set_uniones_mode(UnionesMenuMode::UnionesE3diClasico);
    TEST_ASSERT(app.ribbon().estructura_tabs()[4].groups.size() == 2);
    TEST_ASSERT(app.ribbon().estructura_tabs()[4].groups[0].id == "uniones_auto");
    TEST_ASSERT(app.ribbon().estructura_tabs()[4].groups[1].id == "Placas_Anclaje");
    app.ribbon().set_uniones_mode(UnionesMenuMode::UnionesEd);
    TEST_ASSERT(app.ribbon().estructura_tabs()[4].groups.size() == 1);

    // Verificar grupos y submenús de la Solapa Cimentación ("menu_unico")
    const auto& cim_groups = app.ribbon().cimentacion_groups();
    TEST_ASSERT(cim_groups.size() == 4);
    TEST_ASSERT(cim_groups[0].id == "menu_obra"        && cim_groups[0].title == "Obra");
    TEST_ASSERT(cim_groups[1].id == "menu_cimentacion" && cim_groups[1].title == "Elementos de cimentación");
    TEST_ASSERT(cim_groups[2].id == "limites"          && cim_groups[2].title == "Límites");
    TEST_ASSERT(cim_groups[3].id == "menu_calculo"     && cim_groups[3].title == "Cálculo");

    const auto* cmd_opciones_cim = app.ribbon().find_command("menu_opciones");
    TEST_ASSERT(cmd_opciones_cim != nullptr && cmd_opciones_cim->children.size() == 9);
    const auto* cmd_tablas_cim = app.ribbon().find_command("submenu_tablas_armado");
    TEST_ASSERT(cmd_tablas_cim != nullptr && cmd_tablas_cim->children.size() == 4);
    const auto* cmd_nuevo_cim = app.ribbon().find_command("nuevo_elemento");
    TEST_ASSERT(cmd_nuevo_cim != nullptr && cmd_nuevo_cim->children.size() == 7);
    const auto* cmd_rotar_cim = app.ribbon().find_command("rotar_elementos_aislados");
    TEST_ASSERT(cmd_rotar_cim != nullptr && cmd_rotar_cim->children.size() == 3);
    const auto* cmd_limite_cim = app.ribbon().find_command("nuevo_limite");
    TEST_ASSERT(cmd_limite_cim != nullptr && cmd_limite_cim->children.size() == 2);

    // Verificar barras de menús clásicas (12 menús en Estructura, 6 en Cimentación)
    TEST_ASSERT(app.ribbon().classic_menubar(model::SolapaActiva::Estructura).size() == 12);
    TEST_ASSERT(app.ribbon().classic_menubar(model::SolapaActiva::Cimentacion).size() == 6);

    // Probar ejecución de interruptores, receptores de herramienta y generación de zapatas
    const auto* cmd_div = app.ribbon().find_command("Barra_ConDivisionBarras");
    TEST_ASSERT(cmd_div != nullptr && cmd_div->toggled);
    TEST_ASSERT(app.execute_command("Barra_ConDivisionBarras"));
    TEST_ASSERT(!app.ribbon().find_command("Barra_ConDivisionBarras")->toggled);

    TEST_ASSERT(app.execute_command("Nudo_Nuevo"));
    TEST_ASSERT(app.active_receptor_id() == "Nudo_Nuevo");

    TEST_ASSERT(app.execute_command("nuevo_zapata_horm_arm"));
    TEST_ASSERT(app.active_receptor_id() == "nuevo_zapata_horm_arm");

    // En la obra creada por el Generador de Pórticos hay 2 apoyos empotrados (N1 y N5):
    // al ejecutar "generar_zapatas_y_vigas" deben generarse 2 zapatas automáticamente
    TEST_ASSERT(app.obra().cimentacion().elementos.empty());
    TEST_ASSERT(app.execute_command("generar_zapatas_y_vigas"));
    TEST_ASSERT(app.obra().cimentacion().elementos.size() == 2);
}

