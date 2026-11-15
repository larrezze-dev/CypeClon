#include "cad/results_visualization_engine.hpp"
#include "codes/design_check_engine.hpp"
#include "gui/app_window.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>

namespace {

void check(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define TEST_ASSERT(expr) check((expr), #expr, __FILE__, __LINE__)
#define TEST_NEAR(a, b, tol) check(std::abs((a) - (b)) <= (tol), #a " ~= " #b, __FILE__, __LINE__)

void test_p11_1_color_scales_and_isovalue_legend() {
    using namespace cypeclon::cad;

    const auto ley_jet = ResultsVisualizationEngine::construir_leyenda_isovalores(
        -50.0, 150.0, "Tensión de Von Mises", "MPa", TipoEscalaMapaColor::AzulARojo_Jet, 10);
    TEST_ASSERT(ley_jet.bandas.size() == 10);
    TEST_ASSERT(ley_jet.niveles_isolineas.size() == 9);
    TEST_NEAR(ley_jet.incremento_isolineas, 20.0, 1.0e-6);
    TEST_ASSERT(ley_jet.evaluar_color_rgb(-50.0) == 0x0000FF); // Azul en el mínimo
    TEST_ASSERT(ley_jet.evaluar_color_rgb(150.0) == 0xFF0000); // Rojo en el máximo
    TEST_ASSERT(ley_jet.generar_texto_leyenda().find("LEYENDA DE ISOVALORES") != std::string::npos);

    const auto ley_gris = ResultsVisualizationEngine::construir_leyenda_isovalores(
        -20.0, 80.0, "Esfuerzo Nx", "kN/m", TipoEscalaMapaColor::ValoresNegativosEnGris, 8);
    TEST_ASSERT(ley_gris.evaluar_color_rgb(-5.0) == 0x808080); // Negativos en gris

    const auto ley_blanco = ResultsVisualizationEngine::construir_leyenda_isovalores(
        0.0, 100.0, "Desplazamiento", "mm", TipoEscalaMapaColor::BlancoValorMaximo, 8);
    TEST_ASSERT(ley_blanco.evaluar_color_rgb(99.5) == 0xFFFFFF); // Blanco en el valor máximo
}

void test_p11_1_deformed_shape_3d_and_harmonic_animation() {
    using namespace cypeclon::cad;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({0.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n3 = obra.add_nudo({6.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({6.0, 0.0, 0.0}, VinculacionExterior::Empotrado);

    (void)obra.add_barra(n1, n2, "HEB 200", "S275");
    const auto b_viga = obra.add_barra(n2, n3, "IPE 240", "S275");
    (void)obra.add_barra(n4, n3, "HEB 200", "S275");
    obra.add_carga_lineal_barra(b_viga, 1, 16.0, 16.0, {0.0, 0.0, -1.0});
    obra.add_carga_puntual_nudo(n2, 1, {15.0, 0.0, 0.0});

    auto solver_res = SolverEngine::analyze_linear_static(obra);
    solver_res.analisis_modal = SolverEngine::analyze_modal_spectral(obra, 3);
    solver_res.analisis_pandeo_global = SolverEngine::analyze_global_buckling(obra, {{1, 1.0}});

    // 1. Deformada estática 3D
    const auto def_est = ResultsVisualizationEngine::generar_deformada_3d(
        obra, solver_res, ModoOrigenDeformada::HipotesisSimple, 1,
        ComponenteColorDeformada::DesplazamientoTotal_U, 50.0, 12);
    TEST_ASSERT(def_est.barras_deformadas.size() == 3);
    TEST_ASSERT(def_est.barras_deformadas[1].puntos.size() == 13);
    TEST_ASSERT(def_est.desplazamiento_maximo_mm > 0.1);
    TEST_ASSERT(def_est.nudo_desplazamiento_maximo_id != 0);

    // 2. Fotogramas de animación armónica (morphing en fase 0, pi/2 y 3*pi/2)
    const auto frame_zero = def_est.generar_fotograma_animacion(0.0);
    const auto frame_pos = def_est.generar_fotograma_animacion(0.5 * std::numbers::pi);
    const auto frame_neg = def_est.generar_fotograma_animacion(1.5 * std::numbers::pi);
    const auto& pt_mid_0 = frame_zero.barras_deformadas[1].puntos[6];
    const auto& pt_mid_pos = frame_pos.barras_deformadas[1].puntos[6];
    const auto& pt_mid_neg = frame_neg.barras_deformadas[1].puntos[6];
    TEST_NEAR(pt_mid_0.pos_deformada_escalada_m.distance_to(pt_mid_0.pos_indeformada_m), 0.0, 1.0e-6);
    TEST_ASSERT(pt_mid_pos.pos_deformada_escalada_m.z < pt_mid_0.pos_indeformada_m.z);
    TEST_ASSERT(pt_mid_neg.pos_deformada_escalada_m.z > pt_mid_0.pos_indeformada_m.z);

    // 3. Deformada de modo de vibración sísmico y modo de pandeo global
    const auto def_modal = ResultsVisualizationEngine::generar_deformada_3d(
        obra, solver_res, ModoOrigenDeformada::ModoVibracionSismico, 1);
    TEST_ASSERT(def_modal.titulo_caso.find("Modo de vibración") != std::string::npos);

    const auto def_pandeo = ResultsVisualizationEngine::generar_deformada_3d(
        obra, solver_res, ModoOrigenDeformada::ModoPandeoGlobal, 1);
    TEST_ASSERT(def_pandeo.titulo_caso.find("Modo de pandeo") != std::string::npos);
}

void test_p11_1_bar_force_diagrams_envelopes_and_point_cursor() {
    using namespace cypeclon::cad;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({6.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto b1 = obra.add_barra(n1, n2, "IPE 240", "S275");
    obra.add_carga_lineal_barra(b1, 1, 20.0, 20.0, {0.0, 0.0, -1.0});

    const auto solver_res = SolverEngine::analyze_linear_static(obra, 11);

    // 1. Diagramas 3D de momento flector My (envolvente y caso simple)
    const auto diags_env = ResultsVisualizationEngine::generar_diagramas_esfuerzos_barras(
        obra, solver_res, ComponenteEsfuerzoBarra::Flector_My, true);
    TEST_ASSERT(diags_env.size() == 1);
    TEST_ASSERT(diags_env.front().es_envolvente);
    TEST_ASSERT(diags_env.front().curva_principal_o_max.size() == 11);
    TEST_ASSERT(diags_env.front().curva_envolvente_min.size() == 11);
    TEST_ASSERT(diags_env.front().valor_maximo > 10.0);
    TEST_ASSERT(diags_env.front().valor_minimo < -10.0);

    // 2. Cursor de lectura puntual sobre la barra en x = 0, x = 3.0 m (centro) y x = 6.0 m
    const auto cur_ini = ResultsVisualizationEngine::consultar_cursor_barra_en_abscisa(
        obra, solver_res, b1, 0.0, 1, false);
    const auto cur_mid = ResultsVisualizationEngine::consultar_cursor_barra_en_abscisa(
        obra, solver_res, b1, 3.0, 1, false);
    TEST_NEAR(cur_mid.coordenada_3d_m.x, 3.0, 1.0e-5);
    // En viga biempotrada con q = 20 kN/m y L = 6 m: V(0) ~= +60 kN, V(3) ~= 0 kN
    TEST_NEAR(std::abs(cur_ini.tz_kn), 60.0, 5.0);
    TEST_NEAR(cur_mid.tz_kn, 0.0, 1.0e-2);
    TEST_ASSERT(cur_mid.desp_global_mm.z < -0.5);
}

void test_p11_1_shell_isovalues_and_isoline_contouring() {
    using namespace cypeclon::cad;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    Obra obra;
    obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    obra.add_nudo({4.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    obra.add_nudo({4.0, 4.0, 0.0}, VinculacionExterior::Empotrado);
    obra.add_nudo({0.0, 4.0, 0.0}, VinculacionExterior::Empotrado);
    const auto l1 = obra.add_lamina(
        {{0.0, 0.0, 0.0}, {4.0, 0.0, 0.0}, {4.0, 4.0, 0.0}, {0.0, 4.0, 0.0}},
        0.20, "C25/30"
    );
    for (std::uint32_t ar = 0; ar < 4; ++ar) {
        VinculacionAristaLamina va{};
        va.arista_index = ar;
        va.vinc_exterior = VinculacionExterior::Empotrado;
        obra.set_vinculacion_arista_lamina(l1, va);
    }
    obra.discretizar_todas_las_laminas(1.0);

    CargaLamina3d cl{};
    cl.lamina_id = l1;
    cl.hipotesis_id = 1;
    cl.tipo = TipoCargaLamina::SuperficialUniforme;
    cl.valor_kn_m2_o_kn = 15.0;
    cl.direccion = {0.0, 0.0, -1.0};
    obra.add_carga_lamina(cl);

    const auto solver_res = SolverEngine::analyze_linear_static(obra);
    TEST_ASSERT(solver_res.active_shell_triangle_count > 0);

    // Mapa de isovalores de tensión de Von Mises con suavizado nodal e isolíneas
    const auto mapa_vm = ResultsVisualizationEngine::generar_mapa_isovalores_laminas(
        obra, solver_res, MagnitudIsovalorLamina::Tension_VonMises_Max, 1, false, true);
    TEST_ASSERT(!mapa_vm.triangulos.empty());
    TEST_ASSERT(mapa_vm.valor_maximo_global > 0.0);
    TEST_ASSERT(!mapa_vm.leyenda.bandas.empty());

    // Mapa de cuantías de armado Wood-Armer sin alisar
    const auto mapa_wa = ResultsVisualizationEngine::generar_mapa_isovalores_laminas(
        obra, solver_res, MagnitudIsovalorLamina::Cuantia_WoodArmer_InfX, 1, false, false);
    TEST_ASSERT(mapa_wa.triangulos.size() == mapa_vm.triangulos.size());
    TEST_ASSERT(mapa_wa.valor_maximo_global > 0.0);
}

void test_p11_1_support_reactions_utilization_colors_and_app_window() {
    using namespace cypeclon::cad;
    using namespace cypeclon::codes;
    using namespace cypeclon::gui;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    AppWindow app;
    auto& obra = app.obra_mut();
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({0.0, 0.0, 3.5}, VinculacionExterior::Libre);
    const auto n3 = obra.add_nudo({5.0, 0.0, 3.5}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({5.0, 0.0, 0.0}, VinculacionExterior::Empotrado);

    (void)obra.add_barra(n1, n2, "HEB 220", "S275");
    const auto b_dintel = obra.add_barra(n2, n3, "IPE 100", "S275"); // sobrecargado (> 100%)
    (void)obra.add_barra(n4, n3, "HEB 220", "S275");
    obra.add_carga_lineal_barra(b_dintel, 1, 22.0, 22.0, {0.0, 0.0, -1.0});

    TEST_ASSERT(app.execute_command("Calculo_calcular"));
    TEST_ASSERT(app.execute_command("Calculo_Desplazamientos"));
    TEST_ASSERT(app.last_deformed_scene().barras_deformadas.size() == 3);
    TEST_ASSERT(app.last_deformed_scene().desplazamiento_maximo_mm > 0.0);

    TEST_ASSERT(app.execute_command("Calculo_Reacciones"));
    TEST_ASSERT(app.last_support_reactions().size() == 2);
    TEST_ASSERT(app.last_support_reactions().front().resultante_fuerza_kn > 20.0);

    TEST_ASSERT(app.execute_command("Calculo_esfuerzos"));
    TEST_ASSERT(app.last_bar_diagrams().size() == 3);

    TEST_ASSERT(app.execute_command("Calculo_esfuerzos3D"));
    TEST_ASSERT(app.execute_command("Calculo_ModosVibracion"));
    TEST_ASSERT(app.execute_command("Calculo_ModosPandeo"));

    TEST_ASSERT(app.execute_command("Calculo_comprueba_elementos"));
    TEST_ASSERT(app.last_utilization_colors().size() == 3);
    bool hay_rojo = false;
    bool hay_verde = false;
    for (const auto& col : app.last_utilization_colors()) {
        if (col.rango == RangoColorAprovechamiento::Superado_Mayor100Pct) {
            hay_rojo = true;
        }
        if (col.rango == RangoColorAprovechamiento::Holgado_Menor50Pct) {
            hay_verde = true;
        }
    }
    TEST_ASSERT(hay_rojo && hay_verde);
}

} // namespace

void run_results_phase10_tests() {
    test_p11_1_color_scales_and_isovalue_legend();
    test_p11_1_deformed_shape_3d_and_harmonic_animation();
    test_p11_1_bar_force_diagrams_envelopes_and_point_cursor();
    test_p11_1_shell_isovalues_and_isoline_contouring();
    test_p11_1_support_reactions_utilization_colors_and_app_window();
}
