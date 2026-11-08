#include "codes/normativa_engine.hpp"
#include "gui/welcome_and_dialogs.hpp"
#include "model/obra.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void check_p4(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define TEST_ASSERT(expr) check_p4((expr), #expr, __FILE__, __LINE__)

} // namespace

void run_codes_phase4_tests() {
    using namespace cypeclon::codes;
    using namespace cypeclon::model;
    using namespace cypeclon::gui;

    // =========================================================================
    // 1. P-05.1 — Normativas y Catálogos de Materiales (`cypenorma`, `datmater`, `dmatext`, `eurocodigo9`)
    // =========================================================================
    {
        const auto& normas_h = NormativaEngine::catalogo_normas(FamiliaNormativa::Hormigon);
        TEST_ASSERT(normas_h.size() >= 6);
        const auto* n_ce = NormativaEngine::buscar_norma(FamiliaNormativa::Hormigon, "Código Estructural");
        TEST_ASSERT(n_ce != nullptr);
        TEST_ASSERT(std::abs(n_ce->gamma_principal - 1.50) < 1e-6);
        TEST_ASSERT(n_ce->material_defecto == "HA-25");

        const auto* n_aisc = NormativaEngine::buscar_norma(FamiliaNormativa::AceroLaminado, "AISC 360");
        TEST_ASSERT(n_aisc != nullptr);
        TEST_ASSERT(n_aisc->material_defecto == "A36 (ASTM A36)");

        // Catálogo de Hormigón (`dathorm`)
        const auto* c25 = NormativaEngine::buscar_hormigon("C25/30");
        TEST_ASSERT(c25 != nullptr);
        TEST_ASSERT(std::abs(c25->fck_mpa - 25.0) < 1e-6);
        TEST_ASSERT(std::abs(c25->fcd_mpa(1.50) - (25.0 / 1.50)) < 1e-4);
        TEST_ASSERT(c25->ecm_mpa > 31000.0 && c25->ecm_mpa < 32000.0);
        TEST_ASSERT(std::abs(c25->poisson_nu - 0.20) < 1e-6);

        const auto* ha30 = NormativaEngine::buscar_hormigon("HA-30");
        TEST_ASSERT(ha30 != nullptr && std::abs(ha30->fck_mpa - 30.0) < 1e-6);

        // Catálogo de Acero laminado y espesores (`acerolam_limite_elastico_segun_espesor`)
        const auto* s275 = NormativaEngine::buscar_acero_estructural("S275");
        TEST_ASSERT(s275 != nullptr);
        TEST_ASSERT(std::abs(s275->e_mpa - 210000.0) < 1e-3);
        TEST_ASSERT(std::abs(s275->fy_segun_espesor(12.0) - 275.0) < 1e-3);
        TEST_ASSERT(std::abs(s275->fy_segun_espesor(25.0) - 265.0) < 1e-3);
        TEST_ASSERT(std::abs(s275->fy_segun_espesor(50.0) - 255.0) < 1e-3);

        const auto* s350gd = NormativaEngine::buscar_acero_estructural("S350GD");
        TEST_ASSERT(s350gd != nullptr && s350gd->es_conformado && std::abs(s350gd->fy_nominal_mpa - 350.0) < 1e-3);

        // Catálogo de Acero de armaduras y longitud de anclaje (`dxancla_long_anclaje_pos_I` / `pos_II`)
        const auto* b500sd = NormativaEngine::buscar_acero_armaduras("B 500 SD");
        TEST_ASSERT(b500sd != nullptr && b500sd->alta_ductilidad);
        const double lb_pos1 = b500sd->longitud_anclaje_basica_mm(16.0, 1.80, false);
        const double lb_pos2 = b500sd->longitud_anclaje_basica_mm(16.0, 1.80, true);
        TEST_ASSERT(lb_pos1 > 300.0 && lb_pos2 > lb_pos1);

        // Catálogo de Madera (`C18`..`C30`, `GL24h`..`GL32h`)
        const auto* c24 = NormativaEngine::buscar_madera("C24");
        TEST_ASSERT(c24 != nullptr && !c24->es_laminada_encolada && std::abs(c24->fm_k_mpa - 24.0) < 1e-3);
        const auto* gl28h = NormativaEngine::buscar_madera("GL28h");
        TEST_ASSERT(gl28h != nullptr && gl28h->es_laminada_encolada && std::abs(gl28h->fm_k_mpa - 28.0) < 1e-3);

        // Catálogo de Aluminio (`eurocodigo9.dll` — `ec9mat_*`)
        const auto* aw6082 = NormativaEngine::buscar_aluminio("EN AW-6082 - T6");
        TEST_ASSERT(aw6082 != nullptr);
        TEST_ASSERT(std::abs(aw6082->e_mpa - 70000.0) < 1e-3);
        TEST_ASSERT(std::abs(aw6082->fo_mpa - 250.0) < 1e-3);
        TEST_ASSERT(std::abs(aw6082->rho_o_haz - 0.50) < 1e-3);

        // Catálogo de Pernos y Tornillos (`4.6`..`10.9`, `A325`, `A490`)
        const auto* p109 = NormativaEngine::buscar_perno_tornillo("10.9");
        TEST_ASSERT(p109 != nullptr && std::abs(p109->fyb_mpa - 900.0) < 1e-3 && std::abs(p109->fub_mpa - 1000.0) < 1e-3);
        TEST_ASSERT(NormativaEngine::catalogo_diametros_pernos().size() == 8);
    }

    // =========================================================================
    // 2. P-05.2 — Categorías de Uso, Hipótesis y Combinaciones ELU/ELS (`sestlim`, `combs`, `catuso`, `ghipocal`)
    // =========================================================================
    {
        const auto& cat_g1 = NormativaEngine::obtener_categoria_uso(CodigoCategoriaUso::G1_CubiertaConservacion);
        TEST_ASSERT(std::abs(cat_g1.psi_0) < 1e-6 && std::abs(cat_g1.psi_2) < 1e-6);

        const auto& cat_e = NormativaEngine::obtener_categoria_uso(CodigoCategoriaUso::E_Almacenamiento);
        TEST_ASSERT(std::abs(cat_e.psi_0 - 1.0) < 1e-6 && std::abs(cat_e.psi_1 - 0.9) < 1e-6 && std::abs(cat_e.psi_2 - 0.8) < 1e-6);

        auto hips = NormativaEngine::crear_hipotesis_por_defecto(true, true, true);
        TEST_ASSERT(hips.size() == 10); // PP, CM, Q1, V1..V4, N1, SX, SY

        ParametrosGeneracionCombinaciones params;
        params.incluir_combinaciones_sismicas = true;
        params.incluir_combinaciones_incendio = true;
        params.incluir_els_desplazamientos = true;
        params.incluir_tensiones_terreno = true;

        const auto combs = NormativaEngine::generar_combinaciones(hips, params);
        TEST_ASSERT(combs.size() >= 25);

        std::size_t n_elu = 0;
        std::size_t n_sismo = 0;
        std::size_t n_fuego = 0;
        std::size_t n_els = 0;
        std::size_t n_terreno = 0;

        for (const auto& c : combs) {
            TEST_ASSERT(!c.nombre_formula.empty());
            // Verificar que V1 y V2 (incompatibles entre sí en grupo 1) nunca aparecen juntas en la misma combinación
            const double cv1 = c.coeficiente_de(4); // V1
            const double cv2 = c.coeficiente_de(5); // V2
            TEST_ASSERT(!(std::abs(cv1) > 1e-6 && std::abs(cv2) > 1e-6));

            if (c.situacion == SituacionProyecto::ELU_PersistenteTransitoria) ++n_elu;
            else if (c.situacion == SituacionProyecto::ELU_Sismica) ++n_sismo;
            else if (c.situacion == SituacionProyecto::ELU_Incendio) ++n_fuego;
            else if (c.situacion == SituacionProyecto::ELS_Caracteristica ||
                     c.situacion == SituacionProyecto::ELS_Frecuente ||
                     c.situacion == SituacionProyecto::ELS_Cuasipermanente) ++n_els;
            else if (c.situacion == SituacionProyecto::TensionTerreno_Persistente) ++n_terreno;
        }
        TEST_ASSERT(n_elu >= 10);
        TEST_ASSERT(n_sismo == 8); // 8 combinaciones direccionales +-1.0 Ex +- 0.3 Ey y +-0.3 Ex +- 1.0 Ey
        TEST_ASSERT(n_fuego >= 4);
        TEST_ASSERT(n_els >= 6);
        TEST_ASSERT(n_terreno >= 4);
    }

    // =========================================================================
    // 3. P-05.3 — Acción Sísmica, Viento y Resistencia al Fuego (`sismo`, `veviento`, `rstfuego`, `catfabpin`, `catfabmor`)
    // =========================================================================
    {
        ConfiguracionSismo sismo;
        sismo.activo = true;
        sismo.norma = NormaSismica::NCSE_02_Espana;
        sismo.aceleracion_basica_ag_g = 0.16;
        sismo.tipo_terreno = TipoTerrenoSismico::TipoIII_SueloMedio_C;
        sismo.factor_comportamiento_qx = 3.0;

        const double sd_meseta = sismo.calcular_aceleracion_diseno_g(0.35, false);
        const double sd_largo = sismo.calcular_aceleracion_diseno_g(2.50, false);
        TEST_ASSERT(sd_meseta > sd_largo && sd_largo > 0.02);

        const auto curva = sismo.generar_curva_espectro(50, 4.0);
        TEST_ASSERT(curva.size() == 50);
        TEST_ASSERT(std::abs(curva.back().periodo_s - 4.0) < 1e-6);

        // Viento (`veviento.dll`)
        ConfiguracionViento viento;
        viento.zona_eolica = ZonaEolicaCte::ZonaC_29ms;
        viento.grado_aspereza = GradoAsperezaEntorno::GradoII_RuralLlano;
        TEST_ASSERT(std::abs(viento.presion_dinamica_basica_kn_m2() - 0.52) < 1e-6);
        TEST_ASSERT(viento.coeficiente_exposicion_ce(10.0) > viento.coeficiente_exposicion_ce(3.0));
        TEST_ASSERT(viento.presion_estatica_z_kn_m2(10.0) > 1.0);

        // Resistencia al Fuego y catálogos intumescentes (`rstfuego`, `catfabpin`, `catfabmor`)
        ConfiguracionResistenciaFuego fuego;
        fuego.comprobar_resistencia_fuego = true;
        fuego.resistencia_requerida = ClaseResistenciaFuego::R90;
        TEST_ASSERT(fuego.tiempo_requerido_minutos() == 90);
        TEST_ASSERT(fuego.etiqueta_clase() == "R 90");

        const auto* pintura = NormativaEngine::buscar_revestimiento_ignifugo("PROMAPAINT_SC4");
        TEST_ASSERT(pintura != nullptr);
        const double esp_r90 = fuego.calcular_espesor_proteccion_necesario_mm(180.0, 550.0, *pintura);
        TEST_ASSERT(esp_r90 >= pintura->espesor_min_mm && esp_r90 <= pintura->espesor_max_mm);
    }

    // =========================================================================
    // 4. P-05.4 — Clonación del Generador de Pórticos (`PorticoGeneratorEngine` / `ed3genport`)
    //    y Sincronización con `DialogoDatosGenerales` / `AsistenteNuevaObra`
    // =========================================================================
    {
        ConfiguracionGeneradorPorticos cfg_port;
        cfg_port.num_modulos_longitudinales = 5; // 6 pórticos transversales
        cfg_port.separacion_entre_porticos_m = 6.0;
        cfg_port.vanos = {
            VanoPorticoSpec{TipologiaVanoPortico::DosAguas, 20.0, 7.0, 7.0, 9.5, "HEB 260", "IPE 330"},
            VanoPorticoSpec{TipologiaVanoPortico::DosAguas, 16.0, 7.0, 6.5, 8.8, "HEB 240", "IPE 300"}
        };
        cfg_port.con_sobrecarga_nieve = true;
        cfg_port.viento.activo = true;
        cfg_port.edificio_con_huecos_abiertos = true;

        const auto dim_correas = PorticoGeneratorEngine::dimensionar_correas_cubierta(cfg_port);
        TEST_ASSERT(dim_correas.cumple);
        TEST_ASSERT(!dim_correas.perfil_optimo.empty());
        TEST_ASSERT(dim_correas.num_correas_por_faldon >= 6);
        TEST_ASSERT(dim_correas.peso_total_correas_kg > 500.0);

        Obra obra_nave;
        const auto resumen = PorticoGeneratorEngine::exportar_a_obra(cfg_port, obra_nave);
        TEST_ASSERT(resumen.nudos_creados > 40);
        TEST_ASSERT(resumen.barras_creadas > 50);
        TEST_ASSERT(resumen.panyos_creados == 6); // 4 faldones de cubierta (2 vanos a dos aguas) + 2 fachadas laterales
        TEST_ASSERT(resumen.hipotesis_creadas == 12); // PP, CM, Q1, N1 + 8 hipótesis de viento V1..V8
        TEST_ASSERT(resumen.cargas_superficiales_creadas >= 40);
        TEST_ASSERT(resumen.cargas_lineales_creadas >= 36);
        TEST_ASSERT(resumen.combinaciones_generadas >= 20);

        // Verificar también la integración con DialogoDatosGenerales (`seleccionar_norma_familia`)
        DialogoDatosGenerales dlg_dg;
        dlg_dg.load_from_obra(obra_nave);
        dlg_dg.seleccionar_norma_familia(FamiliaNormativa::Hormigon, "Código Estructural");
        dlg_dg.seleccionar_norma_familia(FamiliaNormativa::AceroLaminado, "AISC 360");
        dlg_dg.config_sismo_mut().activo = true;
        dlg_dg.config_sismo_mut().aceleracion_basica_ag_g = 0.18;
        dlg_dg.config_fuego_mut().comprobar_resistencia_fuego = true;
        dlg_dg.config_fuego_mut().resistencia_requerida = ClaseResistenciaFuego::R120;
        dlg_dg.apply_to_obra(obra_nave);

        TEST_ASSERT(obra_nave.datos_generales().hormigon_estructural == "HA-25");
        TEST_ASSERT(obra_nave.datos_generales().acero_laminado == "A36 (ASTM A36)");
        TEST_ASSERT(obra_nave.datos_generales().con_sismo_dinamico);
        TEST_ASSERT(std::abs(obra_nave.datos_generales().aceleracion_sismica_basica_g - 0.18) < 1e-6);
        TEST_ASSERT(obra_nave.datos_generales().resistencia_al_fuego_activa);
        TEST_ASSERT(obra_nave.datos_generales().clase_resistencia_fuego == "R 120");
    }
}

