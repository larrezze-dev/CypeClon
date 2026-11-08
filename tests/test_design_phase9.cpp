#include "codes/design_check_engine.hpp"
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

void test_p10_1_section_classification_and_buckling_curves() {
    using namespace cypeclon::codes;
    using namespace cypeclon::model;

    ProfileLibraryManager lib;

    // 1. Clasificación de perfil laminado IPE 200 en S275 a flexión pura -> Clase 1
    const auto* ipe200 = lib.find_profile("IPE 200");
    TEST_ASSERT(ipe200 != nullptr);
    const auto clas_ipe = MemberDesignEngine::clasificar_seccion_metalica(*ipe200, 275.0, 0.0, 45.0, 0.0, false);
    TEST_ASSERT(clas_ipe.clase_global == ClaseSeccionTransversal::Clase1_PlasticaDuctil ||
                clas_ipe.clase_global == ClaseSeccionTransversal::Clase2_PlasticaCompacta);
    TEST_NEAR(clas_ipe.factor_reduccion_area_rho, 1.0, 1.0e-6);
    TEST_ASSERT(clas_ipe.wy_calculo_cm3 >= ipe200->wely_cm3);

    // 2. Clasificación de perfil conformado en frío esbelto a compresión -> Clase 4 (sección eficaz de Winter)
    ProfileSectionProperties cf_esbelto = *ipe200;
    cf_esbelto.familia = FamiliaMaterialPerfil::AceroConformado;
    cf_esbelto.tipo_forma = GeometriaSeccionTipo::ConformadoC;
    cf_esbelto.h_mm = 260.0;
    cf_esbelto.b_mm = 90.0;
    cf_esbelto.tw_mm = 1.8;
    cf_esbelto.tf_mm = 1.8;
    const auto clas_cf = MemberDesignEngine::clasificar_seccion_metalica(cf_esbelto, 355.0, -120.0, 10.0, 0.0, false);
    TEST_ASSERT(clas_cf.clase_global == ClaseSeccionTransversal::Clase4_EsbeltaEfectiva);
    TEST_ASSERT(clas_cf.factor_reduccion_area_rho < 1.0 && clas_cf.factor_reduccion_area_rho >= 0.35);
    TEST_ASSERT(clas_cf.area_eficaz_cm2 < cf_esbelto.area_cm2);

    // 3. Clasificación de perfil de aluminio EN AW-6082 (Eurocódigo 9)
    const auto* alu_prof = lib.find_profile("ALU-I 120x60x6");
    TEST_ASSERT(alu_prof != nullptr);
    const auto clas_alu = MemberDesignEngine::clasificar_seccion_metalica(*alu_prof, 250.0, -50.0, 20.0, 0.0, true);
    TEST_NEAR(clas_alu.epsilon_material, 1.0, 1.0e-3);
    TEST_ASSERT(clas_alu.area_eficaz_cm2 > 0.0);

    // 4. Curvas de pandeo europeas (CTE DB SE-A / EN 1993-1-1 Art. 6.3.1.2)
    const double alpha_y = MemberDesignEngine::factor_imperfeccion_pandeo_acero(*ipe200, true, 275.0);
    const double alpha_z = MemberDesignEngine::factor_imperfeccion_pandeo_acero(*ipe200, false, 275.0);
    TEST_ASSERT(alpha_y <= alpha_z);
    TEST_NEAR(MemberDesignEngine::factor_reduccion_pandeo_chi(0.15, alpha_y), 1.0, 1.0e-6);
    const double chi_1 = MemberDesignEngine::factor_reduccion_pandeo_chi(1.00, 0.21); // curva a en lambda=1.0 -> ~0.6656
    TEST_NEAR(chi_1, 0.6656, 0.01);
}

