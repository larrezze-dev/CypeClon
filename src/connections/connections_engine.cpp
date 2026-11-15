/**
 * @file connections_engine.cpp
 * @brief Fase 11 (P-12.1 / T-017 / A-030): Implementación del motor de Placas de Anclaje y Uniones Soldadas y Atornilladas.
 */

#include "connections/connections_engine.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>

#include "model/structure_elements.hpp"

namespace cypeclon::connections {

namespace {

std::string fmt_num(double v, int dec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(dec) << v;
    return oss.str();
}

void extraer_DIMENSIONES_perfil_mm(const std::string& ref_perfil, double& h_mm, double& b_mm, double& tw_mm, double& tf_mm) {
    static const model::ProfileLibraryManager lib;
    if (const auto* p = lib.find_profile(ref_perfil)) {
        h_mm = std::max(80.0, p->h_mm);
        b_mm = std::max(50.0, p->b_mm);
        tw_mm = std::max(4.0, p->tw_mm);
        tf_mm = std::max(6.0, p->tf_mm);
        return;
    }
    h_mm = 200.0;
    b_mm = 100.0;
    tw_mm = 6.0;
    tf_mm = 9.0;
}

void construir_prismas_3d_placa(const model::Obra& obra, PlacaAnclaje3d& placa) {
    placa.elementos_3d.clear();
    const auto* nudo = obra.find_nudo(placa.nudo_id);
    const core::Vec3d p0 = nudo ? nudo->pos : core::Vec3d{0.0, 0.0, 0.0};

    // 1. Placa base horizontal
    Prisma3dElementoUnion pr_placa{};
    pr_placa.nombre = "Placa base " + fmt_num(placa.ancho_lx_mm, 0) + "x" +
                      fmt_num(placa.largo_ly_mm, 0) + "x" + fmt_num(placa.espesor_e_mm, 0);
    pr_placa.centro_global_m = {
        p0.x + placa.desp_excentrico_x_mm * 0.001,
        p0.y + placa.desp_excentrico_y_mm * 0.001,
        p0.z - 0.5 * placa.espesor_e_mm * 0.001
    };
    pr_placa.semidimensiones_locales_m = {
        0.5 * placa.ancho_lx_mm * 0.001,
        0.5 * placa.largo_ly_mm * 0.001,
        0.5 * placa.espesor_e_mm * 0.001
    };
    pr_placa.color_rgb = 0x64748B;
    placa.elementos_3d.push_back(pr_placa);

    // 2. Pernos de anclaje verticales
    for (const auto& perno : placa.pernos) {
        Prisma3dElementoUnion pr_p{};
        pr_p.nombre = "Perno Ø" + fmt_num(perno.diametro_mm, 0) + " L=" + fmt_num(perno.longitud_anclaje_mm, 0);
        pr_p.centro_global_m = {
            p0.x + (placa.desp_excentrico_x_mm + perno.x_local_mm) * 0.001,
            p0.y + (placa.desp_excentrico_y_mm + perno.y_local_mm) * 0.001,
            p0.z - 0.5 * perno.longitud_anclaje_mm * 0.001
        };
        pr_p.semidimensiones_locales_m = {
            0.5 * perno.diametro_mm * 0.001,
            0.5 * perno.diametro_mm * 0.001,
            0.5 * perno.longitud_anclaje_mm * 0.001
        };
        pr_p.color_rgb = 0xF59E0B;
        placa.elementos_3d.push_back(pr_p);
    }

    // 3. Rigidizadores verticales
    for (const auto& rig : placa.rigidizadores) {
        Prisma3dElementoUnion pr_r{};
        pr_r.nombre = "Rigidizador " + rig.codigo;
        const core::Vec2d mid = (rig.pos_inicio_mm + rig.pos_fin_mm) * 0.5;
        pr_r.centro_global_m = {
            p0.x + mid.x * 0.001,
            p0.y + mid.y * 0.001,
            p0.z + 0.5 * rig.altura_mm * 0.001
        };
        pr_r.semidimensiones_locales_m = {
            0.5 * rig.longitud_base_mm * 0.001,
            0.5 * rig.espesor_mm * 0.001,
            0.5 * rig.altura_mm * 0.001
        };
        pr_r.color_rgb = 0x38BDF8;
        placa.elementos_3d.push_back(pr_r);
    }
}

} // namespace

// ==============================================================================
// 1. Catálogo de Tornillería (`unionbase2.dll`: `ub2eltos_*`)
// ==============================================================================

const std::vector<DatosTornilloCatalogo>& ConnectionsEngine::catalogo_tornillos_metricos() {
    static const std::vector<DatosTornilloCatalogo> kCat = {
        {"M12", 12.0, 13.0, 113.10,  84.3, 18.0, 20.8,  7.5, 10.8, 24.0, 2.5,  90.0,  130.0},
        {"M16", 16.0, 18.0, 201.06, 157.0, 24.0, 27.7, 10.0, 14.8, 30.0, 3.0, 225.0,  320.0},
        {"M20", 20.0, 22.0, 314.16, 245.0, 30.0, 34.6, 12.5, 18.0, 37.0, 3.0, 450.0,  650.0},
        {"M22", 22.0, 24.0, 380.13, 303.0, 34.0, 39.3, 14.0, 19.4, 39.0, 3.0, 610.0,  870.0},
        {"M24", 24.0, 26.0, 452.39, 353.0, 36.0, 41.6, 15.0, 21.5, 44.0, 4.0, 780.0, 1100.0},
        {"M27", 27.0, 30.0, 572.56, 459.0, 41.0, 47.3, 17.0, 23.8, 50.0, 4.0,1150.0, 1650.0},
        {"M30", 30.0, 33.0, 706.86, 561.0, 46.0, 53.1, 18.7, 25.6, 56.0, 4.0,1550.0, 2200.0},
        {"M36", 36.0, 39.0,1017.88, 817.0, 55.0, 63.5, 22.5, 31.0, 66.0, 5.0,2700.0, 3850.0}
    };
    return kCat;
}

DatosTornilloCatalogo ConnectionsEngine::obtener_datos_tornillo(double diametro_mm) {
    const auto& cat = catalogo_tornillos_metricos();
    for (const auto& t : cat) {
        if (t.diametro_nominal_mm + 0.5 >= diametro_mm) {
            return t;
        }
    }
    return cat.back();
}

void ConnectionsEngine::propiedades_mecanicas_clase_tornillo(ClaseAceroTornillo clase, double& fyb_mpa, double& fub_mpa) {
    switch (clase) {
        case ClaseAceroTornillo::Clase_4_6:
            fyb_mpa = 240.0;
            fub_mpa = 400.0;
            return;
        case ClaseAceroTornillo::Clase_5_6:
            fyb_mpa = 300.0;
            fub_mpa = 500.0;
            return;
        case ClaseAceroTornillo::Clase_6_8:
            fyb_mpa = 480.0;
            fub_mpa = 600.0;
            return;
        case ClaseAceroTornillo::Clase_8_8:
            fyb_mpa = 640.0;
            fub_mpa = 800.0;
            return;
        case ClaseAceroTornillo::Clase_10_9:
            fyb_mpa = 900.0;
            fub_mpa = 1000.0;
            return;
        case ClaseAceroTornillo::ASTM_A307:
            fyb_mpa = 248.0;
            fub_mpa = 414.0;
            return;
        case ClaseAceroTornillo::ASTM_A325:
            fyb_mpa = 634.0;
            fub_mpa = 827.0;
            return;
        case ClaseAceroTornillo::ASTM_A490:
            fyb_mpa = 896.0;
            fub_mpa = 1034.0;
            return;
    }
    fyb_mpa = 640.0;
    fub_mpa = 800.0;
}

double ConnectionsEngine::coeficiente_rozamiento_superficie(ClaseSuperficieRozamiento clase) {
    switch (clase) {
        case ClaseSuperficieRozamiento::ClaseA_Granallado_Mu050:
            return 0.50;
        case ClaseSuperficieRozamiento::ClaseB_SilicatoAlcalinoZinc_Mu040:
            return 0.40;
        case ClaseSuperficieRozamiento::ClaseC_CepilladoAlambre_Mu030:
            return 0.30;
        case ClaseSuperficieRozamiento::ClaseD_SinTratar_Mu020:
            return 0.20;
    }
    return 0.30;
}

// ==============================================================================
// 2. Subsistema de Placas de Anclaje (`placancl.dll`: `plplacan_*`)
// ==============================================================================

PlacaAnclaje3d ConnectionsEngine::generar_placa_anclaje_por_defecto(
    const model::Obra& obra,
    std::uint32_t nudo_id,
    const OpcionesCalculoPlacasAnclaje& opciones
) {
    PlacaAnclaje3d placa{};
    placa.id = nudo_id;
    placa.nudo_id = nudo_id;
    placa.acero_placa = opciones.acero_placas;
    placa.fy_placa_mpa = (opciones.acero_placas.find("355") != std::string::npos) ? 355.0 : 275.0;
    placa.fu_placa_mpa = (placa.fy_placa_mpa > 300.0) ? 510.0 : 430.0;
    placa.espesor_mortero_nivelacion_mm = opciones.espesor_mortero_nivelacion_mm;
    placa.superficie_hormigon.fck_mpa = opciones.fck_hormigon_cimentacion_mpa;

    // Buscar barra metálica o pilar incidente en el nudo
    for (const auto& b : obra.estructura().barras) {
        if (b.nudo_inicial_id == nudo_id || b.nudo_final_id == nudo_id) {
            placa.barra_pilar_id = b.id;
            placa.perfil_pilar = b.perfil;
            break;
        }
    }

    double h_mm = 200.0, b_mm = 100.0, tw_mm = 6.0, tf_mm = 9.0;
    extraer_DIMENSIONES_perfil_mm(placa.perfil_pilar, h_mm, b_mm, tw_mm, tf_mm);

    // Dimensiones iniciales redondeadas al múltiplo de 50 mm superior (`ubcalc_redondea_dimension_interna_por_exceso_005m_1_8p`)
    placa.ancho_lx_mm = std::ceil((h_mm + 160.0) / 50.0) * 50.0;
    placa.largo_ly_mm = std::ceil((b_mm + 160.0) / 50.0) * 50.0;
    placa.espesor_e_mm = std::max(16.0, std::ceil(tf_mm * 1.8 / 2.0) * 2.0);
    placa.garganta_soldadura_alas_mm = std::max(5.0, std::floor(0.7 * tf_mm));
    placa.garganta_soldadura_alma_mm = std::max(4.0, std::floor(0.7 * tw_mm));

    // 4 pernos en las esquinas por defecto (`plplacan_inicializa_pernos`)
    const double dx = 0.5 * placa.ancho_lx_mm - 45.0;
    const double dy = 0.5 * placa.largo_ly_mm - 45.0;
    const double xs[4] = {-dx,  dx,  dx, -dx};
    const double ys[4] = {-dy, -dy,  dy,  dy};
    for (int i = 0; i < 4; ++i) {
        PernoAnclaje p{};
        p.id = static_cast<std::uint32_t>(i + 1);
        p.x_local_mm = xs[i];
        p.y_local_mm = ys[i];
        p.diametro_mm = 20.0;
        p.longitud_anclaje_mm = 450.0;
        p.longitud_patilla_mm = 100.0;
        p.acero_perno = opciones.acero_pernos;
        p.fyk_mpa = 500.0;
        p.fuk_mpa = 550.0;
        p.terminacion = TipoTerminacionPerno::Patilla90Grados;
        p.montaje = TipoMontajePernoPlaca::TuercaYContratuercaNivelacion;
        const double area_m2 = 0.25 * std::numbers::pi * std::pow(p.diametro_mm * 0.001, 2);
        const double long_tot_m = (p.longitud_anclaje_mm + p.longitud_patilla_mm + 90.0) * 0.001;
        p.peso_unitario_kg = area_m2 * long_tot_m * 7850.0 + 0.18;
        placa.pernos.push_back(p);
    }

    placa.superficie_hormigon.ancho_pedestal_x_mm = std::max(600.0, placa.ancho_lx_mm + 300.0);
    placa.superficie_hormigon.largo_pedestal_y_mm = std::max(600.0, placa.largo_ly_mm + 300.0);

    construir_prismas_3d_placa(obra, placa);
    return placa;
}

std::vector<PlacaAnclaje3d> ConnectionsEngine::generar_todas_las_placas_anclaje(
    const model::Obra& obra,
    const OpcionesCalculoPlacasAnclaje& opciones
) {
    std::vector<PlacaAnclaje3d> placas;
    for (const auto& n : obra.estructura().nudos) {
        if (n.vinc_ext == model::VinculacionExterior::Libre) {
            continue;
        }
        // Verificar que incida al menos una barra metálica
        bool tiene_barra_metalica = false;
        for (const auto& b : obra.estructura().barras) {
            if ((b.nudo_inicial_id == n.id || b.nudo_final_id == n.id) &&
                b.material.find("C2") != 0 && b.material.find("C3") != 0 && b.material.find("HA") != 0) {
                tiene_barra_metalica = true;
                break;
            }
        }
        if (tiene_barra_metalica) {
            placas.push_back(generar_placa_anclaje_por_defecto(obra, n.id, opciones));
        }
    }
    return placas;
}

ResultadoComprobacionPlacaAnclaje ConnectionsEngine::comprobar_placa_anclaje(
    const model::Obra& obra,
    PlacaAnclaje3d& placa,
    const solver::SolverSummary& solver_results
) {
    ResultadoComprobacionPlacaAnclaje res{};

    // 1. Obtener reacción pésima sobre el apoyo / arranque del pilar
    double n_comp_max_kn = 50.0;
    double n_tracc_max_kn = 0.0;
    double v_horiz_max_kn = 15.0;
    double m_flex_max_knm = 20.0;

    const auto& lista_casos = (!solver_results.resultados_combinaciones.empty())
        ? solver_results.resultados_combinaciones
        : solver_results.resultados_hipotesis;

    for (const auto& c : lista_casos) {
        if (const auto* r = c.find_reaccion_nudo(placa.nudo_id)) {
            if (r->fz_kn >= 0.0) {
                n_comp_max_kn = std::max(n_comp_max_kn, r->fz_kn);
            } else {
                n_tracc_max_kn = std::max(n_tracc_max_kn, -r->fz_kn);
            }
            const double vh = std::hypot(r->fx_kn, r->fy_kn);
            const double mf = std::hypot(r->mx_knm, r->my_knm);
            v_horiz_max_kn = std::max(v_horiz_max_kn, vh);
            m_flex_max_knm = std::max(m_flex_max_knm, mf);
        }
    }

    double h_mm = 200.0, b_mm = 100.0, tw_mm = 6.0, tf_mm = 9.0;
    extraer_DIMENSIONES_perfil_mm(placa.perfil_pilar, h_mm, b_mm, tw_mm, tf_mm);

    // 2. Comprobación de compresión localizada sobre el hormigón de soporte (`f_jd = beta_j * k_j * f_cd`)
    const double fcd_mpa = placa.superficie_hormigon.fck_mpa / 1.50;
    const double ac0 = placa.ancho_lx_mm * placa.largo_ly_mm;
    const double ac1 = std::max(ac0, placa.superficie_hormigon.ancho_pedestal_x_mm * placa.superficie_hormigon.largo_pedestal_y_mm);
    const double kj = std::min(3.0, std::sqrt(ac1 / std::max(1.0, ac0)));
    const double beta_j = 0.67;
    res.resistencia_calculo_hormigon_fjd_mpa = beta_j * kj * fcd_mpa;

    const double w_placa_mm3 = (placa.largo_ly_mm * placa.ancho_lx_mm * placa.ancho_lx_mm) / 6.0;
    const double sigma_n_mpa = (n_comp_max_kn * 1000.0) / std::max(1.0, ac0);
    const double sigma_m_mpa = (m_flex_max_knm * 1.0e6) / std::max(1.0, w_placa_mm3);
    res.tension_max_hormigon_mpa = sigma_n_mpa + 0.55 * sigma_m_mpa;

    ComprobacionIndividualUnion c_horm{};
    c_horm.codigo = "PLACA_HORM_COMP";
    c_horm.titulo = "Compresión localizada sobre el hormigón de apoyo";
    c_horm.articulo_norma = "EN 1993-1-8 §6.2.5 / CE-21 Art. 61.2";
    c_horm.valor_solicitante = res.tension_max_hormigon_mpa;
    c_horm.valor_resistente = res.resistencia_calculo_hormigon_fjd_mpa;
    c_horm.unidad = "MPa";
    c_horm.aprovechamiento_pct = 100.0 * c_horm.valor_solicitante / std::max(1e-6, c_horm.valor_resistente);
    c_horm.cumple = (c_horm.aprovechamiento_pct <= 100.0);
    c_horm.expresion_justificativa = "σ_c,Ed = " + fmt_num(c_horm.valor_solicitante) + " MPa <= f_jd = " + fmt_num(c_horm.valor_resistente) + " MPa";
    res.comprobaciones.push_back(c_horm);

    // 3. Comprobación de flexión y tensión equivalente de Von Mises en los vuelos de la placa base
    const double vuelo_x_mm = std::max(10.0, 0.5 * (placa.ancho_lx_mm - h_mm));
    const double factor_rig = placa.rigidizadores.empty() ? 1.0 : 0.42;
    const double m_vuelo_nmm_mm = factor_rig * 0.5 * res.tension_max_hormigon_mpa * vuelo_x_mm * vuelo_x_mm;
    const double w_pl_unit_mm2 = (placa.espesor_e_mm * placa.espesor_e_mm) / 4.0;
    res.tension_von_mises_placa_mpa = m_vuelo_nmm_mm / std::max(1.0, w_pl_unit_mm2);
    res.limite_elastico_placa_fyd_mpa = placa.fy_placa_mpa / 1.05;

    ComprobacionIndividualUnion c_placa{};
    c_placa.codigo = "PLACA_VON_MISES";
    c_placa.titulo = "Tensión equivalente de Von Mises en vuelos de placa base";
    c_placa.articulo_norma = "EN 1993-1-8 §6.2.5 / CTE DB SE-A §8.7";
    c_placa.valor_solicitante = res.tension_von_mises_placa_mpa;
    c_placa.valor_resistente = res.limite_elastico_placa_fyd_mpa;
    c_placa.unidad = "MPa";
    c_placa.aprovechamiento_pct = 100.0 * c_placa.valor_solicitante / std::max(1e-6, c_placa.valor_resistente);
    c_placa.cumple = (c_placa.aprovechamiento_pct <= 100.0);
    c_placa.expresion_justificativa = "σ_vm,Ed = " + fmt_num(c_placa.valor_solicitante) + " MPa <= f_yd = " + fmt_num(c_placa.valor_resistente) + " MPa";
    res.comprobaciones.push_back(c_placa);

    // 4. Comprobación de pernos de anclaje (tracción, cortante, adherencia, cono de hormigón e interacción)
    const std::size_t n_pernos = std::max<std::size_t>(1, placa.pernos.size());
    const std::size_t n_pernos_tracc = std::max<std::size_t>(1, n_pernos / 2);
    const double brazo_pernos_m = std::max(0.12, (placa.ancho_lx_mm - 80.0) * 0.001);
    const double tracc_por_momento_kn = std::max(0.0, (m_flex_max_knm / brazo_pernos_m) - 0.45 * n_comp_max_kn);
    res.traccion_max_perno_kn = (n_tracc_max_kn / static_cast<double>(n_pernos)) +
                                (tracc_por_momento_kn / static_cast<double>(n_pernos_tracc));
    res.cortante_max_perno_kn = v_horiz_max_kn / static_cast<double>(n_pernos);

    const double d_perno_mm = placa.pernos.empty() ? 20.0 : placa.pernos.front().diametro_mm;
    const double l_ancl_mm = placa.pernos.empty() ? 450.0 : placa.pernos.front().longitud_anclaje_mm;
    const double fyk_perno = placa.pernos.empty() ? 500.0 : placa.pernos.front().fyk_mpa;
    const double fuk_perno = placa.pernos.empty() ? 550.0 : placa.pernos.front().fuk_mpa;
    const double as_perno_mm2 = 0.78 * (0.25 * std::numbers::pi * d_perno_mm * d_perno_mm);

    res.resistencia_traccion_acero_perno_kn = (as_perno_mm2 * (fyk_perno / 1.15)) * 0.001;
    const double fbd_mpa = 2.25 * 0.7 * 0.30 * std::pow(placa.superficie_hormigon.fck_mpa, 2.0 / 3.0) / 1.50;
    const double factor_patilla = (!placa.pernos.empty() && placa.pernos.front().terminacion != TipoTerminacionPerno::ProlongacionRecta) ? 1.40 : 1.00;
    res.resistencia_adherencia_anclaje_perno_kn = (std::numbers::pi * d_perno_mm * l_ancl_mm * fbd_mpa * factor_patilla) * 0.001;
    res.resistencia_cono_hormigon_perno_kn = (7.2 * std::sqrt(placa.superficie_hormigon.fck_mpa) * std::pow(l_ancl_mm, 1.5) / 1.50) * 0.001;
    res.resistencia_cortante_perno_kn = (0.50 * as_perno_mm2 * fuk_perno / 1.25) * 0.001;
    res.longitud_anclaje_necesaria_mm = std::max(150.0, (res.traccion_max_perno_kn * 1000.0) / std::max(1.0, std::numbers::pi * d_perno_mm * fbd_mpa * factor_patilla));

    const double n_rd_perno_kn = std::min({
        res.resistencia_traccion_acero_perno_kn,
        res.resistencia_adherencia_anclaje_perno_kn,
        res.resistencia_cono_hormigon_perno_kn
    });
    const double beta_n = res.traccion_max_perno_kn / std::max(1e-6, n_rd_perno_kn);
    const double beta_v = res.cortante_max_perno_kn / std::max(1e-6, res.resistencia_cortante_perno_kn);
    const double inter_perno_pct = 100.0 * (std::pow(beta_n, 1.5) + std::pow(beta_v, 1.5));

    ComprobacionIndividualUnion c_perno{};
    c_perno.codigo = "PERNO_ANCLAJE_ELU";
    c_perno.titulo = "Resistencia e interacción N+V en pernos de anclaje y cono de hormigón";
    c_perno.articulo_norma = "EN 1992-4 §7.2 / CE-21 Anejo 19";
    c_perno.valor_solicitante = inter_perno_pct;
    c_perno.valor_resistente = 100.0;
    c_perno.unidad = "%";
    c_perno.aprovechamiento_pct = inter_perno_pct;
    c_perno.cumple = (c_perno.aprovechamiento_pct <= 100.0);
    c_perno.expresion_justificativa =
        "N_Ed=" + fmt_num(res.traccion_max_perno_kn) + " kN (N_Rd=" + fmt_num(n_rd_perno_kn) +
        " kN), V_Ed=" + fmt_num(res.cortante_max_perno_kn) + " kN (V_Rd=" + fmt_num(res.resistencia_cortante_perno_kn) + " kN)";
    res.comprobaciones.push_back(c_perno);

    // 5. Medición de pesos de acero (`plplacan_peso_acero_placa_descompuesto`, `plplacan_peso_total_pernos_para_medicion`)
    res.peso_placa_base_kg = (placa.ancho_lx_mm * placa.largo_ly_mm * placa.espesor_e_mm * 1.0e-9) * 7850.0;
    res.peso_rigidizadores_kg = 0.0;
    for (const auto& rig : placa.rigidizadores) {
        res.peso_rigidizadores_kg += (rig.longitud_base_mm * rig.altura_mm * rig.espesor_mm * 1.0e-9) * 7850.0 * 0.75;
    }
    res.peso_total_pernos_kg = 0.0;
    for (const auto& p : placa.pernos) {
        res.peso_total_pernos_kg += p.peso_unitario_kg;
    }
    res.peso_total_conjunto_kg = res.peso_placa_base_kg + res.peso_rigidizadores_kg + res.peso_total_pernos_kg;

    // 6. Estado global
    res.cumple_global = true;
    res.aprovechamiento_max_pct = 0.0;
    for (const auto& chk : res.comprobaciones) {
        if (!chk.cumple) {
            res.cumple_global = false;
        }
        if (chk.aprovechamiento_pct >= res.aprovechamiento_max_pct) {
            res.aprovechamiento_max_pct = chk.aprovechamiento_pct;
            res.comprobacion_critica = chk.titulo;
        }
    }

    res.texto_cuadro_arranques =
        "Arranque N" + std::to_string(placa.nudo_id) + " (" + placa.perfil_pilar + "): Placa " +
        fmt_num(placa.ancho_lx_mm, 0) + "x" + fmt_num(placa.largo_ly_mm, 0) + "x" + fmt_num(placa.espesor_e_mm, 0) +
        " mm (" + placa.acero_placa + ") + " + std::to_string(placa.pernos.size()) + " pernos Ø" +
        fmt_num(d_perno_mm, 0) + " L=" + fmt_num(l_ancl_mm, 0) + " mm";

    std::ostringstream mem;
    mem << "=== MEMORIA DE CÁLCULO DE PLACA DE ANCLAJE (Nudo " << placa.nudo_id << ") ===\n";
    mem << "- Perfil: " << placa.perfil_pilar << " | Placa: " << placa.ancho_lx_mm << "x" << placa.largo_ly_mm
        << "x" << placa.espesor_e_mm << " mm (" << placa.acero_placa << ")\n";
    mem << "- Pernos: " << placa.pernos.size() << " Ø" << d_perno_mm << " mm, L_ancl = " << l_ancl_mm
        << " mm (L_nec = " << fmt_num(res.longitud_anclaje_necesaria_mm, 0) << " mm)\n";
    for (const auto& chk : res.comprobaciones) {
        mem << "  * [" << (chk.cumple ? "CUMPLE" : "NO CUMPLE") << "] " << chk.titulo << " (" << chk.articulo_norma
            << "): η = " << fmt_num(chk.aprovechamiento_pct, 1) << "% — " << chk.expresion_justificativa << "\n";
    }
    mem << "- Medición total acero: " << fmt_num(res.peso_total_conjunto_kg, 2) << " kg\n";
    res.memoria_calculo_detallada = mem.str();

    placa.ultima_comprobacion = res;
    construir_prismas_3d_placa(obra, placa);
    return res;
}

ResultadoComprobacionPlacaAnclaje ConnectionsEngine::dimensionar_placa_anclaje(
    const model::Obra& obra,
    PlacaAnclaje3d& placa,
    const solver::SolverSummary& solver_results
) {
    if (placa.bloqueada) {
        return comprobar_placa_anclaje(obra, placa, solver_results);
    }

    for (int iter = 0; iter < 12; ++iter) {
        const auto res = comprobar_placa_anclaje(obra, placa, solver_results);
        if (res.cumple_global) {
            return res;
        }
        // Incrementar dimensiones, espesor, pernos o rigidizadores según la comprobación crítica
        if (!res.comprobaciones.empty() && !res.comprobaciones[0].cumple) {
            placa.ancho_lx_mm += 50.0;
            placa.largo_ly_mm += 50.0;
        }
        if (res.comprobaciones.size() > 1 && !res.comprobaciones[1].cumple) {
            if (placa.espesor_e_mm < 30.0) {
                placa.espesor_e_mm += 4.0;
            } else if (placa.rigidizadores.empty()) {
                RigidizadorPlacaAnclaje r1{};
                r1.id = 1;
                r1.codigo = "R1";
                r1.espesor_mm = 12.0;
                r1.altura_mm = 160.0;
                r1.longitud_base_mm = 120.0;
                placa.rigidizadores.push_back(r1);
                RigidizadorPlacaAnclaje r2 = r1;
                r2.id = 2;
                r2.codigo = "R2";
                placa.rigidizadores.push_back(r2);
            } else {
                placa.espesor_e_mm += 4.0;
            }
        }
        if (res.comprobaciones.size() > 2 && !res.comprobaciones[2].cumple) {
            for (auto& p : placa.pernos) {
                if (p.diametro_mm < 32.0) {
                    p.diametro_mm += 4.0;
                }
                p.longitud_anclaje_mm += 100.0;
            }
        }
    }
    return comprobar_placa_anclaje(obra, placa, solver_results);
}

void ConnectionsEngine::igualar_placas_anclaje(
    const PlacaAnclaje3d& origen,
    std::vector<PlacaAnclaje3d>& destino_placas,
    const std::vector<std::uint32_t>& nudos_destino
) {
    for (auto& pl : destino_placas) {
        if (pl.bloqueada) {
            continue;
        }
        if (nudos_destino.empty() ||
            std::find(nudos_destino.begin(), nudos_destino.end(), pl.nudo_id) != nudos_destino.end()) {
            const auto id_keep = pl.id;
            const auto nudo_keep = pl.nudo_id;
            const auto barra_keep = pl.barra_pilar_id;
            const auto perf_keep = pl.perfil_pilar;
            pl = origen;
            pl.id = id_keep;
            pl.nudo_id = nudo_keep;
            pl.barra_pilar_id = barra_keep;
            pl.perfil_pilar = perf_keep;
        }
    }
}

// ==============================================================================
// 3. Subsistema de Uniones Soldadas y Atornilladas (`uniones.dll`, `unionbase.dll`)
// ==============================================================================

std::vector<UnionEstructural3d> ConnectionsEngine::detectar_y_generar_uniones_obra(
    const model::Obra& obra,
    const solver::SolverSummary& solver_results,
    bool preferir_atornilladas,
    ClaseAceroTornillo clase_tornillos,
    bool tornillos_pretensados
) {
    std::vector<UnionEstructural3d> uniones;
    std::uint32_t next_id = 1;

    for (const auto& nudo : obra.estructura().nudos) {
        std::vector<const model::Barra3d*> inc;
        for (const auto& b : obra.estructura().barras) {
            if ((b.nudo_inicial_id == nudo.id || b.nudo_final_id == nudo.id) &&
                b.material.find("C2") != 0 && b.material.find("C3") != 0 && b.material.find("HA") != 0) {
                inc.push_back(&b);
            }
        }
        if (inc.empty()) {
            continue;
        }

        // Si es nudo de apoyo con una sola barra vertical, es placa de anclaje
        if (nudo.vinc_ext != model::VinculacionExterior::Libre && inc.size() == 1) {
            UnionEstructural3d u{};
            u.id = next_id++;
            u.referencia = "U" + std::to_string(u.id);
            u.nudo_id = nudo.id;
            u.tipologia = TipologiaUnionEstructural::PlacaAnclajeBasePilar;
            u.es_atornillada = true;
            u.barras_concurrentes_ids.push_back(inc.front()->id);
            dimensionar_union_estructural(obra, u, solver_results);
            uniones.push_back(std::move(u));
            continue;
        }

        if (inc.size() < 2) {
            continue;
        }

        UnionEstructural3d u{};
        u.id = next_id++;
        u.referencia = "U" + std::to_string(u.id);
        u.nudo_id = nudo.id;
        u.es_atornillada = preferir_atornilladas;
        for (const auto* b : inc) {
            u.barras_concurrentes_ids.push_back(b->id);
        }

        // Clasificar tipología según barras concurrentes (`unclunioned_detecta_todas_las_uniones_en_nudos`)
        bool hay_tubular = false;
        bool hay_tirante = false;
        bool hay_vertical = false;
        bool hay_inclinada = false;
        for (const auto* b : inc) {
            if (b->perfil.find("CHS") != std::string::npos ||
                b->perfil.find("RHS") != std::string::npos ||
                b->perfil.find("SHS") != std::string::npos) {
                hay_tubular = true;
            }
            if (b->es_tirante) {
                hay_tirante = true;
            }
            const auto* ni = obra.find_nudo(b->nudo_inicial_id);
            const auto* nf = obra.find_nudo(b->nudo_final_id);
            if (ni && nf) {
                const core::Vec3d dir = (nf->pos - ni->pos).normalized();
                if (std::abs(dir.z) > 0.85) {
                    hay_vertical = true;
                } else if (std::abs(dir.z) > 0.05) {
                    hay_inclinada = true;
                }
            }
        }

        if (hay_tubular && inc.size() >= 2) {
            u.tipologia = TipologiaUnionEstructural::NudoCelosiaTubular_CHS_RHS;
            u.es_atornillada = false;
        } else if (hay_tirante && inc.size() == 2) {
            u.tipologia = TipologiaUnionEstructural::AnclajeTiranteConCartela;
        } else if (hay_vertical) {
            u.tipologia = preferir_atornilladas
                ? TipologiaUnionEstructural::PilarVigaChapaFrontalAtornillada
                : TipologiaUnionEstructural::PilarVigaDintelSoldada;
            u.lleva_cartela_inferior = hay_inclinada;
            u.longitud_cartela_mm = hay_inclinada ? 600.0 : 0.0;
            u.canto_cartela_mm = hay_inclinada ? 220.0 : 0.0;
        } else if (inc.size() == 2 && hay_inclinada) {
            u.tipologia = preferir_atornilladas
                ? TipologiaUnionEstructural::CumbreraNaveChapaFrontalAtornillada
                : TipologiaUnionEstructural::CumbreraNaveSoldada;
            u.lleva_cartela_inferior = true;
            u.longitud_cartela_mm = 500.0;
            u.canto_cartela_mm = 200.0;
        } else if (inc.size() == 2) {
            u.tipologia = preferir_atornilladas
                ? TipologiaUnionEstructural::EmpalmePiezasChapaFrontalAtornillada
                : TipologiaUnionEstructural::EmpalmePiezasCubrejuntasAtornillada;
        } else {
            u.tipologia = TipologiaUnionEstructural::VigaSecundariaVigaPrincipalDobleAngular;
        }

        // Configurar grupo de tornillos inicial si es unión atornillada
        if (u.es_atornillada) {
            GrupoTornillosUnion gt{};
            gt.id = 1;
            gt.descripcion = "Tornillería principal de unión " + u.referencia;
            gt.diametro_designacion = "M20";
            gt.clase = clase_tornillos;
            gt.pretensados = tornillos_pretensados;
            gt.serie = tornillos_pretensados ? SerieTornilleria::EN_14399_HV_Pretensable : SerieTornilleria::ISO_4014_4017_Ordinario;
            gt.num_filas = u.lleva_cartela_inferior ? 4 : 3;
            gt.num_columnas = 2;
            u.grupos_tornillos.push_back(gt);
        }

        dimensionar_union_estructural(obra, u, solver_results);
        uniones.push_back(std::move(u));
    }

    agrupar_uniones_equivalentes(uniones);
    return uniones;
}

void ConnectionsEngine::comprobar_union_estructural(
    const model::Obra& obra,
    UnionEstructural3d& union_est,
    const solver::SolverSummary& solver_results
) {
    union_est.comprobaciones.clear();
    union_est.rigideces_rotacionales.clear();
    union_est.prismas_3d.clear();

    const auto* nudo = obra.find_nudo(union_est.nudo_id);
    const core::Vec3d p_nudo = nudo ? nudo->pos : core::Vec3d{0.0, 0.0, 0.0};

    // Obtener esfuerzos pésimos de las barras conectadas al nudo
    double m_ed_max_knm = 25.0;
    double v_ed_max_kn = 35.0;
    double n_ed_max_kn = 40.0;
    std::string ref_perfil_viga = "IPE 240";
    std::string ref_perfil_pilar = "HEB 200";
    std::uint32_t barra_viga_id = union_est.barras_concurrentes_ids.empty() ? 1 : union_est.barras_concurrentes_ids.back();
    bool viga_en_ini = true;

    const auto& lista_casos = (!solver_results.resultados_combinaciones.empty())
        ? solver_results.resultados_combinaciones
        : solver_results.resultados_hipotesis;

    for (const auto bid : union_est.barras_concurrentes_ids) {
        const auto* b = obra.find_barra(bid);
        if (!b) {
            continue;
        }
        const auto* ni = obra.find_nudo(b->nudo_inicial_id);
        const auto* nf = obra.find_nudo(b->nudo_final_id);
        if (ni && nf) {
            const core::Vec3d dir = (nf->pos - ni->pos).normalized();
            if (std::abs(dir.z) > 0.85) {
                ref_perfil_pilar = b->perfil;
            } else {
                ref_perfil_viga = b->perfil;
                barra_viga_id = b->id;
                viga_en_ini = (b->nudo_inicial_id == union_est.nudo_id);
            }
        }
        for (const auto& c : lista_casos) {
            if (const auto* eb = c.find_esfuerzos_barra(bid)) {
                if (!eb->puntos.empty()) {
                    const auto& pt = (b->nudo_inicial_id == union_est.nudo_id) ? eb->puntos.front() : eb->puntos.back();
                    m_ed_max_knm = std::max(m_ed_max_knm, std::hypot(pt.my_knm, pt.mz_knm));
                    v_ed_max_kn = std::max(v_ed_max_kn, std::hypot(pt.vy_kn, pt.vz_kn));
                    n_ed_max_kn = std::max(n_ed_max_kn, std::abs(pt.n_kn));
                }
            }
        }
    }

    double h_viga = 240.0, b_viga = 120.0, tw_viga = 6.2, tf_viga = 9.8;
    double h_pilar = 200.0, b_pilar = 200.0, tw_pilar = 9.0, tf_pilar = 15.0;
    extraer_DIMENSIONES_perfil_mm(ref_perfil_viga, h_viga, b_viga, tw_viga, tf_viga);
    extraer_DIMENSIONES_perfil_mm(ref_perfil_pilar, h_pilar, b_pilar, tw_pilar, tf_pilar);

    // Asegurar chapas y soldaduras iniciales si están vacías
    if (union_est.chapas_y_rigidizadores.empty()) {
        ChapaRigidizadorUnion ch{};
        ch.id = 1;
        ch.nombre = "Chapa frontal " + union_est.referencia;
        ch.ancho_mm = std::max(b_viga + 40.0, 160.0);
        ch.alto_mm = h_viga + (union_est.lleva_cartela_inferior ? union_est.canto_cartela_mm : 60.0);
        ch.espesor_mm = 16.0;
        ch.acero = "S275";
        ch.fy_mpa = 275.0;
        ch.peso_kg = (ch.ancho_mm * ch.alto_mm * ch.espesor_mm * 1.0e-9) * 7850.0;
        union_est.chapas_y_rigidizadores.push_back(ch);

        if (union_est.tipologia == TipologiaUnionEstructural::PilarVigaChapaFrontalAtornillada ||
            union_est.tipologia == TipologiaUnionEstructural::PilarVigaDintelSoldada) {
            ChapaRigidizadorUnion rig{};
            rig.id = 2;
            rig.nombre = "Pareja de rigidizadores horizontales alma de pilar";
            rig.ancho_mm = 0.5 * (b_pilar - tw_pilar);
            rig.alto_mm = h_pilar - 2.0 * tf_pilar;
            rig.espesor_mm = std::max(10.0, tf_viga);
            rig.acero = "S275";
            rig.fy_mpa = 275.0;
            rig.peso_kg = 4.0 * (rig.ancho_mm * rig.alto_mm * rig.espesor_mm * 1.0e-9) * 7850.0;
            union_est.chapas_y_rigidizadores.push_back(rig);
        }
    }

    if (union_est.soldaduras.empty()) {
        CordonSoldaduraUnion s1{};
        s1.id = 1;
        s1.descripcion = "Soldadura en ángulo alas de viga (" + ref_perfil_viga + ")";
        s1.garganta_efectiva_a_mm = std::max(5.0, std::floor(0.7 * tf_viga));
        s1.cateto_s_mm = s1.garganta_efectiva_a_mm * 1.414;
        s1.longitud_efectiva_mm = 2.0 * b_viga;
        union_est.soldaduras.push_back(s1);

        CordonSoldaduraUnion s2{};
        s2.id = 2;
        s2.descripcion = "Soldadura doble en ángulo alma de viga (" + ref_perfil_viga + ")";
        s2.garganta_efectiva_a_mm = std::max(4.0, std::floor(0.7 * tw_viga));
        s2.cateto_s_mm = s2.garganta_efectiva_a_mm * 1.414;
        s2.longitud_efectiva_mm = 2.0 * (h_viga - 2.0 * tf_viga);
        union_est.soldaduras.push_back(s2);
    }

    const double brazo_z_mm = std::max(120.0, (h_viga - tf_viga) + (union_est.lleva_cartela_inferior ? 0.85 * union_est.canto_cartela_mm : 0.0));
    const double f_tracc_ala_kn = (m_ed_max_knm * 1000.0) / brazo_z_mm + 0.5 * n_ed_max_kn;

    // 1. Comprobación de soldaduras (`ubgsnorm_articulo_comprobacion_soldaduras_angulo`, `ubgsaws_*`)
    for (auto& s : union_est.soldaduras) {
        const double f_vw_d_mpa = (s.fu_metal_base_mpa / std::sqrt(3.0)) / (s.coef_correlacion_beta_w * 1.25);
        const double area_garganta_mm2 = std::max(10.0, s.garganta_efectiva_a_mm * s.longitud_efectiva_mm);
        const double f_sol_n = ((s.id == 1) ? f_tracc_ala_kn : v_ed_max_kn) * 1000.0;
        s.tension_equivalente_ed_mpa = f_sol_n / area_garganta_mm2;
        s.tension_resistente_rd_mpa = f_vw_d_mpa;
        s.aprovechamiento_pct = 100.0 * s.tension_equivalente_ed_mpa / std::max(1e-6, s.tension_resistente_rd_mpa);
        s.cumple = (s.aprovechamiento_pct <= 100.0);

        ComprobacionIndividualUnion cs{};
        cs.codigo = "SOLD_" + std::to_string(s.id);
        cs.titulo = s.descripcion;
        cs.articulo_norma = "EN 1993-1-8 §4.5.3 / CTE DB SE-A §8.6";
        cs.valor_solicitante = s.tension_equivalente_ed_mpa;
        cs.valor_resistente = s.tension_resistente_rd_mpa;
        cs.unidad = "MPa";
        cs.aprovechamiento_pct = s.aprovechamiento_pct;
        cs.cumple = s.cumple;
        cs.expresion_justificativa = "a=" + fmt_num(s.garganta_efectiva_a_mm, 1) + " mm, L_ef=" +
                                     fmt_num(s.longitud_efectiva_mm, 0) + " mm -> τ_w,Ed=" +
                                     fmt_num(s.tension_equivalente_ed_mpa) + " MPa <= f_vw,d=" + fmt_num(s.tension_resistente_rd_mpa) + " MPa";
        union_est.comprobaciones.push_back(cs);
    }

    // 2. Comprobación de grupos de tornillos (`ubgsnorm_articulos_comprobaciones_tornillos`, T-stub y aplastamiento)
    for (auto& gt : union_est.grupos_tornillos) {
        double d_nom = 20.0;
        if (gt.diametro_designacion.size() > 1) {
            try {
                d_nom = std::stod(gt.diametro_designacion.substr(1));
            } catch (...) {
                d_nom = 20.0;
            }
        }
        const auto dt = obtener_datos_tornillo(d_nom);
        double fyb = 640.0, fub = 800.0;
        propiedades_mecanicas_clase_tornillo(gt.clase, fyb, fub);

        const std::uint32_t n_tot_torn = std::max<std::uint32_t>(2, gt.num_filas * gt.num_columnas);
        const std::uint32_t n_torn_tracc = std::max<std::uint32_t>(2, (gt.num_filas - 1) * gt.num_columnas);

        gt.traccion_max_tornillo_kn = f_tracc_ala_kn / static_cast<double>(n_torn_tracc);
        gt.cortante_max_tornillo_kn = v_ed_max_kn / static_cast<double>(n_tot_torn);

        gt.resistencia_traccion_ft_rd_kn = (0.90 * fub * dt.area_resistente_traccion_mm2 / 1.25) * 0.001;
        gt.resistencia_cortante_fv_rd_kn = (0.60 * fub * dt.area_resistente_traccion_mm2 / 1.25) * 0.001;

        const double t_chapa_mm = union_est.chapas_y_rigidizadores.empty() ? 16.0 : union_est.chapas_y_rigidizadores.front().espesor_mm;
        const double fu_chapa_mpa = 430.0;
        gt.resistencia_aplastamiento_fb_rd_kn = (2.5 * 0.8 * fu_chapa_mpa * dt.diametro_nominal_mm * t_chapa_mm / 1.25) * 0.001;
        const double dm_mm = 0.5 * (dt.distancia_entre_caras_cabeza_mm + dt.distancia_entre_aristas_cabeza_mm);
        gt.resistencia_punzonamiento_bp_rd_kn = (0.60 * std::numbers::pi * dm_mm * t_chapa_mm * fu_chapa_mpa / 1.25) * 0.001;

        const double f_t_eff_rd = std::min(gt.resistencia_traccion_ft_rd_kn, gt.resistencia_punzonamiento_bp_rd_kn);
        const double f_v_eff_rd = std::min(gt.resistencia_cortante_fv_rd_kn, gt.resistencia_aplastamiento_fb_rd_kn);

        const double ratio_v = gt.cortante_max_tornillo_kn / std::max(1e-6, f_v_eff_rd);
        const double ratio_t = gt.traccion_max_tornillo_kn / std::max(1e-6, f_t_eff_rd);
        const double ratio_comb = ratio_v + (gt.traccion_max_tornillo_kn / std::max(1e-6, 1.40 * f_t_eff_rd));

        gt.aprovechamiento_max_pct = 100.0 * std::max({ratio_v, ratio_t, ratio_comb});
        gt.cumple = (gt.aprovechamiento_max_pct <= 100.0);

        ComprobacionIndividualUnion ct{};
        ct.codigo = "TORN_GRUPO_" + std::to_string(gt.id);
        ct.titulo = gt.descripcion + " (" + std::to_string(gt.num_filas) + "x" + std::to_string(gt.num_columnas) + " " + gt.diametro_designacion + ")";
        ct.articulo_norma = "EN 1993-1-8 Tabla 3.4 / CTE DB SE-A §8.5";
        ct.valor_solicitante = gt.aprovechamiento_max_pct;
        ct.valor_resistente = 100.0;
        ct.unidad = "%";
        ct.aprovechamiento_pct = gt.aprovechamiento_max_pct;
        ct.cumple = gt.cumple;
        ct.expresion_justificativa =
            "F_t,Ed=" + fmt_num(gt.traccion_max_tornillo_kn) + " kN (F_t,Rd=" + fmt_num(f_t_eff_rd) +
            " kN), F_v,Ed=" + fmt_num(gt.cortante_max_tornillo_kn) + " kN (F_v,Rd=" + fmt_num(f_v_eff_rd) + " kN)";
        union_est.comprobaciones.push_back(ct);
    }

    // 3. Si es unión tubular CHS/RHS (`ubgsnorm_chs_*`, `ublrfd360_rhs_*`)
    if (union_est.tipologia == TipologiaUnionEstructural::NudoCelosiaTubular_CHS_RHS) {
        const double fy_cordon = 275.0;
        const double t0_mm = std::max(5.0, tw_pilar);
        const double b0_mm = std::max(100.0, b_pilar);
        const double b1_mm = std::max(60.0, b_viga);
        const double beta = std::clamp(b1_mm / b0_mm, 0.25, 0.85);
        const double n_rd_plastif_kn = (fy_cordon * t0_mm * t0_mm * (2.0 / (1.0 - beta) + 4.0 / std::sqrt(1.0 - beta))) * 0.001;

        ComprobacionIndividualUnion c_tub{};
        c_tub.codigo = "TUBULAR_CHS_RHS";
        c_tub.titulo = "Plastificación de la cara del cordón y punzonamiento en nudo tubular CHS/RHS";
        c_tub.articulo_norma = "EN 1993-1-8 §7.4–7.5 / CIDECT / AISC 360 Cap. K";
        c_tub.valor_solicitante = n_ed_max_kn;
        c_tub.valor_resistente = std::max(80.0, n_rd_plastif_kn);
        c_tub.unidad = "kN";
        c_tub.aprovechamiento_pct = 100.0 * c_tub.valor_solicitante / c_tub.valor_resistente;
        c_tub.cumple = (c_tub.aprovechamiento_pct <= 100.0);
        c_tub.expresion_justificativa = "β = " + fmt_num(beta, 2) + " -> N_1,Ed = " + fmt_num(c_tub.valor_solicitante) +
                                         " kN <= N_1,Rd = " + fmt_num(c_tub.valor_resistente) + " kN";
        union_est.comprobaciones.push_back(c_tub);
    }

    // 4. Análisis de rigidez rotacional por el método de las componentes (`undtrigunion_*`, EN 1993-1-8 §6.3)
    AnalisisRigidezRotacionalUnion rig{};
    rig.barra_conectada_id = barra_viga_id;
    rig.en_extremo_inicial = viga_en_ini;
    rig.brazo_palanca_equivalente_z_mm = brazo_z_mm;
    rig.k1_alma_pilar_cortante_mm = (0.38 * (h_pilar * tw_pilar)) / std::max(10.0, brazo_z_mm);
    rig.k2_alma_pilar_compresion_mm = 0.7 * 180.0 * tw_pilar / std::max(10.0, h_pilar - 2.0 * tf_pilar);
    rig.k3_alma_pilar_traccion_mm = rig.k2_alma_pilar_compresion_mm;
    rig.k4_ala_pilar_flexion_mm = 0.9 * 140.0 * std::pow(tf_pilar, 3) / std::pow(40.0, 3);
    const double t_ep = union_est.chapas_y_rigidizadores.empty() ? 16.0 : union_est.chapas_y_rigidizadores.front().espesor_mm;
    rig.k5_chapa_frontal_flexion_mm = 0.9 * 150.0 * std::pow(t_ep, 3) / std::pow(42.0, 3);
    rig.k10_tornillos_traccion_mm = 1.6 * 245.0 / 45.0;

    const double sum_inv_k = (1.0 / std::max(0.5, rig.k1_alma_pilar_cortante_mm)) +
                             (1.0 / std::max(0.5, rig.k4_ala_pilar_flexion_mm)) +
                             (1.0 / std::max(0.5, rig.k5_chapa_frontal_flexion_mm)) +
                             (1.0 / std::max(0.5, rig.k10_tornillos_traccion_mm));
    const double e_acero_mpa = 210000.0;
    // Sj,ini en kN·m/rad: E [N/mm^2] * z^2 [mm^2] / sum(1/k_i [1/mm]) * 1e-6 [kN·m / N·mm]
    rig.rigidez_inicial_sj_ini_knm_rad = (e_acero_mpa * brazo_z_mm * brazo_z_mm / sum_inv_k) * 1.0e-6;
    rig.coef_modificacion_rigidez_eta = union_est.es_atornillada ? 2.0 : 1.5;
    rig.rigidez_secante_propuesta_sj_knm_rad = rig.rigidez_inicial_sj_ini_knm_rad / rig.coef_modificacion_rigidez_eta;
    rig.momento_resistente_mj_rd_knm = std::max(m_ed_max_knm * 1.15, (f_tracc_ala_kn * 1.25) * brazo_z_mm * 0.001);
    rig.capacidad_rotacion_phi_cd_mrad = 32.0;

    if (const auto* bv = obra.find_barra(barra_viga_id)) {
        rig.rigidez_asignada_en_modelo_knm_rad = viga_en_ini
            ? bv->empotramiento.k_rot_xz_ini_knm_rad
            : bv->empotramiento.k_rot_xz_fin_knm_rad;
    }

    // Clasificación según EN 1993-1-8 §5.2.2.5 (EI_b / L_b)
    const double ei_l_knm = 12000.0 / 6.0;
    if (rig.rigidez_inicial_sj_ini_knm_rad >= 8.0 * ei_l_knm) {
        rig.clasificacion = ClasificacionRigidezUnion::Rigida;
    } else if (rig.rigidez_inicial_sj_ini_knm_rad <= 0.5 * ei_l_knm) {
        rig.clasificacion = ClasificacionRigidezUnion::NominalmenteArticulada;
    } else {
        rig.clasificacion = ClasificacionRigidezUnion::Semirrigida;
    }

    if (rig.rigidez_asignada_en_modelo_knm_rad > 1.0) {
        rig.diferencia_relativa_rigidez_pct = 100.0 * std::abs(rig.rigidez_secante_propuesta_sj_knm_rad - rig.rigidez_asignada_en_modelo_knm_rad) /
                                              rig.rigidez_secante_propuesta_sj_knm_rad;
        rig.diferencia_rigidez_tolerable = (rig.diferencia_relativa_rigidez_pct <= 20.0);
    } else {
        rig.diferencia_relativa_rigidez_pct = 0.0;
        rig.diferencia_rigidez_tolerable = true;
    }

    // Curva Momento-Rotación trilineal EN 1993-1-8 Fig. 6.1
    const double m_el = (2.0 / 3.0) * rig.momento_resistente_mj_rd_knm;
    const double phi_el_mrad = 1000.0 * (m_el / std::max(1.0, rig.rigidez_inicial_sj_ini_knm_rad));
    const double phi_u_mrad = 1000.0 * (rig.momento_resistente_mj_rd_knm / std::max(1.0, rig.rigidez_secante_propuesta_sj_knm_rad));
    rig.curva_rotacion_mrad_momento_knm = {
        {0.0, 0.0},
        {phi_el_mrad, m_el},
        {phi_u_mrad, rig.momento_resistente_mj_rd_knm},
        {rig.capacidad_rotacion_phi_cd_mrad, rig.momento_resistente_mj_rd_knm}
    };
    union_est.rigideces_rotacionales.push_back(rig);

    // 5. Prismas 3D y plano de detalle 2D
    for (const auto& ch : union_est.chapas_y_rigidizadores) {
        Prisma3dElementoUnion pr{};
        pr.nombre = ch.nombre;
        pr.centro_global_m = p_nudo;
        pr.semidimensiones_locales_m = {
            0.5 * ch.espesor_mm * 0.001,
            0.5 * ch.ancho_mm * 0.001,
            0.5 * ch.alto_mm * 0.001
        };
        pr.color_rgb = 0x38BDF8;
        union_est.prismas_3d.push_back(pr);
    }

    union_est.plano_detalle.titulo_lamina = "DETALLE CONSTRUCTIVO UNIÓN " + union_est.referencia + " (Nudo N" + std::to_string(union_est.nudo_id) + ")";
    union_est.plano_detalle.escala = "1:10";
    union_est.plano_detalle.vistas_incluidas = {"Alzado frontal", "Sección A-A", "Planta superior", "Vista isométrica 3D del despiece"};
    union_est.plano_detalle.cotas_y_simbolos_soldadura = {
        "Soldadura en ángulo alas a=" + fmt_num(union_est.soldaduras.front().garganta_efectiva_a_mm, 0) + " mm",
        "Chapa frontal e=" + fmt_num(t_ep, 0) + " mm (" + (union_est.chapas_y_rigidizadores.empty() ? "S275" : union_est.chapas_y_rigidizadores.front().acero) + ")"
    };

    // 6. Totales y estado de cumplimiento
    union_est.peso_total_acero_chapas_kg = 0.0;
    for (const auto& ch : union_est.chapas_y_rigidizadores) {
        union_est.peso_total_acero_chapas_kg += ch.peso_kg;
    }
    union_est.peso_total_tornilleria_kg = 0.0;
    for (const auto& gt : union_est.grupos_tornillos) {
        union_est.peso_total_tornilleria_kg += static_cast<double>(gt.num_filas * gt.num_columnas) * 0.36;
    }
    union_est.longitud_total_soldadura_m = 0.0;
    for (const auto& s : union_est.soldaduras) {
        union_est.longitud_total_soldadura_m += s.longitud_efectiva_mm * 0.001;
    }

    union_est.cumple_global = true;
    union_est.aprovechamiento_max_pct = 0.0;
    for (const auto& chk : union_est.comprobaciones) {
        if (!chk.cumple) {
            union_est.cumple_global = false;
        }
        union_est.aprovechamiento_max_pct = std::max(union_est.aprovechamiento_max_pct, chk.aprovechamiento_pct);
    }

    std::ostringstream mem;
    mem << "=== MEMORIA DE CÁLCULO DE UNIÓN " << union_est.referencia << " (Nudo N" << union_est.nudo_id << ") ===\n";
    mem << "- Rigidez rotacional inicial Sj,ini = " << fmt_num(rig.rigidez_inicial_sj_ini_knm_rad, 0)
        << " kN·m/rad | Rigidez secante Sj = " << fmt_num(rig.rigidez_secante_propuesta_sj_knm_rad, 0)
        << " kN·m/rad | Mj,Rd = " << fmt_num(rig.momento_resistente_mj_rd_knm, 1) << " kN·m\n";
    for (const auto& chk : union_est.comprobaciones) {
        mem << "  * [" << (chk.cumple ? "CUMPLE" : "NO CUMPLE") << "] " << chk.titulo
            << " (" << chk.articulo_norma << "): η = " << fmt_num(chk.aprovechamiento_pct, 1) << "% — " << chk.expresion_justificativa << "\n";
    }
    union_est.memoria_calculo_union = mem.str();
}

void ConnectionsEngine::dimensionar_union_estructural(
    const model::Obra& obra,
    UnionEstructural3d& union_est,
    const solver::SolverSummary& solver_results
) {
    if (union_est.bloqueada) {
        comprobar_union_estructural(obra, union_est, solver_results);
        return;
    }

    for (int iter = 0; iter < 10; ++iter) {
        comprobar_union_estructural(obra, union_est, solver_results);
        if (union_est.cumple_global) {
            return;
        }
        for (auto& s : union_est.soldaduras) {
            if (!s.cumple) {
                s.garganta_efectiva_a_mm += 1.5;
                s.cateto_s_mm = s.garganta_efectiva_a_mm * 1.414;
                s.longitud_efectiva_mm *= 1.15;
            }
        }
        for (auto& gt : union_est.grupos_tornillos) {
            if (!gt.cumple) {
                if (gt.diametro_designacion == "M16") {
                    gt.diametro_designacion = "M20";
                } else if (gt.diametro_designacion == "M20") {
                    gt.diametro_designacion = "M24";
                } else if (gt.diametro_designacion == "M24") {
                    gt.diametro_designacion = "M27";
                } else {
                    gt.num_filas += 1;
                }
            }
        }
        for (auto& ch : union_est.chapas_y_rigidizadores) {
            if (ch.espesor_mm < 30.0) {
                ch.espesor_mm += 4.0;
                ch.peso_kg = (ch.ancho_mm * ch.alto_mm * ch.espesor_mm * 1.0e-9) * 7850.0;
            }
        }
    }
}

std::size_t ConnectionsEngine::agrupar_uniones_equivalentes(std::vector<UnionEstructural3d>& uniones) {
    std::uint32_t next_group = 1;
    for (auto& u : uniones) {
        u.grupo_union_id = 0;
    }
    for (std::size_t i = 0; i < uniones.size(); ++i) {
        if (uniones[i].grupo_union_id != 0) {
            continue;
        }
        uniones[i].grupo_union_id = next_group;
        uniones[i].referencia = "U" + std::to_string(next_group);
        for (std::size_t j = i + 1; j < uniones.size(); ++j) {
            if (uniones[j].grupo_union_id == 0 &&
                uniones[j].tipologia == uniones[i].tipologia &&
                uniones[j].es_atornillada == uniones[i].es_atornillada &&
                uniones[j].barras_concurrentes_ids.size() == uniones[i].barras_concurrentes_ids.size()) {
                uniones[j].grupo_union_id = next_group;
                uniones[j].referencia = uniones[i].referencia;
            }
        }
        ++next_group;
    }
    return (next_group > 1) ? static_cast<std::size_t>(next_group - 1) : 0;
}

std::size_t ConnectionsEngine::aplicar_rigideces_rotacionales_propuestas_a_obra(
    model::Obra& obra,
    const std::vector<UnionEstructural3d>& uniones
) {
    std::size_t actualizadas = 0;
    for (const auto& u : uniones) {
        for (const auto& rig : u.rigideces_rotacionales) {
            if (auto* b = obra.find_barra_mut(rig.barra_conectada_id)) {
                if (rig.en_extremo_inicial) {
                    b->empotramiento.usar_rigidez_rotacional_ini = true;
                    b->empotramiento.k_rot_xz_ini_knm_rad = rig.rigidez_secante_propuesta_sj_knm_rad;
                } else {
                    b->empotramiento.usar_rigidez_rotacional_fin = true;
                    b->empotramiento.k_rot_xz_fin_knm_rad = rig.rigidez_secante_propuesta_sj_knm_rad;
                }
                ++actualizadas;
            }
        }
    }
    return actualizadas;
}

std::string ConnectionsEngine::exportar_bimjsunion_json(
    const model::Obra& obra,
    const std::vector<UnionEstructural3d>& uniones,
    const std::vector<PlacaAnclaje3d>& placas
) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format\": \"cype_bimjsunion_v1\",\n";
    oss << "  \"obra\": \"" << obra.datos_generales().clave << "\",\n";
    oss << "  \"num_nudos\": " << obra.estructura().nudos.size() << ",\n";
    oss << "  \"num_uniones\": " << uniones.size() << ",\n";
    oss << "  \"num_placas_anclaje\": " << placas.size() << ",\n";
    oss << "  \"uniones\": [\n";
    for (std::size_t i = 0; i < uniones.size(); ++i) {
        const auto& u = uniones[i];
        oss << "    {\"id\": " << u.id
            << ", \"ref\": \"" << u.referencia
            << "\", \"nudo_id\": " << u.nudo_id
            << ", \"tipologia\": " << static_cast<int>(u.tipologia)
            << ", \"cumple\": " << (u.cumple_global ? "true" : "false")
            << ", \"aprovechamiento_pct\": " << fmt_num(u.aprovechamiento_max_pct, 1)
            << "}" << (i + 1 < uniones.size() ? "," : "") << "\n";
    }
    oss << "  ],\n";
    oss << "  \"placas_anclaje\": [\n";
    for (std::size_t i = 0; i < placas.size(); ++i) {
        const auto& p = placas[i];
        oss << "    {\"id\": " << p.id
            << ", \"nudo_id\": " << p.nudo_id
            << ", \"perfil\": \"" << p.perfil_pilar
            << "\", \"lx_mm\": " << fmt_num(p.ancho_lx_mm, 0)
            << ", \"ly_mm\": " << fmt_num(p.largo_ly_mm, 0)
            << ", \"e_mm\": " << fmt_num(p.espesor_e_mm, 0)
            << ", \"pernos\": " << p.pernos.size()
            << ", \"cumple\": " << (p.ultima_comprobacion.cumple_global ? "true" : "false")
            << "}" << (i + 1 < placas.size() ? "," : "") << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

} // namespace cypeclon::connections
