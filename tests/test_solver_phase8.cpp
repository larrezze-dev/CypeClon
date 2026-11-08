#include "gui/app_window.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

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
#define TEST_NEAR(a, b, tol) check(std::abs((a) - (b)) <= (tol), #a " ~= " #b, __FILE__, __LINE__)

void test_p09_1_linear_algebra_and_eigen_solver() {
    using namespace cypeclon::solver;

    // 1. Sistema lineal 3x3 simétrico definido positivo
    const std::vector<double> a = {
         4.0, -1.0,  0.0,
        -1.0,  4.0, -1.0,
         0.0, -1.0,  4.0
    };
    const std::vector<double> b = {2.0, 4.0, 2.0};
    std::vector<double> x;
    TEST_ASSERT(LinearAlgebraEngine::solucion_sistema_ecuaciones(a, b, 3, x));
    TEST_ASSERT(x.size() == 3);
    TEST_NEAR(x[0], 0.857142857, 1.0e-6);
    TEST_NEAR(x[1], 1.428571428, 1.0e-6);
    TEST_NEAR(x[2], 0.857142857, 1.0e-6);

    // 2. Resolución paralela OpenMP de múltiples columnas de carga
    std::vector<std::vector<double>> b_cols = {b, {6.0, 12.0, 6.0}};
    std::vector<std::vector<double>> x_cols;
    TEST_ASSERT(LinearAlgebraEngine::solucion_multiples_hipotesis(a, b_cols, 3, x_cols));
    TEST_ASSERT(x_cols.size() == 2);
    TEST_NEAR(x_cols[1][1], 3.0 * x[1], 1.0e-6);

    // 3. Problema generalizado de autovalores [K - lambda * M] * phi = 0 (2 GDL de cortante)
    const std::vector<double> k2 = {
         200.0, -100.0,
        -100.0,  100.0
    };
    const std::vector<double> m2 = {2.0, 1.0};
    const auto eig = LinearAlgebraEngine::calcular_autovalores_y_autovectores_generalizado(k2, m2, 2, 2);
    TEST_ASSERT(eig.converged);
    TEST_ASSERT(eig.eigenvalues.size() == 2);
    // Autovalores exactos de det([200 - 2*lambda, -100; -100, 100 - lambda]) = 2*lambda^2 - 400*lambda + 10000 = 0
    // lambda = 100 +/- 50*sqrt(2) => lambda_1 = 29.28932, lambda_2 = 170.71068
    TEST_NEAR(eig.eigenvalues[0], 29.28932188, 1.0e-4);
    TEST_NEAR(eig.eigenvalues[1], 170.7106781, 1.0e-4);
}

