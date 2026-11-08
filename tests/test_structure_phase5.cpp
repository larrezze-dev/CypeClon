#include "gui/app_window.hpp"
#include "model/obra.hpp"
#include "model/structure_elements.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void check_p5(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL Phase5] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define P5_ASSERT(expr) check_p5((expr), #expr, __FILE__, __LINE__)
#define P5_NEAR(a, b, tol) P5_ASSERT(std::abs((a) - (b)) <= (tol))

// ============================================================================
// P-06.1: Operaciones sobre Nudos (`ed3dlib.dll`, `calcesf.dll`)
// ============================================================================
void test_p06_1_operaciones_nudos() {
    using namespace cypeclon::model;

    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({0.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n3 = obra.add_nudo({6.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({6.0, 0.0, 0.0}, VinculacionExterior::ArticuladoFijo);

    const auto b1 = obra.add_barra(n1, n2, "HEB 200", "S275");
    const auto b2 = obra.add_barra(n2, n3, "IPE 300", "S275");
    const auto b3 = obra.add_barra(n3, n4, "HEB 200", "S275");

    P5_ASSERT(obra.estructura().nudos.size() == 4);
    P5_ASSERT(obra.estructura().barras.size() == 3);
    P5_NEAR(obra.longitud_barra_m(b2), 6.0, 1e-6);

    // Mover nudo actualizando longitud de barra y verificando Undo/Redo
    P5_ASSERT(obra.move_nudo(n3, {8.0, 0.0, 4.0}));
    P5_NEAR(obra.longitud_barra_m(b2), 8.0, 1e-6);
    P5_ASSERT(obra.undo_redo().undo());
    P5_NEAR(obra.longitud_barra_m(b2), 6.0, 1e-6);
    P5_ASSERT(obra.undo_redo().redo());
    P5_NEAR(obra.longitud_barra_m(b2), 8.0, 1e-6);

    // Desempatar nudo n2 en la barra b2 y volver a vincular/empatar
    const auto n2_dup = obra.desempatar_nudo(n2, b2);
    P5_ASSERT(n2_dup != 0 && n2_dup != n2);
    P5_ASSERT(obra.estructura().nudos.size() == 5);
    P5_ASSERT(obra.find_barra(b2)->nudo_inicial_id == n2_dup);
    P5_ASSERT(obra.find_barra(b1)->nudo_final_id == n2);

    P5_ASSERT(obra.vincular_empatar_nudos(n2, n2_dup));
    P5_ASSERT(obra.estructura().nudos.size() == 4);
    P5_ASSERT(obra.find_barra(b2)->nudo_inicial_id == n2);

    // Vinculación exterior: deslizaderas en plano/recta, muelles elásticos y apoyo no lineal con levantamiento
    auto datos_plano = DatosVinculacionExteriorNudo::from_tipo(VinculacionExterior::DeslizaderaPlanoInclinado, false);
    datos_plano.vector_direccion_o_normal = cypeclon::core::Vec3d{0.0, 0.6, 0.8}.normalized();
    P5_ASSERT(obra.set_vinculacion_exterior_nudo(n4, VinculacionExterior::DeslizaderaPlanoInclinado, datos_plano));
    P5_ASSERT(obra.find_nudo(n4)->vinc_ext == VinculacionExterior::DeslizaderaPlanoInclinado);
    P5_NEAR(obra.find_nudo(n4)->vinc_ext_datos.vector_direccion_o_normal.norm(), 1.0, 1e-6);

    auto datos_elast = DatosVinculacionExteriorNudo::from_tipo(VinculacionExterior::ApoyoElastico, false);
    datos_elast.kx_kn_m = 15000.0;
    datos_elast.ky_kn_m = 15000.0;
    datos_elast.kz_kn_m = 80000.0;
    datos_elast.kty_knm_rad = 12000.0;
    P5_ASSERT(obra.set_vinculacion_exterior_nudo(n1, VinculacionExterior::ApoyoElastico, datos_elast));
    P5_NEAR(obra.find_nudo(n1)->vinc_ext_datos.kz_kn_m, 80000.0, 1e-6);

    auto datos_nl = DatosVinculacionExteriorNudo::from_tipo(VinculacionExterior::ApoyoNoLinealSoloCompresion, true);
    P5_ASSERT(datos_nl.solo_compresion && !datos_nl.solo_traccion && datos_nl.giros_coaccionados);
    P5_ASSERT(obra.set_vinculacion_exterior_nudo(n1, VinculacionExterior::ApoyoNoLinealSoloCompresion, datos_nl));

    // Vinculación interior parcial (0 <= alpha <= 1)
    P5_ASSERT(obra.set_vinculacion_interior_nudo(n2, TipoVinculacionInterior::ParcialmenteEmpotrado, 0.65));
    P5_ASSERT(obra.find_nudo(n2)->tipo_vinc_int == TipoVinculacionInterior::ParcialmenteEmpotrado);
    P5_NEAR(obra.find_nudo(n2)->grado_empotramiento_interior, 0.65, 1e-6);

    // Ligaduras de desplazamiento y Diafragmas rígidos por nivel de planta
    const auto lig_id = obra.add_ligadura_nudos("Ligadura Cabezas", {n2, n3}, true, true, false);
    P5_ASSERT(lig_id == 1 && obra.estructura().ligaduras.size() == 1);

    obra.add_nivel_planta("Planta Cubierta", 4.0, 4.0);
    const auto num_diaf = obra.generar_diafragmas_por_niveles(0.05);
    P5_ASSERT(num_diaf == 1);
    P5_ASSERT(obra.find_nudo(n2)->diafragma_id == obra.find_nudo(n3)->diafragma_id);
    P5_ASSERT(obra.find_nudo(n1)->diafragma_id == 0);

    // Borrar nudo limpiando barras incidentes + Undo
    P5_ASSERT(obra.delete_nudo(n4, true));
    P5_ASSERT(obra.estructura().nudos.size() == 3);
    P5_ASSERT(obra.find_barra(b3) == nullptr);
    P5_ASSERT(obra.undo_redo().undo());
    P5_ASSERT(obra.estructura().nudos.size() == 4);
    P5_ASSERT(obra.find_barra(b3) != nullptr);
}

// ============================================================================
// P-06.2: Operaciones sobre Barras y Bibliotecas de Perfiles (`bibperf3`, `seccion`, `piezamet`, `pzmadera`)
// ============================================================================
void test_p06_2_barras_y_biblioteca_perfiles() {
    using namespace cypeclon::model;

    ProfileLibraryManager lib;
    P5_ASSERT(lib.catalog().size() >= 85);

    const auto series = lib.list_series();
    P5_ASSERT(series.size() >= 15);

    // Verificar propiedades exactas de IPE 200 (h=200, b=100, tw=5.6, tf=8.5, r=12 -> A≈28.5 cm2, Iy≈1943 cm4, Wel,y≈194.3 cm3)
    const auto* ipe200 = lib.find_profile("IPE 200");
    P5_ASSERT(ipe200 != nullptr);
    P5_NEAR(ipe200->area_cm2, 28.5, 0.4);
    P5_NEAR(ipe200->iy_cm4, 1943.0, 25.0);
    P5_NEAR(ipe200->iz_cm4, 142.0, 3.0);
    P5_NEAR(ipe200->wely_cm3, 194.3, 2.5);
    P5_NEAR(ipe200->wply_cm3, 220.6, 3.5);
    P5_ASSERT(ipe200->it_cm4 > 6.0 && ipe200->iw_cm6 > 12000.0);

    // Verificar HEB 300, UPN 200, SHS, RHS, CHS, CF conformado, Madera GL y Aluminio
    const auto* heb300 = lib.find_profile("HEB 300");
    const auto* upn200 = lib.find_profile("UPN 200");
    const auto* shs100 = lib.find_profile("SHS 100x100x5");
    const auto* chs114 = lib.find_profile("CHS 114.3x5.0");
    const auto* cf200  = lib.find_profile("CF 200x60x20x2.5");
    const auto* gl200  = lib.find_profile("GL 200x600");
    const auto* alu_i  = lib.find_profile("ALU-I 120x60x6");
    P5_ASSERT(heb300 && upn200 && shs100 && chs114 && cf200 && gl200 && alu_i);
    P5_ASSERT(heb300->iy_cm4 > 24000.0);
    P5_ASSERT(shs100->it_cm4 > shs100->iy_cm4); // Tubo cerrado tiene It de Bredt elevado
    P5_ASSERT(gl200->familia == FamiliaMaterialPerfil::Madera);
    P5_ASSERT(alu_i->familia == FamiliaMaterialPerfil::Aluminio);

    // Secciones compuestas: 2x UPN 200 en cajón cerrado soldado y viga Boyd alveolar
    const auto cajon_upn = ProfileLibraryManager::compute_composite_section(
        *upn200, DisposicionCompuestaPerfil::DobleEnCajonSoldado, 0.0);
    P5_NEAR(cajon_upn.area_cm2, 2.0 * upn200->area_cm2, 1e-4);
    P5_ASSERT(cajon_upn.iz_cm4 > 2.0 * upn200->iz_cm4);
    P5_ASSERT(cajon_upn.it_cm4 > 10.0 * upn200->it_cm4);

    const auto boyd = ProfileLibraryManager::compute_composite_section(
        *ipe200, DisposicionCompuestaPerfil::VigaAlveolarCastelada);
    P5_NEAR(boyd.h_mm, 300.0, 1e-4);
    P5_ASSERT(boyd.iy_cm4 > 1.8 * ipe200->iy_cm4);

    // Sección acartelada en extremo (IPE 300 + cartela de 250 mm)
    const auto* ipe300 = lib.find_profile("IPE 300");
    P5_ASSERT(ipe300 != nullptr);
    const auto haunched = ProfileLibraryManager::compute_haunched_section_at_end(*ipe300, 250.0, false);
    P5_NEAR(haunched.h_mm, 550.0, 1e-4);
    P5_ASSERT(haunched.iy_cm4 > 3.0 * ipe300->iy_cm4);

    // Operaciones topológicas sobre Barras: intersecciones 3D, piezas y agrupaciones
    Obra obra;
    const auto na = obra.add_nudo({0.0, 0.0, 0.0});
    const auto nb = obra.add_nudo({6.0, 6.0, 0.0});
    const auto nc = obra.add_nudo({0.0, 6.0, 0.0});
    const auto nd = obra.add_nudo({6.0, 0.0, 0.0});

    const auto diag1 = obra.add_barra(na, nb, "IPE 200", "S275");
    const auto diag2 = obra.add_barra(nc, nd, "IPE 200", "S275");
    P5_ASSERT(obra.estructura().nudos.size() == 4);
    P5_ASSERT(obra.estructura().barras.size() == 2);

    const auto cruces = obra.generar_nudos_y_dividir_barras_en_cruces(1e-3);
    P5_ASSERT(cruces == 1);
    P5_ASSERT(obra.estructura().nudos.size() == 5);
    P5_ASSERT(obra.estructura().barras.size() == 4);

    // Crear pieza continua y dividir pieza
    const auto pid = obra.crear_pieza({diag1, 3}, "Diagonal continua 1");
    P5_ASSERT(pid == 1);
    P5_NEAR(obra.estructura().piezas.front().longitud_total_m, std::sqrt(72.0), 1e-4);
    P5_ASSERT(obra.dividir_pieza(pid));
    P5_ASSERT(obra.estructura().piezas.empty());

    // Agrupar y desagrupar barras
    const auto aid = obra.agrupar_barras({diag1, diag2}, "Diagonales IPE 200");
    P5_ASSERT(aid == 1 && obra.find_barra(diag1)->agrupacion_id == aid);
    P5_ASSERT(obra.desagrupar_barras(aid));
    P5_ASSERT(obra.find_barra(diag1)->agrupacion_id == 0);
}

// ============================================================================
// P-06.3: Propiedades Mecánicas de Barras (`elements`, `compflecha`, `mtlbas`, `pushover`)
// ============================================================================
void test_p06_3_propiedades_mecanicas_barras() {
    using namespace cypeclon::model;

    // 1. Disposición de sección: puntos de ajuste cardinales y excentricidades
    DisposicionSeccionBarra disp{};
    disp.punto_ajuste = PuntoAjusteSeccion::SuperiorCentro;
    disp.desp_y_ini_m = 0.015;
    disp.desp_z_ini_m = -0.020;
    const auto off_ini = disp.offset_cdg_extremo_inicial_m(0.150, 0.300);
    P5_NEAR(off_ini.x, 0.015, 1e-6);
    P5_NEAR(off_ini.y, -0.170, 1e-6);

    // 2. Pandeo automático Julian-Lawrence / Wood (`vpandeo_beta_intraslacional` y `vpandeo_beta_traslacional`)
    const double beta_biemp_intra = DatosPandeoBarra::vpandeo_beta_intraslacional(0.0, 0.0);
    const double beta_biart_intra = DatosPandeoBarra::vpandeo_beta_intraslacional(1.0, 1.0);
    const double beta_emp_art_intra = DatosPandeoBarra::vpandeo_beta_intraslacional(0.0, 1.0);
    P5_NEAR(beta_biemp_intra, 0.50, 0.02);
    P5_NEAR(beta_biart_intra, 1.00, 0.02);
    P5_NEAR(beta_emp_art_intra, 0.70, 0.02);

    const double beta_biemp_tras = DatosPandeoBarra::vpandeo_beta_traslacional(0.0, 0.0);
    const double beta_emp_art_tras = DatosPandeoBarra::vpandeo_beta_traslacional(0.0, 1.0);
    P5_NEAR(beta_biemp_tras, 1.00, 0.02);
    P5_NEAR(beta_emp_art_tras, 2.00, 0.05);

    // 3. Momento crítico elástico de pandeo lateral Mcr (kN·m)
    ProfileLibraryManager lib;
    const auto* ipe300 = lib.find_profile("IPE 300");
    P5_ASSERT(ipe300 != nullptr);
    DatosPandeoLateralBarra plt{};
    plt.coeficiente_c1 = 1.13;
    const double mcr_6m = plt.compute_mcr_knm(*ipe300, 6.0, true);
    const double mcr_3m = plt.compute_mcr_knm(*ipe300, 3.0, true);
    P5_ASSERT(mcr_6m > 40.0 && mcr_6m < 400.0);
    P5_ASSERT(mcr_3m > 2.2 * mcr_6m); // Al reducir Llt a la mitad, Mcr aumenta más del doble

    // 4. Límites de flecha (`compflecha.dll`) y Grupos de flecha
    DatosFlechaBarra df{};
    P5_NEAR(df.flecha_activa.flecha_admisible_mm(6.0), 10.0, 1e-6); // min(6000/400=15 mm, 10 mm) = 10 mm
    P5_NEAR(df.flecha_total_plazo_infinito.flecha_admisible_mm(6.0), 20.0, 1e-6); // 6000/300 = 20 mm

    // 5. Recuadro arriostrado de Tirantes en X (`esp3dcmb_son_tirantes_geometricamente_compatibles_para_formar_cuadro_arriostramiento`)
    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::ArticuladoFijo);
    const auto n2 = obra.add_nudo({0.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n3 = obra.add_nudo({5.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({5.0, 0.0, 0.0}, VinculacionExterior::ArticuladoFijo);

    const auto col1 = obra.add_barra(n1, n2, "HEB 180", "S275");
    const auto viga = obra.add_barra(n2, n3, "IPE 240", "S275");
    const auto col2 = obra.add_barra(n4, n3, "HEB 180", "S275");
    const auto tir1 = obra.add_barra(n1, n3, "R 20", "S275");
    const auto tir2 = obra.add_barra(n4, n2, "R 20", "S275");

    const auto cuadro = obra.emparejar_tirantes_recuadro(tir1, tir2);
    P5_ASSERT(cuadro.geometricamente_valido);
    P5_ASSERT(obra.find_barra(tir1)->es_tirante && obra.find_barra(tir2)->es_tirante);
    P5_NEAR(obra.find_barra(tir1)->empotramiento.emp_xy_ini, 0.0, 1e-6);

    const auto gf_id = obra.crear_grupo_flecha({viga}, TipoMedicionFlecha::SecanteEntreExtremos);
    P5_ASSERT(gf_id == 1 && obra.find_barra(viga)->flecha.grupo_flecha_id == gf_id);

    const auto n_pan = obra.calcular_pandeo_automatico_estructura(false, true);
    P5_ASSERT(n_pan == 3); // col1, viga, col2 (los 2 tirantes tienen beta = 0)
    P5_ASSERT(obra.find_barra(col1)->pandeo.beta_xy < 1.0);
    P5_ASSERT(obra.find_barra(col1)->pandeo.beta_xz > 1.0);

    // 6. Biblioteca y asignación de rótulas plásticas (`pushover.dll`)
    const auto rot_id = obra.add_definicion_rotula_plastica(DefinicionRotulaPlastica{
        0, "ASCE41-Viga-IPE240", TipoRotulaPlastica::FlexionEjeFuerteM3, 0.02, 0.012, 0.028, 0.045, 0.055, 0.20
    });
    auto* bm_viga = obra.find_barra_mut(viga);
    bm_viga->rotulas_plasticas.tiene_rotula_ini = true;
    bm_viga->rotulas_plasticas.rotula_ini_id = rot_id;
    bm_viga->rotulas_plasticas.tiene_rotula_fin = true;
    bm_viga->rotulas_plasticas.rotula_fin_id = rot_id;
    P5_ASSERT(bm_viga->rotulas_plasticas.tiene_rotula_ini && bm_viga->rotulas_plasticas.rotula_fin_id == rot_id);
    (void)col2;
}

// ============================================================================
// P-06.4: Pilares y Vigas de Hormigón Armado (`pilares`, `porvigas`, `comppilhorm`, `discap`)
// ============================================================================
void test_p06_4_pilares_y_vigas_hormigon() {
    using namespace cypeclon::model;

    Obra obra;

    // 1. Pilar de Hormigón Armado multi-planta con 2 tramos (Planta Baja 0->3.5 m y Planta 1ª 3.5->6.8 m)
    PilarEstructural p1{};
    p1.referencia = "P-1";
    p1.pos_planta_xy = {0.0, 0.0};
    p1.con_vinculacion_exterior = true;

    TramoPilarPlanta t1{};
    t1.nivel_inferior_id = 0;
    t1.nivel_superior_id = 1;
    t1.cota_inf_m = 0.0;
    t1.cota_sup_m = 3.5;
    t1.tipo_seccion = TipoSeccionPilar::HormigonRectangular;
    t1.ancho_x_m = 0.40;
    t1.ancho_y_m = 0.40;
    t1.armado.num_barras_esquina = 4;
    t1.armado.diametro_esquina_mm = 20;
    t1.armado.num_barras_cara_x = 2;
    t1.armado.diametro_cara_x_mm = 16;
    t1.armado.num_barras_cara_y = 2;
    t1.armado.diametro_cara_y_mm = 16;
    t1.armado.separacion_zona_central_cm = 15.0;
    t1.armado.separacion_zona_critica_cm = 10.0;

    TramoPilarPlanta t2 = t1;
    t2.nivel_inferior_id = 1;
    t2.nivel_superior_id = 2;
    t2.cota_inf_m = 3.5;
    t2.cota_sup_m = 6.8;
    t2.ancho_x_m = 0.35;
    t2.ancho_y_m = 0.35;

    p1.tramos = {t1, t2};
    const auto pid = obra.add_pilar_estructural(p1, true);
    P5_ASSERT(pid == 1 && obra.estructura().pilares.size() == 1);
    P5_ASSERT(obra.estructura().barras.size() == 2);

    // Comprobación normativa y de capacidad sísmica del tramo de pilar (`comppilhorm`, `discap`)
    const auto comp_pilar = ConcreteStructureEngine::verificar_tramo_pilar(t1, 140.0, 1.30, true);
    P5_ASSERT(comp_pilar.cumple_dimension_minima);
    P5_ASSERT(comp_pilar.cumple_cuantia_min_max);
    P5_ASSERT(comp_pilar.cumple_separacion_estribos);
    P5_ASSERT(comp_pilar.cumple_capacidad_flexion_nudo);
    P5_ASSERT(comp_pilar.cumple_global);
    P5_ASSERT(comp_pilar.cortante_capacidad_sismica_kn > 50.0);

    // 2. Pórtico de Vigas de Hormigón Armado (`seccviga`, `porvigas`, `discap`)
    VanoVigaHormigon vano{};
    vano.referencia = "V-101";
    vano.tipo_seccion = TipoSeccionViga::Rectangular;
    vano.ancho_alma_b_m = 0.30;
    vano.canto_total_h_m = 0.50;
    vano.armado.num_montaje_sup = 3;
    vano.armado.diametro_montaje_sup_mm = 16;
    vano.armado.num_ref_sup_izq = 2;
    vano.armado.diametro_ref_sup_izq_mm = 20;
    vano.armado.num_ref_sup_der = 2;
    vano.armado.diametro_ref_sup_der_mm = 20;
    vano.armado.num_montaje_inf = 3;
    vano.armado.diametro_montaje_inf_mm = 16;
    vano.armado.num_ref_inf_vano = 2;
    vano.armado.diametro_ref_inf_vano_mm = 16;
    vano.armado.diametro_estribo_mm = 10;
    vano.armado.num_ramas_estribo = 2;
    vano.armado.sep_extremo_izq_cm = 10.0;
    vano.armado.sep_extremo_der_cm = 10.0;

    PorticoVigasEstructural portico{};
    portico.referencia = "Pórtico Eje A";
    portico.vanos.push_back(vano);
    const auto port_id = obra.add_portico_vigas(portico);
    P5_ASSERT(port_id == 1 && obra.estructura().porticos_vigas.size() == 1);

    const auto comp_viga = ConcreteStructureEngine::verificar_vano_viga(vano, 5.60, 22.0, 1.20, true);
    P5_ASSERT(comp_viga.cumple_armadura_minima_flexion);
    P5_ASSERT(comp_viga.cumple_armadura_piel);
    P5_ASSERT(comp_viga.cumple_cortante_capacidad);
    P5_ASSERT(comp_viga.cumple_global);
    P5_ASSERT(comp_viga.mrd_neg_izq_knm > 150.0);
    P5_ASSERT(comp_viga.mrd_pos_vano_knm > 120.0);

    // 3. Integración con AppWindow para comandos Ribbon de Fase 5
    cypeclon::gui::AppWindow app;
    P5_ASSERT(app.execute_command("Gestion_biblioteca_perfiles"));
    P5_ASSERT(app.execute_command("Calculo_BibRotulasPlasticas"));
    P5_ASSERT(app.execute_command("Obra_reiniciar_numeracion"));
    P5_ASSERT(app.execute_command("gen_diafrags"));
    P5_ASSERT(app.execute_command("Obra_RepararGeometria"));
    P5_ASSERT(app.execute_command("Calculo_AsignaCoefsPan"));
}

} // namespace

void run_structure_phase5_tests() {
    test_p06_1_operaciones_nudos();
    test_p06_2_barras_y_biblioteca_perfiles();
    test_p06_3_propiedades_mecanicas_barras();
    test_p06_4_pilares_y_vigas_hormigon();
}
