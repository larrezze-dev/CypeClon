#include "foundation/foundation_engine.hpp"
#include "gui/app_window.hpp"
#include "solver/solver_engine.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void check_p12(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL Phase 12] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define TEST_P12_ASSERT(expr) check_p12((expr), #expr, __FILE__, __LINE__)

} // namespace

void run_foundation_phase12_tests() {
    using namespace cypeclon;
    using namespace cypeclon::foundation;

    // 1. Verificar catálogos normativos de zapatas, pilotes/micropilotes y vigas de cimentación
    const auto tabla_zp = FoundationEngine::build_footing_rebar_table();
    TEST_P12_ASSERT(tabla_zp.size() >= 15);
    TEST_P12_ASSERT(tabla_zp.front().cuantia_cm2_m < tabla_zp.back().cuantia_cm2_m);

    const auto cat_pilotes = FoundationEngine::build_pile_and_micropile_catalog();
    TEST_P12_ASSERT(cat_pilotes.size() >= 8);

    const auto cat_vigas = FoundationEngine::build_foundation_beam_catalog();
    TEST_P12_ASSERT(cat_vigas.size() >= 6);

    // 2. Crear pórtico 3D con 4 pilares empotrados y analizar con SolverEngine
    model::Obra obra;
    obra.datos_generales_mut().con_sismo_dinamico = true;
    obra.datos_generales_mut().tension_terreno_persistente_mpa = 0.200;
    obra.datos_generales_mut().tension_terreno_accidental_mpa = 0.300;

    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, model::VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({5.0, 0.0, 0.0}, model::VinculacionExterior::Empotrado);
    const auto n3 = obra.add_nudo({0.0, 4.0, 0.0}, model::VinculacionExterior::Empotrado);
    const auto n4 = obra.add_nudo({5.0, 4.0, 0.0}, model::VinculacionExterior::Empotrado);

    const auto n5 = obra.add_nudo({0.0, 0.0, 3.5}, model::VinculacionExterior::Libre);
    const auto n6 = obra.add_nudo({5.0, 0.0, 3.5}, model::VinculacionExterior::Libre);
    const auto n7 = obra.add_nudo({0.0, 4.0, 3.5}, model::VinculacionExterior::Libre);
    const auto n8 = obra.add_nudo({5.0, 4.0, 3.5}, model::VinculacionExterior::Libre);

    obra.add_barra(n1, n5, "HEB 240", "S275");
    obra.add_barra(n2, n6, "HEB 240", "S275");
    obra.add_barra(n3, n7, "HEB 240", "S275");
    obra.add_barra(n4, n8, "HEB 240", "S275");
    obra.add_barra(n5, n6, "IPE 300", "S275");
    obra.add_barra(n7, n8, "IPE 300", "S275");
    obra.add_barra(n5, n7, "IPE 240", "S275");
    obra.add_barra(n6, n8, "IPE 240", "S275");

    obra.add_carga_puntual_nudo(n5, 1, {15.0, 5.0, -320.0}, {10.0, 22.0, 0.0});
    obra.add_carga_puntual_nudo(n6, 1, {15.0, 5.0, -360.0}, {12.0, 25.0, 0.0});
    obra.add_carga_puntual_nudo(n7, 1, {10.0, 5.0, -280.0}, {8.0, 18.0, 0.0});
    obra.add_carga_puntual_nudo(n8, 1, {10.0, 5.0, -310.0}, {9.0, 20.0, 0.0});

    const auto solver_res = solver::SolverEngine::analyze_linear_static(obra);
    TEST_P12_ASSERT(solver_res.converged);

    // 3. Extraer arranques y comprobar contornos de punzonamiento y longitud de anclaje (`arranque.dll`)
    const auto arranques = FoundationEngine::extract_foundation_starters(obra, &solver_res);
    TEST_P12_ASSERT(arranques.size() == 4);
    TEST_P12_ASSERT(arranques[0].contorno_punzonamiento_2d_m.size() == 4);
    TEST_P12_ASSERT(arranques[0].longitud_anclaje_requerida_m >= 0.30);
    TEST_P12_ASSERT(!arranques[0].combinaciones.empty());

    // 4. Comprobar recorte poligonal con límites de cimentación (`planocim.dll`: `plclimites_recorta_contorno`)
    ZapataCimentacion zp_lim;
    zp_lim.id = 1;
    zp_lim.referencia = "Z_MED";
    zp_lim.centro_planta_m = {0.0, 0.0};
    zp_lim.ancho_x_m = 2.00;
    zp_lim.ancho_y_m = 2.00;
    const auto poly_orig = FoundationEngine::build_footing_rectangle_polygon(zp_lim);
    TEST_P12_ASSERT(std::abs(FoundationEngine::compute_polygon_area_m2(poly_orig) - 4.00) < 1.0e-6);

    model::LimiteCimentacion lim;
    lim.id = 1;
    lim.vertices = {{0.0, -5.0}, {10.0, -5.0}, {10.0, 5.0}, {0.0, 5.0}}; // Recorta semiespacio x >= 0
    const auto poly_clipped = FoundationEngine::clip_polygon_against_limit(poly_orig, lim);
    const double area_clipped = FoundationEngine::compute_polygon_area_m2(poly_clipped);
    TEST_P12_ASSERT(std::abs(area_clipped - 2.00) < 1.0e-4);

    // 5. Dimensionar zapata de Hormigón Armado y zapata de Hormigón en Masa (`zptipos.dll`)
    ZapataCimentacion zp_ha;
    zp_ha.id = 10;
    zp_ha.referencia = "Z_HA1";
    zp_ha.tipo_material = model::TipoElementoCimentacion::ZapataHormigonArmado;
    zp_ha.nudos_arranque_ids = {n1};
    zp_ha.centro_planta_m = {0.0, 0.0};
    zp_ha.ancho_x_m = 1.00;
    zp_ha.ancho_y_m = 1.00;
    zp_ha.canto_total_m = 0.40;
    const auto res_zp_ha = FoundationEngine::design_footing(zp_ha, arranques, obra.datos_generales());
    TEST_P12_ASSERT(res_zp_ha.cumple_global);
    TEST_P12_ASSERT(zp_ha.ancho_x_m >= 1.20);
    TEST_P12_ASSERT(zp_ha.parrilla_inferior.area_dispuesta_x_cm2_m >= zp_ha.parrilla_inferior.area_necesaria_x_cm2_m);
    TEST_P12_ASSERT(!res_zp_ha.listado_justificativo.empty());

    ZapataCimentacion zp_hm;
    zp_hm.id = 11;
    zp_hm.referencia = "Z_HM1";
    zp_hm.tipo_material = model::TipoElementoCimentacion::ZapataHormigonMasa;
    zp_hm.nudos_arranque_ids = {n3};
    zp_hm.centro_planta_m = {0.0, 4.0};
    zp_hm.ancho_x_m = 1.20;
    zp_hm.ancho_y_m = 1.20;
    zp_hm.canto_total_m = 0.50;
    const auto res_zp_hm = FoundationEngine::design_footing(zp_hm, arranques, obra.datos_generales());
    TEST_P12_ASSERT(res_zp_hm.cumple_global);
    TEST_P12_ASSERT(res_zp_hm.cumple_traccion_hormigon_masa);

    // 6. Dimensionar Encepados sobre Pilotes y Micropilotes (`encepado.dll`, `pilotes.dll`, `micropilotes.dll`)
    for (auto tipo_enc : {TipoEncepado::Encepado1Pilote, TipoEncepado::Encepado2Pilotes,
                          TipoEncepado::Encepado3Pilotes, TipoEncepado::Encepado4Pilotes}) {
        EncepadoCimentacion enc;
        enc.id = static_cast<std::uint32_t>(tipo_enc);
        enc.referencia = "ENC_" + std::to_string(enc.id);
        enc.nudo_arranque_id = n2;
        enc.tipo_encepado = tipo_enc;
        enc.pilote_catalogo = cat_pilotes[1];
        const auto res_enc = FoundationEngine::design_pile_cap(enc, arranques[1], obra.datos_generales());
        TEST_P12_ASSERT(res_enc.cumple_global);
        TEST_P12_ASSERT(enc.area_tirante_dispuesta_cm2 >= enc.area_tirante_necesaria_cm2);
        TEST_P12_ASSERT(!res_enc.listado_justificativo.empty());
    }

    // 7. Dimensionar Viga de Atado y Viga Centradora (`vigacim.dll`)
    VigaCimentacion viga_atado;
    viga_atado.id = 20;
    viga_atado.referencia = "VA_TEST";
    viga_atado.tipo = model::TipoElementoCimentacion::VigaAtado;
    viga_atado.nudo_ini_id = n1;
    viga_atado.nudo_fin_id = n2;
    viga_atado.longitud_eje_a_eje_m = 5.0;
    const auto res_va = FoundationEngine::design_foundation_beam(
        viga_atado, arranques[0], arranques[1], obra.datos_generales());
    TEST_P12_ASSERT(res_va.cumple_global);

    VigaCimentacion viga_cent;
    viga_cent.id = 21;
    viga_cent.referencia = "VC_TEST";
    viga_cent.tipo = model::TipoElementoCimentacion::VigaCentradora;
    viga_cent.nudo_ini_id = n1;
    viga_cent.nudo_fin_id = n2;
    viga_cent.centra_extremo_ini = true;
    viga_cent.longitud_eje_a_eje_m = 5.0;
    const auto res_vc = FoundationEngine::design_foundation_beam(
        viga_cent, arranques[0], arranques[1], obra.datos_generales(), {}, 0.45, 0.0);
    TEST_P12_ASSERT(res_vc.cumple_global);
    TEST_P12_ASSERT(viga_cent.rigidez_rotacional_equivalente_knm_rad > 1000.0);

    // 8. Generación automática, fusión de solapes, medición y escena 3D desde AppWindow
    gui::AppWindow app;
    app.obra_mut() = obra;
    app.switch_solapa(model::SolapaActiva::Cimentacion);
    TEST_P12_ASSERT(app.execute_command("generar_zapatas_y_vigas"));
    TEST_P12_ASSERT(app.last_foundation_model().zapatas.size() == 4);
    TEST_P12_ASSERT(!app.last_foundation_model().vigas.empty());

    TEST_P12_ASSERT(app.execute_command("dimensionar"));
    for (const auto& rz : app.last_foundation_model().resultados_zapatas) {
        TEST_P12_ASSERT(rz.cumple_global);
    }
    TEST_P12_ASSERT(app.last_foundation_model().medicion.volumen_hormigon_total_m3 > 1.0);
    TEST_P12_ASSERT(app.last_foundation_model().medicion.peso_acero_total_con_mermas_kg > 20.0);

    // Forzar solape entre las dos primeras zapatas y ejecutar "eliminar_solapes"
    app.last_foundation_model_mut().zapatas[1].centro_planta_m = {0.80, 0.0};
    TEST_P12_ASSERT(app.execute_command("eliminar_solapes"));
    TEST_P12_ASSERT(app.last_foundation_model().zapatas.size() == 3);
    TEST_P12_ASSERT(app.last_foundation_model().zapatas.front().subtipo == SubtipoZapata::CombinadaMultipleArranques);

    TEST_P12_ASSERT(app.execute_command("vista_3D"));
    const auto scene3d = FoundationEngine::build_3d_scene(app.last_foundation_model());
    TEST_P12_ASSERT(!scene3d.bloques_hormigon.empty());
    TEST_P12_ASSERT(!scene3d.barras_armadura_3d.empty());
}
