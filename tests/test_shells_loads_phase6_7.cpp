#include "gui/app_window.hpp"
#include "model/obra.hpp"
#include "model/shells_panels_loads.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>

namespace {

void check_p67(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL Phase6-7] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define P67_ASSERT(expr) check_p67((expr), #expr, __FILE__, __LINE__)
#define P67_NEAR(a, b, tol) P67_ASSERT(std::abs((a) - (b)) <= (tol))

// ============================================================================
// P-07.1: Subsistema de Láminas MEF Shell (`calcmef`, `calcmallad3d`, `visesf3d`)
// ============================================================================
void test_p07_1_laminas_shell_mef() {
    using namespace cypeclon::model;

    Obra obra;
    // Losa rectangular de 6.0 x 4.0 m a cota Z = 3.0 m (Área bruta = 24.0 m^2)
    const auto lam_id = obra.add_lamina(
        {{0.0, 0.0, 3.0}, {6.0, 0.0, 3.0}, {6.0, 4.0, 3.0}, {0.0, 4.0, 3.0}},
        0.25,
        "C25/30"
    );
    P67_ASSERT(lam_id == 1 && obra.estructura().laminas.size() == 1);

    auto* lam = obra.find_lamina_mut(lam_id);
    P67_ASSERT(lam != nullptr);
    P67_NEAR(ShellElementEngine::calcular_area_poligono_3d(lam->vertices), 24.0, 1e-6);

    // Ejes locales de la lámina y giro de ortotropía
    const auto axes0 = ShellElementEngine::calcular_ejes_locales_lamina(lam->vertices, 0, 0.0, false);
    P67_NEAR(axes0.u.x, 1.0, 1e-6);
    P67_NEAR(axes0.v.y, 1.0, 1e-6);
    P67_NEAR(axes0.w.z, 1.0, 1e-6);

    const auto axes_inv = ShellElementEngine::calcular_ejes_locales_lamina(lam->vertices, 0, 0.0, true);
    P67_NEAR(axes_inv.w.z, -1.0, 1e-6);

    // Añadir hueco interior de 2.0 x 1.0 m (Área del hueco = 2.0 m^2 -> Área neta = 22.0 m^2)
    const auto hueco_id = obra.add_hueco_lamina(
        lam_id,
        {{2.0, 1.5, 3.0}, {4.0, 1.5, 3.0}, {4.0, 2.5, 3.0}, {2.0, 2.5, 3.0}}
    );
    P67_ASSERT(hueco_id == 1);
    P67_ASSERT(ShellElementEngine::es_punto_interior_lamina(*lam, {1.0, 1.0, 3.0}));
    P67_ASSERT(!ShellElementEngine::es_punto_interior_lamina(*lam, {3.0, 2.0, 3.0})); // Centro del hueco

    // Vinculación interior y exterior en aristas (`cm3dmalla_crea_vinc_ext_arista_lamina`)
    P67_ASSERT(obra.set_vinculacion_arista_lamina(lam_id, VinculacionAristaLamina{
        0, TipoVinculacionInteriorArista::Empotrada, 0.0, VinculacionExterior::ArticuladoFijo, 0.0
    }));
    P67_ASSERT(obra.set_vinculacion_arista_lamina(lam_id, VinculacionAristaLamina{
        2, TipoVinculacionInteriorArista::Articulada, 0.0, VinculacionExterior::ApoyoElastico, 15000.0
    }));
    P67_ASSERT(lam->vinculaciones_aristas.size() == 2);

    // Discretización automática en elementos finitos triangulares (`cm3dmalla_genera_mallado_de_elementos_modelo3d`)
    lam->tamano_discretizacion_m = 0.50;
    const auto total_tris = obra.discretizar_todas_las_laminas(0.50);
    P67_ASSERT(total_tris > 150);
    P67_NEAR(lam->malla.area_total_mallada_m2, 22.0, 0.05);

    // Transformación tensorial de esfuerzos, Von Mises y Banda de integración (`calcmef.dll`)
    EsfuerzosUnitariosLamina esf{};
    esf.nx_kn_m = 120.0;
    esf.ny_kn_m = 40.0;
    esf.nxy_kn_m = 30.0;
    esf.mx_knm_m = 45.0;
    esf.my_knm_m = 20.0;
    esf.mxy_knm_m = 10.0;
    esf.qx_kn_m = 65.0;
    esf.qy_kn_m = 25.0;

    // Invariante primer tensor (Nx + Ny y Mx + My se conservan ante rotación de ejes)
    const auto esf_rot = ShellElementEngine::transformar_esfuerzos_lamina(esf, 0.65);
    P67_NEAR(esf_rot.nx_kn_m + esf_rot.ny_kn_m, esf.nx_kn_m + esf.ny_kn_m, 1e-6);
    P67_NEAR(esf_rot.mx_knm_m + esf_rot.my_knm_m, esf.mx_knm_m + esf.my_knm_m, 1e-6);

    const double vm_sup = ShellElementEngine::calcular_tension_von_mises_mpa(esf, 0.25, +1);
    const double vm_med = ShellElementEngine::calcular_tension_von_mises_mpa(esf, 0.25, 0);
    P67_ASSERT(vm_sup > vm_med && vm_med > 0.1);

    BandaIntegracionLamina banda{};
    banda.nombre = "Banda Central X";
    banda.punto_inicial = {0.0, 2.0, 3.0};
    banda.punto_final = {6.0, 2.0, 3.0};
    banda.ancho_izquierdo_m = 0.75;
    banda.ancho_derecho_m = 0.75;
    const auto bid = obra.add_banda_integracion_lamina(lam_id, banda);
    P67_ASSERT(bid == 1);

    const auto int_banda = ShellElementEngine::integrar_esfuerzos_en_banda(esf, banda);
    P67_NEAR(int_banda.axil_n_kn, 120.0 * 1.50, 1e-6);
    P67_NEAR(int_banda.flector_my_knm, 45.0 * 1.50, 1e-6);

    // Cuantías de armado Wood-Armer en las 4 caras (`cefcal_calcula_cuantias_necesarias_combinacion`)
    const auto wa = ShellElementEngine::calcular_cuantias_wood_armer(esf, 0.25, 0.030);
    P67_ASSERT(wa.cumple_cuantia_minima);
    P67_ASSERT(wa.asx_inf_cm2_m > wa.asy_inf_cm2_m);
    P67_ASSERT(wa.asx_sup_cm2_m >= 2.25); // >= cuantía geométrica mínima

    // Dividir y unir láminas
    const auto [l_sub1, l_sub2] = obra.dividir_lamina(lam_id, 0, 2);
    P67_ASSERT(l_sub1 != 0 && l_sub2 != 0 && obra.estructura().laminas.size() == 2);
    const auto l_unida = obra.unir_laminas(l_sub1, l_sub2);
    P67_ASSERT(l_unida != 0 && obra.estructura().laminas.size() == 1);
    P67_NEAR(ShellElementEngine::calcular_area_poligono_3d(obra.find_lamina(l_unida)->vertices), 24.0, 1e-4);
}

// ============================================================================
// P-07.2: Subsistema de Paños y Reparto Unidireccional a Barras (`panyos3d.dll`)
// ============================================================================
void test_p07_2_panyos_y_reparto_cargas() {
    using namespace cypeclon::model;

    Obra obra;
    // Crear 3 pórticos paralelos separados 5.0 m en X (X = 0, 5, 10 m) y de longitud 6.0 m en Y (Y = 0 -> 6 m)
    const auto n1a = obra.add_nudo({0.0, 0.0, 4.0});
    const auto n1b = obra.add_nudo({0.0, 6.0, 4.0});
    const auto n2a = obra.add_nudo({5.0, 0.0, 4.0});
    const auto n2b = obra.add_nudo({5.0, 6.0, 4.0});
    const auto n3a = obra.add_nudo({10.0, 0.0, 4.0});
    const auto n3b = obra.add_nudo({10.0, 6.0, 4.0});

    const auto b_izq = obra.add_barra(n1a, n1b, "IPE 240", "S275");
    const auto b_cen = obra.add_barra(n2a, n2b, "IPE 270", "S275");
    const auto b_der = obra.add_barra(n3a, n3b, "IPE 240", "S275");

    // Paño rectangular de 10.0 x 6.0 m con dirección de reparto según eje X ({1, 0, 0})
    const auto panyo_id = obra.add_panyo(
        {{0.0, 0.0, 4.0}, {10.0, 0.0, 4.0}, {10.0, 6.0, 4.0}, {0.0, 6.0, 4.0}},
        {1.0, 0.0, 0.0}
    );
    P67_ASSERT(panyo_id == 1);

    // Carga superficial uniforme de 4.0 kN/m^2 en hipótesis Q1 (id=3)
    obra.add_carga_superficial_panyo(panyo_id, 3, 4.0, {0.0, 0.0, -1.0}, false, "Forjado Planta");

    // Descomponer cargas del paño en cargas lineales sobre las 3 vigas:
    // Anchos tributarios: b_izq = 2.5 m (q = 10 kN/m), b_cen = 5.0 m (q = 20 kN/m), b_der = 2.5 m (q = 10 kN/m)
    const auto num_gen = obra.actualizar_reparto_cargas_panyos_a_barras();
    P67_ASSERT(num_gen == 3);
    P67_ASSERT(obra.estructura().cargas_lineales.size() == 3);

    double q_izq = 0.0, q_cen = 0.0, q_der = 0.0;
    for (const auto& cl : obra.estructura().cargas_lineales) {
        P67_ASSERT(cl.generada_por_panyo && cl.panyo_origen_id == panyo_id);
        if (cl.barra_id == b_izq) q_izq = cl.q_ini_kn_m;
        if (cl.barra_id == b_cen) q_cen = cl.q_ini_kn_m;
        if (cl.barra_id == b_der) q_der = cl.q_ini_kn_m;
    }
    P67_NEAR(q_izq, 10.0, 1e-5);
    P67_NEAR(q_cen, 20.0, 1e-5);
    P67_NEAR(q_der, 10.0, 1e-5);
}

// ============================================================================
// P-08.1: Subsistema Completo de Cargas en Nudos, Barras, Láminas y Paños
// ============================================================================
void test_p08_1_subsistema_cargas() {
    using namespace cypeclon::model;

    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({6.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto b1 = obra.add_barra(n1, n2, "IPE 300", "S275");

    // 1. Cargas en nudos: alta, edición, cambio de hipótesis y borrado
    const auto cn_id = obra.add_carga_puntual_nudo(n2, 1, {10.0, 0.0, -50.0}, {0.0, 15.0, 0.0});
    P67_ASSERT(obra.editar_carga_nudo(cn_id, {20.0, -5.0, -80.0}, {0.0, 25.0, 0.0}));
    P67_ASSERT(obra.cambiar_hipotesis_carga_nudo(cn_id, 2));
    P67_ASSERT(obra.estructura().cargas_nudos.front().hipotesis_id == 2);
    P67_NEAR(obra.estructura().cargas_nudos.front().fuerzas_kn.z, -80.0, 1e-6);

    // 2. Cargas en barras: uniforme, puntual, trapezoidal, momento puntual, temperatura y pretensado
    CargaLinealBarra c_unif{};
    c_unif.barra_id = b1;
    c_unif.hipotesis_id = 1;
    c_unif.tipo = TipoCargaBarra::Uniforme;
    c_unif.q_ini_kn_m = 12.0;
    c_unif.q_fin_kn_m = 12.0;
    const auto r_unif = MemberLoadCalculator::calcular_empotramiento_perfecto(c_unif, 6.0);
    P67_NEAR(r_unif.f_local_ini_kn, 36.0, 1e-6);
    P67_NEAR(r_unif.m_empotramiento_ini_knm, 36.0, 1e-6); // q*L^2/12 = 12*36/12 = 36 kN·m

    CargaLinealBarra c_punt{};
    c_punt.barra_id = b1;
    c_punt.hipotesis_id = 2;
    c_punt.tipo = TipoCargaBarra::Puntual;
    c_punt.q_ini_kn_m = 40.0; // P = 40 kN en centro de vano (x_ini_rel = 0.5)
    c_punt.x_ini_rel = 0.5;
    const auto r_punt = MemberLoadCalculator::calcular_empotramiento_perfecto(c_punt, 6.0);
    P67_NEAR(r_punt.f_local_ini_kn, 20.0, 1e-6);
    P67_NEAR(r_punt.m_empotramiento_ini_knm, 30.0, 1e-6); // P*L/8 = 40*6/8 = 30 kN·m

    CargaLinealBarra c_temp{};
    c_temp.barra_id = b1;
    c_temp.hipotesis_id = 3;
    c_temp.tipo = TipoCargaBarra::IncrementoTemperatura;
    c_temp.delta_temp_uniforme_c = 25.0;
    c_temp.delta_temp_gradiente_c = 10.0;
    const auto id_temp = obra.add_carga_especial_barra(c_temp);
    P67_ASSERT(id_temp != 0);
    const auto r_temp = MemberLoadCalculator::calcular_empotramiento_perfecto(c_temp, 6.0, 0.00538, 8.356e-5, 0.30);
    P67_ASSERT(r_temp.axil_termico_o_pretensado_kn > 300.0);
    P67_ASSERT(r_temp.m_empotramiento_ini_knm > 5.0);

    // 3. Cargas sobre láminas (`cm3dmalla_append_carga_*_a_lamina`)
    const auto lam_id = obra.add_lamina({{0.0, 0.0, 0.0}, {4.0, 0.0, 0.0}, {4.0, 4.0, 0.0}, {0.0, 4.0, 0.0}}, 0.20, "C25/30");
    CargaLamina3d cl{};
    cl.lamina_id = lam_id;
    cl.hipotesis_id = 2;
    cl.tipo = TipoCargaLamina::SuperficialUniforme;
    cl.valor_kn_m2_o_kn = 5.0;
    const auto cl_id = obra.add_carga_lamina(cl);
    P67_ASSERT(cl_id == 1 && obra.estructura().cargas_laminas.size() == 1);
    P67_ASSERT(obra.cambiar_hipotesis_carga_lamina(cl_id, 3));
    P67_ASSERT(obra.delete_carga_lamina(cl_id));
    P67_ASSERT(obra.estructura().cargas_laminas.empty());

    // 4. Comandos Ribbon de visualización de cargas en AppWindow
    cypeclon::gui::AppWindow app;
    P67_ASSERT(app.execute_command("Carga_hipotesis_vista"));
    P67_ASSERT(app.execute_command("Carga_escalas"));
}

} // namespace

void run_shells_loads_phase6_7_tests() {
    test_p07_1_laminas_shell_mef();
    test_p07_2_panyos_y_reparto_cargas();
    test_p08_1_subsistema_cargas();
}