void test_p09_1_timoshenko_cantilever_and_pinned_beam() {
    using namespace cypeclon;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    // 1. Ménsula 3D de 4.0 m (IPE 200, S275) con carga puntual Pz = -10 kN en el extremo libre
    {
        Obra obra;
        const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
        const auto n2 = obra.add_nudo({4.0, 0.0, 0.0}, VinculacionExterior::Libre);
        const auto b1 = obra.add_barra(n1, n2, "IPE 200", "S275");
        obra.find_barra_mut(b1)->incluir_peso_propio = false;

        obra.add_hipotesis_carga("Q1", "Carga puntual en punta", 2);
        obra.add_carga_puntual_nudo(n2, 1, {0.0, 0.0, -10.0});

        const auto res = SolverEngine::resolver_caso_carga(obra, {{1, 1.0}}, 1, "Q1", "Mensula");
        const auto* reacc = res.find_reaccion_nudo(n1);
        TEST_ASSERT(reacc != nullptr);
        TEST_NEAR(reacc->fz_kn, 10.0, 1.0e-3);
        TEST_NEAR(reacc->my_knm, -40.0, 1.0e-2);

        const auto* desp = res.find_desplazamiento_nudo(n2);
        TEST_ASSERT(desp != nullptr);
        // Flecha teórica de Timoshenko: uz = -P*L^3/(3*E*Iy) - P*L/(G*Avz)
        const auto props = TimoshenkoBarElement3d::extraer_propiedades_barra(obra, *obra.find_barra(b1));
        const double uz_euler = -(10.0 * std::pow(4.0, 3)) / (3.0 * props.e_kpa * props.iy_m4);
        const double uz_shear = -(10.0 * 4.0) / (props.g_kpa * props.avz_m2);
        TEST_NEAR(desp->uz_m, uz_euler + uz_shear, 1.0e-4);

        const auto* eb = res.find_esfuerzos_barra(b1);
        TEST_ASSERT(eb != nullptr && eb->puntos.size() == 11);
        TEST_NEAR(eb->puntos.front().vz_kn, 10.0, 1.0e-2);
        TEST_NEAR(eb->puntos.front().my_knm, -40.0, 1.0e-2);
        TEST_NEAR(eb->puntos.back().my_knm, 0.0, 1.0e-2);
    }

    // 2. Viga biarticulada de L = 6.0 m con carga uniforme qz = 12 kN/m hacia -Z
    {
        Obra obra;
        const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::ArticuladoFijo);
        const auto n2 = obra.add_nudo({6.0, 0.0, 0.0}, VinculacionExterior::ArticuladoFijo);
        const auto b1 = obra.add_barra(n1, n2, "IPE 300", "S275");
        auto* bar = obra.find_barra_mut(b1);
        bar->incluir_peso_propio = false;
        bar->empotramiento = EmpotramientoExtremosBarra::biarticulado();

        obra.add_hipotesis_carga("Q", "Sobrecarga uniforme", 2);
        obra.add_carga_lineal_barra(b1, 1, 12.0, 12.0, {0.0, 0.0, -1.0}, false);

        const auto res = SolverEngine::resolver_caso_carga(obra, {{1, 1.0}}, 1, "Q", "Biapoyada", false, 11);
        const auto* r1 = res.find_reaccion_nudo(n1);
        const auto* r2 = res.find_reaccion_nudo(n2);
        TEST_ASSERT(r1 != nullptr && r2 != nullptr);
        // Reacción vertical Rz = q * L / 2 = 12 * 6 / 2 = 36 kN
        TEST_NEAR(r1->fz_kn, 36.0, 1.0e-2);
        TEST_NEAR(r2->fz_kn, 36.0, 1.0e-2);

        const auto* eb = res.find_esfuerzos_barra(b1);
        TEST_ASSERT(eb != nullptr && eb->puntos.size() == 11);
        // Momento en extremos = 0, momento en centro de vano (estación 5, x = 3.0 m): M = q*L^2/8 = 12*36/8 = 54 kN·m
        TEST_NEAR(eb->puntos.front().my_knm, 0.0, 1.0e-2);
        TEST_NEAR(eb->puntos.back().my_knm, 0.0, 1.0e-2);
        TEST_NEAR(std::abs(eb->puntos[5].my_knm), 54.0, 1.0e-1);
        TEST_ASSERT(eb->flecha_max_xz_mm > 1.0);
    }
}

