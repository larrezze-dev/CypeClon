/**
 * @file test_connections_phase11.cpp
 * @brief Suite de pruebas unitarias para la Fase 11 (P-12.1 / T-017 / A-030):
 *        Placas de Anclaje (`placancl.dll`, `pneleme.dll`) y Uniones Soldadas y Atornilladas
 *        (`uniones.dll`, `unionbase.dll`, `unionbase2.dll`, `bimjsunion.dll`).
 */

#include <cmath>
#include <cstdlib>
#include <iostream>

#include "connections/connections_engine.hpp"
#include "gui/app_window.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

namespace {

void check(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

void check_near(double actual, double expected, double tol, const char* expr, const char* file, int line) {
    if (std::abs(actual - expected) > tol) {
        std::cerr << "[FAIL] " << file << ":" << line << " -> " << expr
                  << " (actual=" << actual << ", expected=" << expected << ", tol=" << tol << ")\n";
        std::exit(1);
    }
}

#define TEST_ASSERT(expr) check((expr), #expr, __FILE__, __LINE__)
#define TEST_NEAR(a, b, tol) check_near((a), (b), (tol), #a " ~= " #b, __FILE__, __LINE__)

void test_p12_1_bolt_catalog_and_baseplates() {
    using namespace cypeclon::connections;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    // 1. Catálogo de tornillería métrica (`unionbase2.dll`: `ub2eltos_*`)
    const auto& cat = ConnectionsEngine::catalogo_tornillos_metricos();
    TEST_ASSERT(cat.size() == 8);
    const auto m20 = ConnectionsEngine::obtener_datos_tornillo(20.0);
    TEST_ASSERT(m20.designacion == "M20");
    TEST_NEAR(m20.area_resistente_traccion_mm2, 245.0, 1.0);
    TEST_NEAR(m20.par_apriete_8_8_nm, 450.0, 1.0);

    double fyb = 0.0, fub = 0.0;
    ConnectionsEngine::propiedades_mecanicas_clase_tornillo(ClaseAceroTornillo::Clase_10_9, fyb, fub);
    TEST_NEAR(fyb, 900.0, 1.0);
    TEST_NEAR(fub, 1000.0, 1.0);

    // 2. Generación, comprobación, dimensionamiento e igualación de Placas de Anclaje (`placancl.dll`)
    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({8.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n3 = obra.add_nudo({0.0, 0.0, 5.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({8.0, 0.0, 5.0}, VinculacionExterior::Libre);

    obra.add_barra(n1, n3, "HEB 240", "S275");
    obra.add_barra(n2, n4, "HEB 240", "S275");
    const auto b_dintel = obra.add_barra(n3, n4, "IPE 300", "S275");
    obra.add_carga_lineal_barra(b_dintel, 1, 30.0, 30.0, {0.0, 0.0, -1.0}, false);
    obra.add_carga_puntual_nudo(n3, 1, {40.0, 0.0, 0.0});

    const auto solver_res = SolverEngine::analyze_linear_static(obra);
    auto placas = ConnectionsEngine::generar_todas_las_placas_anclaje(obra);
    TEST_ASSERT(placas.size() == 2);
    TEST_ASSERT(placas[0].pernos.size() == 4);
    TEST_ASSERT(!placas[0].elementos_3d.empty());

    const auto res_dim = ConnectionsEngine::dimensionar_placa_anclaje(obra, placas[0], solver_res);
    TEST_ASSERT(res_dim.cumple_global);
    TEST_ASSERT(res_dim.peso_total_conjunto_kg > 10.0);
    TEST_ASSERT(!res_dim.texto_cuadro_arranques.empty());
    TEST_ASSERT(!res_dim.memoria_calculo_detallada.empty());

    // Igualar placa 0 sobre el resto
    ConnectionsEngine::igualar_placas_anclaje(placas[0], placas, {n2});
    TEST_NEAR(placas[1].ancho_lx_mm, placas[0].ancho_lx_mm, 1e-3);
    TEST_NEAR(placas[1].espesor_e_mm, placas[0].espesor_e_mm, 1e-3);
}

void test_p12_1_welded_bolted_connections_and_rotational_stiffness() {
    using namespace cypeclon::connections;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;
    using namespace cypeclon::gui;

    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({12.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n3 = obra.add_nudo({0.0, 0.0, 6.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({6.0, 0.0, 7.5}, VinculacionExterior::Libre);
    const auto n5 = obra.add_nudo({12.0, 0.0, 6.0}, VinculacionExterior::Libre);

    obra.add_barra(n1, n3, "HEB 240", "S275");
    obra.add_barra(n2, n5, "HEB 240", "S275");
    const auto b_din1 = obra.add_barra(n3, n4, "IPE 300", "S275");
    const auto b_din2 = obra.add_barra(n4, n5, "IPE 300", "S275");
    obra.add_carga_lineal_barra(b_din1, 1, 18.0, 18.0, {0.0, 0.0, -1.0}, false);
    obra.add_carga_lineal_barra(b_din2, 1, 18.0, 18.0, {0.0, 0.0, -1.0}, false);

    const auto solver_res = SolverEngine::analyze_linear_static(obra);
    auto uniones = ConnectionsEngine::detectar_y_generar_uniones_obra(
        obra, solver_res, true, ClaseAceroTornillo::Clase_8_8, false);

    // 5 nudos: 2 apoyos (placas de anclaje), 2 aleros pilar-dintel (n3 y n5) y 1 cumbrera (n4)
    TEST_ASSERT(uniones.size() == 5);

    bool vio_pilar_viga = false;
    bool vio_cumbrera = false;
    for (const auto& u : uniones) {
        TEST_ASSERT(u.cumple_global);
        TEST_ASSERT(!u.rigideces_rotacionales.empty());
        TEST_ASSERT(u.rigideces_rotacionales.front().rigidez_inicial_sj_ini_knm_rad > 100.0);
        TEST_ASSERT(u.rigideces_rotacionales.front().curva_rotacion_mrad_momento_knm.size() == 4);
        TEST_ASSERT(!u.prismas_3d.empty());
        TEST_ASSERT(!u.memoria_calculo_union.empty());
        if (u.tipologia == TipologiaUnionEstructural::PilarVigaChapaFrontalAtornillada) {
            vio_pilar_viga = true;
        }
        if (u.tipologia == TipologiaUnionEstructural::CumbreraNaveChapaFrontalAtornillada) {
            vio_cumbrera = true;
        }
    }
    TEST_ASSERT(vio_pilar_viga);
    TEST_ASSERT(vio_cumbrera);

    // Aplicar rigideces propuestas a la obra y exportar JSON BIM (`bimjsunion.dll`)
    const auto n_rig = ConnectionsEngine::aplicar_rigideces_rotacionales_propuestas_a_obra(obra, uniones);
    TEST_ASSERT(n_rig >= 3);
    TEST_ASSERT(obra.find_barra(b_din1)->empotramiento.k_rot_xz_ini_knm_rad > 100.0 ||
                obra.find_barra(b_din1)->empotramiento.k_rot_xz_fin_knm_rad > 100.0);

    const auto placas = ConnectionsEngine::generar_todas_las_placas_anclaje(obra);
    const std::string json_bim = ConnectionsEngine::exportar_bimjsunion_json(obra, uniones, placas);
    TEST_ASSERT(json_bim.find("cype_bimjsunion_v1") != std::string::npos);

    // Verificar despacho desde AppWindow
    AppWindow app;
    app.obra_mut() = obra;
    TEST_ASSERT(app.execute_command("Placas_Anclaje_dimensionar"));
    TEST_ASSERT(app.last_baseplates().size() == 2);
    TEST_ASSERT(app.execute_command("ued_generar"));
    TEST_ASSERT(app.last_connections().size() == 5);
    TEST_ASSERT(app.execute_command("ued_analisis_rigidez_rotacional"));
}

} // namespace

void run_connections_phase11_tests() {
    test_p12_1_bolt_catalog_and_baseplates();
    test_p12_1_welded_bolted_connections_and_rotational_stiffness();
}
