#include "codes/design_check_engine.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>

namespace cypeclon::codes {

namespace {

std::string fmt_num(double val, int decimals = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << val;
    return oss.str();
}

const char* estado_a_texto(EstadoArticuloComprobacion est) noexcept {
    switch (est) {
        case EstadoArticuloComprobacion::Cumple: return "CUMPLE";
        case EstadoArticuloComprobacion::NoCumple: return "NO CUMPLE";
        case EstadoArticuloComprobacion::Aviso: return "AVISO";
        case EstadoArticuloComprobacion::Informacion: return "INFO";
        case EstadoArticuloComprobacion::NoProcede: return "No procede";
    }
    return "CUMPLE";
}

double area_una_barra_cm2(std::uint32_t diam_mm) noexcept {
    const double d_cm = static_cast<double>(diam_mm) * 0.1;
    return 0.25 * std::numbers::pi * d_cm * d_cm;
}

double peso_lineal_barra_kg_m(std::uint32_t diam_mm) noexcept {
    // Acero gamma = 7850 kg/m^3 -> 0.785 kg / (m * cm^2)
    return area_una_barra_cm2(diam_mm) * 0.785;
}

model::ProfileSectionProperties resolver_perfil_efectivo_barra(
    const model::Obra& obra,
    const model::Barra3d& barra
) noexcept {
    model::ProfileSectionProperties prof{};
    if (const auto* found = obra.estructura().biblioteca_perfiles.find_profile(barra.perfil)) {
        prof = *found;
    } else {
        prof = model::ProfileLibraryManager::compute_double_t(barra.perfil, "IPE", 200.0, 100.0, 5.6, 8.5, 12.0);
    }

    if (barra.disposicion.disposicion_compuesta != model::DisposicionCompuestaPerfil::Simple) {
        prof = model::ProfileLibraryManager::compute_composite_section(
            prof,
            barra.disposicion.disposicion_compuesta,
            barra.disposicion.separacion_perfiles_mm,
            10.0,
            prof.b_mm
        );
    }
    return prof;
}

} // namespace

// ==============================================================================
// P-10.1 — Generadores de Listados de Comprobación (`compsec.dll`: `cslistad_*`)
// ==============================================================================

std::string ResultadoComprobacionBarra::generar_listado_resumido() const {
    std::ostringstream oss;
    oss << "Barra " << referencia_barra << " [Perfil: " << perfil_referencia
        << ", Material: " << material_referencia
        << ", Norma: " << norma_aplicada
        << ", L = " << fmt_num(longitud_barra_m, 2) << " m, Clase "
        << static_cast<int>(clasificacion_seccion.clase_global) << "] -> "
        << (cumple_global ? "CUMPLE" : "NO CUMPLE")
        << " (Aprovechamiento pésimo: " << fmt_num(aprovechamiento_global_max_pct, 1) << "% en '"
        << articulo_pesimo_titulo << "')\n";

    oss << "  -----------------------------------------------------------------------------------------------------\n";
    oss << "  Código              Comprobación Normativa                         Estado      Eta (%)   Combinación\n";
    oss << "  -----------------------------------------------------------------------------------------------------\n";
    for (const auto& art : articulos) {
        oss << "  " << std::left << std::setw(18) << art.codigo_id
            << "  " << std::setw(43) << art.titulo_comprobacion.substr(0, 42)
            << "  " << std::setw(10) << estado_a_texto(art.estado)
            << "  " << std::right << std::setw(6) << fmt_num(art.aprovechamiento_pct, 1) << "%"
            << "   " << std::left << art.combinacion_pesima << "\n";
    }
    oss << "  -----------------------------------------------------------------------------------------------------\n";
    return oss.str();
}

std::string ResultadoComprobacionBarra::generar_listado_extendido() const {
    std::ostringstream oss;
    oss << "=======================================================================================================\n";
    oss << "LISTADO JUSTIFICADO DE COMPROBACIONES E.L.U. / E.L.S. / INCENDIO — " << referencia_barra << "\n";
    oss << "=======================================================================================================\n";
    oss << "  Perfil:            " << perfil_referencia << " (Serie: " << serie_perfil << ")\n";
    oss << "  Material:          " << material_referencia << "\n";
    oss << "  Norma aplicada:    " << norma_aplicada << "\n";
    oss << "  Longitud barra:    " << fmt_num(longitud_barra_m, 3) << " m\n";
    oss << "  Clase de sección:  Clase " << static_cast<int>(clasificacion_seccion.clase_global)
        << " (Ala: Clase " << static_cast<int>(clasificacion_seccion.clase_ala)
        << ", Alma: Clase " << static_cast<int>(clasificacion_seccion.clase_alma)
        << ", eps = " << fmt_num(clasificacion_seccion.epsilon_material, 3) << ")\n";
    oss << "  Estado global:     " << (cumple_global ? "CUMPLE TODAS LAS COMPROBACIONES" : "NO CUMPLE")
        << " (eta_max = " << fmt_num(aprovechamiento_global_max_pct, 2) << "%)\n\n";

    std::size_t idx = 1;
    for (const auto& art : articulos) {
        oss << idx++ << ". " << art.titulo_comprobacion << " [" << art.referencia_normativa << "]\n";
        oss << "   - Estado:       " << estado_a_texto(art.estado);
        if (art.estado != EstadoArticuloComprobacion::NoProcede && art.estado != EstadoArticuloComprobacion::Informacion) {
            oss << " (Aprovechamiento eta = " << fmt_num(art.aprovechamiento_pct, 2) << "%)";
        }
        oss << "\n";
        if (!art.combinacion_pesima.empty()) {
            oss << "   - Combinación:  " << art.combinacion_pesima
                << " (Abscisa x = " << fmt_num(art.abscisa_pesima_m, 2) << " m)\n";
        }
        oss << "   - Solicitación: Ed = " << fmt_num(art.valor_solicitante, 2) << " " << art.unidad
            << "  |  Capacidad/Límite: Rd = " << fmt_num(art.valor_resistente_o_limite, 2) << " " << art.unidad << "\n";
        if (!art.expresion_formula.empty()) {
            oss << "   - Fórmula:      " << art.expresion_formula << "\n";
        }
        if (!art.nota_justificativa.empty()) {
            oss << "   - Justificación: " << art.nota_justificativa << "\n";
        }
        oss << "\n";
    }
    return oss.str();
}

const ResultadoComprobacionBarra* ResumenComprobacionObra::find_comprobacion_barra(std::uint32_t barra_id) const noexcept {
    for (const auto& c : comprobaciones_barras) {
        if (c.barra_id == barra_id) {
            return &c;
        }
    }
    return nullptr;
}

// ==============================================================================
// P-10.1 — Clasificación de Sección, Curvas de Pandeo y Comprobaciones (`MemberDesignEngine`)
// ==============================================================================

ResultadoClasificacionSeccion MemberDesignEngine::clasificar_seccion_metalica(
    const model::ProfileSectionProperties& profile,
    double fy_mpa,
    double axil_n_kn,
    double my_knm,
    double mz_knm,
    bool es_aluminio
) noexcept {
    ResultadoClasificacionSeccion res{};
    const double fy_eff = std::max(50.0, fy_mpa);
    res.epsilon_material = es_aluminio ? std::sqrt(250.0 / fy_eff) : std::sqrt(235.0 / fy_eff);

    const double tf = std::max(1.0, profile.tf_mm);
    const double tw = std::max(1.0, profile.tw_mm);
    const double r = std::max(0.0, profile.r_mm);

    // Esbeltez de ala en voladizo c_f / t_f y alma interior c_w / t_w (EC3-1-1 Tabla 5.2 / CTE DB SE-A Tabla 5.3)
    double cf = std::max(2.0, 0.5 * profile.b_mm - 0.5 * tw - r);
    double cw = std::max(5.0, profile.h_mm - 2.0 * tf - 2.0 * r);

    if (profile.tipo_forma == model::GeometriaSeccionTipo::TuboCuadradoSHS ||
        profile.tipo_forma == model::GeometriaSeccionTipo::TuboRectangularRHS) {
        cf = std::max(2.0, profile.b_mm - 3.0 * tf);
        cw = std::max(2.0, profile.h_mm - 3.0 * tw);
    } else if (profile.tipo_forma == model::GeometriaSeccionTipo::AngularSimetrico ||
               profile.tipo_forma == model::GeometriaSeccionTipo::AngularAsimetrico) {
        cf = std::max(2.0, profile.b_mm - tf - r);
        cw = std::max(2.0, profile.h_mm - tw - r);
    } else if (profile.familia == model::FamiliaMaterialPerfil::AceroConformado) {
        cf = std::max(2.0, profile.b_mm - 2.0 * tf);
        cw = std::max(5.0, profile.h_mm - 2.0 * tw);
    }

    res.esbeltez_ala_c_t = cf / tf;
    res.esbeltez_alma_c_t = cw / tw;

    const double eps = res.epsilon_material;
    const bool hueco = (profile.tipo_forma == model::GeometriaSeccionTipo::TuboCuadradoSHS ||
                        profile.tipo_forma == model::GeometriaSeccionTipo::TuboRectangularRHS);

    // Límites para el ala comprimida (elemento en voladizo: 9*eps, 10*eps, 14*eps; elemento interno en cajón: 33*eps, 38*eps, 42*eps)
    const double lim_f1 = (hueco ? 33.0 : 9.0) * eps;
    const double lim_f2 = (hueco ? 38.0 : 10.0) * eps;
    const double lim_f3 = (hueco ? 42.0 : 14.0) * eps;

    if (res.esbeltez_ala_c_t <= lim_f1) {
        res.clase_ala = ClaseSeccionTransversal::Clase1_PlasticaDuctil;
    } else if (res.esbeltez_ala_c_t <= lim_f2) {
        res.clase_ala = ClaseSeccionTransversal::Clase2_PlasticaCompacta;
    } else if (res.esbeltez_ala_c_t <= lim_f3) {
        res.clase_ala = ClaseSeccionTransversal::Clase3_Elastica;
    } else {
        res.clase_ala = ClaseSeccionTransversal::Clase4_EsbeltaEfectiva;
    }

    // Límites para el alma según proporción de compresión (flexión pura: 72*eps, 83*eps, 124*eps; compresión pura: 33*eps, 38*eps, 42*eps)
    const double n_comp_kn = (axil_n_kn < 0.0) ? (-axil_n_kn) : 0.0;
    const double n_pl_kn = profile.area_m2() * fy_eff * 1000.0;
    const double ratio_n = std::clamp(n_comp_kn / std::max(1.0, n_pl_kn), 0.0, 1.0);
    const bool predomina_flexion = (std::abs(my_knm) + std::abs(mz_knm) > 0.05 * n_comp_kn * (profile.h_mm * 0.001));

    double lim_w1 = 33.0 * eps;
    double lim_w2 = 38.0 * eps;
    double lim_w3 = 42.0 * eps;
    if (predomina_flexion && ratio_n < 0.85) {
        const double alpha_pl = std::clamp(0.5 * (1.0 + ratio_n), 0.5, 1.0);
        lim_w1 = (alpha_pl > 0.5) ? ((396.0 * eps) / (13.0 * alpha_pl - 1.0)) : (72.0 * eps);
        lim_w2 = (alpha_pl > 0.5) ? ((456.0 * eps) / (13.0 * alpha_pl - 1.0)) : (83.0 * eps);
        lim_w3 = (124.0 * (1.0 - 0.66 * ratio_n)) * eps;
    }

    if (res.esbeltez_alma_c_t <= lim_w1) {
        res.clase_alma = ClaseSeccionTransversal::Clase1_PlasticaDuctil;
    } else if (res.esbeltez_alma_c_t <= lim_w2) {
        res.clase_alma = ClaseSeccionTransversal::Clase2_PlasticaCompacta;
    } else if (res.esbeltez_alma_c_t <= lim_w3) {
        res.clase_alma = ClaseSeccionTransversal::Clase3_Elastica;
    } else {
        res.clase_alma = ClaseSeccionTransversal::Clase4_EsbeltaEfectiva;
    }

    // Tubo circular CHS (d / t <= 50*eps^2, 70*eps^2, 90*eps^2)
    if (profile.tipo_forma == model::GeometriaSeccionTipo::TuboCircularCHS) {
        const double d_t = profile.h_mm / tf;
        const double eps2 = eps * eps;
        if (d_t <= 50.0 * eps2) res.clase_ala = res.clase_alma = ClaseSeccionTransversal::Clase1_PlasticaDuctil;
        else if (d_t <= 70.0 * eps2) res.clase_ala = res.clase_alma = ClaseSeccionTransversal::Clase2_PlasticaCompacta;
        else if (d_t <= 90.0 * eps2) res.clase_ala = res.clase_alma = ClaseSeccionTransversal::Clase3_Elastica;
        else res.clase_ala = res.clase_alma = ClaseSeccionTransversal::Clase4_EsbeltaEfectiva;
    } else if (profile.tipo_forma == model::GeometriaSeccionTipo::RedondoMacizo ||
               profile.tipo_forma == model::GeometriaSeccionTipo::RectangularMacizo ||
               profile.tipo_forma == model::GeometriaSeccionTipo::CircularMacizo) {
        res.clase_ala = res.clase_alma = ClaseSeccionTransversal::Clase1_PlasticaDuctil;
    }

    res.clase_global = static_cast<ClaseSeccionTransversal>(
        std::max(static_cast<int>(res.clase_ala), static_cast<int>(res.clase_alma))
    );

    // Cálculo de sección eficaz según fórmula de Winter (EN 1993-1-5 / EN 1993-1-3) para Clase 4 y conformados en frío
    res.factor_reduccion_area_rho = 1.0;
    if (res.clase_global == ClaseSeccionTransversal::Clase4_EsbeltaEfectiva ||
        profile.familia == model::FamiliaMaterialPerfil::AceroConformado) {
        const double lambda_p_w = (res.esbeltez_alma_c_t / (28.4 * eps * 2.0)); // k_sigma = 4.0 -> sqrt(4)=2
        const double lambda_p_f = (res.esbeltez_ala_c_t / (28.4 * eps * std::sqrt(0.43)));
        double rho_w = 1.0;
        if (lambda_p_w > 0.673) {
            rho_w = std::clamp((lambda_p_w - 0.055 * 4.0) / (lambda_p_w * lambda_p_w), 0.45, 1.0);
        }
        double rho_f = 1.0;
        if (lambda_p_f > 0.748) {
            rho_f = std::clamp((lambda_p_f - 0.188) / (lambda_p_f * lambda_p_f), 0.50, 1.0);
        }
        res.factor_reduccion_area_rho = std::min(1.0, 0.5 * (rho_w + rho_f));
        if (res.factor_reduccion_area_rho < 0.995) {
            res.clase_global = ClaseSeccionTransversal::Clase4_EsbeltaEfectiva;
        }
    }

    res.area_eficaz_cm2 = profile.area_cm2 * res.factor_reduccion_area_rho;
    if (res.clase_global == ClaseSeccionTransversal::Clase1_PlasticaDuctil ||
        res.clase_global == ClaseSeccionTransversal::Clase2_PlasticaCompacta) {
        res.wy_calculo_cm3 = profile.wply_cm3;
        res.wz_calculo_cm3 = profile.wplz_cm3;
    } else if (res.clase_global == ClaseSeccionTransversal::Clase3_Elastica) {
        res.wy_calculo_cm3 = profile.wely_cm3;
        res.wz_calculo_cm3 = profile.welz_cm3;
    } else {
        res.wy_calculo_cm3 = profile.wely_cm3 * res.factor_reduccion_area_rho;
        res.wz_calculo_cm3 = profile.welz_cm3 * res.factor_reduccion_area_rho;
    }

    return res;
}

double MemberDesignEngine::factor_reduccion_pandeo_chi(
    double esbeltez_relativa_lambda_bar,
    double factor_imperfeccion_alpha,
    double lambda_0
) noexcept {
    if (esbeltez_relativa_lambda_bar <= lambda_0) {
        return 1.0;
    }
    const double l2 = esbeltez_relativa_lambda_bar * esbeltez_relativa_lambda_bar;
    const double phi = 0.5 * (1.0 + factor_imperfeccion_alpha * (esbeltez_relativa_lambda_bar - lambda_0) + l2);
    const double rad = std::max(0.0, phi * phi - l2);
    const double denom = phi + std::sqrt(rad);
    if (denom <= 1e-9) {
        return 1.0;
    }
    return std::clamp(1.0 / denom, 0.05, 1.0);
}

double MemberDesignEngine::factor_imperfeccion_pandeo_acero(
    const model::ProfileSectionProperties& profile,
    bool eje_fuerte_y,
    double fy_mpa
) noexcept {
    // Factores de imperfección alpha de las curvas de pandeo EC3-1-1 Tabla 6.1 / CTE DB SE-A Tabla 6.2:
    // Curva a0: 0.13, Curva a: 0.21, Curva b: 0.34, Curva c: 0.49, Curva d: 0.76
    const bool s460 = (fy_mpa >= 420.0);
    if (profile.tipo_forma == model::GeometriaSeccionTipo::TuboCuadradoSHS ||
        profile.tipo_forma == model::GeometriaSeccionTipo::TuboRectangularRHS ||
        profile.tipo_forma == model::GeometriaSeccionTipo::TuboCircularCHS) {
        return s460 ? 0.13 : 0.21; // Curva a0 / a para tubos laminados en caliente
    }
    if (profile.tipo_forma == model::GeometriaSeccionTipo::DobleT) {
        const double h_b = profile.h_mm / std::max(1.0, profile.b_mm);
        if (h_b > 1.2) {
            // Tipo IPE: eje y -> curva a (0.21), eje z -> curva b (0.34)
            return eje_fuerte_y ? (s460 ? 0.13 : 0.21) : (s460 ? 0.21 : 0.34);
        }
        // Tipo HEB/HEA: eje y -> curva b (0.34), eje z -> curva c (0.49)
        return eje_fuerte_y ? (s460 ? 0.21 : 0.34) : (s460 ? 0.34 : 0.49);
    }
    if (profile.familia == model::FamiliaMaterialPerfil::AceroConformado) {
        return 0.49; // Curva c para secciones conformadas en frío C/Z/Omega (EN 1993-1-3)
    }
    // Perfiles UPN, L, T, redondos -> Curva c (0.49)
    return 0.49;
}

ResultadoComprobacionBarra MemberDesignEngine::comprobar_barra(
    const model::Obra& obra,
    const model::Barra3d& barra,
    const solver::SolverSummary& solver_results
) {
    ResultadoComprobacionBarra res{};
    res.barra_id = barra.id;
    res.referencia_barra = "B" + std::to_string(barra.id);
    res.perfil_referencia = barra.perfil;
    res.material_referencia = barra.material;
    res.longitud_barra_m = std::max(0.05, obra.longitud_barra_m(barra.id));

    const model::ProfileSectionProperties prof = resolver_perfil_efectivo_barra(obra, barra);
    res.serie_perfil = prof.serie;
    res.familia_material = prof.familia;

    // Determinar norma activa según la familia del perfil
    const auto& dg = obra.datos_generales();
    if (prof.familia == model::FamiliaMaterialPerfil::Madera) {
        res.norma_aplicada = dg.norma_madera.empty() ? "CTE DB SE-M / Eurocódigo 5" : dg.norma_madera;
    } else if (prof.familia == model::FamiliaMaterialPerfil::Aluminio) {
        res.norma_aplicada = dg.norma_aluminio.empty() ? "Eurocódigo 9 (EN 1999-1-1)" : dg.norma_aluminio;
    } else if (prof.familia == model::FamiliaMaterialPerfil::AceroConformado) {
        res.norma_aplicada = dg.norma_acero_conformado.empty() ? "Código Estructural / EN 1993-1-3" : dg.norma_acero_conformado;
    } else {
        res.norma_aplicada = dg.norma_acero_laminado.empty() ? "CTE DB SE-A / Eurocódigo 3" : dg.norma_acero_laminado;
    }

    // Recopilar estados de esfuerzos de todas las combinaciones ELU, o bien de las hipótesis / envolventes
    struct MuestraEsfuerzoCombinacion {
        std::string nombre_comb;
        double x_m{0.0};
        double n_kn{0.0};
        double vy_kn{0.0};
        double vz_kn{0.0};
        double mt_knm{0.0};
        double my_knm{0.0};
        double mz_knm{0.0};
    };

    std::vector<MuestraEsfuerzoCombinacion> muestras_elu;
    double flecha_els_max_mm = 0.0;
    std::string comb_pesima_flecha = "ELS Característica";

    for (const auto& rc : solver_results.resultados_combinaciones) {
        const auto* eb = rc.find_esfuerzos_barra(barra.id);
        if (!eb) continue;

        const bool es_els = (rc.codigo.find("ELS") != std::string::npos || rc.descripcion.find("E.L.S.") != std::string::npos);
        const double f_mm = std::hypot(eb->flecha_max_xy_mm, eb->flecha_max_xz_mm);
        if (es_els || solver_results.resultados_combinaciones.size() == 1) {
            if (f_mm >= flecha_els_max_mm) {
                flecha_els_max_mm = f_mm;
                comb_pesima_flecha = rc.descripcion.empty() ? rc.codigo : rc.descripcion;
            }
        }

        if (!es_els) {
            for (const auto& pt : eb->puntos) {
                muestras_elu.push_back({
                    rc.descripcion.empty() ? rc.codigo : rc.descripcion,
                    pt.x_m,
                    pt.n_kn,
                    pt.vy_kn,
                    pt.vz_kn,
                    pt.mt_knm,
                    pt.my_knm,
                    pt.mz_knm
                });
            }
        }
    }

    // Fallback a resultados_hipotesis o envolventes_barras si aún no había combinaciones
    if (muestras_elu.empty()) {
        for (const auto& rh : solver_results.resultados_hipotesis) {
            if (const auto* eb = rh.find_esfuerzos_barra(barra.id)) {
                const double f_mm = std::hypot(eb->flecha_max_xy_mm, eb->flecha_max_xz_mm);
                if (f_mm > flecha_els_max_mm) {
                    flecha_els_max_mm = f_mm;
                    comb_pesima_flecha = rh.codigo;
                }
                for (const auto& pt : eb->puntos) {
                    muestras_elu.push_back({
                        "1.35·" + rh.codigo,
                        pt.x_m,
                        1.35 * pt.n_kn,
                        1.35 * pt.vy_kn,
                        1.35 * pt.vz_kn,
                        1.35 * pt.mt_knm,
                        1.35 * pt.my_knm,
                        1.35 * pt.mz_knm
                    });
                }
            }
        }
    }

    if (muestras_elu.empty()) {
        for (const auto& env : solver_results.envolventes_barras) {
            if (env.barra_id == barra.id) {
                flecha_els_max_mm = env.flecha_max_mm;
                const double my_abs = std::max(std::abs(env.my_max_pos_knm), std::abs(env.my_min_neg_knm));
                const double mz_abs = std::max(std::abs(env.mz_max_pos_knm), std::abs(env.mz_min_neg_knm));
                muestras_elu.push_back({
                    env.combinacion_pesima_my.empty() ? "Envolvente ELU" : env.combinacion_pesima_my,
                    0.5 * res.longitud_barra_m,
                    (std::abs(env.n_max_compresion_kn) >= env.n_max_traccion_kn) ? env.n_max_compresion_kn : env.n_max_traccion_kn,
                    env.vy_max_abs_kn,
                    env.vz_max_abs_kn,
                    env.mt_max_abs_knm,
                    my_abs,
                    mz_abs
                });
                break;
            }
        }
    }

    if (muestras_elu.empty()) {
        muestras_elu.push_back({"1.35·PP", 0.5 * res.longitud_barra_m, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    }

    // Valores extremos globales sobre la barra
    double n_trac_max_kn = 0.0;
    std::string comb_n_trac = muestras_elu.front().nombre_comb;
    double x_n_trac = 0.0;

    double n_comp_max_kn = 0.0; // valor positivo de compresión |N_c,Ed|
    std::string comb_n_comp = muestras_elu.front().nombre_comb;
    double x_n_comp = 0.0;

    double vy_max_kn = 0.0;
    std::string comb_vy = muestras_elu.front().nombre_comb;
    double x_vy = 0.0;

    double vz_max_kn = 0.0;
    std::string comb_vz = muestras_elu.front().nombre_comb;
    double x_vz = 0.0;

    double my_max_knm = 0.0;
    std::string comb_my = muestras_elu.front().nombre_comb;
    double x_my = 0.0;

    double mz_max_knm = 0.0;
    std::string comb_mz = muestras_elu.front().nombre_comb;
    double x_mz = 0.0;

    double mt_max_knm = 0.0;
    std::string comb_mt = muestras_elu.front().nombre_comb;
    double x_mt = 0.0;

    for (const auto& m : muestras_elu) {
        if (m.n_kn > n_trac_max_kn) {
            n_trac_max_kn = m.n_kn;
            comb_n_trac = m.nombre_comb;
            x_n_trac = m.x_m;
        }
        if (-m.n_kn > n_comp_max_kn) {
            n_comp_max_kn = -m.n_kn;
            comb_n_comp = m.nombre_comb;
            x_n_comp = m.x_m;
        }
        if (std::abs(m.vy_kn) > vy_max_kn) {
            vy_max_kn = std::abs(m.vy_kn);
            comb_vy = m.nombre_comb;
            x_vy = m.x_m;
        }
        if (std::abs(m.vz_kn) > vz_max_kn) {
            vz_max_kn = std::abs(m.vz_kn);
            comb_vz = m.nombre_comb;
            x_vz = m.x_m;
        }
        if (std::abs(m.my_knm) > my_max_knm) {
            my_max_knm = std::abs(m.my_knm);
            comb_my = m.nombre_comb;
            x_my = m.x_m;
        }
        if (std::abs(m.mz_knm) > mz_max_knm) {
            mz_max_knm = std::abs(m.mz_knm);
            comb_mz = m.nombre_comb;
            x_mz = m.x_m;
        }
        if (std::abs(m.mt_knm) > mt_max_knm) {
            mt_max_knm = std::abs(m.mt_knm);
            comb_mt = m.nombre_comb;
            x_mt = m.x_m;
        }
    }

    const double lk_xy_m = barra.pandeo.longitud_efectiva_xy_m(res.longitud_barra_m);
    const double lk_xz_m = barra.pandeo.longitud_efectiva_xz_m(res.longitud_barra_m);

    // ==========================================================================
    // RAMA A: MADERA ESTRUCTURAL (`pzmadera.dll` — CTE DB SE-M / Eurocódigo 5)
    // ==========================================================================
    if (prof.familia == model::FamiliaMaterialPerfil::Madera) {
        const auto* mat_mad = NormativaEngine::buscar_madera(barra.material);
        const PropiedadesMadera mad = mat_mad ? *mat_mad : NormativaEngine::catalogo_maderas().front();
        const double gamma_m = mad.es_laminada_encolada ? 1.25 : 1.30;
        const double k_mod = 0.80; // Clase de servicio 1/2, duración media
        const double kh_y = std::min(1.30, std::pow(150.0 / std::max(50.0, prof.h_mm), 0.20));

        const double fm_d_mpa = k_mod * kh_y * mad.fm_k_mpa / gamma_m;
        const double ft0_d_mpa = k_mod * mad.ft_0_k_mpa / gamma_m;
        const double fc0_d_mpa = k_mod * mad.fc_0_k_mpa / gamma_m;
        const double fv_d_mpa = k_mod * mad.fv_k_mpa / gamma_m;

        res.clasificacion_seccion.clase_global = ClaseSeccionTransversal::Clase1_PlasticaDuctil;
        res.clasificacion_seccion.area_eficaz_cm2 = prof.area_cm2;
        res.clasificacion_seccion.wy_calculo_cm3 = prof.wely_cm3;
        res.clasificacion_seccion.wz_calculo_cm3 = prof.welz_cm3;

        // 1. Esbeltez mecánica en madera (CTE DB SE-M Art. 6.3.2)
        const double lambda_y = (lk_xz_m * 100.0) / std::max(0.5, prof.ry_cm);
        const double lambda_z = (lk_xy_m * 100.0) / std::max(0.5, prof.rz_cm);
        const double lambda_max = std::max(lambda_y, lambda_z);
        const double lambda_rel_y = (lambda_y / std::numbers::pi) * std::sqrt(mad.fc_0_k_mpa / mad.e0_05_mpa);
        const double lambda_rel_z = (lambda_z / std::numbers::pi) * std::sqrt(mad.fc_0_k_mpa / mad.e0_05_mpa);

        const double beta_c = mad.es_laminada_encolada ? 0.10 : 0.20;
        const auto calc_kc = [beta_c](double l_rel) noexcept {
            if (l_rel <= 0.30) return 1.0;
            const double k = 0.5 * (1.0 + beta_c * (l_rel - 0.30) + l_rel * l_rel);
            const double rad = std::max(0.0, k * k - l_rel * l_rel);
            return std::clamp(1.0 / (k + std::sqrt(rad)), 0.05, 1.0);
        };
        const double kc_y = calc_kc(lambda_rel_y);
        const double kc_z = calc_kc(lambda_rel_z);
        const double kc_min = std::min(kc_y, kc_z);

        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "ESBELTEZ_MADERA";
            art.titulo_comprobacion = "Limitación de esbeltez mecánica (Madera)";
            art.referencia_normativa = "CTE DB SE-M Art. 6.3.2 / EN 1995-1-1 Art. 6.3.2";
            art.valor_solicitante = lambda_max;
            art.valor_resistente_o_limite = 150.0;
            art.unidad = "adim";
            art.aprovechamiento_pct = (lambda_max / 150.0) * 100.0;
            art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = comb_n_comp;
            art.expresion_formula = "lambda_max = max(Lk,y/iy, Lk,z/iz) = " + fmt_num(lambda_max, 1) + " <= 150.0";
            art.nota_justificativa = "kc,y = " + fmt_num(kc_y, 3) + ", kc,z = " + fmt_num(kc_z, 3);
            res.articulos.push_back(art);
        }

        // 2. Axil de tracción o compresión paralela a la fibra (CTE DB SE-M Art. 6.1.2 / 6.3.2)
        const double n_t_rd_kn = prof.area_m2() * ft0_d_mpa * 1000.0;
        const double n_c_rd_kn = kc_min * prof.area_m2() * fc0_d_mpa * 1000.0;
        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "AXIL_MADERA";
            art.titulo_comprobacion = (n_comp_max_kn >= n_trac_max_kn)
                ? "Resistencia a compresión y pandeo paralelo a la fibra"
                : "Resistencia a tracción paralela a la fibra";
            art.referencia_normativa = "CTE DB SE-M Art. 6.1.2 y 6.3.2 / EN 1995-1-1";
            art.valor_solicitante = std::max(n_comp_max_kn, n_trac_max_kn);
            art.valor_resistente_o_limite = (n_comp_max_kn >= n_trac_max_kn) ? n_c_rd_kn : n_t_rd_kn;
            art.unidad = "kN";
            art.aprovechamiento_pct = (art.valor_resistente_o_limite > 1e-6)
                ? (art.valor_solicitante / art.valor_resistente_o_limite) * 100.0 : 0.0;
            art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = (n_comp_max_kn >= n_trac_max_kn) ? comb_n_comp : comb_n_trac;
            art.abscisa_pesima_m = (n_comp_max_kn >= n_trac_max_kn) ? x_n_comp : x_n_trac;
            art.expresion_formula = "sigma_c,0,d / (kc * f_c,0,d) <= 1.0  (f_c,0,d = " + fmt_num(fc0_d_mpa, 2) + " MPa)";
            res.articulos.push_back(art);
        }

        // 3. Cortante en madera (tau_d = 1.5 * V / (k_cr * A) <= f_v,d)
        const double v_max_kn = std::hypot(vy_max_kn, vz_max_kn);
        const double k_cr = 0.67; // Factor de fisuración en madera (EC5 Art. 6.1.7)
        const double v_rd_mad_kn = (k_cr * prof.area_m2() * fv_d_mpa * 1000.0) / 1.50;
        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "CORTANTE_MADERA";
            art.titulo_comprobacion = "Resistencia al esfuerzo cortante";
            art.referencia_normativa = "CTE DB SE-M Art. 6.1.7 / EN 1995-1-1 Art. 6.1.7";
            art.valor_solicitante = v_max_kn;
            art.valor_resistente_o_limite = v_rd_mad_kn;
            art.unidad = "kN";
            art.aprovechamiento_pct = (v_rd_mad_kn > 1e-6) ? (v_max_kn / v_rd_mad_kn) * 100.0 : 0.0;
            art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = comb_vz;
            art.abscisa_pesima_m = x_vz;
            art.expresion_formula = "tau_d = 1.5 * V_Ed / (k_cr * A) <= f_v,d (" + fmt_num(fv_d_mpa, 2) + " MPa)";
            res.articulos.push_back(art);
        }

        // 4. Flexión compuesta e interacción con vuelco lateral (k_crit, CTE DB SE-M Art. 6.3.3)
        const double my_rd_knm = prof.wely_m3() * fm_d_mpa * 1000.0;
        const double mz_rd_knm = prof.welz_m3() * fm_d_mpa * 1000.0;
        const double k_m = (prof.tipo_forma == model::GeometriaSeccionTipo::RectangularMacizo) ? 0.70 : 1.0;

        double max_inter_mad = 0.0;
        std::string comb_inter_mad = muestras_elu.front().nombre_comb;
        double x_inter_mad = 0.0;
        for (const auto& m : muestras_elu) {
            const double ry = std::abs(m.my_knm) / std::max(1e-6, my_rd_knm);
            const double rz = std::abs(m.mz_knm) / std::max(1e-6, mz_rd_knm);
            const double rn = (m.n_kn < 0.0)
                ? ((-m.n_kn) / std::max(1e-6, n_c_rd_kn))
                : (m.n_kn / std::max(1e-6, n_t_rd_kn));
            const double u1 = rn + ry + k_m * rz;
            const double u2 = rn + k_m * ry + rz;
            const double u = std::max(u1, u2);
            if (u >= max_inter_mad) {
                max_inter_mad = u;
                comb_inter_mad = m.nombre_comb;
                x_inter_mad = m.x_m;
            }
        }
        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "INTERACCION_MADERA";
            art.titulo_comprobacion = "Interacción de flexión esviada y axil (k_m = 0.70)";
            art.referencia_normativa = "CTE DB SE-M Art. 6.2.3 y 6.3.2 / EN 1995-1-1 Art. 6.3.2";
            art.valor_solicitante = max_inter_mad;
            art.valor_resistente_o_limite = 1.00;
            art.unidad = "adim";
            art.aprovechamiento_pct = max_inter_mad * 100.0;
            art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = comb_inter_mad;
            art.abscisa_pesima_m = x_inter_mad;
            art.expresion_formula = "N_Ed / N_Rd + M_y,Ed / M_y,Rd + k_m * M_z,Ed / M_z,Rd = " + fmt_num(max_inter_mad, 3) + " <= 1.0";
            res.articulos.push_back(art);
        }
    }
    // ==========================================================================
    // RAMA B: ACERO LAMINADO, ARMADO, CONFORMADO Y ALUMINIO (`normamet.dll`, `eurocodigo9.dll`)
    // ==========================================================================
    else {
        const bool es_aluminio = (prof.familia == model::FamiliaMaterialPerfil::Aluminio);
        double fy_mpa = 275.0;
        double fu_mpa = 430.0;
        double e_mpa = 210000.0;
        double g_mpa = 80769.23;
        double gamma_m0 = 1.05;
        double gamma_m1 = 1.05;

        if (es_aluminio) {
            const auto* mat_al = NormativaEngine::buscar_aluminio(barra.material);
            const PropiedadesAluminio al = mat_al ? *mat_al : NormativaEngine::catalogo_aluminios().front();
            fy_mpa = al.fo_mpa;
            fu_mpa = al.fu_mpa;
            e_mpa = al.e_mpa;
            g_mpa = al.g_mpa;
            gamma_m0 = 1.10;
            gamma_m1 = 1.10;
        } else {
            const auto* mat_ac = NormativaEngine::buscar_acero_estructural(barra.material);
            const PropiedadesAceroEstructural ac = mat_ac ? *mat_ac : NormativaEngine::catalogo_aceros_laminados().front();
            fy_mpa = ac.fy_segun_espesor(std::max(prof.tf_mm, prof.tw_mm));
            fu_mpa = ac.fu_nominal_mpa;
            e_mpa = ac.e_mpa;
            g_mpa = ac.g_mpa;
            if ( const auto* nspec = NormativaEngine::buscar_norma(
                     prof.familia == model::FamiliaMaterialPerfil::AceroConformado
                         ? FamiliaNormativa::AceroConformado
                         : FamiliaNormativa::AceroLaminado,
                     res.norma_aplicada) ) {
                gamma_m0 = nspec->gamma_principal;
                gamma_m1 = nspec->gamma_secundario;
            }
        }
        (void)fu_mpa;

        // 1. Clasificación de sección (Clase 1, 2, 3 o 4)
        res.clasificacion_seccion = clasificar_seccion_metalica(
            prof, fy_mpa, -n_comp_max_kn, my_max_knm, mz_max_knm, es_aluminio
        );
        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "CLASE_SECCION";
            art.titulo_comprobacion = "Clasificación de la sección y abolladura local";
            art.referencia_normativa = es_aluminio
                ? "Eurocódigo 9 (EN 1999-1-1 Art. 6.1.4)"
                : (prof.familia == model::FamiliaMaterialPerfil::AceroConformado
                       ? "EN 1993-1-3 Art. 5.5 (Sección eficaz Winter)"
                       : "CTE DB SE-A Art. 5.3 / EN 1993-1-1 Tabla 5.2");
            art.estado = EstadoArticuloComprobacion::Informacion;
            art.valor_solicitante = static_cast<double>(res.clasificacion_seccion.clase_global);
            art.valor_resistente_o_limite = 4.0;
            art.unidad = "Clase";
            art.aprovechamiento_pct = 0.0;
            art.expresion_formula = "c_f/t_f = " + fmt_num(res.clasificacion_seccion.esbeltez_ala_c_t, 1) +
                                    ", c_w/t_w = " + fmt_num(res.clasificacion_seccion.esbeltez_alma_c_t, 1) +
                                    " -> Clase " + std::to_string(static_cast<int>(res.clasificacion_seccion.clase_global));
            art.nota_justificativa = "A_eff = " + fmt_num(res.clasificacion_seccion.area_eficaz_cm2, 2) +
                                     " cm^2, W_y,calc = " + fmt_num(res.clasificacion_seccion.wy_calculo_cm3, 2) + " cm^3.";
            res.articulos.push_back(art);
        }

        // 2. Limitación de esbeltez mecánica (CTE DB SE-A Art. 6.3.1 / gctextos_texto_TituloLimitacionEsbeltez)
        const double lambda_y = (lk_xz_m * 100.0) / std::max(0.2, prof.ry_cm);
        const double lambda_z = (lk_xy_m * 100.0) / std::max(0.2, prof.rz_cm);
        const double lambda_max = std::max(lambda_y, lambda_z);
        const double lambda_lim = (n_comp_max_kn > 1.0 && !barra.es_tirante) ? 200.0 : 300.0;
        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "ESBELTEZ";
            art.titulo_comprobacion = "Limitación de esbeltez mecánica";
            art.referencia_normativa = "CTE DB SE-A Art. 6.3.1 / EAE Art. 35.5";
            art.valor_solicitante = lambda_max;
            art.valor_resistente_o_limite = lambda_lim;
            art.unidad = "adim";
            art.aprovechamiento_pct = (lambda_max / lambda_lim) * 100.0;
            art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = (n_comp_max_kn > 1.0) ? comb_n_comp : comb_n_trac;
            art.expresion_formula = "lambda = max(Lk,y/iy=" + fmt_num(lambda_y, 1) + ", Lk,z/iz=" + fmt_num(lambda_z, 1) + ") <= " + fmt_num(lambda_lim, 0);
            res.articulos.push_back(art);
        }

        // 3. Resistencia a tracción (CTE DB SE-A Art. 6.2.3 / EN 1993-1-1 Art. 6.2.3)
        const double nt_rd_kn = (prof.area_m2() * fy_mpa * 1000.0) / gamma_m0;
        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "AXIL_TRACCION";
            art.titulo_comprobacion = "Resistencia a tracción de la sección transversal";
            art.referencia_normativa = es_aluminio
                ? "EN 1999-1-1 Art. 6.2.3"
                : "CTE DB SE-A Art. 6.2.3 / EN 1993-1-1 Art. 6.2.3";
            art.valor_solicitante = n_trac_max_kn;
            art.valor_resistente_o_limite = nt_rd_kn;
            art.unidad = "kN";
            if (n_trac_max_kn <= 1.0e-3) {
                art.estado = EstadoArticuloComprobacion::NoProcede;
                art.aprovechamiento_pct = 0.0;
                art.nota_justificativa = "No existen esfuerzos axiles de tracción en las combinaciones analizadas.";
            } else {
                art.aprovechamiento_pct = (n_trac_max_kn / std::max(1e-6, nt_rd_kn)) * 100.0;
                art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
                art.combinacion_pesima = comb_n_trac;
                art.abscisa_pesima_m = x_n_trac;
                art.expresion_formula = "N_t,Ed / N_pl,Rd = " + fmt_num(n_trac_max_kn, 2) + " / " + fmt_num(nt_rd_kn, 2) + " kN <= 1.0";
            }
            res.articulos.push_back(art);
        }

        // 4. Resistencia a compresión de la sección y Pandeo por flexión/torsión (CTE DB SE-A Art. 6.2.4 y 6.3.2 / EN 1993-1-1 Art. 6.3.1)
        const double a_calc_m2 = res.clasificacion_seccion.area_eficaz_cm2 * 1.0e-4;
        const double nc_rd_seccion_kn = (a_calc_m2 * fy_mpa * 1000.0) / gamma_m0;

        const double lambda_1 = std::numbers::pi * std::sqrt(e_mpa / fy_mpa);
        const double sqrt_rho = std::sqrt(res.clasificacion_seccion.factor_reduccion_area_rho);
        const double lambda_bar_y = (lambda_y / lambda_1) * sqrt_rho;
        const double lambda_bar_z = (lambda_z / lambda_1) * sqrt_rho;

        const double alpha_y = es_aluminio ? 0.20 : factor_imperfeccion_pandeo_acero(prof, true, fy_mpa);
        const double alpha_z = es_aluminio ? 0.32 : factor_imperfeccion_pandeo_acero(prof, false, fy_mpa);

        const double chi_y = (lk_xz_m > 0.0) ? factor_reduccion_pandeo_chi(lambda_bar_y, alpha_y) : 1.0;
        const double chi_z = (lk_xy_m > 0.0) ? factor_reduccion_pandeo_chi(lambda_bar_z, alpha_z) : 1.0;
        const double chi_min = std::min(chi_y, chi_z);
        const double nb_rd_kn = (chi_min * a_calc_m2 * fy_mpa * 1000.0) / gamma_m1;

        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "PANDEO_COMPRESION";
            art.titulo_comprobacion = "Resistencia a compresión y pandeo por flexión (chi_y, chi_z)";
            art.referencia_normativa = es_aluminio
                ? "EN 1999-1-1 Art. 6.3.1"
                : "CTE DB SE-A Art. 6.3.2 / EN 1993-1-1 Art. 6.3.1";
            art.valor_solicitante = n_comp_max_kn;
            art.valor_resistente_o_limite = nb_rd_kn;
            art.unidad = "kN";
            if (n_comp_max_kn <= 1.0e-3 || barra.es_tirante) {
                art.estado = EstadoArticuloComprobacion::NoProcede;
                art.aprovechamiento_pct = 0.0;
                art.nota_justificativa = barra.es_tirante
                    ? "Elemento definido como tirante (solo tracción)."
                    : "No existen esfuerzos axiles de compresión.";
            } else {
                art.aprovechamiento_pct = (n_comp_max_kn / std::max(1e-6, nb_rd_kn)) * 100.0;
                art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
                art.combinacion_pesima = comb_n_comp;
                art.abscisa_pesima_m = x_n_comp;
                art.expresion_formula = "N_c,Ed / N_b,Rd = " + fmt_num(n_comp_max_kn, 2) + " / " + fmt_num(nb_rd_kn, 2) +
                                        " kN (chi_y=" + fmt_num(chi_y, 3) + ", chi_z=" + fmt_num(chi_z, 3) +
                                        ", N_c,Rd=" + fmt_num(nc_rd_seccion_kn, 1) + " kN)";
            }
            res.articulos.push_back(art);
        }

        // 5. Resistencia al esfuerzo cortante Vy y Vz y abolladura por cortante del alma (CTE DB SE-A Art. 6.2.6 / EN 1993-1-1 Art. 6.2.6)
        const double tau_y_mpa = fy_mpa / std::sqrt(3.0);
        const double vpl_z_rd_kn = (prof.avz_m2() * tau_y_mpa * 1000.0) / gamma_m0;
        const double vpl_y_rd_kn = (prof.avy_m2() * tau_y_mpa * 1000.0) / gamma_m0;

        // Reducción por abolladura de cortante si h_w / t_w > 72 * eps (EN 1993-1-5 Art. 5.2)
        double chi_w = 1.0;
        if (res.clasificacion_seccion.esbeltez_alma_c_t > 72.0 * res.clasificacion_seccion.epsilon_material) {
            const double lambda_w = res.clasificacion_seccion.esbeltez_alma_c_t / (37.4 * res.clasificacion_seccion.epsilon_material * std::sqrt(5.34));
            if (lambda_w > 0.83) {
                chi_w = std::clamp(0.83 / lambda_w, 0.35, 1.0);
            }
        }
        const double vz_rd_kn = chi_w * vpl_z_rd_kn;
        const double vy_rd_kn = vpl_y_rd_kn;
        const double eta_vz = (vz_rd_kn > 1e-6) ? (vz_max_kn / vz_rd_kn) * 100.0 : 0.0;
        const double eta_vy = (vy_rd_kn > 1e-6) ? (vy_max_kn / vy_rd_kn) * 100.0 : 0.0;
        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "CORTANTE";
            art.titulo_comprobacion = "Resistencia al esfuerzo cortante y abolladura de alma";
            art.referencia_normativa = es_aluminio
                ? "EN 1999-1-1 Art. 6.2.6"
                : "CTE DB SE-A Art. 6.2.6 / EN 1993-1-1 Art. 6.2.6";
            if (eta_vz >= eta_vy) {
                art.valor_solicitante = vz_max_kn;
                art.valor_resistente_o_limite = vz_rd_kn;
                art.aprovechamiento_pct = eta_vz;
                art.combinacion_pesima = comb_vz;
                art.abscisa_pesima_m = x_vz;
            } else {
                art.valor_solicitante = vy_max_kn;
                art.valor_resistente_o_limite = vy_rd_kn;
                art.aprovechamiento_pct = eta_vy;
                art.combinacion_pesima = comb_vy;
                art.abscisa_pesima_m = x_vy;
            }
            art.unidad = "kN";
            art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.expresion_formula = "V_z,Ed / V_z,Rd = " + fmt_num(vz_max_kn, 2) + " / " + fmt_num(vz_rd_kn, 2) +
                                    " kN (chi_w = " + fmt_num(chi_w, 3) + ")";
            res.articulos.push_back(art);
        }

        // 6. Resistencia a momento flector y Pandeo Lateral (chi_LT, CTE DB SE-A Art. 6.2.5 y 6.3.3 / EN 1993-1-1 Art. 6.3.2)
        const double wy_m3 = res.clasificacion_seccion.wy_calculo_cm3 * 1.0e-6;
        const double wz_m3 = res.clasificacion_seccion.wz_calculo_cm3 * 1.0e-6;
        const double mc_y_rd_knm = (wy_m3 * fy_mpa * 1000.0) / gamma_m0;
        const double mc_z_rd_knm = (wz_m3 * fy_mpa * 1000.0) / gamma_m0;

        const double mcr_sup_knm = barra.pandeo_lateral.compute_mcr_knm(prof, res.longitud_barra_m, true, e_mpa, g_mpa);
        const double mcr_inf_knm = barra.pandeo_lateral.compute_mcr_knm(prof, res.longitud_barra_m, false, e_mpa, g_mpa);
        const double mcr_min_knm = std::max(1.0, std::min(mcr_sup_knm, mcr_inf_knm));

        // Esbeltez adimensional de pandeo lateral lambda_bar_LT = sqrt(Wy * fy / Mcr)
        double chi_lt = 1.0;
        double lambda_bar_lt = 0.0;
        const bool susceptible_lt = (prof.tipo_forma == model::GeometriaSeccionTipo::DobleT ||
                                     prof.tipo_forma == model::GeometriaSeccionTipo::CanalUPN ||
                                     prof.familia == model::FamiliaMaterialPerfil::AceroConformado);
        if (susceptible_lt && mcr_min_knm < 1.0e8) {
            lambda_bar_lt = std::sqrt((wy_m3 * fy_mpa * 1000.0) / mcr_min_knm);
            const double alpha_lt = (prof.h_mm / std::max(1.0, prof.b_mm) <= 2.0) ? 0.21 : 0.34;
            chi_lt = factor_reduccion_pandeo_chi(lambda_bar_lt, alpha_lt, 0.40);
        }
        const double mb_rd_knm = (chi_lt * wy_m3 * fy_mpa * 1000.0) / gamma_m1;

        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "PANDEO_LATERAL";
            art.titulo_comprobacion = "Resistencia a flexión y pandeo lateral (chi_LT)";
            art.referencia_normativa = es_aluminio
                ? "EN 1999-1-1 Art. 6.3.2"
                : "CTE DB SE-A Art. 6.3.3 / EN 1993-1-1 Art. 6.3.2";
            art.valor_solicitante = my_max_knm;
            art.valor_resistente_o_limite = mb_rd_knm;
            art.unidad = "kN·m";
            if (my_max_knm <= 1.0e-3) {
                art.estado = EstadoArticuloComprobacion::NoProcede;
                art.aprovechamiento_pct = 0.0;
                art.nota_justificativa = "Momento flector My nulo.";
            } else {
                art.aprovechamiento_pct = (my_max_knm / std::max(1e-6, mb_rd_knm)) * 100.0;
                art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
                art.combinacion_pesima = comb_my;
                art.abscisa_pesima_m = x_my;
                art.expresion_formula = "M_y,Ed / M_b,Rd = " + fmt_num(my_max_knm, 2) + " / " + fmt_num(mb_rd_knm, 2) +
                                        " kN·m (M_cr=" + fmt_num(mcr_min_knm, 1) + " kN·m, lambda_LT=" +
                                        fmt_num(lambda_bar_lt, 3) + ", chi_LT=" + fmt_num(chi_lt, 3) + ")";
            }
            res.articulos.push_back(art);
        }

        // 7. Resistencia a torsión uniforme (CTE DB SE-A Art. 6.2.7 / EN 1993-1-1 Art. 6.2.7)
        const double t_max_mm = std::max(1.0, std::max(prof.tf_mm, prof.tw_mm));
        const double wt_m3 = std::max(1.0e-7, prof.it_m4() / (t_max_mm * 0.001));
        const double t_rd_knm = (wt_m3 * tau_y_mpa * 1000.0) / gamma_m0;
        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "TORSION";
            art.titulo_comprobacion = "Resistencia a torsión";
            art.referencia_normativa = "CTE DB SE-A Art. 6.2.7 / EN 1993-1-1 Art. 6.2.7";
            art.valor_solicitante = mt_max_knm;
            art.valor_resistente_o_limite = t_rd_knm;
            art.unidad = "kN·m";
            if (mt_max_knm <= 1.0e-3) {
                art.estado = EstadoArticuloComprobacion::NoProcede;
                art.aprovechamiento_pct = 0.0;
                art.nota_justificativa = "Momento torsor solicitante despreciable.";
            } else {
                art.aprovechamiento_pct = (mt_max_knm / std::max(1e-6, t_rd_knm)) * 100.0;
                art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
                art.combinacion_pesima = comb_mt;
                art.abscisa_pesima_m = x_mt;
                art.expresion_formula = "T_Ed / T_Rd = " + fmt_num(mt_max_knm, 2) + " / " + fmt_num(t_rd_knm, 2) + " kN·m <= 1.0";
            }
            res.articulos.push_back(art);
        }

        // 8. Interacción de Esfuerzos N + My + Mz + Vy + Vz (CTE DB SE-A Art. 6.2.8 y 6.3.4 / EN 1993-1-1 Art. 6.3.3)
        double max_interaccion = 0.0;
        std::string comb_inter_pesima = muestras_elu.front().nombre_comb;
        double x_inter_pesima = 0.0;
        double kyy_pes = 1.0;
        double kzz_pes = 1.0;

        for (const auto& m : muestras_elu) {
            // Reducción de resistencia a flexión por cortante alto cuando V_Ed > 0.5 * V_pl,Rd (Art. 6.2.8)
            double rho_v_z = 0.0;
            if (std::abs(m.vz_kn) > 0.50 * vz_rd_kn && vz_rd_kn > 1e-6) {
                const double ratio_v = std::min(1.0, std::abs(m.vz_kn) / vz_rd_kn);
                rho_v_z = (2.0 * ratio_v - 1.0) * (2.0 * ratio_v - 1.0);
            }
            const double red_v = std::clamp(1.0 - 0.35 * rho_v_z, 0.50, 1.0);

            if (m.n_kn < 0.0 && !barra.es_tirante) {
                const double nc_ed = -m.n_kn;
                const double n_ratio_y = nc_ed / std::max(1e-6, (chi_y * a_calc_m2 * fy_mpa * 1000.0) / gamma_m1);
                const double n_ratio_z = nc_ed / std::max(1e-6, (chi_z * a_calc_m2 * fy_mpa * 1000.0) / gamma_m1);

                // Factores de interacción k_yy y k_zz (Método Anexo B EC3-1-1 / CTE DB SE-A)
                const double cmy = 0.90;
                const double cmz = 0.90;
                const double kyy = cmy * (1.0 + std::min(0.80, (lambda_bar_y - 0.20)) * n_ratio_y);
                const double kzz = cmz * (1.0 + std::min(1.40, (2.0 * lambda_bar_z - 0.60)) * n_ratio_z);
                const double kyz = 0.60 * kzz;
                const double kzy = 0.60 * kyy;

                const double term_my = std::abs(m.my_knm) / std::max(1e-6, red_v * mb_rd_knm);
                const double term_mz = std::abs(m.mz_knm) / std::max(1e-6, (wz_m3 * fy_mpa * 1000.0) / gamma_m1);

                const double eq_6_61 = n_ratio_y + kyy * term_my + kyz * term_mz;
                const double eq_6_62 = n_ratio_z + kzy * term_my + kzz * term_mz;
                const double u = std::max(eq_6_61, eq_6_62);
                if (u >= max_interaccion) {
                    max_interaccion = u;
                    comb_inter_pesima = m.nombre_comb;
                    x_inter_pesima = m.x_m;
                    kyy_pes = kyy;
                    kzz_pes = kzz;
                }
            } else {
                // Flexotracción
                const double n_ratio = std::max(0.0, m.n_kn) / std::max(1e-6, nt_rd_kn);
                const double term_my = std::abs(m.my_knm) / std::max(1e-6, red_v * mc_y_rd_knm);
                const double term_mz = std::abs(m.mz_knm) / std::max(1e-6, mc_z_rd_knm);
                const double u = n_ratio + term_my + term_mz;
                if (u >= max_interaccion) {
                    max_interaccion = u;
                    comb_inter_pesima = m.nombre_comb;
                    x_inter_pesima = m.x_m;
                }
            }
        }

        {
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "INTERACCION_N_MY_MZ";
            art.titulo_comprobacion = "Interacción de esfuerzos (Axil + Flexión esviada + Cortante)";
            art.referencia_normativa = es_aluminio
                ? "EN 1999-1-1 Art. 6.3.3"
                : "CTE DB SE-A Art. 6.3.4 / EN 1993-1-1 Art. 6.3.3 (Ecs. 6.61 y 6.62)";
            art.valor_solicitante = max_interaccion;
            art.valor_resistente_o_limite = 1.00;
            art.unidad = "adim";
            art.aprovechamiento_pct = max_interaccion * 100.0;
            art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = comb_inter_pesima;
            art.abscisa_pesima_m = x_inter_pesima;
            art.expresion_formula = "N_Ed/(chi*N_Rk/gamma_M1) + k_yy*M_y,Ed/M_b,Rd + k_yz*M_z,Ed/M_z,Rd = " +
                                    fmt_num(max_interaccion, 3) + " <= 1.0 (k_yy=" + fmt_num(kyy_pes, 2) + ", k_zz=" + fmt_num(kzz_pes, 2) + ")";
            res.articulos.push_back(art);
        }
    }

    // ==========================================================================
    // COMPROBACIÓN E.L.S. DE FLECHA (`compflecha.dll`: `cfcalc_*`)
    // ==========================================================================
    double l_ref_flecha_m = res.longitud_barra_m;
    if (barra.flecha.grupo_flecha_id != 0) {
        for (const auto& gf : obra.estructura().grupos_flecha) {
            if (gf.id == barra.flecha.grupo_flecha_id && gf.longitud_referencia_m > 0.0) {
                l_ref_flecha_m = gf.longitud_referencia_m;
                break;
            }
        }
    }
    const double f_adm_mm = barra.flecha.flecha_total_plazo_infinito.flecha_admisible_mm(l_ref_flecha_m);
    {
        ArticuloComprobacionNormativa art{};
        art.codigo_id = "FLECHA_ELS";
        art.titulo_comprobacion = "Estado Límite de Servicio: Limitación de flecha";
        art.referencia_normativa = "CTE DB SE Art. 4.3.3 / EN 1990 Anexo A1.4";
        art.valor_solicitante = flecha_els_max_mm;
        art.valor_resistente_o_limite = f_adm_mm;
        art.unidad = "mm";
        if (!barra.flecha.flecha_total_plazo_infinito.activa && !barra.flecha.flecha_activa.activa) {
            art.estado = EstadoArticuloComprobacion::NoProcede;
            art.aprovechamiento_pct = 0.0;
        } else {
            art.aprovechamiento_pct = (f_adm_mm > 1e-6) ? (flecha_els_max_mm / f_adm_mm) * 100.0 : 0.0;
            art.estado = (art.aprovechamiento_pct <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = comb_pesima_flecha;
            art.abscisa_pesima_m = 0.5 * res.longitud_barra_m;
            const double rel_l_n = (flecha_els_max_mm > 1e-5) ? ((l_ref_flecha_m * 1000.0) / flecha_els_max_mm) : 99999.0;
            art.expresion_formula = "f_max = " + fmt_num(flecha_els_max_mm, 2) + " mm (L/" + fmt_num(rel_l_n, 0) +
                                    ") <= f_adm = " + fmt_num(f_adm_mm, 2) + " mm (L/" +
                                    fmt_num(barra.flecha.flecha_total_plazo_infinito.divisor_relativo_l_n, 0) + ")";
        }
        res.articulos.push_back(art);
        res.aprovechamiento_els_max_pct = art.aprovechamiento_pct;
        res.cumple_els_flecha = (art.estado != EstadoArticuloComprobacion::NoCumple);
    }

    // ==========================================================================
    // COMPROBACIÓN DE RESISTENCIA AL FUEGO (`rstfuego.dll`: `rfcomprevinc_calcula`, `inc_acero_*`)
    // ==========================================================================
    if (dg.resistencia_al_fuego_activa) {
        ConfiguracionResistenciaFuego cfg_fuego{};
        cfg_fuego.comprobar_resistencia_fuego = true;
        if (dg.clase_resistencia_fuego.find("15") != std::string::npos) cfg_fuego.resistencia_requerida = ClaseResistenciaFuego::R15;
        else if (dg.clase_resistencia_fuego.find("30") != std::string::npos) cfg_fuego.resistencia_requerida = ClaseResistenciaFuego::R30;
        else if (dg.clase_resistencia_fuego.find("45") != std::string::npos) cfg_fuego.resistencia_requerida = ClaseResistenciaFuego::R45;
        else if (dg.clase_resistencia_fuego.find("90") != std::string::npos) cfg_fuego.resistencia_requerida = ClaseResistenciaFuego::R90;
        else if (dg.clase_resistencia_fuego.find("120") != std::string::npos) cfg_fuego.resistencia_requerida = ClaseResistenciaFuego::R120;
        else if (dg.clase_resistencia_fuego.find("180") != std::string::npos) cfg_fuego.resistencia_requerida = ClaseResistenciaFuego::R180;
        else if (dg.clase_resistencia_fuego.find("240") != std::string::npos) cfg_fuego.resistencia_requerida = ClaseResistenciaFuego::R240;
        else cfg_fuego.resistencia_requerida = ClaseResistenciaFuego::R60;

        if (dg.revestimiento_fuego_acero.find("Sin") != std::string::npos) {
            cfg_fuego.tipo_proteccion_acero = TipoRevestimientoIgnifugo::SinProteccion;
        } else if (dg.revestimiento_fuego_acero.find("Mortero") != std::string::npos) {
            cfg_fuego.tipo_proteccion_acero = TipoRevestimientoIgnifugo::MorteroProyeccion;
        } else {
            cfg_fuego.tipo_proteccion_acero = TipoRevestimientoIgnifugo::PinturaIntumescente;
        }

        const double t_min = static_cast<double>(cfg_fuego.tiempo_requerido_minutos());

        if (prof.familia == model::FamiliaMaterialPerfil::Madera) {
            // Método de la sección reducida por carbonización (CTE DB SI 6 / EN 1995-1-2 Art. 4.2.2)
            // d_ef = beta_0 * t + k_0 * d_0 (con d_0 = 7 mm)
            const double d_char_mm = cfg_fuego.velocidad_carbonizacion_madera_mm_min * t_min + 7.0;
            const double b_res_mm = std::max(0.0, prof.b_mm - 2.0 * d_char_mm);
            const double h_res_mm = std::max(0.0, prof.h_mm - 2.0 * d_char_mm);
            const double ratio_w_res = (b_res_mm * h_res_mm * h_res_mm) / std::max(1.0, prof.b_mm * prof.h_mm * prof.h_mm);
            // En situación accidental de incendio, solicitación E_fi,d ≈ 0.60 * E_d y k_mod,fi = 1.0, gamma_M,fi = 1.0
            const double eta_fuego = (ratio_w_res > 0.05)
                ? (0.45 * (res.articulos[3].aprovechamiento_pct) / ratio_w_res)
                : 999.0;

            ArticuloComprobacionNormativa art{};
            art.codigo_id = "INCENDIO_MADERA";
            art.titulo_comprobacion = "Resistencia al fuego (Sección reducida por carbonización " + cfg_fuego.etiqueta_clase() + ")";
            art.referencia_normativa = "CTE DB SI Anejo E / EN 1995-1-2 Art. 4.2.2";
            art.valor_solicitante = eta_fuego;
            art.valor_resistente_o_limite = 100.0;
            art.unidad = "%";
            art.aprovechamiento_pct = eta_fuego;
            art.estado = (eta_fuego <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = "Situación accidental de incendio (" + cfg_fuego.etiqueta_clase() + ")";
            art.expresion_formula = "d_ef = beta_0*t + 7 mm = " + fmt_num(d_char_mm, 1) +
                                    " mm -> Sección residual " + fmt_num(b_res_mm, 1) + "x" + fmt_num(h_res_mm, 1) + " mm";
            res.articulos.push_back(art);
            res.aprovechamiento_fuego_max_pct = eta_fuego;
            res.cumple_fuego = (art.estado == EstadoArticuloComprobacion::Cumple);
        } else {
            // Acero / Aluminio en situación de incendio (CTE DB SI Anejo D / EN 1993-1-2 Art. 4.2)
            // Grado de utilización en incendio mu_0 = E_fi,d / R_fi,d,0 ≈ 0.55 * (eta_ELU / 100)
            double max_eta_elu_actual = 0.0;
            for (const auto& a : res.articulos) {
                if (a.codigo_id != "FLECHA_ELS" && a.aprovechamiento_pct > max_eta_elu_actual) {
                    max_eta_elu_actual = a.aprovechamiento_pct;
                }
            }
            const double mu_0 = std::clamp(0.55 * (max_eta_elu_actual / 100.0), 0.05, 0.95);
            // Temperatura crítica del acero según EN 1993-1-2 Ec. (4.22):
            // theta_a,cr = 39.19 * ln(1 / (0.9674 * mu_0^3.833) - 1) + 482
            const double denom_mu = std::max(0.01, 0.9674 * std::pow(mu_0, 3.833));
            const double arg_ln = std::max(0.05, (1.0 / denom_mu) - 1.0);
            res.temperatura_critica_fuego_c = std::clamp(39.19 * std::log(arg_ln) + 482.0, 420.0, 720.0);

            const auto* rev = (cfg_fuego.tipo_proteccion_acero == TipoRevestimientoIgnifugo::MorteroProyeccion)
                ? &NormativaEngine::catalogo_morteros_intumescentes().front()
                : &NormativaEngine::catalogo_pinturas_intumescentes().front();

            if (cfg_fuego.tipo_proteccion_acero == TipoRevestimientoIgnifugo::SinProteccion) {
                const double theta_gas = 20.0 + 345.0 * std::log10(8.0 * t_min + 1.0);
                const double factor_masiv = 1.0 - std::exp(-0.0022 * prof.masividad_am_v_m_inv * (t_min / 15.0));
                res.temperatura_alcanzada_fuego_c = 20.0 + (theta_gas - 20.0) * factor_masiv;
                res.espesor_proteccion_necesario_mm = 0.0;
            } else {
                res.espesor_proteccion_necesario_mm = cfg_fuego.calcular_espesor_proteccion_necesario_mm(
                    prof.masividad_am_v_m_inv, res.temperatura_critica_fuego_c, *rev
                );
                res.temperatura_alcanzada_fuego_c = res.temperatura_critica_fuego_c * 0.92;
            }

            const double eta_fuego = (res.temperatura_alcanzada_fuego_c / std::max(100.0, res.temperatura_critica_fuego_c)) * 100.0;
            ArticuloComprobacionNormativa art{};
            art.codigo_id = "INCENDIO_ACERO";
            art.titulo_comprobacion = "Resistencia al fuego (" + cfg_fuego.etiqueta_clase() + ", Am/V = " + fmt_num(prof.masividad_am_v_m_inv, 0) + " m^-1)";
            art.referencia_normativa = "CTE DB SI Anejo D / EN 1993-1-2 Art. 4.2";
            art.valor_solicitante = res.temperatura_alcanzada_fuego_c;
            art.valor_resistente_o_limite = res.temperatura_critica_fuego_c;
            art.unidad = "°C";
            art.aprovechamiento_pct = eta_fuego;
            art.estado = (eta_fuego <= 100.0) ? EstadoArticuloComprobacion::Cumple : EstadoArticuloComprobacion::NoCumple;
            art.combinacion_pesima = "Situación accidental de incendio (" + cfg_fuego.etiqueta_clase() + ")";
            art.expresion_formula = "theta_a = " + fmt_num(res.temperatura_alcanzada_fuego_c, 1) +
                                    " °C <= theta_cr = " + fmt_num(res.temperatura_critica_fuego_c, 1) +
                                    " °C (Espesor revestimiento d_p = " + fmt_num(res.espesor_proteccion_necesario_mm, 2) + " mm)";
            res.articulos.push_back(art);
            res.aprovechamiento_fuego_max_pct = eta_fuego;
            res.cumple_fuego = (art.estado == EstadoArticuloComprobacion::NoCumple) ? false : true;
        }
    }

    // ==========================================================================
    // SÍNTESIS DE CUMPLIMIENTO GLOBAL Y APROVECHAMIENTO PÉSIMO (`cscomps_*`)
    // ==========================================================================
    res.cumple_elu = true;
    res.aprovechamiento_elu_max_pct = 0.0;
    res.aprovechamiento_global_max_pct = 0.0;

    for (const auto& art : res.articulos) {
        if (art.estado == EstadoArticuloComprobacion::NoProcede ||
            art.estado == EstadoArticuloComprobacion::Informacion) {
            continue;
        }
        const bool es_els = (art.codigo_id == "FLECHA_ELS");
        const bool es_fuego = (art.codigo_id.find("INCENDIO") != std::string::npos);
        if (!es_els && !es_fuego) {
            if (art.estado == EstadoArticuloComprobacion::NoCumple) {
                res.cumple_elu = false;
            }
            if (art.aprovechamiento_pct > res.aprovechamiento_elu_max_pct) {
                res.aprovechamiento_elu_max_pct = art.aprovechamiento_pct;
            }
        }
        if (art.aprovechamiento_pct >= res.aprovechamiento_global_max_pct) {
            res.aprovechamiento_global_max_pct = art.aprovechamiento_pct;
            res.articulo_pesimo_titulo = art.titulo_comprobacion;
            res.combinacion_pesima_global = art.combinacion_pesima;
        }
    }

    res.cumple_global = res.cumple_elu && res.cumple_els_flecha && res.cumple_fuego;
    return res;
}

ResumenComprobacionObra MemberDesignEngine::comprobar_todas_las_barras(
    const model::Obra& obra,
    const solver::SolverSummary& solver_results
) {
    ResumenComprobacionObra resumen{};
    std::ostringstream oss_listado;
    oss_listado << "=======================================================================================================\n";
    oss_listado << "RESUMEN DE COMPROBACIONES E.L.U. / E.L.S. / INCENDIO DE LA ESTRUCTURA ("
                << obra.estructura().barras.size() << " BARRAS)\n";
    oss_listado << "=======================================================================================================\n";

    for (const auto& barra : obra.estructura().barras) {
        auto comp = comprobar_barra(obra, barra, solver_results);
        if (comp.cumple_global) {
            ++resumen.barras_que_cumplen;
        } else {
            ++resumen.barras_que_no_cumplen;
        }
        if (comp.aprovechamiento_global_max_pct >= resumen.aprovechamiento_maximo_obra_pct) {
            resumen.aprovechamiento_maximo_obra_pct = comp.aprovechamiento_global_max_pct;
            resumen.barra_mas_solicitada_id = barra.id;
        }
        oss_listado << comp.generar_listado_resumido() << "\n";
        resumen.comprobaciones_barras.push_back(std::move(comp));
    }

    resumen.listado_resumido_estructura = oss_listado.str();
    return resumen;
}

ResultadoDimensionamientoBarra MemberDesignEngine::dimensionar_barra_en_serie(
    model::Obra& obra,
    std::uint32_t barra_id,
    const solver::SolverSummary& solver_results,
    ModoDimensionamientoSerie modo,
    bool aplicar_cambio_en_obra
) {
    ResultadoDimensionamientoBarra out{};
    out.barra_id = barra_id;

    auto* barra = obra.find_barra_mut(barra_id);
    if (!barra) {
        return out;
    }

    out.perfil_inicial = barra->perfil;
    out.perfil_optimo_seleccionado = barra->perfil;

    const auto prof_inicial = resolver_perfil_efectivo_barra(obra, *barra);
    out.serie_analizada = prof_inicial.serie;
    out.peso_inicial_kg_m = prof_inicial.peso_kg_m;
    out.peso_optimo_kg_m = prof_inicial.peso_kg_m;

    auto candidatos = obra.estructura().biblioteca_perfiles.profiles_in_series(prof_inicial.serie);
    if (candidatos.empty()) {
        candidatos.push_back(prof_inicial);
    }

    // Ordenar la serie por peso lineal creciente (kg/m) para selección de peso mínimo (`pncmpser_*`)
    std::sort(candidatos.begin(), candidatos.end(),
              [](const model::ProfileSectionProperties& a, const model::ProfileSectionProperties& b) {
                  return a.peso_kg_m < b.peso_kg_m;
              });

    const std::string backup_perfil = barra->perfil;
    bool elegido = false;

    for (const auto& cand : candidatos) {
        if (modo == ModoDimensionamientoSerie::DesdePerfilActual &&
            cand.peso_kg_m + 1e-4 < prof_inicial.peso_kg_m) {
            continue;
        }

        barra->perfil = cand.referencia;
        const auto comp = comprobar_barra(obra, *barra, solver_results);

        EvaluacionPerfilEnSerie ev{};
        ev.perfil_referencia = cand.referencia;
        ev.peso_kg_m = cand.peso_kg_m;
        ev.area_cm2 = cand.area_cm2;
        ev.h_mm = cand.h_mm;
        ev.cumple_global = comp.cumple_global;
        ev.cumple_elu = comp.cumple_elu;
        ev.cumple_els = comp.cumple_els_flecha;
        ev.cumple_fuego = comp.cumple_fuego;
        ev.aprovechamiento_elu_pct = comp.aprovechamiento_elu_max_pct;
        ev.aprovechamiento_els_pct = comp.aprovechamiento_els_max_pct;
        ev.aprovechamiento_fuego_pct = comp.aprovechamiento_fuego_max_pct;
        ev.aprovechamiento_max_pct = comp.aprovechamiento_global_max_pct;
        ev.clase_seccion = comp.clasificacion_seccion.clase_global;
        out.tabla_comparativa_serie.push_back(ev);

        if (!elegido && comp.cumple_global) {
            elegido = true;
            out.encontrado_perfil_que_cumple = true;
            out.perfil_optimo_seleccionado = cand.referencia;
            out.peso_optimo_kg_m = cand.peso_kg_m;
            out.aprovechamiento_optimo_pct = comp.aprovechamiento_global_max_pct;
        }
    }

    if (!elegido && !out.tabla_comparativa_serie.empty()) {
        // Si ninguno de la serie llega al 100%, asignar el de mayor capacidad de la serie
        const auto& ultimo = out.tabla_comparativa_serie.back();
        out.encontrado_perfil_que_cumple = false;
        out.perfil_optimo_seleccionado = ultimo.perfil_referencia;
        out.peso_optimo_kg_m = ultimo.peso_kg_m;
        out.aprovechamiento_optimo_pct = ultimo.aprovechamiento_max_pct;
    }

    if (aplicar_cambio_en_obra) {
        barra->perfil = out.perfil_optimo_seleccionado;
        out.perfil_modificado = (barra->perfil != backup_perfil);
        // Si la barra pertenece a una agrupación (`AgrupacionBarras`), sincronizar el perfil en todas las barras del grupo
        if (barra->agrupacion_id != 0) {
            for (auto& ag : obra.estructura_mut().agrupaciones) {
                if (ag.id == barra->agrupacion_id) {
                    ag.perfil_comun = out.perfil_optimo_seleccionado;
                    for (std::uint32_t bid : ag.barra_ids) {
                        if (auto* b_miembro = obra.find_barra_mut(bid)) {
                            b_miembro->perfil = out.perfil_optimo_seleccionado;
                        }
                    }
                    break;
                }
            }
        }
    } else {
        barra->perfil = backup_perfil;
        out.perfil_modificado = false;
    }

    return out;
}

ResumenComprobacionObra MemberDesignEngine::dimensionar_estructura_completa(
    model::Obra& obra,
    solver::SolverSummary& solver_results,
    ModoDimensionamientoSerie modo,
    bool recalcular_rigidez_tras_dimensionar
) {
    if (!solver_results.converged || solver_results.resultados_hipotesis.empty()) {
        solver_results = solver::SolverEngine::analyze_linear_static(obra);
    }

    std::vector<ResultadoDimensionamientoBarra> dims;
    dims.reserve(obra.estructura().barras.size());

    // Dimensionar barras individuales y agrupaciones (eligiendo para cada agrupación el perfil más pesado requerido por sus miembros)
    for (const auto& barra : obra.estructura().barras) {
        auto d = dimensionar_barra_en_serie(obra, barra.id, solver_results, modo, true);
        dims.push_back(std::move(d));
    }

    // Armonizar agrupaciones al perfil de mayor capacidad requerido entre sus barras
    for (auto& ag : obra.estructura_mut().agrupaciones) {
        double max_peso = 0.0;
        std::string mejor_perfil = ag.perfil_comun;
        for (std::uint32_t bid : ag.barra_ids) {
            if (const auto* b = obra.find_barra(bid)) {
                const auto p = resolver_perfil_efectivo_barra(obra, *b);
                if (p.peso_kg_m >= max_peso) {
                    max_peso = p.peso_kg_m;
                    mejor_perfil = b->perfil;
                }
            }
        }
        ag.perfil_comun = mejor_perfil;
        for (std::uint32_t bid : ag.barra_ids) {
            if (auto* b = obra.find_barra_mut(bid)) {
                b->perfil = mejor_perfil;
            }
        }
    }

    if (recalcular_rigidez_tras_dimensionar) {
        solver_results = solver::SolverEngine::analyze_linear_static(obra);
    }

    auto resumen = comprobar_todas_las_barras(obra, solver_results);
    resumen.dimensionamientos_barras = std::move(dims);
    return resumen;
}

// ==============================================================================
// P-10.2 — Editores Avanzados de Pilares y Vigas de Hormigón Armado
// (`pilares.dll`, `pilararm.dll`, `visesfpil.dll`, `porvigas.dll`, `seccviga.dll`, `comphorm.dll`)
// ==============================================================================

const std::vector<std::uint32_t>& ReinforcedConcreteEditorEngine::diametros_comerciales_mm() {
    static const std::vector<std::uint32_t> kDiams = {8, 10, 12, 16, 20, 25, 32};
    return kDiams;
}

namespace {

struct CapaArmaduraSeccion {
    double coord_m{0.0}; ///< Coordenada desde el borde más comprimido [0..h] (m)
    double area_m2{0.0};
};

std::vector<CapaArmaduraSeccion> construir_capas_armadura_pilar(
    const model::TramoPilarPlanta& tramo,
    bool alrededor_eje_y
) {
    std::vector<CapaArmaduraSeccion> capas;
    const bool circular = (tramo.tipo_seccion == model::TipoSeccionPilar::HormigonCircular ||
                           tramo.tipo_seccion == model::TipoSeccionPilar::MixtoTubularRellenoHormigon);

    if (circular) {
        const double r_ext = 0.5 * tramo.diametro_m;
        const double r_arm = std::max(0.04, r_ext - tramo.recubrimiento_mecanico_m);
        const std::uint32_t nb = std::max<std::uint32_t>(4, tramo.armado.num_barras_circular);
        const double a_bar_m2 = area_una_barra_cm2(tramo.armado.diametro_circular_mm) * 1.0e-4;
        for (std::uint32_t i = 0; i < nb; ++i) {
            const double theta = (2.0 * std::numbers::pi * static_cast<double>(i)) / static_cast<double>(nb);
            const double y_from_top = r_ext - r_arm * std::cos(theta);
            capas.push_back({y_from_top, a_bar_m2});
        }
        return capas;
    }

    const double h_m = alrededor_eje_y ? tramo.ancho_x_m : tramo.ancho_y_m;
    const double d_rec = std::min(0.35 * h_m, std::max(0.030, tramo.recubrimiento_mecanico_m));
    const double y_top = d_rec;
    const double y_bot = h_m - d_rec;

    const double a_esq_m2 = area_una_barra_cm2(tramo.armado.diametro_esquina_mm) * 1.0e-4;
    const std::uint32_t n_cara_paralela = alrededor_eje_y ? tramo.armado.num_barras_cara_x : tramo.armado.num_barras_cara_y;
    const std::uint32_t d_cara_paralela = alrededor_eje_y ? tramo.armado.diametro_cara_x_mm : tramo.armado.diametro_cara_y_mm;
    const std::uint32_t n_cara_perp = alrededor_eje_y ? tramo.armado.num_barras_cara_y : tramo.armado.num_barras_cara_x;
    const std::uint32_t d_cara_perp = alrededor_eje_y ? tramo.armado.diametro_cara_y_mm : tramo.armado.diametro_cara_x_mm;

    const double a_cara_perp_m2 = static_cast<double>(n_cara_perp) * area_una_barra_cm2(d_cara_perp) * 1.0e-4;

    // Capa superior comprimida (2 esquinas + barras de la cara extrema comprimida)
    capas.push_back({y_top, 2.0 * a_esq_m2 + a_cara_perp_m2});
    // Capa inferior traccionada (2 esquinas + barras de la cara extrema traccionada)
    capas.push_back({y_bot, 2.0 * a_esq_m2 + a_cara_perp_m2});

    // Barras intermedias distribuidas en las 2 caras laterales paralelas al plano de flexión
    if (n_cara_paralela > 0) {
        const double a_inter_par_m2 = 2.0 * area_una_barra_cm2(d_cara_paralela) * 1.0e-4;
        for (std::uint32_t j = 1; j <= n_cara_paralela; ++j) {
            const double frac = static_cast<double>(j) / static_cast<double>(n_cara_paralela + 1);
            const double y_j = y_top + frac * (y_bot - y_top);
            capas.push_back({y_j, a_inter_par_m2});
        }
    }

    return capas;
}

} // namespace

std::vector<PuntoDiagramaInteraccionNM> ReinforcedConcreteEditorEngine::generar_diagrama_interaccion_pilar(
    const model::TramoPilarPlanta& tramo,
    double fcd_mpa,
    double fyd_mpa,
    double es_mpa,
    bool alrededor_eje_y,
    std::size_t num_puntos
) {
    std::vector<PuntoDiagramaInteraccionNM> curva;
    const std::size_t n_pts = std::max<std::size_t>(9, num_puntos);
    curva.reserve(n_pts);

    const bool circular = (tramo.tipo_seccion == model::TipoSeccionPilar::HormigonCircular ||
                           tramo.tipo_seccion == model::TipoSeccionPilar::MixtoTubularRellenoHormigon);
    const double h_m = circular ? tramo.diametro_m : (alrededor_eje_y ? tramo.ancho_x_m : tramo.ancho_y_m);
    const double b_m = circular ? (0.25 * std::numbers::pi * tramo.diametro_m) : (alrededor_eje_y ? tramo.ancho_y_m : tramo.ancho_x_m);
    const double d_max_m = std::max(0.06, h_m - tramo.recubrimiento_mecanico_m);

    const auto capas = construir_capas_armadura_pilar(tramo, alrededor_eje_y);
    double as_tot_m2 = 0.0;
    for (const auto& c : capas) as_tot_m2 += c.area_m2;

    // 1. Punto extremo superior: Compresión centrada pura (Dominio 5, x -> +inf, eps_c = 2.0 por mil)
    {
        PuntoDiagramaInteraccionNM p{};
        p.profundidad_fibra_neutra_x_m = 10.0 * h_m;
        p.deformacion_hormigon_max_permil = 2.0;
        p.deformacion_acero_max_permil = -2.0;
        p.axil_resistente_n_rd_kn = tramo.capacidad_axil_plastica_kn(fcd_mpa, fyd_mpa);
        curva.push_back(p);
    }

    // 2. Barrido de profundidades de fibra neutra x desde 1.25*h hasta 0.05*h (Dominios 4a, 4, 3 y 2)
    const std::size_t n_inter = n_pts - 2;
    for (std::size_t i = 0; i < n_inter; ++i) {
        const double u = static_cast<double>(i + 1) / static_cast<double>(n_inter + 1);
        const double x_m = (1.20 - 1.12 * u) * h_m;

        // Bloque rectangular equivalente de Whitney / EC2 Art. 3.1.7 (lambda = 0.80, eta = 1.0, alpha_cc = 0.85)
        const double y_comp_m = std::clamp(0.80 * x_m, 0.0, h_m);
        const double fc_kn = 0.85 * (fcd_mpa * 1000.0) * b_m * y_comp_m;
        const double m_hormigon_knm = fc_kn * (0.5 * h_m - 0.5 * y_comp_m);

        // Ley de deformaciones compatible con pivote B (eps_cu2 = 3.5 por mil en borde comprimido) o pivote A (eps_su = 10 por mil)
        const double eps_cu = 0.0035;
        double n_acero_kn = 0.0;
        double m_acero_knm = 0.0;
        double max_eps_trac = 0.0;

        for (const auto& capa : capas) {
            // Deformación en la capa (> 0 compresión, < 0 tracción)
            const double eps_s = eps_cu * (x_m - capa.coord_m) / std::max(1.0e-4, x_m);
            const double sigma_s_mpa = std::clamp(eps_s * es_mpa, -fyd_mpa, +fyd_mpa);
            const double fs_kn = capa.area_m2 * (sigma_s_mpa * 1000.0);
            n_acero_kn += fs_kn;
            m_acero_knm += fs_kn * (0.5 * h_m - capa.coord_m);
            if (-eps_s > max_eps_trac) {
                max_eps_trac = -eps_s;
            }
        }

        PuntoDiagramaInteraccionNM p{};
        p.profundidad_fibra_neutra_x_m = x_m;
        p.deformacion_hormigon_max_permil = 3.5;
        p.deformacion_acero_max_permil = max_eps_trac * 1000.0;
        p.axil_resistente_n_rd_kn = fc_kn + n_acero_kn;
        const double m_tot = std::max(0.0, m_hormigon_knm + m_acero_knm);
        if (alrededor_eje_y) {
            p.momento_resistente_my_rd_knm = m_tot;
        } else {
            p.momento_resistente_mz_rd_knm = m_tot;
        }
        p.momento_resultante_m_rd_knm = m_tot;
        curva.push_back(p);
    }

    // 3. Punto extremo inferior: Tracción centrada pura (Dominio 1, x -> -inf, eps_s = 10 por mil)
    {
        PuntoDiagramaInteraccionNM p{};
        p.profundidad_fibra_neutra_x_m = 0.0;
        p.deformacion_hormigon_max_permil = 0.0;
        p.deformacion_acero_max_permil = 10.0;
        p.axil_resistente_n_rd_kn = -as_tot_m2 * (fyd_mpa * 1000.0);
        curva.push_back(p);
    }
    (void)d_max_m;

    return curva;
}

namespace {

double interpolar_momento_resistente_en_curva(
    const std::vector<PuntoDiagramaInteraccionNM>& curva,
    double n_ed_comp_kn
) noexcept {
    if (curva.empty()) return 0.0;
    const double n_max = curva.front().axil_resistente_n_rd_kn;
    const double n_min = curva.back().axil_resistente_n_rd_kn;
    if (n_ed_comp_kn > n_max || n_ed_comp_kn < n_min) {
        return 0.0;
    }

    for (std::size_t i = 0; i + 1 < curva.size(); ++i) {
        const double n0 = curva[i].axil_resistente_n_rd_kn;
        const double n1 = curva[i + 1].axil_resistente_n_rd_kn;
        if ((n_ed_comp_kn <= n0 && n_ed_comp_kn >= n1) || (n_ed_comp_kn >= n0 && n_ed_comp_kn <= n1)) {
            const double dn = std::abs(n0 - n1);
            if (dn < 1e-6) return curva[i].momento_resultante_m_rd_knm;
            const double t = std::abs(n_ed_comp_kn - n0) / dn;
            return curva[i].momento_resultante_m_rd_knm +
                   t * (curva[i + 1].momento_resultante_m_rd_knm - curva[i].momento_resultante_m_rd_knm);
        }
    }
    return 0.0;
}

} // namespace

ComprobacionCombinacionTramoPilar ReinforcedConcreteEditorEngine::comprobar_combinacion_tramo_pilar(
    const model::TramoPilarPlanta& tramo,
    std::uint32_t combinacion_id,
    std::string_view nombre_combinacion,
    bool en_cabeza_tramo,
    double ned_compresion_kn,
    double my_ed_knm,
    double mz_ed_knm,
    double vy_ed_kn,
    double vz_ed_kn,
    double fcd_mpa,
    double fyd_mpa
) {
    ComprobacionCombinacionTramoPilar res{};
    res.combinacion_id = combinacion_id;
    res.nombre_combinacion = std::string(nombre_combinacion);
    res.en_cabeza_tramo = en_cabeza_tramo;
    res.ned_compresion_kn = ned_compresion_kn;
    res.my_ed_1er_orden_knm = std::abs(my_ed_knm);
    res.mz_ed_1er_orden_knm = std::abs(mz_ed_knm);
    res.vy_ed_kn = std::abs(vy_ed_kn);
    res.vz_ed_kn = std::abs(vz_ed_kn);

    const bool circular = (tramo.tipo_seccion == model::TipoSeccionPilar::HormigonCircular ||
                           tramo.tipo_seccion == model::TipoSeccionPilar::MixtoTubularRellenoHormigon);
    const double hx_m = circular ? tramo.diametro_m : tramo.ancho_x_m;
    const double hy_m = circular ? tramo.diametro_m : tramo.ancho_y_m;
    const double l_tramo_m = std::max(0.50, tramo.cota_sup_m - tramo.cota_inf_m);

    // 1. Excentricidad mínima accidental e_0 = max(h / 30, 20 mm) y excentricidad de 2.º orden e_2 por curvatura nominal (EC2 Art. 5.8.8 / CE)
    const double e0_x_m = std::max(hx_m / 30.0, 0.020);
    const double e0_y_m = std::max(hy_m / 30.0, 0.020);

    const double l0_x_m = tramo.beta_pandeo_x * l_tramo_m;
    const double l0_y_m = tramo.beta_pandeo_y * l_tramo_m;
    const double ix_m = hx_m / std::sqrt(12.0);
    const double iy_m = hy_m / std::sqrt(12.0);
    const double lambda_x = l0_x_m / std::max(0.02, ix_m);
    const double lambda_y = l0_y_m / std::max(0.02, iy_m);

    double e2_x_m = 0.0;
    double e2_y_m = 0.0;
    if (ned_compresion_kn > 1.0) {
        const double d_x = std::max(0.05, hx_m - tramo.recubrimiento_mecanico_m);
        const double d_y = std::max(0.05, hy_m - tramo.recubrimiento_mecanico_m);
        const double eps_yd = fyd_mpa / 200000.0;
        const double r_inv_0_x = eps_yd / (0.45 * d_x);
        const double r_inv_0_y = eps_yd / (0.45 * d_y);
        if (lambda_x > 25.0) {
            e2_x_m = (r_inv_0_x * l0_x_m * l0_x_m) / 10.0;
        }
        if (lambda_y > 25.0) {
            e2_y_m = (r_inv_0_y * l0_y_m * l0_y_m) / 10.0;
        }
    }

    const double n_pos_kn = std::max(0.0, ned_compresion_kn);
    res.my_ed_amplificado_knm = std::max(res.my_ed_1er_orden_knm + n_pos_kn * e2_x_m, n_pos_kn * e0_x_m);
    res.mz_ed_amplificado_knm = std::max(res.mz_ed_1er_orden_knm + n_pos_kn * e2_y_m, n_pos_kn * e0_y_m);

    // 2. Evaluación sobre las curvas de interacción N-My y N-Mz y contorno de Bresler (EC2 Art. 5.8.9)
    const auto curva_y = generar_diagrama_interaccion_pilar(tramo, fcd_mpa, fyd_mpa, 200000.0, true, 25);
    const auto curva_z = generar_diagrama_interaccion_pilar(tramo, fcd_mpa, fyd_mpa, 200000.0, false, 25);

    res.n_rd_agotamiento_kn = curva_y.front().axil_resistente_n_rd_kn;
    res.my_rd_agotamiento_knm = interpolar_momento_resistente_en_curva(curva_y, ned_compresion_kn);
    res.mz_rd_agotamiento_knm = interpolar_momento_resistente_en_curva(curva_z, ned_compresion_kn);

    // Exponente de Bresler a en [1.0, 2.0] según N_Ed / N_Rd (EC2 Art. 5.8.9 Ec. 5.39)
    const double n_ratio = std::clamp(n_pos_kn / std::max(1.0, res.n_rd_agotamiento_kn), 0.0, 1.2);
    double exp_bresler = 1.15;
    if (circular) {
        exp_bresler = 2.0;
    } else if (n_ratio <= 0.10) {
        exp_bresler = 1.0;
    } else if (n_ratio <= 0.70) {
        exp_bresler = 1.0 + 0.50 * ((n_ratio - 0.10) / 0.60);
    } else {
        exp_bresler = 1.50 + 0.50 * std::min(1.0, (n_ratio - 0.70) / 0.30);
    }

    double eta_nm = 0.0;
    if (ned_compresion_kn > res.n_rd_agotamiento_kn || ned_compresion_kn < curva_y.back().axil_resistente_n_rd_kn) {
        eta_nm = (std::abs(ned_compresion_kn) / std::max(1.0, std::abs(res.n_rd_agotamiento_kn))) * 100.0;
    } else {
        const double ratio_my = res.my_ed_amplificado_knm / std::max(0.5, res.my_rd_agotamiento_knm);
        const double ratio_mz = res.mz_ed_amplificado_knm / std::max(0.5, res.mz_rd_agotamiento_knm);
        const double bresler_sum = std::pow(ratio_my, exp_bresler) + std::pow(ratio_mz, exp_bresler);
        const double ratio_radial = std::pow(bresler_sum, 1.0 / exp_bresler);
        eta_nm = std::max(n_ratio * 100.0, ratio_radial * 100.0);
    }
    res.aprovechamiento_flexocompresion_pct = eta_nm;

    // 3. Estado del plano de deformación en equilibrio (`chcalc_barras_equilibrio`)
    res.profundidad_eje_neutro_x_m = std::clamp((0.25 + 0.75 * n_ratio) * hx_m, 0.05 * hx_m, 1.25 * hx_m);
    res.angulo_eje_neutro_deg = std::atan2(res.mz_ed_amplificado_knm, std::max(0.01, res.my_ed_amplificado_knm)) * (180.0 / std::numbers::pi);
    res.eps_c_max_permil = std::min(3.5, 3.5 * (eta_nm / 100.0));
    res.eps_s_traccion_permil = (res.profundidad_eje_neutro_x_m < 0.85 * hx_m)
        ? (3.5 * (hx_m - res.profundidad_eje_neutro_x_m) / res.profundidad_eje_neutro_x_m)
        : 0.0;

    // 4. Resistencia a cortante del tramo de pilar VRd,c + VRd,s
    const double asw_x_cm2 = static_cast<double>(tramo.armado.num_ramas_x) * area_una_barra_cm2(tramo.armado.diametro_estribo_mm);
    const double sep_m = std::max(0.05, tramo.armado.separacion_zona_critica_cm * 0.01);
    const double z_m = 0.90 * std::max(0.10, std::min(hx_m, hy_m) - tramo.recubrimiento_mecanico_m);
    const double vrd_s_kn = ((asw_x_cm2 * 1.0e-4) / sep_m) * z_m * (fyd_mpa * 1000.0);
    const double vrd_c_kn = 0.12 * std::cbrt(100.0 * 0.01 * fcd_mpa * 1.50) * (tramo.area_bruta_hormigon_cm2() * 1.0e-4) * 1000.0;
    res.v_rd_cortante_kn = vrd_s_kn + vrd_c_kn;

    const double v_ed_tot_kn = std::hypot(res.vy_ed_kn, res.vz_ed_kn);
    res.aprovechamiento_cortante_pct = (res.v_rd_cortante_kn > 1e-6) ? (v_ed_tot_kn / res.v_rd_cortante_kn) * 100.0 : 0.0;
    res.cumple = (res.aprovechamiento_flexocompresion_pct <= 100.0 && res.aprovechamiento_cortante_pct <= 100.0);
    return res;
}

ResumenEditorTramoPilar ReinforcedConcreteEditorEngine::analizar_tramo_pilar_editor(
    const model::Obra& obra,
    const model::PilarEstructural& pilar,
    std::size_t indice_tramo,
    const solver::SolverSummary& solver_results
) {
    ResumenEditorTramoPilar out{};
    out.pilar_id = pilar.id;
    out.referencia_pilar = pilar.referencia;
    out.indice_tramo = indice_tramo;

    if (indice_tramo >= pilar.tramos.size()) {
        return out;
    }

    const auto& tramo = pilar.tramos[indice_tramo];
    out.cota_inf_m = tramo.cota_inf_m;
    out.cota_sup_m = tramo.cota_sup_m;
    out.armado_dispuesto = tramo.armado;

    const bool circular = (tramo.tipo_seccion == model::TipoSeccionPilar::HormigonCircular ||
                           tramo.tipo_seccion == model::TipoSeccionPilar::MixtoTubularRellenoHormigon);
    out.area_hormigon_cm2 = tramo.area_bruta_hormigon_cm2();
    out.area_acero_dispuesta_cm2 = tramo.armado.area_total_longitudinal_cm2(circular);
    out.cuantia_geometrica_pct = tramo.armado.cuantia_geometrica(out.area_hormigon_cm2, circular) * 100.0;

    const auto* mat_h = NormativaEngine::buscar_hormigon(tramo.hormigon);
    const auto* mat_s = NormativaEngine::buscar_acero_armaduras(tramo.acero_barras);
    const double fcd_mpa = mat_h ? mat_h->fcd_mpa(1.50) : 16.67;
    const double fyd_mpa = mat_s ? mat_s->fyd_mpa(1.15) : 434.78;
    const double fctd_mpa = mat_h ? (mat_h->fctm_mpa / 1.50) : 1.70;

    const double h_libre_m = std::max(0.50, tramo.cota_sup_m - tramo.cota_inf_m);
    const double hx_m = circular ? tramo.diametro_m : tramo.ancho_x_m;
    const double hy_m = circular ? tramo.diametro_m : tramo.ancho_y_m;
    out.esbeltez_mecanica_lambda_x = (tramo.beta_pandeo_x * h_libre_m) / std::max(0.02, hx_m / std::sqrt(12.0));
    out.esbeltez_mecanica_lambda_y = (tramo.beta_pandeo_y * h_libre_m) / std::max(0.02, hy_m / std::sqrt(12.0));
    out.esbeltez_limite_inferior_lambda_lim = 25.0;

    // 1. Generar curvas del volumen de interacción N-My y N-Mz
    out.curva_interaccion_n_my = generar_diagrama_interaccion_pilar(tramo, fcd_mpa, fyd_mpa, 200000.0, true, 25);
    out.curva_interaccion_n_mz = generar_diagrama_interaccion_pilar(tramo, fcd_mpa, fyd_mpa, 200000.0, false, 25);

    // 2. Comprobar todas las combinaciones en pie y cabeza del tramo (`visesfpil.dll`: `vepesfycombpil_*`)
    for (const auto& rc : solver_results.resultados_combinaciones) {
        const bool es_els = (rc.codigo.find("ELS") != std::string::npos || rc.descripcion.find("E.L.S.") != std::string::npos);
        if (es_els) continue;

        const auto* eb = rc.find_esfuerzos_barra(tramo.barra_3d_id);
        if (!eb || eb->puntos.empty()) continue;

        const auto& pt_pie = eb->puntos.front();
        const auto& pt_cab = eb->puntos.back();
        const std::string nom = rc.descripcion.empty() ? rc.codigo : rc.descripcion;

        out.comprobaciones_combinaciones.push_back(comprobar_combinacion_tramo_pilar(
            tramo, rc.id, nom, false,
            -pt_pie.n_kn, pt_pie.my_knm, pt_pie.mz_knm, pt_pie.vy_kn, pt_pie.vz_kn,
            fcd_mpa, fyd_mpa
        ));
        out.comprobaciones_combinaciones.push_back(comprobar_combinacion_tramo_pilar(
            tramo, rc.id, nom, true,
            -pt_cab.n_kn, pt_cab.my_knm, pt_cab.mz_knm, pt_cab.vy_kn, pt_cab.vz_kn,
            fcd_mpa, fyd_mpa
        ));
    }

    if (out.comprobaciones_combinaciones.empty()) {
        // Si no hay resultados en solver para esa barra_3d_id, evaluar un estado gravitatorio + viento representativo
        out.comprobaciones_combinaciones.push_back(comprobar_combinacion_tramo_pilar(
            tramo, 1, "1.35·PP + 1.50·Q1", false,
            0.22 * out.curva_interaccion_n_my.front().axil_resistente_n_rd_kn,
            0.18 * out.curva_interaccion_n_my[out.curva_interaccion_n_my.size() / 2].momento_resultante_m_rd_knm,
            0.10 * out.curva_interaccion_n_mz[out.curva_interaccion_n_mz.size() / 2].momento_resultante_m_rd_knm,
            18.0, 12.0, fcd_mpa, fyd_mpa
        ));
    }

    double max_eta = 0.0;
    bool todas_cumplen = true;
    for (const auto& cc : out.comprobaciones_combinaciones) {
        const double eta_c = std::max(cc.aprovechamiento_flexocompresion_pct, cc.aprovechamiento_cortante_pct);
        if (!cc.cumple) todas_cumplen = false;
        if (eta_c >= max_eta) {
            max_eta = eta_c;
            out.combinacion_pesima = cc;
        }
    }

    // 3. Verificación normativa geométrica, cuantías y capacidad sísmica (`ConcreteStructureEngine`)
    out.verificacion_normativa_y_capacidad = model::ConcreteStructureEngine::verificar_tramo_pilar(
        tramo,
        0.0,
        1.30,
        obra.datos_generales().con_sismo_dinamico,
        fcd_mpa,
        fyd_mpa
    );

    // 4. Despiece de armaduras longitudinales (con longitud de solape `lb,net`) y estribos (con patillas `chcalc_longitud_patilla_estribo`)
    const PropiedadesAceroArmaduras prop_s = mat_s ? *mat_s : NormativaEngine::catalogo_aceros_armaduras().front();
    if (circular) {
        const double l_sol_m = prop_s.longitud_anclaje_basica_mm(tramo.armado.diametro_circular_mm, fctd_mpa) * 0.001;
        PosicionDespieceArmadura pos{};
        pos.posicion_id = 1;
        pos.marca = "Pos. 1 (Barras longitudinales circulares)";
        pos.numero_barras = tramo.armado.num_barras_circular;
        pos.diametro_mm = tramo.armado.diametro_circular_mm;
        pos.longitud_recta_m = h_libre_m;
        pos.longitud_anclaje_o_solape_m = l_sol_m;
        pos.longitud_total_unitaria_m = h_libre_m + l_sol_m;
        pos.peso_total_kg = static_cast<double>(pos.numero_barras) * pos.longitud_total_unitaria_m * peso_lineal_barra_kg_m(pos.diametro_mm);
        out.peso_total_acero_tramo_kg += pos.peso_total_kg;
        out.despiece_armaduras.push_back(pos);
    } else {
        const double l_sol_esq_m = prop_s.longitud_anclaje_basica_mm(tramo.armado.diametro_esquina_mm, fctd_mpa) * 0.001;
        PosicionDespieceArmadura pos_esq{};
        pos_esq.posicion_id = 1;
        pos_esq.marca = "Pos. 1 (Barras de esquina)";
        pos_esq.numero_barras = tramo.armado.num_barras_esquina;
        pos_esq.diametro_mm = tramo.armado.diametro_esquina_mm;
        pos_esq.longitud_recta_m = h_libre_m;
        pos_esq.longitud_anclaje_o_solape_m = l_sol_esq_m;
        pos_esq.longitud_total_unitaria_m = h_libre_m + l_sol_esq_m;
        pos_esq.peso_total_kg = static_cast<double>(pos_esq.numero_barras) * pos_esq.longitud_total_unitaria_m * peso_lineal_barra_kg_m(pos_esq.diametro_mm);
        out.peso_total_acero_tramo_kg += pos_esq.peso_total_kg;
        out.despiece_armaduras.push_back(pos_esq);

        if (tramo.armado.num_barras_cara_x + tramo.armado.num_barras_cara_y > 0) {
            const std::uint32_t nb_caras = 2 * (tramo.armado.num_barras_cara_x + tramo.armado.num_barras_cara_y);
            const std::uint32_t d_caras = std::max(tramo.armado.diametro_cara_x_mm, tramo.armado.diametro_cara_y_mm);
            const double l_sol_c_m = prop_s.longitud_anclaje_basica_mm(d_caras, fctd_mpa) * 0.001;
            PosicionDespieceArmadura pos_c{};
            pos_c.posicion_id = 2;
            pos_c.marca = "Pos. 2 (Barras de caras laterales)";
            pos_c.numero_barras = nb_caras;
            pos_c.diametro_mm = d_caras;
            pos_c.longitud_recta_m = h_libre_m;
            pos_c.longitud_anclaje_o_solape_m = l_sol_c_m;
            pos_c.longitud_total_unitaria_m = h_libre_m + l_sol_c_m;
            pos_c.peso_total_kg = static_cast<double>(pos_c.numero_barras) * pos_c.longitud_total_unitaria_m * peso_lineal_barra_kg_m(pos_c.diametro_mm);
            out.peso_total_acero_tramo_kg += pos_c.peso_total_kg;
            out.despiece_armaduras.push_back(pos_c);
        }
    }

    // Estribos transversales
    {
        const double l_crit = std::min(0.5 * h_libre_m, tramo.armado.longitud_zona_critica_m);
        const double l_cent = std::max(0.0, h_libre_m - 2.0 * l_crit);
        const std::uint32_t n_est = static_cast<std::uint32_t>(
            std::ceil((2.0 * l_crit * 100.0) / std::max(5.0, tramo.armado.separacion_zona_critica_cm)) +
            std::ceil((l_cent * 100.0) / std::max(5.0, tramo.armado.separacion_zona_central_cm))
        ) + 1;
        const double perim_m = 2.0 * ( std::max(0.10, hx_m - 2.0 * tramo.recubrimiento_geometrico_m) +
                                       std::max(0.10, hy_m - 2.0 * tramo.recubrimiento_geometrico_m) );
        const double patilla_m = std::max(0.07, 10.0 * static_cast<double>(tramo.armado.diametro_estribo_mm) * 0.001);

        PosicionDespieceArmadura pos_e{};
        pos_e.posicion_id = static_cast<std::uint32_t>(out.despiece_armaduras.size() + 1);
        pos_e.marca = "Estribos de confinamiento";
        pos_e.numero_barras = n_est;
        pos_e.diametro_mm = tramo.armado.diametro_estribo_mm;
        pos_e.longitud_recta_m = perim_m;
        pos_e.longitud_patilla_m = 2.0 * patilla_m;
        pos_e.longitud_total_unitaria_m = perim_m + 2.0 * patilla_m;
        pos_e.peso_total_kg = static_cast<double>(pos_e.numero_barras) * pos_e.longitud_total_unitaria_m * peso_lineal_barra_kg_m(pos_e.diametro_mm);
        out.peso_total_acero_tramo_kg += pos_e.peso_total_kg;
        out.despiece_armaduras.push_back(pos_e);
    }

    out.aprovechamiento_maximo_pct = max_eta;
    out.cumple_tramo = todas_cumplen && out.verificacion_normativa_y_capacidad.cumple_global;

    std::ostringstream oss;
    oss << "Pilar " << out.referencia_pilar << " - Tramo " << (indice_tramo + 1)
        << " [" << fmt_num(out.cota_inf_m, 2) << " m .. " << fmt_num(out.cota_sup_m, 2) << " m] -> "
        << (out.cumple_tramo ? "CUMPLE" : "NO CUMPLE")
        << " | As = " << fmt_num(out.area_acero_dispuesta_cm2, 2) << " cm^2 (" << fmt_num(out.cuantia_geometrica_pct, 2) << "%)"
        << " | Eta_NMyMz = " << fmt_num(out.aprovechamiento_maximo_pct, 1) << "% (" << out.combinacion_pesima.nombre_combinacion << ")\n";
    out.listado_comprobaciones_tramo = oss.str();

    return out;
}

std::vector<ResumenEditorTramoPilar> ReinforcedConcreteEditorEngine::dimensionar_pilar_completo(
    model::Obra& obra,
    model::PilarEstructural& pilar,
    const solver::SolverSummary& solver_results,
    bool redimensionar_geometria_si_no_cumple
) {
    std::vector<ResumenEditorTramoPilar> resultados;
    resultados.reserve(pilar.tramos.size());

    struct ConfigArmadoPilarTabla {
        std::uint32_t d_esq;
        std::uint32_t n_cara;
        std::uint32_t d_cara;
        std::uint32_t d_est;
        double sep_cent_cm;
        double sep_crit_cm;
    };

    static constexpr ConfigArmadoPilarTabla kTablaArmadosPilares[] = {
        {12, 0, 12, 8, 15.0, 10.0}, // 4 phi 12 (4.52 cm^2)
        {16, 0, 12, 8, 15.0, 10.0}, // 4 phi 16 (8.04 cm^2)
        {16, 1, 12, 8, 15.0, 10.0}, // 4 phi 16 + 4 phi 12 (12.57 cm^2)
        {20, 0, 16, 8, 15.0, 10.0}, // 4 phi 20 (12.57 cm^2)
        {20, 1, 16, 8, 15.0, 10.0}, // 4 phi 20 + 4 phi 16 (20.61 cm^2)
        {20, 2, 16, 8, 15.0,  8.0}, // 4 phi 20 + 8 phi 16 (28.65 cm^2)
        {25, 1, 20, 8, 15.0,  8.0}, // 4 phi 25 + 4 phi 20 (32.20 cm^2)
        {25, 2, 20, 10, 15.0, 8.0}, // 4 phi 25 + 8 phi 20 (44.77 cm^2)
        {25, 3, 25, 10, 15.0, 8.0}  // 16 phi 25 (78.54 cm^2)
    };

    for (std::size_t idx = 0; idx < pilar.tramos.size(); ++idx) {
        auto& tramo = pilar.tramos[idx];
        if (!pilar.bloqueado_armado) {
            bool encontrado = false;
            for (int intento_geo = 0; intento_geo < (redimensionar_geometria_si_no_cumple ? 4 : 1); ++intento_geo) {
                for (const auto& cand : kTablaArmadosPilares) {
                    tramo.armado.num_barras_esquina = 4;
                    tramo.armado.diametro_esquina_mm = cand.d_esq;
                    tramo.armado.num_barras_cara_x = cand.n_cara;
                    tramo.armado.diametro_cara_x_mm = cand.d_cara;
                    tramo.armado.num_barras_cara_y = cand.n_cara;
                    tramo.armado.diametro_cara_y_mm = cand.d_cara;
                    tramo.armado.num_barras_circular = 6 + 2 * cand.n_cara;
                    tramo.armado.diametro_circular_mm = cand.d_esq;
                    tramo.armado.diametro_estribo_mm = cand.d_est;
                    tramo.armado.separacion_zona_central_cm = cand.sep_cent_cm;
                    tramo.armado.separacion_zona_critica_cm = cand.sep_crit_cm;

                    const auto eval = analizar_tramo_pilar_editor(obra, pilar, idx, solver_results);
                    if (eval.cumple_tramo) {
                        encontrado = true;
                        break;
                    }
                }
                if (encontrado) break;
                if (redimensionar_geometria_si_no_cumple) {
                    tramo.ancho_x_m += 0.05;
                    tramo.ancho_y_m += 0.05;
                    tramo.diametro_m += 0.05;
                }
            }
        }
        resultados.push_back(analizar_tramo_pilar_editor(obra, pilar, idx, solver_results));
    }

    return resultados;
}

ResumenEditorTramoPilar ReinforcedConcreteEditorEngine::actualizar_armado_manual_tramo_pilar(
    model::Obra& obra,
    std::uint32_t pilar_id,
    std::size_t indice_tramo,
    const model::ArmadoTramoPilar& nuevo_armado,
    const solver::SolverSummary& solver_results
) {
    for (auto& p : obra.estructura_mut().pilares) {
        if (p.id == pilar_id && indice_tramo < p.tramos.size()) {
            p.tramos[indice_tramo].armado = nuevo_armado;
            return analizar_tramo_pilar_editor(obra, p, indice_tramo, solver_results);
        }
    }
    return ResumenEditorTramoPilar{};
}

// ==============================================================================
// P-10.2 — Editor Avanzado de Vigas / Pórticos (`porvigas.dll`, `seccviga.dll`, `comphorm.dll`)
// ==============================================================================

namespace {

double calcular_area_flexion_necesaria_cm2(
    double m_ed_abs_knm,
    double b_comp_m,
    double h_m,
    double recub_m,
    double fcd_mpa,
    double fyd_mpa
) noexcept {
    if (m_ed_abs_knm <= 1e-6) return 0.0;
    const double d_m = std::max(0.05, h_m - recub_m - 0.016);
    const double fcd_kpa = 0.85 * fcd_mpa * 1000.0;
    const double fyd_kpa = fyd_mpa * 1000.0;
    const double mu = m_ed_abs_knm / std::max(1.0, b_comp_m * d_m * d_m * fcd_kpa);
    const double mu_clamped = std::min(0.375, mu);
    const double omega = 1.0 - std::sqrt(std::max(0.0, 1.0 - 2.0 * mu_clamped));
    const double as_m2 = (omega * b_comp_m * d_m * fcd_kpa) / fyd_kpa;
    return as_m2 * 1.0e4;
}

} // namespace

ResumenEditorVanoViga ReinforcedConcreteEditorEngine::analizar_vano_viga_editor(
    const model::Obra& obra,
    const model::PorticoVigasEstructural& portico,
    const model::VanoVigaHormigon& vano,
    const solver::SolverSummary& solver_results,
    std::size_t num_estaciones
) {
    ResumenEditorVanoViga out{};
    out.portico_id = portico.id;
    out.vano_id = vano.id;
    out.referencia_vano = vano.referencia;
    out.armado_dispuesto = vano.armado;

    double L_m = obra.longitud_barra_m(vano.barra_3d_id);
    if (L_m <= 0.10) L_m = 5.00;
    out.luz_vano_m = L_m;

    const auto* mat_h = NormativaEngine::buscar_hormigon(vano.hormigon);
    const auto* mat_s = NormativaEngine::buscar_acero_armaduras(vano.acero_barras);
    const double fcd_mpa = mat_h ? mat_h->fcd_mpa(1.50) : 16.67;
    const double fctm_mpa = mat_h ? mat_h->fctm_mpa : 2.56;
    const double ecm_mpa = mat_h ? mat_h->ecm_mpa : 31476.0;
    const double fyd_mpa = mat_s ? mat_s->fyd_mpa(1.15) : 434.78;
    const double es_mpa = mat_s ? mat_s->es_mpa : 200000.0;

    const double b_m = vano.ancho_alma_b_m;
    const double h_m = vano.canto_total_h_m;
    const double d_m = std::max(0.06, h_m - vano.recubrimiento_geom_m - 0.016);
    const double z_m = 0.90 * d_m;

    // Cuantía mínima de tracción por flexión (EC2 Art. 9.2.1.1 / CE: As,min = max(0.26 * fctm/fyk, 0.0013) * b * d)
    const double fyk_mpa = mat_s ? mat_s->fyk_mpa : 500.0;
    const double rho_min_flex = std::max(0.26 * (fctm_mpa / fyk_mpa), 0.0015);
    const double as_min_flex_cm2 = rho_min_flex * (b_m * 100.0) * (d_m * 100.0);

    // Cuantía mínima de cortante (EC2 Art. 9.2.2: rho_w,min = 0.08 * sqrt(fck) / fyk)
    const double fck_mpa = mat_h ? mat_h->fck_mpa : 25.0;
    const double asw_min_cm2_m = (0.08 * std::sqrt(fck_mpa) / fyk_mpa) * (b_m * 100.0) * 100.0;

    const std::size_t n_est = std::max<std::size_t>(5, num_estaciones);
    out.puntos_grafica.resize(n_est);

    // Construir envolvente de esfuerzos a lo largo del vano desde `solver_results`
    bool tiene_esfuerzos_solver = false;
    for (std::size_t i = 0; i < n_est; ++i) {
        const double x_rel = static_cast<double>(i) / static_cast<double>(n_est - 1);
        const double x_m = x_rel * L_m;
        auto& pt = out.puntos_grafica[i];
        pt.x_m = x_m;
        pt.x_rel = x_rel;

        for (const auto& rc : solver_results.resultados_combinaciones) {
            const bool es_els = (rc.codigo.find("ELS") != std::string::npos || rc.descripcion.find("E.L.S.") != std::string::npos);
            if (es_els) continue;
            const auto* eb = rc.find_esfuerzos_barra(vano.barra_3d_id);
            if (!eb || eb->puntos.empty()) continue;
            tiene_esfuerzos_solver = true;

            // Interpolar en la estación más cercana
            const std::size_t idx_eb = std::min(
                eb->puntos.size() - 1,
                static_cast<std::size_t>(std::round(x_rel * static_cast<double>(eb->puntos.size() - 1)))
            );
            const auto& pe = eb->puntos[idx_eb];
            // En convenio de vigas: momento negativo produce tracción superior, momento positivo produce tracción inferior
            if (pe.my_knm < pt.m_ed_negativo_knm) pt.m_ed_negativo_knm = pe.my_knm;
            if (-pe.my_knm < pt.m_ed_negativo_knm && (x_rel < 0.25 || x_rel > 0.75)) {
                pt.m_ed_negativo_knm = std::min(pt.m_ed_negativo_knm, - std::abs(pe.my_knm));
            }
            if (pe.my_knm > pt.m_ed_positivo_knm) pt.m_ed_positivo_knm = pe.my_knm;
            if (std::abs(pe.my_knm) > pt.m_ed_positivo_knm && (x_rel >= 0.20 && x_rel <= 0.80)) {
                pt.m_ed_positivo_knm = std::max(pt.m_ed_positivo_knm, std::abs(pe.my_knm));
            }
            pt.v_ed_max_kn = std::max(pt.v_ed_max_kn, std::hypot(pe.vy_kn, pe.vz_kn));
            pt.t_ed_max_knm = std::max(pt.t_ed_max_knm, std::abs(pe.mt_knm));
        }
    }

    if (!tiene_esfuerzos_solver) {
        // Ley hiperestática continua por defecto (q_d = 30 kN/m) si la barra no tiene combinaciones calculadas
        const double q_d = 30.0;
        for (std::size_t i = 0; i < n_est; ++i) {
            auto& pt = out.puntos_grafica[i];
            const double x = pt.x_m;
            const double m_x = (-q_d * L_m * L_m / 12.0) + 0.5 * q_d * L_m * x - 0.5 * q_d * x * x;
            pt.m_ed_negativo_knm = std::min(0.0, m_x - 0.02 * q_d * L_m * L_m);
            pt.m_ed_positivo_knm = std::max(0.0, m_x + 0.03 * q_d * L_m * L_m);
            pt.v_ed_max_kn = std::abs(0.5 * q_d * L_m - q_d * x);
        }
    }

    // Evaluar capacidades, áreas necesarias vs. dispuestas y fisuración en cada estación x
    const double a_mont_sup_cm2 = static_cast<double>(vano.armado.num_montaje_sup) * area_una_barra_cm2(vano.armado.diametro_montaje_sup_mm);
    const double a_mont_inf_cm2 = static_cast<double>(vano.armado.num_montaje_inf) * area_una_barra_cm2(vano.armado.diametro_montaje_inf_mm);
    const double a_ref_izq_cm2 = static_cast<double>(vano.armado.num_ref_sup_izq) * area_una_barra_cm2(vano.armado.diametro_ref_sup_izq_mm);
    const double a_ref_der_cm2 = static_cast<double>(vano.armado.num_ref_sup_der) * area_una_barra_cm2(vano.armado.diametro_ref_sup_der_mm);
    const double a_ref_inf_cm2 = static_cast<double>(vano.armado.num_ref_inf_vano) * area_una_barra_cm2(vano.armado.diametro_ref_inf_vano_mm);

    const double vrd_max_biela_kn = 0.30 * (1.0 - fck_mpa / 250.0) * (fcd_mpa * 1000.0) * b_m * z_m;
    const double vrd_c_base_kn = 0.12 * std::cbrt(100.0 * 0.005 * fck_mpa) * b_m * d_m * 1000.0;

    bool todas_estaciones_cumplen = true;
    double max_eta_vano = 0.0;

    for (auto& pt : out.puntos_grafica) {
        // 1. Área necesaria superior e inferior incluyendo decalaje de la ley de momentos a_l = 0.5 * z
        const double m_neg_dec = std::abs(pt.m_ed_negativo_knm) + 0.45 * pt.v_ed_max_kn * z_m;
        const double m_pos_dec = std::abs(pt.m_ed_positivo_knm) + 0.45 * pt.v_ed_max_kn * z_m;

        pt.as_sup_necesaria_cm2 = (std::abs(pt.m_ed_negativo_knm) > 1.0)
            ? std::max(as_min_flex_cm2, calcular_area_flexion_necesaria_cm2(m_neg_dec, b_m, h_m, vano.recubrimiento_geom_m, fcd_mpa, fyd_mpa))
            : (0.50 * as_min_flex_cm2);

        double b_ef_pos = b_m;
        if (vano.tipo_seccion == model::TipoSeccionViga::SeccionEnT) {
            b_ef_pos += vano.ancho_ala_izquierda_m + vano.ancho_ala_derecha_m;
        }
        pt.as_inf_necesaria_cm2 = (pt.m_ed_positivo_knm > 1.0)
            ? std::max(as_min_flex_cm2, calcular_area_flexion_necesaria_cm2(m_pos_dec, b_ef_pos, h_m, vano.recubrimiento_geom_m, fcd_mpa, fyd_mpa))
            : as_min_flex_cm2;

        // 2. Área dispuesta efectiva en abscisa x según las longitudes de refuerzos de negativos y positivos
        double as_sup_disp = a_mont_sup_cm2;
        if (pt.x_m <= vano.armado.longitud_ref_sup_izq_m + 1e-6) {
            as_sup_disp += a_ref_izq_cm2;
        }
        if (pt.x_m >= (L_m - vano.armado.longitud_ref_sup_der_m - 1e-6)) {
            as_sup_disp += a_ref_der_cm2;
        }
        pt.as_sup_dispuesta_efectiva_cm2 = as_sup_disp;
        pt.as_inf_dispuesta_efectiva_cm2 = a_mont_inf_cm2 + a_ref_inf_cm2 + vano.area_armadura_activa_cm2;

        // Momentos resistentes en la abscisa x
        pt.m_rd_negativo_knm = - ( (pt.as_sup_dispuesta_efectiva_cm2 * 1.0e-4) * (fyd_mpa * 1000.0) * z_m );
        pt.m_rd_positivo_knm = + ( (pt.as_inf_dispuesta_efectiva_cm2 * 1.0e-4) * (fyd_mpa * 1000.0) * z_m );

        // 3. Armadura transversal de cortante necesaria vs. dispuesta (cm^2/m)
        const double v_neto_s_kn = std::max(0.0, pt.v_ed_max_kn - 0.50 * vrd_c_base_kn);
        const double asw_req_v_cm2_m = ( (v_neto_s_kn / std::max(0.05, z_m * fyd_mpa * 1000.0)) ) * 1.0e4;
        pt.asw_s_necesaria_cm2_m = std::max(asw_min_cm2_m, asw_req_v_cm2_m);

        double sep_zona_cm = vano.armado.sep_central_cm;
        if (pt.x_m <= vano.armado.longitud_zona_confinada_extremos_m) {
            sep_zona_cm = vano.armado.sep_extremo_izq_cm;
        } else if (pt.x_m >= L_m - vano.armado.longitud_zona_confinada_extremos_m) {
            sep_zona_cm = vano.armado.sep_extremo_der_cm;
        }
        pt.asw_s_dispuesta_cm2_m = vano.armado.area_estribos_cm2_por_m(sep_zona_cm);
        pt.v_rd_max_biela_kn = vrd_max_biela_kn;
        pt.v_rd_total_kn = std::min(vrd_max_biela_kn, vrd_c_base_kn + (pt.asw_s_dispuesta_cm2_m * 1.0e-4) * z_m * fyd_mpa * 1000.0);

        // 4. Abertura característica de fisura w_k = s_{r,max} * (eps_{sm} - eps_{cm}) (EC2 Art. 7.3.4 / `gcdocufis_*`)
        const double m_qp_knm = 0.60 * std::max(std::abs(pt.m_ed_negativo_knm), pt.m_ed_positivo_knm);
        const double as_trac_cm2 = ( std::abs(pt.m_ed_negativo_knm) > pt.m_ed_positivo_knm )
            ? pt.as_sup_dispuesta_efectiva_cm2 : pt.as_inf_dispuesta_efectiva_cm2;
        const double sigma_s_els_mpa = (m_qp_knm / std::max(0.05, z_m)) / std::max(0.5e-4, as_trac_cm2 * 1.0e-4) * 1.0e-3;
        const double h_cef_m = std::min(2.5 * (h_m - d_m), h_m / 3.0);
        const double ac_eff_cm2 = std::max(20.0, b_m * h_cef_m * 1.0e4);
        const double rho_p_eff = std::max(0.005, as_trac_cm2 / ac_eff_cm2);
        const double sr_max_mm = 3.4 * (vano.recubrimiento_geom_m * 1000.0) + (0.425 * 0.8 * 0.5 * 16.0) / rho_p_eff;
        const double diff_eps = std::max(0.6 * (sigma_s_els_mpa / es_mpa),
                                         (sigma_s_els_mpa - 0.4 * (fctm_mpa / rho_p_eff)) / es_mpa);
        pt.abertura_fisura_wk_mm = std::max(0.0, sr_max_mm * diff_eps);
        pt.abertura_fisura_max_adm_mm = 0.30;

        pt.cumple_flexion_superior = (pt.as_sup_dispuesta_efectiva_cm2 + 0.05 >= pt.as_sup_necesaria_cm2);
        pt.cumple_flexion_inferior = (pt.as_inf_dispuesta_efectiva_cm2 + 0.05 >= pt.as_inf_necesaria_cm2);
        pt.cumple_cortante = (pt.asw_s_dispuesta_cm2_m + 0.05 >= pt.asw_s_necesaria_cm2_m) && (pt.v_ed_max_kn <= pt.v_rd_total_kn + 0.1);
        pt.cumple_fisuracion = (pt.abertura_fisura_wk_mm <= pt.abertura_fisura_max_adm_mm + 0.02);

        if (!pt.cumple_flexion_superior || !pt.cumple_flexion_inferior || !pt.cumple_cortante) {
            todas_estaciones_cumplen = false;
        }

        const double eta_sup = (pt.as_sup_necesaria_cm2 / std::max(0.1, pt.as_sup_dispuesta_efectiva_cm2)) * 100.0;
        const double eta_inf = (pt.as_inf_necesaria_cm2 / std::max(0.1, pt.as_inf_dispuesta_efectiva_cm2)) * 100.0;
        const double eta_v = (pt.asw_s_necesaria_cm2_m / std::max(0.1, pt.asw_s_dispuesta_cm2_m)) * 100.0;
        max_eta_vano = std::max({max_eta_vano, eta_sup, eta_inf, eta_v});

        if (pt.x_rel <= 0.30) out.as_sup_nec_max_izq_cm2 = std::max(out.as_sup_nec_max_izq_cm2, pt.as_sup_necesaria_cm2);
        if (pt.x_rel >= 0.70) out.as_sup_nec_max_der_cm2 = std::max(out.as_sup_nec_max_der_cm2, pt.as_sup_necesaria_cm2);
        out.as_inf_nec_max_vano_cm2 = std::max(out.as_inf_nec_max_vano_cm2, pt.as_inf_necesaria_cm2);
        out.asw_nec_max_cm2_m = std::max(out.asw_nec_max_cm2_m, pt.asw_s_necesaria_cm2_m);
        out.abertura_fisura_max_wk_mm = std::max(out.abertura_fisura_max_wk_mm, pt.abertura_fisura_wk_mm);
    }

    out.as_sup_disp_izq_cm2 = vano.armado.area_superior_apoyo_izq_cm2();
    out.as_sup_disp_der_cm2 = vano.armado.area_superior_apoyo_der_cm2();
    out.as_inf_disp_vano_cm2 = vano.armado.area_inferior_centro_vano_cm2();
    out.asw_disp_extremos_cm2_m = vano.armado.area_estribos_cm2_por_m(vano.armado.sep_extremo_izq_cm);

    // 5. Flecha diferida en viga de H.A. por el método de Branson (`chcalc_momento_inercia_*_seccion_homogeneizada_tiempos_calculo`)
    const double ig_m4 = (b_m * std::pow(h_m, 3)) / 12.0;
    const double n_eq = es_mpa / std::max(10000.0, ecm_mpa);
    const double as_inf_m2 = out.as_inf_disp_vano_cm2 * 1.0e-4;
    const double x_fis_m = std::clamp(0.32 * d_m, 0.05 * d_m, 0.60 * d_m);
    const double ifis_m4 = (b_m * std::pow(x_fis_m, 3)) / 3.0 + n_eq * as_inf_m2 * (d_m - x_fis_m) * (d_m - x_fis_m);
    const double m_fis_knm = (fctm_mpa * 1000.0) * (ig_m4 / (0.5 * h_m));
    const double m_act_els_knm = std::max(1.0, 0.65 * out.puntos_grafica[n_est / 2].m_ed_positivo_knm);
    const double ratio_mf = std::min(1.0, m_fis_knm / m_act_els_knm);
    const double ratio_mf3 = ratio_mf * ratio_mf * ratio_mf;
    const double i_branson_m4 = std::min(ig_m4, ratio_mf3 * ig_m4 + (1.0 - ratio_mf3) * ifis_m4);
    const double f_inst_m = (5.0 / 384.0) * (8.0 * m_act_els_knm * L_m * L_m) / std::max(100.0, (ecm_mpa * 1000.0) * i_branson_m4);
    out.flecha_activa_mm = f_inst_m * 1000.0 * 1.85; // Factor diferido 1 + xi = 1.85
    out.flecha_activa_admisible_mm = std::min((L_m * 1000.0) / 400.0, 10.0);

    // 6. Verificación de capacidad sísmica y errores de valoración (`ConcreteStructureEngine::verificar_vano_viga`)
    out.verificacion_capacidad = model::ConcreteStructureEngine::verificar_vano_viga(
        vano,
        L_m,
        20.0,
        1.20,
        obra.datos_generales().con_sismo_dinamico,
        fcd_mpa,
        fyd_mpa
    );

    // 7. Despiece detallado de armaduras del vano de viga
    const double pat_viga_m = std::max(0.15, h_m - 2.0 * vano.recubrimiento_geom_m);
    {
        // Montaje superior
        PosicionDespieceArmadura p1{1, "Montaje superior", vano.armado.num_montaje_sup, vano.armado.diametro_montaje_sup_mm,
                                    L_m, 0.0, 2.0 * pat_viga_m, L_m + 2.0 * pat_viga_m, 0.0};
        p1.peso_total_kg = static_cast<double>(p1.numero_barras) * p1.longitud_total_unitaria_m * peso_lineal_barra_kg_m(p1.diametro_mm);
        out.peso_total_acero_vano_kg += p1.peso_total_kg;
        out.despiece_armaduras.push_back(p1);

        // Montaje inferior
        PosicionDespieceArmadura p2{2, "Montaje inferior", vano.armado.num_montaje_inf, vano.armado.diametro_montaje_inf_mm,
                                    L_m, 0.30, 0.0, L_m + 0.30, 0.0};
        p2.peso_total_kg = static_cast<double>(p2.numero_barras) * p2.longitud_total_unitaria_m * peso_lineal_barra_kg_m(p2.diametro_mm);
        out.peso_total_acero_vano_kg += p2.peso_total_kg;
        out.despiece_armaduras.push_back(p2);

        // Refuerzo negativo izquierdo
        if (vano.armado.num_ref_sup_izq > 0) {
            PosicionDespieceArmadura p3{3, "Refuerzo superior apoyo izq.", vano.armado.num_ref_sup_izq, vano.armado.diametro_ref_sup_izq_mm,
                                        vano.armado.longitud_ref_sup_izq_m, 0.0, pat_viga_m, vano.armado.longitud_ref_sup_izq_m + pat_viga_m, 0.0};
            p3.peso_total_kg = static_cast<double>(p3.numero_barras) * p3.longitud_total_unitaria_m * peso_lineal_barra_kg_m(p3.diametro_mm);
            out.peso_total_acero_vano_kg += p3.peso_total_kg;
            out.despiece_armaduras.push_back(p3);
        }

        // Refuerzo negativo derecho
        if (vano.armado.num_ref_sup_der > 0) {
            PosicionDespieceArmadura p4{4, "Refuerzo superior apoyo der.", vano.armado.num_ref_sup_der, vano.armado.diametro_ref_sup_der_mm,
                                        vano.armado.longitud_ref_sup_der_m, 0.0, pat_viga_m, vano.armado.longitud_ref_sup_der_m + pat_viga_m, 0.0};
            p4.peso_total_kg = static_cast<double>(p4.numero_barras) * p4.longitud_total_unitaria_m * peso_lineal_barra_kg_m(p4.diametro_mm);
            out.peso_total_acero_vano_kg += p4.peso_total_kg;
            out.despiece_armaduras.push_back(p4);
        }

        // Refuerzo positivo en vano
        if (vano.armado.num_ref_inf_vano > 0) {
            PosicionDespieceArmadura p5{5, "Refuerzo positivo centro de vano", vano.armado.num_ref_inf_vano, vano.armado.diametro_ref_inf_vano_mm,
                                        0.85 * L_m, 0.25, 0.0, 0.85 * L_m + 0.25, 0.0};
            p5.peso_total_kg = static_cast<double>(p5.numero_barras) * p5.longitud_total_unitaria_m * peso_lineal_barra_kg_m(p5.diametro_mm);
            out.peso_total_acero_vano_kg += p5.peso_total_kg;
            out.despiece_armaduras.push_back(p5);
        }

        // Estribos del vano
        const std::uint32_t n_estribos = static_cast<std::uint32_t>(L_m * 100.0 / std::max(5.0, vano.armado.sep_central_cm)) + 4;
        const double l_est_m = 2.0 * ((b_m - 2.0 * vano.recubrimiento_geom_m) + (h_m - 2.0 * vano.recubrimiento_geom_m)) + 0.16;
        PosicionDespieceArmadura p6{6, "Estribos transversales", n_estribos, vano.armado.diametro_estribo_mm,
                                    l_est_m - 0.16, 0.0, 0.16, l_est_m, 0.0};
        p6.peso_total_kg = static_cast<double>(p6.numero_barras) * p6.longitud_total_unitaria_m * peso_lineal_barra_kg_m(p6.diametro_mm);
        out.peso_total_acero_vano_kg += p6.peso_total_kg;
        out.despiece_armaduras.push_back(p6);
    }

    out.aprovechamiento_maximo_pct = max_eta_vano;
    out.cumple_vano = todas_estaciones_cumplen && out.verificacion_capacidad.cumple_global;

    std::ostringstream oss;
    oss << "Vano " << out.referencia_vano << " (L = " << fmt_num(out.luz_vano_m, 2) << " m) -> "
        << (out.cumple_vano ? "CUMPLE" : "NO CUMPLE")
        << " | As,sup,izq: " << fmt_num(out.as_sup_disp_izq_cm2, 2) << "/" << fmt_num(out.as_sup_nec_max_izq_cm2, 2) << " cm^2"
        << " | As,inf,vano: " << fmt_num(out.as_inf_disp_vano_cm2, 2) << "/" << fmt_num(out.as_inf_nec_max_vano_cm2, 2) << " cm^2"
        << " | w_k = " << fmt_num(out.abertura_fisura_max_wk_mm, 2) << " mm\n";
    out.listado_comprobaciones_vano = oss.str();

    return out;
}

std::vector<ResumenEditorVanoViga> ReinforcedConcreteEditorEngine::dimensionar_portico_vigas(
    model::Obra& obra,
    model::PorticoVigasEstructural& portico,
    const solver::SolverSummary& solver_results
) {
    std::vector<ResumenEditorVanoViga> resultados;
    resultados.reserve(portico.vanos.size());

    for (auto& vano : portico.vanos) {
        if (!portico.bloqueado_armado) {
            // Evaluar demandas iniciales
            const auto inicial = analizar_vano_viga_editor(obra, portico, vano, solver_results);

            // Armadura de montaje base: 2 phi 12 arriba y 2 phi 12 abajo
            vano.armado.num_montaje_sup = 2;
            vano.armado.diametro_montaje_sup_mm = 12;
            vano.armado.num_montaje_inf = 2;
            vano.armado.diametro_montaje_inf_mm = 12;

            const double a_mont_cm2 = 2.0 * area_una_barra_cm2(12);
            const auto elegir_refuerzo = [](double area_adicional_req_cm2, std::uint32_t& n_out, std::uint32_t& d_out) noexcept {
                if (area_adicional_req_cm2 <= 0.05) {
                    n_out = 1; d_out = 12; return;
                }
                for (std::uint32_t d : {12u, 16u, 20u, 25u}) {
                    for (std::uint32_t n = 1; n <= 5; ++n) {
                        if (static_cast<double>(n) * area_una_barra_cm2(d) >= area_adicional_req_cm2) {
                            n_out = n;
                            d_out = d;
                            return;
                        }
                    }
                }
                n_out = 5;
                d_out = 25;
            };

            elegir_refuerzo(std::max(0.0, inicial.as_sup_nec_max_izq_cm2 * 1.05 - a_mont_cm2),
                            vano.armado.num_ref_sup_izq, vano.armado.diametro_ref_sup_izq_mm);
            elegir_refuerzo(std::max(0.0, inicial.as_sup_nec_max_der_cm2 * 1.05 - a_mont_cm2),
                            vano.armado.num_ref_sup_der, vano.armado.diametro_ref_sup_der_mm);
            elegir_refuerzo(std::max(0.0, inicial.as_inf_nec_max_vano_cm2 * 1.05 - a_mont_cm2),
                            vano.armado.num_ref_inf_vano, vano.armado.diametro_ref_inf_vano_mm);

            vano.armado.longitud_ref_sup_izq_m = std::max(1.20, 0.33 * inicial.luz_vano_m);
            vano.armado.longitud_ref_sup_der_m = std::max(1.20, 0.33 * inicial.luz_vano_m);

            // Armadura de piel automática si h >= 0.60 m
            if (vano.canto_total_h_m >= 0.60 && vano.armado.num_barras_piel_por_cara == 0) {
                vano.armado.num_barras_piel_por_cara = 2;
                vano.armado.diametro_piel_mm = 10;
            }

            // Estribos para cubrir tanto el cortante envolvente como el cortante de capacidad sísmica
            const auto post_long = analizar_vano_viga_editor(obra, portico, vano, solver_results);
            const double z_m = 0.90 * std::max(0.06, vano.canto_total_h_m - vano.recubrimiento_geom_m - 0.016);
            const double asw_cap_cm2_m = (post_long.verificacion_capacidad.cortante_disenyo_capacidad_kn /
                                          std::max(10.0, z_m * 434.78 * 1000.0)) * 1.0e4;
            const double asw_target_cm2_m = std::max(post_long.asw_nec_max_cm2_m, asw_cap_cm2_m) * 1.05;

            for (std::uint32_t d_est : {8u, 10u, 12u}) {
                for (std::uint32_t ramas : {2u, 4u}) {
                    for (double sep_cm : {20.0, 15.0, 12.5, 10.0, 7.5}) {
                        vano.armado.diametro_estribo_mm = d_est;
                        vano.armado.num_ramas_estribo = ramas;
                        vano.armado.sep_extremo_izq_cm = sep_cm;
                        vano.armado.sep_extremo_der_cm = sep_cm;
                        vano.armado.sep_central_cm = std::min(20.0, sep_cm * 1.5);
                        if (vano.armado.area_estribos_cm2_por_m(sep_cm) >= asw_target_cm2_m) {
                            goto estribos_listos;
                        }
                    }
                }
            }
        estribos_listos:;
        }
        resultados.push_back(analizar_vano_viga_editor(obra, portico, vano, solver_results));
    }

    return resultados;
}

ResumenEditorVanoViga ReinforcedConcreteEditorEngine::actualizar_armado_manual_vano_viga(
    model::Obra& obra,
    std::uint32_t portico_id,
    std::uint32_t vano_id,
    const model::ArmadoVanoViga& nuevo_armado,
    const solver::SolverSummary& solver_results
) {
    for (auto& pv : obra.estructura_mut().porticos_vigas) {
        if (pv.id == portico_id) {
            for (auto& v : pv.vanos) {
                if (v.id == vano_id) {
                    v.armado = nuevo_armado;
                    return analizar_vano_viga_editor(obra, pv, v, solver_results);
                }
            }
        }
    }
    return ResumenEditorVanoViga{};
}

} // namespace cypeclon::codes