void test_p10_1_member_elu_els_fire_and_justified_reports() {
    using namespace cypeclon::codes;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    Obra obra;
    obra.datos_generales_mut().resistencia_al_fuego_activa = true;
    obra.datos_generales_mut().clase_resistencia_fuego = "R 60";
    obra.datos_generales_mut().revestimiento_fuego_acero = "Pintura intumescente";

    // Pórtico plano con pilar HEB 200 (S275), dintel sobrecargado IPE 120 (S275),
    // correa de madera C24 y barra de aluminio
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({0.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n3 = obra.add_nudo({6.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({6.0, 0.0, 0.0}, VinculacionExterior::Empotrado);

    const auto b_pilar_izq = obra.add_barra(n1, n2, "HEB 200", "S275");
    const auto b_dintel = obra.add_barra(n2, n3, "IPE 120", "S275");
    (void)obra.add_barra(n4, n3, "HEB 200", "S275");

    // Añadir barra de madera aserrada y barra de aluminio biapoyadas
    const auto n5 = obra.add_nudo({0.0, 3.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n6 = obra.add_nudo({4.0, 3.0, 0.0}, VinculacionExterior::Empotrado);
    const auto b_madera = obra.add_barra(n5, n6, "RECT 150x300", "C24");

    const auto n7 = obra.add_nudo({0.0, 6.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n8 = obra.add_nudo({3.5, 6.0, 0.0}, VinculacionExterior::Empotrado);
    const auto b_aluminio = obra.add_barra(n7, n8, "ALU-I 120x60x6", "EN AW-6082-T6");

    // Cargas gravitatorias intensas sobre el dintel IPE 120 (para que falle ELU/ELS inicialmente)
    // y moderadas sobre madera y aluminio
    obra.add_carga_lineal_barra(b_dintel, 1, 18.0, 18.0, {0.0, 0.0, -1.0});
    obra.add_carga_lineal_barra(b_madera, 1, 3.5, 3.5, {0.0, 0.0, -1.0});
    obra.add_carga_lineal_barra(b_aluminio, 1, 2.0, 2.0, {0.0, 0.0, -1.0});

    const auto solver_res = SolverEngine::analyze_linear_static(obra);
    TEST_ASSERT(solver_res.converged);

    const auto resumen = MemberDesignEngine::comprobar_todas_las_barras(obra, solver_res);
    TEST_ASSERT(resumen.comprobaciones_barras.size() == 5);
    TEST_ASSERT(!resumen.listado_resumido_estructura.empty());

    // El pilar HEB 200 debe cumplir ELU/ELS
    const auto* comp_pilar = resumen.find_comprobacion_barra(b_pilar_izq);
    TEST_ASSERT(comp_pilar != nullptr);
    TEST_ASSERT(comp_pilar->cumple_elu);
    TEST_ASSERT(comp_pilar->articulos.size() >= 6);
    TEST_ASSERT(comp_pilar->temperatura_alcanzada_fuego_c > 100.0);

    // El dintel IPE 120 con 18 kN/m en 6 m de vano no debe cumplir
    const auto* comp_dintel = resumen.find_comprobacion_barra(b_dintel);
    TEST_ASSERT(comp_dintel != nullptr);
    TEST_ASSERT(!comp_dintel->cumple_elu);
    TEST_ASSERT(comp_dintel->aprovechamiento_elu_max_pct > 100.0);
    const std::string listado_ext = comp_dintel->generar_listado_extendido();
    TEST_ASSERT(listado_ext.find("LISTADO JUSTIFICADO DE COMPROBACIONES") != std::string::npos);
    TEST_ASSERT(listado_ext.find("NO CUMPLE") != std::string::npos);

    // La viga de madera C24 y la barra de aluminio deben tener artículos específicos de CTE DB SE-M y Eurocódigo 9
    const auto* comp_madera = resumen.find_comprobacion_barra(b_madera);
    TEST_ASSERT(comp_madera != nullptr);
    TEST_ASSERT(comp_madera->familia_material == FamiliaMaterialPerfil::Madera);
    TEST_ASSERT(comp_madera->articulos.size() >= 5);

    const auto* comp_alu = resumen.find_comprobacion_barra(b_aluminio);
    TEST_ASSERT(comp_alu != nullptr);
    TEST_ASSERT(comp_alu->familia_material == FamiliaMaterialPerfil::Aluminio);
    TEST_ASSERT(comp_alu->articulos.size() >= 5);
}

void test_p10_1_optimal_series_sizing_and_bar_groups() {
    using namespace cypeclon::codes;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({0.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n3 = obra.add_nudo({6.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({6.0, 0.0, 0.0}, VinculacionExterior::Empotrado);

    const auto b_pilar1 = obra.add_barra(n1, n2, "HEB 240", "S275");
    const auto b_dintel = obra.add_barra(n2, n3, "IPE 120", "S275");
    const auto b_pilar2 = obra.add_barra(n4, n3, "HEB 140", "S275");

    // Agrupar los dos pilares para que se dimensionen con el mismo perfil óptimo
    obra.agrupar_barras({b_pilar1, b_pilar2}, "Pilares Pórtico");
    obra.add_carga_lineal_barra(b_dintel, 1, 15.0, 15.0, {0.0, 0.0, -1.0});

    auto solver_res = SolverEngine::analyze_linear_static(obra);

    // 1. Dimensionamiento individual en serie del dintel IPE 120
    const auto dim_dintel = MemberDesignEngine::dimensionar_barra_en_serie(
        obra, b_dintel, solver_res, ModoDimensionamientoSerie::DesdePrimerPerfilSerie, false);
    TEST_ASSERT(dim_dintel.encontrado_perfil_que_cumple);
    TEST_ASSERT(dim_dintel.perfil_optimo_seleccionado != dim_dintel.perfil_inicial);
    TEST_ASSERT(dim_dintel.peso_optimo_kg_m > dim_dintel.peso_inicial_kg_m);
    TEST_ASSERT(dim_dintel.aprovechamiento_optimo_pct <= 100.0);
    TEST_ASSERT(dim_dintel.tabla_comparativa_serie.size() >= 10);

    // 2. Dimensionamiento completo de la estructura respetando agrupaciones y recalculando matriz de rigidez
    const auto resumen_opt = MemberDesignEngine::dimensionar_estructura_completa(
        obra, solver_res, ModoDimensionamientoSerie::DesdePrimerPerfilSerie, true);
    TEST_ASSERT(resumen_opt.barras_que_cumplen == 3);
    TEST_ASSERT(resumen_opt.barras_que_no_cumplen == 0);
    TEST_ASSERT(obra.find_barra(b_dintel)->perfil != "IPE 120");
    // Ambos pilares agrupados deben haber quedado con idéntico perfil
    TEST_ASSERT(obra.find_barra(b_pilar1)->perfil == obra.find_barra(b_pilar2)->perfil);
}

void test_p10_2_rc_column_interaction_diagram_and_editor() {
    using namespace cypeclon::codes;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    // 1. Generación de curva de interacción N-My y N-Mz para sección rectangular y circular
    TramoPilarPlanta tramo_rect;
    tramo_rect.tipo_seccion = TipoSeccionPilar::HormigonRectangular;
    tramo_rect.ancho_x_m = 0.35;
    tramo_rect.ancho_y_m = 0.40;
    tramo_rect.cota_inf_m = 0.0;
    tramo_rect.cota_sup_m = 3.2;
    tramo_rect.armado.diametro_esquina_mm = 16;
    tramo_rect.armado.num_barras_cara_x = 1;
    tramo_rect.armado.diametro_cara_x_mm = 12;
    tramo_rect.armado.num_barras_cara_y = 1;
    tramo_rect.armado.diametro_cara_y_mm = 12;

    const auto curva_y = ReinforcedConcreteEditorEngine::generar_diagrama_interaccion_pilar(
        tramo_rect, 16.67, 434.78, 200000.0, true, 25);
    TEST_ASSERT(curva_y.size() == 25);
    // Compresión pura en el extremo superior y tracción pura en el inferior
    TEST_ASSERT(curva_y.front().axil_resistente_n_rd_kn > 2000.0);
    TEST_ASSERT(curva_y.back().axil_resistente_n_rd_kn < -400.0);
    double max_my_rd = 0.0;
    for (const auto& pt : curva_y) {
        max_my_rd = std::max(max_my_rd, pt.momento_resistente_my_rd_knm);
    }
    TEST_ASSERT(max_my_rd > 120.0);

    TramoPilarPlanta tramo_circ = tramo_rect;
    tramo_circ.tipo_seccion = TipoSeccionPilar::HormigonCircular;
    tramo_circ.diametro_m = 0.40;
    const auto curva_circ = ReinforcedConcreteEditorEngine::generar_diagrama_interaccion_pilar(
        tramo_circ, 16.67, 434.78, 200000.0, true, 21);
    TEST_ASSERT(curva_circ.size() == 21);
    TEST_ASSERT(curva_circ.front().axil_resistente_n_rd_kn > 1800.0);

    // 2. Comprobación biaxial N-My-Mz con amplificación de 2.º orden por esbeltez y contorno de Bresler
    const auto comb_ok = ReinforcedConcreteEditorEngine::comprobar_combinacion_tramo_pilar(
        tramo_rect, 1, "1.35*PP + 1.50*Q", true, 850.0, 55.0, 35.0, 40.0, 25.0);
    TEST_ASSERT(comb_ok.cumple);
    TEST_ASSERT(comb_ok.my_ed_amplificado_knm >= 55.0);
    TEST_ASSERT(comb_ok.mz_ed_amplificado_knm >= 35.0);
    TEST_ASSERT(comb_ok.aprovechamiento_flexocompresion_pct > 15.0 &&
                comb_ok.aprovechamiento_flexocompresion_pct <= 100.0);

    const auto comb_fail = ReinforcedConcreteEditorEngine::comprobar_combinacion_tramo_pilar(
        tramo_rect, 2, "Comb. Extrema", true, 2400.0, 280.0, 220.0, 120.0, 90.0);
    TEST_ASSERT(!comb_fail.cumple);
    TEST_ASSERT(comb_fail.aprovechamiento_flexocompresion_pct > 100.0);

    // 3. Editor Avanzado de Pilares sobre Obra: análisis, dimensionamiento automático y edición manual
    Obra obra;
    obra.add_nivel_planta("Planta 1", 3.2, 3.2);
    PilarEstructural pilar;
    pilar.referencia = "P-1";
    pilar.pos_planta_xy = {0.0, 0.0};
    TramoPilarPlanta t1 = tramo_rect;
    t1.armado.diametro_esquina_mm = 12;
    t1.armado.num_barras_cara_x = 0;
    t1.armado.num_barras_cara_y = 0;
    pilar.tramos.push_back(t1);
    const auto pilar_id = obra.add_pilar_estructural(pilar, true);
    const auto barra_pilar_id = obra.estructura().pilares.front().tramos.front().barra_3d_id;
    const auto* barra_pilar = obra.find_barra(barra_pilar_id);
    TEST_ASSERT(barra_pilar != nullptr);

    // Aplicar axil fuerte y momentos en cabeza del pilar
    obra.add_carga_puntual_nudo(barra_pilar->nudo_final_id, 1, {35.0, 25.0, -900.0}, {40.0, 65.0, 0.0});
    const auto solver_res = SolverEngine::analyze_linear_static(obra);

    auto& pilar_mut = obra.estructura_mut().pilares.front();
    const auto tramos_dim = ReinforcedConcreteEditorEngine::dimensionar_pilar_completo(
        obra, pilar_mut, solver_res, true);
    TEST_ASSERT(tramos_dim.size() == 1);
    TEST_ASSERT(tramos_dim.front().cumple_tramo);
    TEST_ASSERT(!tramos_dim.front().despiece_armaduras.empty());
    TEST_ASSERT(tramos_dim.front().peso_total_acero_tramo_kg > 10.0);
    TEST_ASSERT(tramos_dim.front().listado_comprobaciones_tramo.find("Pilar P-1") != std::string::npos);

    // Edición manual interactiva del armado del tramo
    ArmadoTramoPilar manual = pilar_mut.tramos.front().armado;
    manual.diametro_esquina_mm = 25;
    manual.num_barras_cara_x = 2;
    manual.diametro_cara_x_mm = 20;
    manual.num_barras_cara_y = 2;
    manual.diametro_cara_y_mm = 20;
    const auto res_manual = ReinforcedConcreteEditorEngine::actualizar_armado_manual_tramo_pilar(
        obra, pilar_id, 0, manual, solver_res);
    TEST_ASSERT(res_manual.cumple_tramo);
    TEST_ASSERT(res_manual.area_acero_dispuesta_cm2 > tramos_dim.front().area_acero_dispuesta_cm2);
}

void test_p10_2_rc_beam_frame_editor_and_app_window_commands() {
    using namespace cypeclon::codes;
    using namespace cypeclon::gui;
    using namespace cypeclon::model;
    using namespace cypeclon::solver;

    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 3.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({6.0, 0.0, 3.0}, VinculacionExterior::Empotrado);
    const auto b_viga = obra.add_barra(n1, n2, "HA 30x50", "C25/30");
    obra.add_carga_lineal_barra(b_viga, 1, 28.0, 28.0, {0.0, 0.0, -1.0});

    PorticoVigasEstructural portico;
    portico.referencia = "Pórtico P-101";
    portico.nivel_planta_id = 1;
    VanoVigaHormigon vano;
    vano.id = 1;
    vano.referencia = "V-101";
    vano.barra_3d_id = b_viga;
    vano.ancho_alma_b_m = 0.30;
    vano.canto_total_h_m = 0.50;
    // Armado inicial escaso para que requiera refuerzo al dimensionar
    vano.armado.num_montaje_sup = 2;
    vano.armado.diametro_montaje_sup_mm = 10;
    vano.armado.num_ref_sup_izq = 0;
    vano.armado.num_ref_sup_der = 0;
    vano.armado.num_montaje_inf = 2;
    vano.armado.diametro_montaje_inf_mm = 10;
    vano.armado.num_ref_inf_vano = 0;
    portico.vanos.push_back(vano);
    const auto portico_id = obra.add_portico_vigas(portico);

    const auto solver_res = SolverEngine::analyze_linear_static(obra);

    // 1. Análisis inicial en el Editor Avanzado de Vigas (gráficas As,nec vs As,disp)
    const auto& portico_ref = obra.estructura().porticos_vigas.front();
    const auto analisis_ini = ReinforcedConcreteEditorEngine::analizar_vano_viga_editor(
        obra, portico_ref, portico_ref.vanos.front(), solver_res, 21);
    TEST_ASSERT(analisis_ini.puntos_grafica.size() == 21);
    TEST_ASSERT(analisis_ini.as_sup_nec_max_izq_cm2 > analisis_ini.as_sup_disp_izq_cm2);
    TEST_ASSERT(!analisis_ini.cumple_vano);

    // 2. Dimensionamiento automático del pórtico de vigas
    auto& portico_mut = obra.estructura_mut().porticos_vigas.front();
    const auto vanos_dim = ReinforcedConcreteEditorEngine::dimensionar_portico_vigas(
        obra, portico_mut, solver_res);
    TEST_ASSERT(vanos_dim.size() == 1);
    TEST_ASSERT(vanos_dim.front().cumple_vano);
    TEST_ASSERT(vanos_dim.front().as_sup_disp_izq_cm2 >= vanos_dim.front().as_sup_nec_max_izq_cm2);
    TEST_ASSERT(vanos_dim.front().as_inf_disp_vano_cm2 >= vanos_dim.front().as_inf_nec_max_vano_cm2);
    TEST_ASSERT(vanos_dim.front().flecha_activa_mm > 0.0);
    TEST_ASSERT(vanos_dim.front().flecha_activa_mm <= vanos_dim.front().flecha_activa_admisible_mm);
    TEST_ASSERT(vanos_dim.front().despiece_armaduras.size() >= 4);
    TEST_ASSERT(vanos_dim.front().peso_total_acero_vano_kg > 20.0);

    // 3. Edición manual interactiva del armado del vano
    ArmadoVanoViga armado_manual = portico_mut.vanos.front().armado;
    armado_manual.num_ref_inf_vano = 4;
    armado_manual.diametro_ref_inf_vano_mm = 20;
    const auto analisis_manual = ReinforcedConcreteEditorEngine::actualizar_armado_manual_vano_viga(
        obra, portico_id, 1, armado_manual, solver_res);
    TEST_ASSERT(analisis_manual.cumple_vano);
    TEST_ASSERT(analisis_manual.as_inf_disp_vano_cm2 > vanos_dim.front().as_inf_disp_vano_cm2);

    // 4. Integración con AppWindow y comandos de Ribbon de Fase 9
    AppWindow app;
    auto& app_obra = app.obra_mut();
    const auto an1 = app_obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto an2 = app_obra.add_nudo({0.0, 0.0, 3.5}, VinculacionExterior::Libre);
    const auto ab1 = app_obra.add_barra(an1, an2, "IPE 160", "S275");
    app_obra.add_carga_puntual_nudo(an2, 1, {12.0, 0.0, -40.0});
    app_obra.add_portico_vigas(portico_mut);

    TEST_ASSERT(app.execute_command("Calculo_comprueba_elementos"));
    TEST_ASSERT(app.last_design_summary().comprobaciones_barras.size() == 1);
    TEST_ASSERT(app.last_design_summary().find_comprobacion_barra(ab1) != nullptr);

    TEST_ASSERT(app.execute_command("dimensionamiento_desde_primer_perfil"));
    TEST_ASSERT(app.execute_command("Calculo_Dimensionar_revestimiento_incendio"));
    TEST_ASSERT(app.execute_command("pilares_armaduras"));
    TEST_ASSERT(app.execute_command("pilares_bloquear"));
    TEST_ASSERT(app.execute_command("pilares_desbloquear"));
    TEST_ASSERT(app.execute_command("pv_ed_editar_todos_portico"));
    TEST_ASSERT(app.execute_command("pv_ed_bloquear"));
    TEST_ASSERT(app_obra.estructura().porticos_vigas.front().bloqueado_armado);
    TEST_ASSERT(app.execute_command("pv_ed_desbloquear"));
    TEST_ASSERT(!app_obra.estructura().porticos_vigas.front().bloqueado_armado);
}

} // namespace

void run_design_phase9_tests() {
    test_p10_1_section_classification_and_buckling_curves();
    test_p10_1_member_elu_els_fire_and_justified_reports();
    test_p10_1_optimal_series_sizing_and_bar_groups();
    test_p10_2_rc_column_interaction_diagram_and_editor();
    test_p10_2_rc_beam_frame_editor_and_app_window_commands();
}