void test_p09_1_shells_diaphragms_and_springs() {
    using namespace cypeclon;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    Obra obra;
    // Losa cuadrada 4x4 m apoyada en sus 4 esquinas con pilares y lámina MEF de H.A.
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({4.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n3 = obra.add_nudo({4.0, 4.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n4 = obra.add_nudo({0.0, 4.0, 0.0}, VinculacionExterior::ApoyoElastico);
    DatosVinculacionExteriorNudo datos_muelle{};
    datos_muelle.kx_kn_m = 50000.0;
    datos_muelle.ky_kn_m = 50000.0;
    datos_muelle.kz_kn_m = 80000.0;
    obra.set_vinculacion_exterior_nudo(n4, VinculacionExterior::ApoyoElastico, datos_muelle);

    const auto n5 = obra.add_nudo({0.0, 0.0, 3.0}, VinculacionExterior::Libre);
    const auto n6 = obra.add_nudo({4.0, 0.0, 3.0}, VinculacionExterior::Libre);
    const auto n7 = obra.add_nudo({4.0, 4.0, 3.0}, VinculacionExterior::Libre);
    const auto n8 = obra.add_nudo({0.0, 4.0, 3.0}, VinculacionExterior::Libre);

    obra.add_barra(n1, n5, "HEB 200", "S275");
    obra.add_barra(n2, n6, "HEB 200", "S275");
    obra.add_barra(n3, n7, "HEB 200", "S275");
    obra.add_barra(n4, n8, "HEB 200", "S275");

    obra.add_barra(n5, n6, "IPE 240", "S275");
    obra.add_barra(n6, n7, "IPE 240", "S275");
    obra.add_barra(n7, n8, "IPE 240", "S275");
    obra.add_barra(n8, n5, "IPE 240", "S275");

    // Añadir nivel y generar diafragma rígido en cota Z = 3.0 m
    obra.add_nivel_planta("Forjado 1", 3.0, 3.0);
    TEST_ASSERT(obra.generar_diafragmas_por_niveles() == 1);

    // Añadir lámina MEF en cota Z = 3.0 m y carga superficial sobre lámina en hipótesis Q1 (ID 3)
    const auto lam_id = obra.add_lamina({{0.0, 0.0, 3.0}, {4.0, 0.0, 3.0}, {4.0, 4.0, 3.0}, {0.0, 4.0, 3.0}}, 0.20, "C25/30");
    obra.find_lamina_mut(lam_id)->tamano_discretizacion_m = 2.0;
    obra.discretizar_todas_las_laminas(2.0);

    CargaLamina3d cl;
    cl.lamina_id = lam_id;
    cl.hipotesis_id = 3;
    cl.tipo = TipoCargaLamina::SuperficialUniforme;
    cl.valor_kn_m2_o_kn = 5.0;
    cl.direccion = {0.0, 0.0, -1.0};
    obra.add_carga_lamina(cl);

    // Empuje horizontal en nudo 5: gracias al diafragma rígido, n5 y n6 se desplazan igual en X
    obra.add_carga_puntual_nudo(n5, 3, {40.0, 0.0, 0.0});

    const auto summary = SolverEngine::analyze_linear_static(obra);
    TEST_ASSERT(summary.converged);
    TEST_ASSERT(summary.active_bar_count == 8);
    TEST_ASSERT(summary.active_shell_triangle_count >= 4);
    TEST_ASSERT(summary.resultados_hipotesis.size() == 3);
    TEST_ASSERT(!summary.resultados_combinaciones.empty());
    TEST_ASSERT(summary.envolventes_barras.size() == 8);

    const auto& res_q1 = summary.resultados_hipotesis[2];
    const auto* d5 = res_q1.find_desplazamiento_nudo(n5);
    const auto* d6 = res_q1.find_desplazamiento_nudo(n6);
    TEST_ASSERT(d5 != nullptr && d6 != nullptr);
    TEST_NEAR(d5->ux_m, d6->ux_m, 1.0e-4);
    TEST_ASSERT(!res_q1.esfuerzos_laminas.empty());
    TEST_ASSERT(res_q1.esfuerzos_laminas.front().von_mises_cara_sup_mpa > 0.0);
}

void test_p09_2_modal_pdelta_nonlinear_buckling_and_pushover() {
    using namespace cypeclon;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    // Pórtico plano 2D/3D con recuadro arriostrado por pareja de tirantes en X
    Obra obra;
    obra.datos_generales_mut().con_sismo_dinamico = true;
    obra.datos_generales_mut().aceleracion_sismica_basica_g = 0.16;
    obra.datos_generales_mut().modos_vibracion_sismo = 4;

    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({5.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n3 = obra.add_nudo({0.0, 0.0, 3.5}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({5.0, 0.0, 3.5}, VinculacionExterior::Libre);

    const auto col1 = obra.add_barra(n1, n3, "HEB 200", "S275");
    const auto col2 = obra.add_barra(n2, n4, "HEB 200", "S275");
    const auto viga = obra.add_barra(n3, n4, "IPE 240", "S275");

    obra.add_hipotesis_carga("G1", "Cargas permanentes y empuje", 0);
    obra.add_carga_puntual_nudo(n3, 1, {35.0, 0.0, -120.0});
    obra.add_carga_puntual_nudo(n4, 1, {35.0, 0.0, -120.0});
    obra.add_carga_lineal_barra(viga, 1, 15.0, 15.0, {0.0, 0.0, -1.0});

    // 1. Análisis Modal Espectral (CQC + SRSS + comprobación de cortante basal)
    TEST_NEAR(SolverEngine::correlacion_modal_cqc(15.0, 15.0, 0.05), 1.0, 1.0e-6);
    TEST_ASSERT(SolverEngine::correlacion_modal_cqc(10.0, 40.0, 0.05) < 0.05);

    const auto modal = SolverEngine::analyze_modal_spectral(obra, 4, MetodoCombinacionModal::CQC, 0.05);
    TEST_ASSERT(modal.converged);
    TEST_ASSERT(!modal.modos.empty());
    TEST_ASSERT(modal.modos.front().periodo_s > 0.01);
    TEST_ASSERT(modal.modos.front().frecuencia_hz > 0.1);
    TEST_ASSERT(modal.masa_total_estructura_ton > 1.0);
    TEST_ASSERT(modal.cortante_basal_dinamico_x_kn > 0.0);
    TEST_ASSERT(modal.factor_escala_cortante_basal_x >= 1.0);

    // 2. Efectos de 2.º Orden (P-Delta)
    const auto pdelta = SolverEngine::analyze_second_order_pdelta(obra, {{1, 1.0}});
    TEST_ASSERT(pdelta.converged);
    TEST_ASSERT(pdelta.iteraciones >= 1);
    TEST_ASSERT(pdelta.indice_estabilidad_gamma_z >= 1.0);
    const auto res_1 = SolverEngine::resolver_caso_carga(obra, {{1, 1.0}});
    TEST_ASSERT(pdelta.resultado_amplificado.find_desplazamiento_nudo(n3)->ux_m >=
                res_1.find_desplazamiento_nudo(n3)->ux_m);

    // 3. Análisis de Estabilidad Global / Pandeo por Autovalores (`cyosgruanpan_*`)
    const auto buckling = SolverEngine::analyze_global_buckling(obra, {{1, 1.0}}, 3);
    TEST_ASSERT(buckling.converged);
    TEST_ASSERT(!buckling.modos_pandeo.empty());
    TEST_ASSERT(buckling.lambda_cr_minimo > 1.0);

    // 4. Análisis Sísmico Estático No Lineal Pushover (`pushover.dll`, `pushoverbas.dll`)
    const auto pushover = SolverEngine::analyze_pushover(obra, {1.0, 0.0, 0.0}, n3, 15, 0.18);
    TEST_ASSERT(pushover.converged);
    TEST_ASSERT(pushover.curva_capacidad.size() == 15);
    TEST_ASSERT(pushover.cortante_maximo_vmax_kn > 0.0);
    TEST_ASSERT(pushover.desplazamiento_fluencia_dy_m > 0.0);
    TEST_ASSERT(pushover.tiene_punto_desempeno);
    TEST_ASSERT(pushover.desplazamiento_desempeno_m > 0.0);

    // 5. Cálculo No Lineal Iterativo de Tirantes (solo tracción) y Apoyos con Levantamiento
    const auto diag1 = obra.add_barra(n1, n4, "R 20", "S275"); // Diagonal traccionada bajo empuje +X
    const auto diag2 = obra.add_barra(n2, n3, "R 20", "S275"); // Diagonal comprimida bajo empuje +X
    obra.emparejar_tirantes_recuadro(diag1, diag2);

    const auto nl = SolverEngine::analyze_nonlinear_tension_only_and_uplift(
        obra, {{1, 1.0}}, MetodoCalculoTirantes::MetodoGeneralIterativoOpenSees);
    TEST_ASSERT(nl.converged);
    TEST_ASSERT(!nl.tirantes_desactivados_por_compresion.empty());
    TEST_ASSERT(nl.tirantes_desactivados_por_compresion.front() == diag2);
    // La diagonal 1 está traccionada (N > 0) y la diagonal 2 ha sido desactivada (N == 0)
    TEST_ASSERT(nl.resultado_equilibrio.find_esfuerzos_barra(diag1)->n_max_kn > 1.0);
    TEST_NEAR(nl.resultado_equilibrio.find_esfuerzos_barra(diag2)->n_max_kn, 0.0, 1.0e-6);

    // 6. Integración con Comandos Ribbon en GUI (`AppWindow`)
    gui::AppWindow app;
    app.obra_mut() = obra;
    TEST_ASSERT(app.execute_command("Calculo_calcular"));
    TEST_ASSERT(app.last_solver_summary().converged);
    TEST_ASSERT(app.last_solver_summary().analisis_modal.has_value());
    TEST_ASSERT(app.execute_command("Calculo_CalcPan"));
    TEST_ASSERT(app.last_solver_summary().analisis_pandeo_global.has_value());
    TEST_ASSERT(app.execute_command("PO_Calcular"));
    TEST_ASSERT(app.last_solver_summary().analisis_pushover.has_value());
    (void)col1;
    (void)col2;
}

} // namespace

void run_solver_phase8_tests() {
    test_p09_1_linear_algebra_and_eigen_solver();
    test_p09_1_timoshenko_cantilever_and_pinned_beam();
    test_p09_1_shells_diaphragms_and_springs();
    test_p09_2_modal_pdelta_nonlinear_buckling_and_pushover();
}

