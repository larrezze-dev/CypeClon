#include "foundation/foundation_engine.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>

namespace cypeclon::foundation {

namespace {

constexpr double kPi = std::numbers::pi;

double parse_fck_from_grade(std::string_view grade) noexcept {
    if (grade.find("35") != std::string_view::npos) {
        return 35.0;
    }
    if (grade.find("30") != std::string_view::npos) {
        return 30.0;
    }
    if (grade.find("20") != std::string_view::npos) {
        return 20.0;
    }
    return 25.0; // C25/30 por defecto
}

double parse_fyk_from_grade(std::string_view grade) noexcept {
    if (grade.find("500") != std::string_view::npos) {
        return 500.0;
    }
    return 400.0; // S-400 / B400S por defecto
}

double rebar_area_cm2(double d_mm) noexcept {
    const double r_cm = 0.05 * d_mm;
    return kPi * r_cm * r_cm;
}

double rebar_weight_kg_per_m(double d_mm) noexcept {
    return 0.006165 * d_mm * d_mm;
}

const ArranqueCimentacion* find_starter_by_node(
    const std::vector<ArranqueCimentacion>& arranques,
    std::uint32_t nudo_id) noexcept {
    for (const auto& a : arranques) {
        if (a.nudo_id == nudo_id) {
            return &a;
        }
    }
    return arranques.empty() ? nullptr : &arranques.front();
}

} // namespace

std::size_t FoundationEngine::count_elements(const model::Obra& obra) noexcept {
    return obra.cimentacion().elementos.size();
}

std::vector<EntradaTablaArmadoZapata> FoundationEngine::build_footing_rebar_table() {
    // Equivalente a tbzapata_aislada_* en zptipos.dll
    const double diams[] = {12.0, 16.0, 20.0, 25.0};
    const double seps[] = {0.25, 0.20, 0.15, 0.125, 0.10};

    std::vector<EntradaTablaArmadoZapata> tabla;
    for (double d : diams) {
        const double area1 = rebar_area_cm2(d);
        for (double s : seps) {
            EntradaTablaArmadoZapata row;
            row.diametro_mm = d;
            row.separacion_m = s;
            row.cuantia_cm2_m = area1 / s;
            std::ostringstream oss;
            oss << "Ø" << static_cast<int>(d) << "c/" << static_cast<int>( std::round(s * 100.0) );
            row.etiqueta = oss.str();
            tabla.push_back(row);
        }
    }
    std::sort(tabla.begin(), tabla.end(), [](const EntradaTablaArmadoZapata& a, const EntradaTablaArmadoZapata& b) {
        if (std::abs(a.cuantia_cm2_m - b.cuantia_cm2_m) > 1.0e-3) {
            return a.cuantia_cm2_m < b.cuantia_cm2_m;
        }
        return a.diametro_mm < b.diametro_mm;
    });
    return tabla;
}

std::vector<EntradaCatalogoPilote> FoundationEngine::build_pile_and_micropile_catalog() {
    // Equivalente a ezbibplthorarm_* y ezbibmicroplt_* (encepado.dll / pilotes.dll / micropilotes.dll)
    return {
        {"PIL-D350", FormaPilote::CircularInSitu,       0.35, 10.0,  420.0,  560.0, 100.0, 1.05, 0.0,   0.0},
        {"PIL-D450", FormaPilote::CircularInSitu,       0.45, 12.0,  650.0,  850.0, 150.0, 1.35, 0.0,   0.0},
        {"PIL-D550", FormaPilote::CircularInSitu,       0.55, 14.0,  950.0, 1250.0, 220.0, 1.65, 0.0,   0.0},
        {"PIL-D650", FormaPilote::CircularInSitu,       0.65, 16.0, 1350.0, 1750.0, 300.0, 1.95, 0.0,   0.0},
        {"PIL-D800", FormaPilote::CircularInSitu,       0.80, 18.0, 2000.0, 2600.0, 420.0, 2.40, 0.0,   0.0},
        {"PRE-300",  FormaPilote::CuadradoPrefabricado, 0.30, 12.0,  500.0,  650.0, 130.0, 0.90, 0.0,   0.0},
        {"PRE-400",  FormaPilote::CuadradoPrefabricado, 0.40, 15.0,  850.0, 1100.0, 210.0, 1.20, 0.0,   0.0},
        {"MP-150",   FormaPilote::MicropiloteTubular,   0.15, 12.0,  280.0,  360.0, 180.0, 0.60, 9.0, 550.0},
        {"MP-200",   FormaPilote::MicropiloteTubular,   0.20, 14.0,  450.0,  580.0, 260.0, 0.75, 10.0, 550.0},
        {"MP-250",   FormaPilote::MicropiloteTubular,   0.25, 16.0,  650.0,  820.0, 350.0, 0.90, 12.0, 550.0}
    };
}

std::vector<EntradaTablaVigaCimentacion> FoundationEngine::build_foundation_beam_catalog() {
    // Equivalente a tbvigcim_* y vcvigcen_* en vigacim.dll
    return {
        // Vigas de atado (sísmicas / arriostramiento)
        {"VA-30x30", false, 0.30, 0.30, 2, 12.0, 2, 12.0, 0, 10.0, 8.0, 0.20,  30.0,  30.0,  65.0, 150.0},
        {"VA-40x40", false, 0.40, 0.40, 3, 12.0, 3, 12.0, 0, 10.0, 8.0, 0.20,  55.0,  55.0, 110.0, 230.0},
        {"VA-40x50", false, 0.40, 0.50, 3, 16.0, 3, 16.0, 2, 12.0, 8.0, 0.20,  95.0,  95.0, 145.0, 410.0},
        // Vigas centradoras (equilibrio de momentos de zapatas medianeras / esquina)
        {"VC-40x50", true,  0.40, 0.50, 4, 16.0, 3, 16.0, 2, 12.0, 8.0, 0.15, 125.0,  95.0, 175.0, 260.0},
        {"VC-40x60", true,  0.40, 0.60, 4, 20.0, 4, 16.0, 2, 12.0, 8.0, 0.15, 225.0, 145.0, 225.0, 340.0},
        {"VC-40x70", true,  0.40, 0.70, 5, 20.0, 4, 20.0, 2, 12.0, 10.0, 0.15, 340.0, 270.0, 320.0, 450.0},
        {"VC-50x80", true,  0.50, 0.80, 6, 25.0, 4, 20.0, 4, 12.0, 10.0, 0.12, 620.0, 360.0, 480.0, 600.0},
        {"VC-60x90", true,  0.60, 0.90, 8, 25.0, 5, 25.0, 4, 16.0, 12.0, 0.10, 980.0, 610.0, 720.0, 820.0}
    };
}

std::vector<ArranqueCimentacion> FoundationEngine::extract_foundation_starters(
    const model::Obra& obra,
    const solver::SolverSummary* solver_summary,
    const std::vector<connections::PlacaAnclaje3d>* baseplates) {
    // Equivalente a arqdatos_* y cmbelcimci_* en arranque.dll
    std::vector<ArranqueCimentacion> arranques;
    static const model::ProfileLibraryManager profile_lib;

    const double fck = parse_fck_from_grade(obra.datos_generales().hormigon_cimentacion);
    const double fyk = parse_fyk_from_grade(obra.datos_generales().acero_barras);
    const double fyd = fyk / 1.15;
    const double fctd = (0.21 * std::pow(fck, 2.0 / 3.0)) / 1.50;
    const double fbd = 2.25 * fctd;

    for (const auto& n : obra.estructura().nudos) {
        if (n.vinc_ext == model::VinculacionExterior::Libre) {
            continue;
        }

        ArranqueCimentacion arq;
        arq.nudo_id = n.id;
        arq.posicion_m = n.pos;
        arq.referencia_soporte = "C" + std::to_string(n.id);

        // Buscar barra vertical incidente en el arranque
        for (const auto& b : obra.estructura().barras) {
            if (b.nudo_inicial_id == n.id || b.nudo_final_id == n.id) {
                arq.angulo_giro_deg = b.angulo_giro_rad * (180.0 / kPi);
                if (b.material.find("C2") != std::string::npos || b.material.find("C3") != std::string::npos ||
                    b.material.find("HA") != std::string::npos) {
                    arq.es_soporte_hormigon = true;
                    arq.dim_x_soporte_m = 0.30;
                    arq.dim_y_soporte_m = 0.30;
                } else {
                    const auto* p = profile_lib.find_profile(b.perfil);
                    if (p != nullptr) {
                        arq.dim_x_soporte_m = std::max(0.10, p->h_mm * 1.0e-3);
                        arq.dim_y_soporte_m = std::max(0.10, p->b_mm * 1.0e-3);
                    } else {
                        arq.dim_x_soporte_m = 0.24;
                        arq.dim_y_soporte_m = 0.24;
                    }
                }
                break;
            }
        }

        // Comprobar si tiene un PilarEstructural asociado
        for (const auto& pil : obra.estructura().pilares) {
            if (std::abs(pil.pos_planta_xy.x - n.pos.x) < 0.05 && std::abs(pil.pos_planta_xy.y - n.pos.y) < 0.05 && !pil.tramos.empty()) {
                arq.es_soporte_hormigon = true;
                arq.referencia_soporte = pil.referencia;
                arq.dim_x_soporte_m = std::max(0.25, pil.tramos.front().ancho_x_m);
                arq.dim_y_soporte_m = std::max(0.25, pil.tramos.front().ancho_y_m);
                arq.diametro_armadura_arranque_mm = static_cast<double>(pil.tramos.front().armado.diametro_esquina_mm);
                break;
            }
        }

        // Comprobar placa de anclaje si existe
        if (baseplates != nullptr) {
            for (const auto& bp : *baseplates) {
                if (bp.nudo_id == n.id) {
                    arq.tiene_placa_anclaje = true;
                    arq.ancho_placa_x_m = bp.ancho_lx_mm * 1.0e-3;
                    arq.ancho_placa_y_m = bp.largo_ly_mm * 1.0e-3;
                    break;
                }
            }
        }

        // Longitud de anclaje básica lb,rqd = (phi / 4) * (fyd / fbd) (arqdatos_longitud_dimensionar_anclaje_arranque_en_cimentacion)
        const double phi_m = arq.diametro_armadura_arranque_mm * 1.0e-3;
        arq.longitud_anclaje_requerida_m = std::max(0.30, 0.25 * phi_m * (fyd / std::max(1.5, fbd)));

        // Contorno de arranque para punzonamiento (arqdatos_contorno_arranque_para_punzonamiento)
        const double hx = 0.5 * (arq.tiene_placa_anclaje ? arq.ancho_placa_x_m : arq.dim_x_soporte_m);
        const double hy = 0.5 * (arq.tiene_placa_anclaje ? arq.ancho_placa_y_m : arq.dim_y_soporte_m);
        arq.contorno_punzonamiento_2d_m = {
            {n.pos.x - hx, n.pos.y - hy},
            {n.pos.x + hx, n.pos.y - hy},
            {n.pos.x + hx, n.pos.y + hy},
            {n.pos.x - hx, n.pos.y + hy}
        };

        // Importar reacciones por hipótesis / combinación
        if (solver_summary != nullptr && !solver_summary->resultados_hipotesis.empty()) {
            for (const auto& caso : solver_summary->resultados_hipotesis) {
                for (const auto& r : caso.reacciones_apoyos) {
                    if (r.nudo_id == n.id) {
                        ReaccionCombinacionCimentacion rc_serv;
                        rc_serv.nombre_combinacion = "ELS-" + caso.codigo;
                        rc_serv.es_servicio_caracteristica = true;
                        rc_serv.n_comp_kn = std::max(10.0, r.fz_kn);
                        rc_serv.vx_kn = -r.fx_kn;
                        rc_serv.vy_kn = -r.fy_kn;
                        rc_serv.mx_knm = -r.mx_knm;
                        rc_serv.my_knm = -r.my_knm;
                        rc_serv.t_torsor_knm = -r.mz_knm;
                        arq.combinaciones.push_back(rc_serv);

                        ReaccionCombinacionCimentacion rc_elu;
                        rc_elu.nombre_combinacion = "ELU-1.35*" + caso.codigo;
                        rc_elu.es_servicio_caracteristica = false;
                        rc_elu.n_comp_kn = 1.35 * rc_serv.n_comp_kn;
                        rc_elu.vx_kn = 1.35 * rc_serv.vx_kn;
                        rc_elu.vy_kn = 1.35 * rc_serv.vy_kn;
                        rc_elu.mx_knm = 1.35 * rc_serv.mx_knm;
                        rc_elu.my_knm = 1.35 * rc_serv.my_knm;
                        rc_elu.t_torsor_knm = 1.35 * rc_serv.t_torsor_knm;
                        arq.combinaciones.push_back(rc_elu);
                    }
                }
            }
        }

        // Si no hubo resultados del solver, generar combinación persistente y accidental por defecto
        if (arq.combinaciones.empty()) {
            double fz_sum = 0.0;
            double fx_sum = 0.0;
            double fy_sum = 0.0;
            double mx_sum = 0.0;
            double my_sum = 0.0;
            for (const auto& cn : obra.estructura().cargas_nudos) {
                if (cn.nudo_id == n.id) {
                    fz_sum += std::abs(cn.fuerzas_kn.z);
                    fx_sum += cn.fuerzas_kn.x;
                    fy_sum += cn.fuerzas_kn.y;
                    mx_sum += cn.momentos_knm.x;
                    my_sum += cn.momentos_knm.y;
                }
            }
            if (fz_sum < 1.0) {
                fz_sum = 320.0;
                mx_sum = 18.0;
                my_sum = 25.0;
                fx_sum = 12.0;
            }
            arq.combinaciones.push_back({"ELS-Persistente", false, true, fz_sum, fx_sum, fy_sum, mx_sum, my_sum, 0.0});
            arq.combinaciones.push_back({"ELU-Persistente", false, false, 1.35 * fz_sum, 1.35 * fx_sum, 1.35 * fy_sum, 1.35 * mx_sum, 1.35 * my_sum, 0.0});
            if (obra.datos_generales().con_sismo_dinamico) {
                arq.combinaciones.push_back({"ELU-Sísmica", true, false, 1.00 * fz_sum, 1.50 * fx_sum + 25.0, 1.50 * fy_sum, 1.40 * mx_sum + 20.0, 1.40 * my_sum + 30.0, 0.0});
            }
        }

        arranques.push_back(std::move(arq));
    }

    return arranques;
}

std::vector<core::Vec2d> FoundationEngine::build_footing_rectangle_polygon(const ZapataCimentacion& zapata) {
    const double hx = 0.5 * zapata.ancho_x_m;
    const double hy = 0.5 * zapata.ancho_y_m;
    const double rad = zapata.angulo_giro_deg * (kPi / 180.0);
    const double c = std::cos(rad);
    const double s = std::sin(rad);

    const core::Vec2d corners[4] = {
        {-hx, -hy},
        { hx, -hy},
        { hx,  hy},
        {-hx,  hy}
    };

    std::vector<core::Vec2d> poly;
    poly.reserve(4);
    for (const auto& pt : corners) {
        poly.push_back({
            zapata.centro_planta_m.x + pt.x * c - pt.y * s,
            zapata.centro_planta_m.y + pt.x * s + pt.y * c
        });
    }
    return poly;
}

double FoundationEngine::compute_polygon_area_m2(const std::vector<core::Vec2d>& poly) noexcept {
    if (poly.size() < 3) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const auto& p0 = poly[i];
        const auto& p1 = poly[(i + 1) % poly.size()];
        sum += (p0.x * p1.y - p1.x * p0.y);
    }
    return 0.5 * std::abs(sum);
}

std::vector<core::Vec2d> FoundationEngine::clip_polygon_against_limit(
    const std::vector<core::Vec2d>& subject_poly,
    const model::LimiteCimentacion& limit_poly) {
    // Equivalente a plclimites_recorta_contorno (Sutherland-Hodgman contra un polígono convexo de límite)
    if (subject_poly.size() < 3 || limit_poly.vertices.size() < 3) {
        return subject_poly;
    }

    // Determinar orientación del polígono de límite (CCW vs CW)
    double signed_area = 0.0;
    const auto& clip = limit_poly.vertices;
    for (std::size_t i = 0; i < clip.size(); ++i) {
        const auto& a = clip[i];
        const auto& b = clip[(i + 1) % clip.size()];
        signed_area += (a.x * b.y - b.x * a.y);
    }
    const double sign = (signed_area >= 0.0) ? 1.0 : -1.0;

    std::vector<core::Vec2d> output = subject_poly;
    for (std::size_t i = 0; i < clip.size(); ++i) {
        const core::Vec2d edge_a = clip[i];
        const core::Vec2d edge_b = clip[(i + 1) % clip.size()];
        const core::Vec2d edge_dir{edge_b.x - edge_a.x, edge_b.y - edge_a.y};

        auto is_inside = [&](const core::Vec2d& p) noexcept {
            const double cross = edge_dir.x * (p.y - edge_a.y) - edge_dir.y * (p.x - edge_a.x);
            return (sign * cross) >= -1.0e-9;
        };

        auto intersect = [&](const core::Vec2d& s, const core::Vec2d& e) noexcept -> core::Vec2d {
            const core::Vec2d seg{e.x - s.x, e.y - s.y};
            const double denom = edge_dir.x * seg.y - edge_dir.y * seg.x;
            if (std::abs(denom) < 1.0e-12) {
                return s;
            }
            const double t = ((s.x - edge_a.x) * edge_dir.y - (s.y - edge_a.y) * edge_dir.x) / denom;
            return {s.x + t * seg.x, s.y + t * seg.y};
        };

        std::vector<core::Vec2d> input = std::move(output);
        output.clear();
        if (input.empty()) {
            break;
        }

        core::Vec2d s = input.back();
        for (const auto& e : input) {
            if (is_inside(e)) {
                if (!is_inside(s)) {
                    output.push_back(intersect(s, e));
                }
                output.push_back(e);
            } else if (is_inside(s)) {
                output.push_back(intersect(s, e));
            }
            s = e;
        }
    }

    return output;
}

ResultadoComprobacionZapata FoundationEngine::check_footing(
    const ZapataCimentacion& zapata,
    const std::vector<ArranqueCimentacion>& arranques,
    const model::DatosGeneralesObra& datos_generales,
    const OpcionesCalculoCimentacion& opciones,
    double momento_equilibrado_por_vigas_x_knm,
    double momento_equilibrado_por_vigas_y_knm) {
    // Equivalente a opzapais_*, zmcuad_*, zmrec_* en zptipos.dll
    ResultadoComprobacionZapata res;
    res.zapata_id = zapata.id;
    res.referencia = zapata.referencia;
    res.tension_adm_pers_mpa = std::max(0.05, datos_generales.tension_terreno_persistente_mpa);
    res.tension_adm_acc_mpa = std::max(0.08, datos_generales.tension_terreno_accidental_mpa);
    res.coef_vuelco_exigido = opciones.coef_seguridad_vuelco_persistente;
    res.coef_deslizamiento_exigido = opciones.coef_seguridad_deslizamiento_persistente;

    const double lx = std::max(0.40, zapata.ancho_x_m);
    const double ly = std::max(0.40, zapata.ancho_y_m);
    const double h = std::max(0.25, zapata.canto_total_m);
    const double area_planta = (!zapata.contorno_recortado_planta_m.empty())
        ? std::max(0.16, compute_polygon_area_m2(zapata.contorno_recortado_planta_m))
        : (lx * ly);

    const bool es_masa = (zapata.tipo_material == model::TipoElementoCimentacion::ZapataHormigonMasa);
    const double gamma_c = es_masa
        ? opciones.peso_especifico_hormigon_masa_kn_m3
        : opciones.peso_especifico_hormigon_armado_kn_m3;
    const double peso_propio_kn = area_planta * h * gamma_c;

    // Obtener soporte principal
    const ArranqueCimentacion* arq_ppal = nullptr;
    if (!zapata.nudos_arranque_ids.empty()) {
        arq_ppal = find_starter_by_node(arranques, zapata.nudos_arranque_ids.front());
    } else {
        arq_ppal = find_starter_by_node(arranques, 0);
    }

    const double ax_col = (arq_ppal != nullptr) ? arq_ppal->dim_x_soporte_m : 0.30;
    const double ay_col = (arq_ppal != nullptr) ? arq_ppal->dim_y_soporte_m : 0.30;
    const double lb_req = (arq_ppal != nullptr) ? arq_ppal->longitud_anclaje_requerida_m : 0.35;

    // 1. Comprobación de dimensiones mínimas y anclaje de arranque
    res.canto_minimo_normativo_m = es_masa ? 0.40 : std::max(0.35, lb_req + 0.08);
    res.cumple_dimensiones_minimas = (lx >= ax_col + 0.20) && (ly >= ay_col + 0.20) && (h >= res.canto_minimo_normativo_m - 1.0e-4);
    res.cumple_anclaje_arranque = (h - opciones.recubrimiento_inferior_mm * 1.0e-3) >= (0.85 * lb_req);

    // 2. Comprobación geotécnica: Tensiones en el terreno, Vuelco y Deslizamiento
    std::vector<ReaccionCombinacionCimentacion> combs;
    if (arq_ppal != nullptr && !arq_ppal->combinaciones.empty()) {
        // Si es zapata combinada de varios soportes, sumar sus reacciones respecto al centro de la zapata
        if (zapata.nudos_arranque_ids.size() > 1) {
            const std::size_t n_combs = arq_ppal->combinaciones.size();
            combs.resize(n_combs);
            for (std::size_t ic = 0; ic < n_combs; ++ic) {
                combs[ic].nombre_combinacion = arq_ppal->combinaciones[ic].nombre_combinacion;
                combs[ic].es_servicio_caracteristica = arq_ppal->combinaciones[ic].es_servicio_caracteristica;
                combs[ic].es_sismica_o_accidental = arq_ppal->combinaciones[ic].es_sismica_o_accidental;
                for (std::uint32_t nid : zapata.nudos_arranque_ids) {
                    const auto* a = find_starter_by_node(arranques, nid);
                    if (a != nullptr && ic < a->combinaciones.size()) {
                        const auto& rc = a->combinaciones[ic];
                        const double dx = a->posicion_m.x - zapata.centro_planta_m.x;
                        const double dy = a->posicion_m.y - zapata.centro_planta_m.y;
                        combs[ic].n_comp_kn += rc.n_comp_kn;
                        combs[ic].vx_kn += rc.vx_kn;
                        combs[ic].vy_kn += rc.vy_kn;
                        combs[ic].mx_knm += rc.mx_knm + rc.n_comp_kn * dy;
                        combs[ic].my_knm += rc.my_knm + rc.n_comp_kn * dx;
                    }
                }
            }
        } else {
            combs = arq_ppal->combinaciones;
        }
    } else {
        combs.push_back({"ELS-Defecto", false, true, 300.0, 10.0, 5.0, 15.0, 20.0, 0.0});
        combs.push_back({"ELU-Defecto", false, false, 405.0, 13.5, 6.8, 20.3, 27.0, 0.0});
    }

    double sigma_elu_diseno_mpa = 0.0;
    const double tan_phi = std::tan(opciones.angulo_rozamiento_terreno_deg * (kPi / 180.0));

    for (const auto& c : combs) {
        const double n_tot = std::max(10.0, c.n_comp_kn + (c.es_servicio_caracteristica ? 1.0 : 1.35) * peso_propio_kn);
        // Momentos en la base de la zapata descontando la parte equilibrada por vigas centradoras
        const double mx_base = std::max(0.0, std::abs(c.mx_knm + c.vy_kn * h + c.n_comp_kn * zapata.excentricidad_y_m) - momento_equilibrado_por_vigas_x_knm);
        const double my_base = std::max(0.0, std::abs(c.my_knm + c.vx_kn * h + c.n_comp_kn * zapata.excentricidad_x_m) - momento_equilibrado_por_vigas_y_knm);

        const double ex = std::min(0.35 * lx, my_base / n_tot);
        const double ey = std::min(0.35 * ly, mx_base / n_tot);

        // Ancho eficaz de Meyerhof B' = lx - 2*ex, L' = ly - 2*ey
        const double bx_eff = std::max(0.25 * lx, lx - 2.0 * ex);
        const double by_eff = std::max(0.25 * ly, ly - 2.0 * ey);
        const double sigma_med_mpa = (n_tot / (bx_eff * by_eff)) * 1.0e-3;
        const double sigma_borde_mpa = sigma_med_mpa * (1.0 + 1.5 * (ex / lx + ey / ly));

        if (c.es_servicio_caracteristica || c.es_sismica_o_accidental) {
            if (c.es_sismica_o_accidental) {
                res.tension_max_borde_acc_mpa = std::max(res.tension_max_borde_acc_mpa, sigma_borde_mpa);
            } else {
                res.tension_media_pers_mpa = std::max(res.tension_media_pers_mpa, sigma_med_mpa);
                res.tension_max_borde_pers_mpa = std::max(res.tension_max_borde_pers_mpa, sigma_borde_mpa);
            }

            // Vuelco alrededor del borde de la zapata
            const double m_estab_x = n_tot * (0.5 * lx);
            const double m_estab_y = n_tot * (0.5 * ly);
            const double cv_x = (my_base > 1.0e-3) ? (m_estab_x / my_base) : 99.0;
            const double cv_y = (mx_base > 1.0e-3) ? (m_estab_y / mx_base) : 99.0;
            res.coef_vuelco_x_obtenido = std::min(res.coef_vuelco_x_obtenido, cv_x);
            res.coef_vuelco_y_obtenido = std::min(res.coef_vuelco_y_obtenido, cv_y);

            // Deslizamiento
            const double fh = std::hypot(c.vx_kn, c.vy_kn);
            const double froz = n_tot * tan_phi;
            const double cd = (fh > 1.0e-3) ? (froz / fh) : 99.0;
            res.coef_deslizamiento_obtenido = std::min(res.coef_deslizamiento_obtenido, cd);
        }

        if (!c.es_servicio_caracteristica) {
            // Tensión neta ELU de reacción del terreno (sin peso propio de zapata) para flexión/cortante/punzonamiento
            const double n_neto = std::max(10.0, c.n_comp_kn);
            const double sigma_neta_mpa = (n_neto / (bx_eff * by_eff)) * 1.0e-3;
            sigma_elu_diseno_mpa = std::max(sigma_elu_diseno_mpa, sigma_neta_mpa);
        }
    }

    const double ratio_pers = res.tension_max_borde_pers_mpa / res.tension_adm_pers_mpa;
    const double ratio_acc = (res.tension_max_borde_acc_mpa > 0.0)
        ? (res.tension_max_borde_acc_mpa / res.tension_adm_acc_mpa)
        : 0.0;
    res.aprovechamiento_tension = std::max(ratio_pers, ratio_acc);
    res.cumple_tension_terreno = (res.aprovechamiento_tension <= 1.001);

    res.cumple_vuelco = (res.coef_vuelco_x_obtenido >= res.coef_vuelco_exigido) &&
                        (res.coef_vuelco_y_obtenido >= res.coef_vuelco_exigido);
    res.cumple_deslizamiento = !datos_generales.verificar_deslizamiento_zapatas ||
                               (res.coef_deslizamiento_obtenido >= res.coef_deslizamiento_exigido);

    // 3. Comprobaciones estructurales (Flexión en sección de referencia S1 a 0.15*a hacia el interior, Cortante a d, Punzonamiento a 2d)
    const double fck = parse_fck_from_grade(datos_generales.hormigon_cimentacion);
    const double fcd = (0.85 * fck) / 1.50;
    const double fctm = 0.30 * std::pow(fck, 2.0 / 3.0);
    const double fctd = (0.70 * fctm) / 1.50;
    const double fyk = parse_fyk_from_grade(datos_generales.acero_barras);
    const double fyd = fyk / 1.15;

    const double d_eff = std::max(0.15, h - opciones.recubrimiento_inferior_mm * 1.0e-3 - 0.016);
    const double vuelo_x = std::max(0.05, 0.5 * (lx - ax_col) + std::abs(zapata.excentricidad_x_m));
    const double vuelo_y = std::max(0.05, 0.5 * (ly - ay_col) + std::abs(zapata.excentricidad_y_m));

    // Brazo de sección de referencia S1: L_s1 = vuelo + 0.15 * a_col
    const double l_s1_x = vuelo_x + 0.15 * ax_col;
    const double l_s1_y = vuelo_y + 0.15 * ay_col;
    const double q_neta_kn_m2 = sigma_elu_diseno_mpa * 1000.0;

    res.md_x_knm_m = 0.5 * q_neta_kn_m2 * l_s1_x * l_s1_x;
    res.md_y_knm_m = 0.5 * q_neta_kn_m2 * l_s1_y * l_s1_y;

    if (es_masa) {
        // Zapata de hormigón en masa (zmcuad_*, zmrec_*): comprobación de tracción por flexión W = 1.0 * h^2 / 6
        const double w_m3_m = (h * h) / 6.0;
        const double md_max_knm_m = std::max(res.md_x_knm_m, res.md_y_knm_m);
        res.sigma_ct_masa_ed_mpa = (md_max_knm_m / std::max(1.0e-4, w_m3_m)) * 1.0e-3;
        res.fctd_masa_mpa = fctd;
        res.aprovechamiento_flexion = res.sigma_ct_masa_ed_mpa / std::max(0.50, res.fctd_masa_mpa);
        res.cumple_traccion_hormigon_masa = (res.aprovechamiento_flexion <= 1.0);
        res.cumple_flexion = res.cumple_traccion_hormigon_masa;
    } else {
        // Zapata de hormigón armado: capacidad mecánica de la parrilla inferior
        const double as_disp_x_m2_m = zapata.parrilla_inferior.area_dispuesta_x_cm2_m * 1.0e-4;
        const double as_disp_y_m2_m = zapata.parrilla_inferior.area_dispuesta_y_cm2_m * 1.0e-4;
        res.mr_x_knm_m = as_disp_x_m2_m * (fyd * 1000.0) * (0.90 * d_eff);
        res.mr_y_knm_m = as_disp_y_m2_m * (fyd * 1000.0) * (0.90 * d_eff);
        res.aprovechamiento_flexion = std::max(
            res.md_x_knm_m / std::max(1.0, res.mr_x_knm_m),
            res.md_y_knm_m / std::max(1.0, res.mr_y_knm_m));
        res.cumple_flexion = (res.aprovechamiento_flexion <= 1.001);
    }

    // Cortante unidireccional a distancia d de la cara del soporte
    const double l_cort_x = std::max(0.0, vuelo_x - d_eff);
    const double l_cort_y = std::max(0.0, vuelo_y - d_eff);
    res.vd_cortante_max_kn_m = q_neta_kn_m2 * std::max(l_cort_x, l_cort_y);

    const double k_size = std::min(2.0, 1.0 + std::sqrt(0.20 / d_eff));
    const double rho_l = es_masa
        ? 0.0015
        : std::clamp((zapata.parrilla_inferior.area_dispuesta_x_cm2_m * 1.0e-4) / d_eff, 0.0015, 0.02);
    const double tau_rd_c_mpa = std::max(
        0.12 * k_size * std::pow(100.0 * rho_l * fck, 1.0 / 3.0),
        0.035 * std::pow(k_size, 1.5) * std::sqrt(fck));
    res.vrd_cortante_kn_m = tau_rd_c_mpa * 1000.0 * d_eff;
    res.aprovechamiento_cortante = res.vd_cortante_max_kn_m / std::max(1.0, res.vrd_cortante_kn_m);
    res.cumple_cortante = (res.aprovechamiento_cortante <= 1.001);

    // Punzonamiento en perímetro crítico u1 a distancia 2*d de la cara del soporte
    const double u1_m = 2.0 * (ax_col + ay_col) + 4.0 * kPi * d_eff;
    const double area_dentro_u1 = (ax_col + 4.0 * d_eff) * (ay_col + 4.0 * d_eff);
    const double area_fuera_u1 = std::max(0.0, area_planta - area_dentro_u1);
    const double fsd_punz_kn = q_neta_kn_m2 * area_fuera_u1 * 1.15; // beta = 1.15 por excentricidad
    res.tau_punzonamiento_ed_mpa = (fsd_punz_kn / std::max(0.10, u1_m * d_eff)) * 1.0e-3;
    res.tau_punzonamiento_rd_mpa = std::min(tau_rd_c_mpa * 1.25, 0.50 * fcd);
    res.aprovechamiento_punzonamiento = res.tau_punzonamiento_ed_mpa / std::max(0.10, res.tau_punzonamiento_rd_mpa);
    res.cumple_punzonamiento = (res.aprovechamiento_punzonamiento <= 1.001);

    // Fisuración estimada (opciones_comprobacion_fisuracion_u)
    res.abertura_fisura_estimada_mm = es_masa ? 0.0 : (0.18 * res.aprovechamiento_flexion);
    res.cumple_fisuracion = !opciones.comprobar_fisuracion || (res.abertura_fisura_estimada_mm <= opciones.abertura_max_fisura_mm);

    res.aprovechamiento_maximo = std::max({
        res.aprovechamiento_tension,
        res.aprovechamiento_flexion,
        res.aprovechamiento_cortante,
        res.aprovechamiento_punzonamiento
    });

    res.cumple_global = res.cumple_dimensiones_minimas &&
                        res.cumple_anclaje_arranque &&
                        res.cumple_tension_terreno &&
                        res.cumple_vuelco &&
                        res.cumple_deslizamiento &&
                        res.cumple_flexion &&
                        res.cumple_cortante &&
                        res.cumple_punzonamiento &&
                        res.cumple_fisuracion;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "=== COMPROBACIÓN DE ZAPATA " << zapata.referencia
        << (es_masa ? " (Hormigón en Masa)" : " (Hormigón Armado)") << " ===\n";
    oss << "Dimensiones: " << lx << " x " << ly << " x " << h << " m\n";
    oss << "1. Tensiones sobre el terreno: sigma_med = " << std::setprecision(3) << res.tension_media_pers_mpa
        << " MPa, sigma_max = " << res.tension_max_borde_pers_mpa << " / " << res.tension_adm_pers_mpa
        << " MPa (" << std::setprecision(1) << (res.aprovechamiento_tension * 100.0) << "%) -> "
        << (res.cumple_tension_terreno ? "CUMPLE" : "NO CUMPLE") << "\n";
    oss << "2. Estabilidad: Vuelco = " << std::setprecision(2)
        << std::min(res.coef_vuelco_x_obtenido, res.coef_vuelco_y_obtenido)
        << " (mín " << res.coef_vuelco_exigido << "), Deslizamiento = "
        << res.coef_deslizamiento_obtenido << " (mín " << res.coef_deslizamiento_exigido << ")\n";
    oss << "3. Flexión X/Y: Md,x = " << res.md_x_knm_m << " kN·m/m, Md,y = " << res.md_y_knm_m
        << " kN·m/m (" << (res.aprovechamiento_flexion * 100.0) << "%) -> "
        << (res.cumple_flexion ? "CUMPLE" : "NO CUMPLE") << "\n";
    oss << "4. Cortante y Punzonamiento: Vd = " << res.vd_cortante_max_kn_m << "/" << res.vrd_cortante_kn_m
        << " kN/m, tau_punz = " << std::setprecision(3) << res.tau_punzonamiento_ed_mpa << "/"
        << res.tau_punzonamiento_rd_mpa << " MPa -> "
        << ((res.cumple_cortante && res.cumple_punzonamiento) ? "CUMPLE" : "NO CUMPLE") << "\n";
    res.listado_justificativo = oss.str();

    return res;
}

ResultadoComprobacionZapata FoundationEngine::design_footing(
    ZapataCimentacion& zapata,
    const std::vector<ArranqueCimentacion>& arranques,
    const model::DatosGeneralesObra& datos_generales,
    const OpcionesCalculoCimentacion& opciones) {
    // Dimensionamiento automático iterativo de planta (B, L), canto (H) y parrilla de armado
    const auto tabla_armado = build_footing_rebar_table();
    const double fck = parse_fck_from_grade(datos_generales.hormigon_cimentacion);
    const double fctm = 0.30 * std::pow(fck, 2.0 / 3.0);
    const double fyk = parse_fyk_from_grade(datos_generales.acero_barras);
    const double fyd = fyk / 1.15;

    const ArranqueCimentacion* arq_ppal = !zapata.nudos_arranque_ids.empty()
        ? find_starter_by_node(arranques, zapata.nudos_arranque_ids.front())
        : find_starter_by_node(arranques, 0);

    const double lb_req = (arq_ppal != nullptr) ? arq_ppal->longitud_anclaje_requerida_m : 0.35;
    zapata.canto_total_m = std::max(zapata.canto_total_m, std::ceil((lb_req + 0.10) * 20.0) / 20.0);
    zapata.ancho_x_m = std::max(0.80, zapata.ancho_x_m);
    zapata.ancho_y_m = std::max(0.80, zapata.ancho_y_m);

    const double ratio_xy = (zapata.subtipo == SubtipoZapata::RectangularCentrada || zapata.subtipo == SubtipoZapata::CombinadaMultipleArranques)
        ? std::max(1.0, zapata.ancho_x_m / std::max(0.50, zapata.ancho_y_m))
        : 1.0;

    for (int iter = 0; iter < 40; ++iter) {
        // Seleccionar armado para el canto y vuelos actuales si es H.A.
        const double d_eff = std::max(0.15, zapata.canto_total_m - opciones.recubrimiento_inferior_mm * 1.0e-3 - 0.016);
        const double as_min_cm2_m = std::max(1.50, 10000.0 * 0.0018 * zapata.canto_total_m * (fctm / 2.90));

        auto res_prev = check_footing(zapata, arranques, datos_generales, opciones);
        if (zapata.tipo_material == model::TipoElementoCimentacion::ZapataHormigonArmado) {
            const double as_nec_x_cm2_m = std::max(
                as_min_cm2_m,
                (res_prev.md_x_knm_m / (0.90 * d_eff * fyd * 1000.0)) * 10000.0);
            const double as_nec_y_cm2_m = std::max(
                as_min_cm2_m,
                (res_prev.md_y_knm_m / (0.90 * d_eff * fyd * 1000.0)) * 10000.0);

            zapata.parrilla_inferior.area_necesaria_x_cm2_m = as_nec_x_cm2_m;
            zapata.parrilla_inferior.area_necesaria_y_cm2_m = as_nec_y_cm2_m;

            const EntradaTablaArmadoZapata* sel_x = &tabla_armado.back();
            for (const auto& row : tabla_armado) {
                if (row.cuantia_cm2_m >= as_nec_x_cm2_m) {
                    sel_x = &row;
                    break;
                }
            }
            const EntradaTablaArmadoZapata* sel_y = &tabla_armado.back();
            for (const auto& row : tabla_armado) {
                if (row.cuantia_cm2_m >= as_nec_y_cm2_m) {
                    sel_y = &row;
                    break;
                }
            }

            zapata.parrilla_inferior.diametro_x_mm = sel_x->diametro_mm;
            zapata.parrilla_inferior.separacion_x_m = sel_x->separacion_m;
            zapata.parrilla_inferior.area_dispuesta_x_cm2_m = sel_x->cuantia_cm2_m;
            zapata.parrilla_inferior.num_barras_x = static_cast<std::uint32_t>(
                std::max(4.0, std::ceil((zapata.ancho_y_m - 0.10) / sel_x->separacion_m) + 1.0));
            zapata.parrilla_inferior.longitud_barra_x_m = std::max(0.50, zapata.ancho_x_m - 0.10);

            zapata.parrilla_inferior.diametro_y_mm = sel_y->diametro_mm;
            zapata.parrilla_inferior.separacion_y_m = sel_y->separacion_m;
            zapata.parrilla_inferior.area_dispuesta_y_cm2_m = sel_y->cuantia_cm2_m;
            zapata.parrilla_inferior.num_barras_y = static_cast<std::uint32_t>(
                std::max(4.0, std::ceil((zapata.ancho_x_m - 0.10) / sel_y->separacion_m) + 1.0));
            zapata.parrilla_inferior.longitud_barra_y_m = std::max(0.50, zapata.ancho_y_m - 0.10);
        }

        auto res = check_footing(zapata, arranques, datos_generales, opciones);
        if (res.cumple_global) {
            return res;
        }

        if (!res.cumple_tension_terreno || !res.cumple_vuelco || !res.cumple_deslizamiento) {
            zapata.ancho_y_m += 0.10;
            zapata.ancho_x_m = std::round(zapata.ancho_y_m * ratio_xy * 20.0) / 20.0;
        }
        if (!res.cumple_cortante || !res.cumple_punzonamiento || !res.cumple_dimensiones_minimas ||
            !res.cumple_anclaje_arranque || !res.cumple_traccion_hormigon_masa) {
            zapata.canto_total_m += 0.05;
        }
    }

    return check_footing(zapata, arranques, datos_generales, opciones);
}

void FoundationEngine::populate_pile_positions(EncepadoCimentacion& encepado) {
    // Equivalente a pilote_separacion_por_defecto y geometría de encepados 1, 2, 3, 4 y N pilotes
    const double s = std::max(0.60, encepado.pilote_catalogo.separacion_minima_m);
    const double d_pil = encepado.pilote_catalogo.diametro_o_lado_m;
    const double v = std::max(0.20, encepado.vuelo_borde_m);

    encepado.pilotes.clear();
    switch (encepado.tipo_encepado) {
        case TipoEncepado::Encepado1Pilote:
            encepado.pilotes.push_back({1, {0.0, 0.0}, 0.0, 0.0, 0.0});
            encepado.ancho_x_m = std::max(encepado.ancho_x_m, d_pil + 2.0 * v);
            encepado.ancho_y_m = std::max(encepado.ancho_y_m, d_pil + 2.0 * v);
            break;

        case TipoEncepado::Encepado2Pilotes:
            encepado.pilotes.push_back({1, {-0.5 * s, 0.0}, 0.0, 0.0, 0.0});
            encepado.pilotes.push_back({2, { 0.5 * s, 0.0}, 0.0, 0.0, 0.0});
            encepado.ancho_x_m = std::max(encepado.ancho_x_m, s + d_pil + 2.0 * v);
            encepado.ancho_y_m = std::max(encepado.ancho_y_m, d_pil + 2.0 * v);
            break;

        case TipoEncepado::Encepado3Pilotes: {
            const double r = s / std::sqrt(3.0);
            encepado.pilotes.push_back({1, {-0.5 * s, -0.5 * r}, 0.0, 0.0, 0.0});
            encepado.pilotes.push_back({2, { 0.5 * s, -0.5 * r}, 0.0, 0.0, 0.0});
            encepado.pilotes.push_back({3, { 0.0,          r  }, 0.0, 0.0, 0.0});
            encepado.ancho_x_m = std::max(encepado.ancho_x_m, s + d_pil + 2.0 * v);
            encepado.ancho_y_m = std::max(encepado.ancho_y_m, 1.5 * r + d_pil + 2.0 * v);
            break;
        }

        case TipoEncepado::Encepado4Pilotes:
        default:
            encepado.pilotes.push_back({1, {-0.5 * s, -0.5 * s}, 0.0, 0.0, 0.0});
            encepado.pilotes.push_back({2, { 0.5 * s, -0.5 * s}, 0.0, 0.0, 0.0});
            encepado.pilotes.push_back({3, { 0.5 * s,  0.5 * s}, 0.0, 0.0, 0.0});
            encepado.pilotes.push_back({4, {-0.5 * s,  0.5 * s}, 0.0, 0.0, 0.0});
            encepado.ancho_x_m = std::max(encepado.ancho_x_m, s + d_pil + 2.0 * v);
            encepado.ancho_y_m = std::max(encepado.ancho_y_m, s + d_pil + 2.0 * v);
            break;
    }
}

ResultadoComprobacionEncepado FoundationEngine::check_pile_cap(
    const EncepadoCimentacion& encepado,
    const ArranqueCimentacion& arranque,
    const model::DatosGeneralesObra& datos_generales,
    const OpcionesCalculoCimentacion& opciones) {
    // Equivalente a encepado_comprobacion, pilote_capacidad_portante_* y mplcomphund_*
    ResultadoComprobacionEncepado res;
    res.encepado_id = encepado.id;
    res.referencia = encepado.referencia;
    res.capacidad_pilote_pers_kn = encepado.pilote_catalogo.capacidad_portante_persistente_kn;
    res.capacidad_pilote_acc_kn = encepado.pilote_catalogo.capacidad_portante_accidental_kn;

    EncepadoCimentacion enc_work = encepado;
    if (enc_work.pilotes.empty()) {
        populate_pile_positions(enc_work);
    }

    const std::size_t n_pil = std::max<std::size_t>(1, enc_work.pilotes.size());
    double sum_x2 = 0.0;
    double sum_y2 = 0.0;
    double dist_max_pilote_m = 0.45;
    for (const auto& p : enc_work.pilotes) {
        sum_x2 += p.coord_relativa_m.x * p.coord_relativa_m.x;
        sum_y2 += p.coord_relativa_m.y * p.coord_relativa_m.y;
        dist_max_pilote_m = std::max(
            dist_max_pilote_m,
            std::hypot(p.coord_relativa_m.x, p.coord_relativa_m.y));
    }

    const double peso_encepado_kn =
        enc_work.ancho_x_m * enc_work.ancho_y_m * enc_work.canto_m * opciones.peso_especifico_hormigon_armado_kn_m3;

    double n_pilote_elu_max_kn = 0.0;
    for (const auto& c : arranque.combinaciones) {
        const double factor_pp = c.es_servicio_caracteristica ? 1.0 : 1.35;
        const double n_tot = std::max(10.0, c.n_comp_kn + factor_pp * peso_encepado_kn);
        const double mx = std::abs(c.mx_knm + c.vy_kn * enc_work.canto_m);
        const double my = std::abs(c.my_knm + c.vx_kn * enc_work.canto_m);

        // Si el encepado es de 1 pilote (o de 2 pilotes en el plano perpendicular), los momentos no absorbidos por par de pilotes pasan a viga centradora
        if (sum_y2 < 1.0e-4) {
            res.mx_desequilibrio_viga_centradora_knm = std::max(res.mx_desequilibrio_viga_centradora_knm, mx);
        }
        if (sum_x2 < 1.0e-4) {
            res.my_desequilibrio_viga_centradora_knm = std::max(res.my_desequilibrio_viga_centradora_knm, my);
        }

        for (const auto& p : enc_work.pilotes) {
            const double term_x = (sum_x2 > 1.0e-4) ? (my * std::abs(p.coord_relativa_m.x) / sum_x2) : 0.0;
            const double term_y = (sum_y2 > 1.0e-4) ? (mx * std::abs(p.coord_relativa_m.y) / sum_y2) : 0.0;
            const double n_i = (n_tot / static_cast<double>(n_pil)) + term_x + term_y;

            if (c.es_servicio_caracteristica) {
                res.axil_max_pilote_pers_kn = std::max(res.axil_max_pilote_pers_kn, n_i);
            } else if (c.es_sismica_o_accidental) {
                res.axil_max_pilote_acc_kn = std::max(res.axil_max_pilote_acc_kn, n_i);
                n_pilote_elu_max_kn = std::max(n_pilote_elu_max_kn, n_i);
            } else {
                n_pilote_elu_max_kn = std::max(n_pilote_elu_max_kn, n_i);
            }
        }
    }

    const double ratio_pers = res.axil_max_pilote_pers_kn / std::max(1.0, res.capacidad_pilote_pers_kn);
    const double ratio_acc = (res.axil_max_pilote_acc_kn > 0.0)
        ? (res.axil_max_pilote_acc_kn / std::max(1.0, res.capacidad_pilote_acc_kn))
        : 0.0;
    res.aprovechamiento_pilotes = std::max(ratio_pers, ratio_acc);
    res.cumple_capacidad_pilotes = (res.aprovechamiento_pilotes <= 1.001);

    // 2. Método de Bielas y Tirantes (encepado_dimensiona_completo)
    const double fck = parse_fck_from_grade(datos_generales.hormigon_cimentacion);
    const double fcd = (0.85 * fck) / 1.50;
    const double fyk = parse_fyk_from_grade(datos_generales.acero_barras);
    const double fyd = fyk / 1.15;

    const double d_eff = std::max(0.30, enc_work.canto_m - 0.10);
    const double brazo_vuelo_m = std::max(0.15, dist_max_pilote_m - 0.25 * arranque.dim_x_soporte_m);
    res.angulo_biela_comprimida_deg = std::atan2(0.85 * d_eff, brazo_vuelo_m) * (180.0 / kPi);
    res.cumple_rigidez_biela = (brazo_vuelo_m <= 2.0 * d_eff) && (res.angulo_biela_comprimida_deg >= 26.5);

    // Tracción en el tirante principal T_d = N_pil,Ed * brazo / (0.85 * d)
    res.traccion_tirante_ed_kn = (enc_work.tipo_encepado == TipoEncepado::Encepado1Pilote)
        ? (0.25 * n_pilote_elu_max_kn) // Zunchado transversal en encepado de 1 pilote
        : (n_pilote_elu_max_kn * brazo_vuelo_m / (0.85 * d_eff));

    res.capacidad_tirante_rd_kn = (enc_work.area_tirante_dispuesta_cm2 * 1.0e-4) * (fyd * 1000.0);
    res.aprovechamiento_tirante = res.traccion_tirante_ed_kn / std::max(1.0, res.capacidad_tirante_rd_kn);
    res.cumple_tirante_principal = (res.aprovechamiento_tirante <= 1.001);

    // Comprobación de biela comprimida en nudo de pilote y soporte
    const double sin_theta = std::sin(res.angulo_biela_comprimida_deg * (kPi / 180.0));
    const double f_biela_kn = n_pilote_elu_max_kn / std::max(0.35, sin_theta);
    const double area_nodal_pilote_m2 = std::max(0.04, 0.25 * kPi * enc_work.pilote_catalogo.diametro_o_lado_m * enc_work.pilote_catalogo.diametro_o_lado_m);
    res.tension_biela_nodal_ed_mpa = (f_biela_kn / area_nodal_pilote_m2) * 1.0e-3;
    res.tension_biela_nodal_rd_mpa = 0.85 * fcd;
    res.aprovechamiento_biela_comprimida = res.tension_biela_nodal_ed_mpa / std::max(1.0, res.tension_biela_nodal_rd_mpa);
    res.cumple_biela_comprimida = (res.aprovechamiento_biela_comprimida <= 1.001);

    // Armadura secundaria de suspensión / cercos verticales
    res.cortante_o_suspension_ed_kn = 0.20 * n_pilote_elu_max_kn * static_cast<double>(n_pil);
    const double area_una_rama_cm2 = rebar_area_cm2(enc_work.diametro_cercos_verticales_mm);
    const double asw_s_cm2_m = (enc_work.num_ramas_cercos * area_una_rama_cm2) / std::max(0.08, enc_work.separacion_cercos_m);
    res.cortante_o_suspension_rd_kn = (asw_s_cm2_m * 1.0e-4) * (0.90 * d_eff) * (fyd * 1000.0) + 120.0;
    res.cumple_armadura_secundaria = (res.cortante_o_suspension_ed_kn <= res.cortante_o_suspension_rd_kn);

    res.aprovechamiento_maximo = std::max({
        res.aprovechamiento_pilotes,
        res.aprovechamiento_tirante,
        res.aprovechamiento_biela_comprimida
    });

    res.cumple_global = res.cumple_capacidad_pilotes &&
                        res.cumple_rigidez_biela &&
                        res.cumple_tirante_principal &&
                        res.cumple_biela_comprimida &&
                        res.cumple_armadura_secundaria;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "=== COMPROBACIÓN DE ENCEPADO " << encepado.referencia
        << " (" << n_pil << " pilotes " << enc_work.pilote_catalogo.codigo << ") ===\n";
    oss << "1. Capacidad portante pilotes: N_max,serv = " << res.axil_max_pilote_pers_kn
        << " / " << res.capacidad_pilote_pers_kn << " kN ("
        << std::setprecision(1) << (res.aprovechamiento_pilotes * 100.0) << "%) -> "
        << (res.cumple_capacidad_pilotes ? "CUMPLE" : "NO CUMPLE") << "\n";
    oss << "2. Bielas y Tirantes: ángulo biela = " << res.angulo_biela_comprimida_deg
        << " deg, Td = " << std::setprecision(2) << res.traccion_tirante_ed_kn
        << " / " << res.capacidad_tirante_rd_kn << " kN ("
        << std::setprecision(1) << (res.aprovechamiento_tirante * 100.0) << "%) -> "
        << (res.cumple_tirante_principal ? "CUMPLE" : "NO CUMPLE") << "\n";
    res.listado_justificativo = oss.str();

    return res;
}

ResultadoComprobacionEncepado FoundationEngine::design_pile_cap(
    EncepadoCimentacion& encepado,
    const ArranqueCimentacion& arranque,
    const model::DatosGeneralesObra& datos_generales,
    const OpcionesCalculoCimentacion& opciones) {
    // Dimensiona catálogo de pilote, canto rígido y tirante principal por bielas y tirantes
    const auto catalogo_pilotes = build_pile_and_micropile_catalog();
    const double fyk = parse_fyk_from_grade(datos_generales.acero_barras);
    const double fyd = fyk / 1.15;

    populate_pile_positions(encepado);

    for (int iter = 0; iter < 20; ++iter) {
        auto res = check_pile_cap(encepado, arranque, datos_generales, opciones);

        // Si los pilotes no cumplen capacidad portante, subir al siguiente diámetro del catálogo
        if (!res.cumple_capacidad_pilotes) {
            bool upgraded = false;
            for (const auto& cat : catalogo_pilotes) {
                if (cat.forma == encepado.pilote_catalogo.forma &&
                    cat.capacidad_portante_persistente_kn > encepado.pilote_catalogo.capacidad_portante_persistente_kn + 1.0) {
                    encepado.pilote_catalogo = cat;
                    populate_pile_positions(encepado);
                    upgraded = true;
                    break;
                }
            }
            if (!upgraded && encepado.tipo_encepado != TipoEncepado::Encepado4Pilotes) {
                encepado.tipo_encepado = TipoEncepado::Encepado4Pilotes;
                populate_pile_positions(encepado);
            }
            continue;
        }

        // Si el ángulo de biela o la compresión nodal no cumplen, aumentar canto
        if (!res.cumple_rigidez_biela || !res.cumple_biela_comprimida) {
            encepado.canto_m += 0.10;
            continue;
        }

        // Dimensionar armadura del tirante principal
        const double as_nec_cm2 = std::max(
            6.0,
            (res.traccion_tirante_ed_kn / (fyd * 1000.0)) * 10000.0);
        encepado.area_tirante_necesaria_cm2 = as_nec_cm2;

        const double area_barra_cm2 = rebar_area_cm2(encepado.diametro_tirante_ppal_mm);
        encepado.num_barras_tirante_ppal = static_cast<std::uint32_t>(
            std::max(4.0, std::ceil(as_nec_cm2 / area_barra_cm2)));
        encepado.area_tirante_dispuesta_cm2 = encepado.num_barras_tirante_ppal * area_barra_cm2;

        res = check_pile_cap(encepado, arranque, datos_generales, opciones);
        if (res.cumple_global) {
            return res;
        }
    }

    return check_pile_cap(encepado, arranque, datos_generales, opciones);
}

ResultadoComprobacionVigaCimentacion FoundationEngine::check_foundation_beam(
    const VigaCimentacion& viga,
    const ArranqueCimentacion& arranque_ini,
    const ArranqueCimentacion& arranque_fin,
    const model::DatosGeneralesObra& datos_generales,
    const OpcionesCalculoCimentacion& /*opciones*/,
    double excentricidad_zapata_ini_m,
    double excentricidad_zapata_fin_m) {
    // Equivalente a vigacim_SALIDA_IDIOMA_comprobacion_viga_cimentacion en vigacim.dll
    ResultadoComprobacionVigaCimentacion res;
    res.viga_id = viga.id;
    res.referencia = viga.referencia;
    res.tipo = viga.tipo;

    double n_max_ini_kn = 250.0;
    double m_base_ini_knm = 15.0;
    for (const auto& c : arranque_ini.combinaciones) {
        if (!c.es_servicio_caracteristica) {
            n_max_ini_kn = std::max(n_max_ini_kn, c.n_comp_kn);
            m_base_ini_knm = std::max(m_base_ini_knm, std::hypot(c.mx_knm, c.my_knm));
        }
    }

    double n_max_fin_kn = 250.0;
    double m_base_fin_knm = 15.0;
    for (const auto& c : arranque_fin.combinaciones) {
        if (!c.es_servicio_caracteristica) {
            n_max_fin_kn = std::max(n_max_fin_kn, c.n_comp_kn);
            m_base_fin_knm = std::max(m_base_fin_knm, std::hypot(c.mx_knm, c.my_knm));
        }
    }

    // Axil de atado sísmico / arriostramiento mínimo (CTE DB SE-C / NCSE-02)
    const double ag_ratio = std::max(0.08, datos_generales.aceleracion_sismica_basica_g);
    res.axil_diseno_atado_kn = std::max(40.0, ag_ratio * std::max(n_max_ini_kn, n_max_fin_kn));
    res.capacidad_axil_atado_kn = std::max(80.0, viga.seccion_armado.axil_atado_ult_kn);
    res.aprovechamiento_axil = res.axil_diseno_atado_kn / res.capacidad_axil_atado_kn;

    if (viga.tipo == model::TipoElementoCimentacion::VigaCentradora) {
        const double l_eje = std::max(1.50, viga.longitud_eje_a_eje_m);
        const double m_exc_ini = viga.centra_extremo_ini
            ? (n_max_ini_kn * std::max(0.15, std::abs(excentricidad_zapata_ini_m)) + m_base_ini_knm)
            : 0.0;
        const double m_exc_fin = viga.centra_extremo_fin
            ? (n_max_fin_kn * std::max(0.15, std::abs(excentricidad_zapata_fin_m)) + m_base_fin_knm)
            : 0.0;

        res.momento_negativo_ed_knm = std::max(m_exc_ini, m_exc_fin);
        res.momento_positivo_ed_knm = 0.25 * res.momento_negativo_ed_knm;
        res.cortante_max_ed_kn = (m_exc_ini + m_exc_fin) / l_eje + 15.0;

        res.aprovechamiento_flexion = std::max(
            res.momento_negativo_ed_knm / std::max(10.0, viga.seccion_armado.momento_ult_negativo_knm),
            res.momento_positivo_ed_knm / std::max(10.0, viga.seccion_armado.momento_ult_positivo_knm));
        res.aprovechamiento_cortante = res.cortante_max_ed_kn / std::max(10.0, viga.seccion_armado.cortante_ult_kn);
    } else {
        // Viga de atado: momento mínimo de peso propio / asiento diferencial
        res.momento_negativo_ed_knm = 12.0;
        res.momento_positivo_ed_knm = 12.0;
        res.cortante_max_ed_kn = 20.0;
        res.aprovechamiento_flexion = res.momento_negativo_ed_knm / std::max(10.0, viga.seccion_armado.momento_ult_negativo_knm);
        res.aprovechamiento_cortante = res.cortante_max_ed_kn / std::max(10.0, viga.seccion_armado.cortante_ult_kn);
    }

    res.cumple_dimensiones_minimas = (viga.seccion_armado.ancho_b_m >= 0.25) && (viga.seccion_armado.canto_h_m >= 0.25);
    res.cumple_armado_longitudinal = (res.aprovechamiento_axil <= 1.001) && (res.aprovechamiento_flexion <= 1.001);
    res.cumple_armado_transversal = (res.aprovechamiento_cortante <= 1.001);
    res.cumple_anclaje_extremos = (viga.longitud_anclaje_extremo_ini_m >= 0.30) && (viga.longitud_anclaje_extremo_fin_m >= 0.30);

    res.aprovechamiento_maximo = std::max({
        res.aprovechamiento_axil,
        res.aprovechamiento_flexion,
        res.aprovechamiento_cortante
    });
    res.cumple_global = res.cumple_dimensiones_minimas &&
                        res.cumple_armado_longitudinal &&
                        res.cumple_armado_transversal &&
                        res.cumple_anclaje_extremos;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "=== COMPROBACIÓN DE VIGA DE CIMENTACIÓN " << viga.referencia
        << " (" << viga.seccion_armado.codigo << ") ===\n";
    oss << "Sección: " << (viga.seccion_armado.ancho_b_m * 100.0) << " x "
        << (viga.seccion_armado.canto_h_m * 100.0) << " cm\n";
    oss << "Esfuerzos: N_atado = " << res.axil_diseno_atado_kn << " kN, M(-) = "
        << res.momento_negativo_ed_knm << " kN·m, Vd = " << res.cortante_max_ed_kn << " kN\n";
    oss << "Aprovechamiento máximo = " << std::setprecision(1) << (res.aprovechamiento_maximo * 100.0)
        << "% -> " << (res.cumple_global ? "CUMPLE" : "NO CUMPLE") << "\n";
    res.listado_justificativo = oss.str();

    return res;
}

ResultadoComprobacionVigaCimentacion FoundationEngine::design_foundation_beam(
    VigaCimentacion& viga,
    const ArranqueCimentacion& arranque_ini,
    const ArranqueCimentacion& arranque_fin,
    const model::DatosGeneralesObra& datos_generales,
    const OpcionesCalculoCimentacion& opciones,
    double excentricidad_zapata_ini_m,
    double excentricidad_zapata_fin_m) {
    // Selecciona de la tabla de armado (tbvigcim_* / vcvigcen_*) la primera sección que cumple
    const auto catalogo = build_foundation_beam_catalog();
    const bool requiere_centradora = (viga.tipo == model::TipoElementoCimentacion::VigaCentradora);

    for (const auto& entry : catalogo) {
        if (requiere_centradora && !entry.es_centradora) {
            continue;
        }
        viga.seccion_armado = entry;
        viga.longitud_anclaje_extremo_ini_m = std::max(0.35, 28.0 * entry.diametro_sup_mm * 1.0e-3);
        viga.longitud_anclaje_extremo_fin_m = std::max(0.35, 28.0 * entry.diametro_sup_mm * 1.0e-3);

        // Rigidez rotacional equivalente para cálculo de cimentación (vigacim_append_rigideces_viga_centradora_calculo_cimentacion)
        const double ec_kpa = 31000000.0;
        const double iz_m4 = (entry.ancho_b_m * std::pow(entry.canto_h_m, 3)) / 12.0;
        viga.rigidez_rotacional_equivalente_knm_rad = (3.0 * ec_kpa * iz_m4) / std::max(1.0, viga.longitud_eje_a_eje_m);

        auto res = check_foundation_beam(
            viga, arranque_ini, arranque_fin, datos_generales, opciones,
            excentricidad_zapata_ini_m, excentricidad_zapata_fin_m);
        if (res.cumple_global) {
            return res;
        }
    }

    viga.seccion_armado = catalogo.back();
    return check_foundation_beam(
        viga, arranque_ini, arranque_fin, datos_generales, opciones,
        excentricidad_zapata_ini_m, excentricidad_zapata_fin_m);
}

ModeloCompletoCimentacion FoundationEngine::generate_automatic_foundation(
    model::Obra& obra,
    const solver::SolverSummary* solver_summary,
    bool usar_encepados,
    bool generar_vigas_perimetrales) {
    // Equivalente a "generar_zapatas_y_vigas" (genzpvig en planocim.dll)
    ModeloCompletoCimentacion modelo;
    modelo.arranques = extract_foundation_starters(obra, solver_summary);
    modelo.limites = obra.cimentacion().limites;

    obra.cimentacion_mut().elementos.clear();
    const auto tabla_zp = build_footing_rebar_table();
    const auto cat_pilotes = build_pile_and_micropile_catalog();
    const auto cat_vigas = build_foundation_beam_catalog();

    std::uint32_t next_id = 1;
    for (const auto& arq : modelo.arranques) {
        if (usar_encepados) {
            EncepadoCimentacion enc;
            enc.id = next_id++;
            enc.referencia = "E" + std::to_string(enc.id);
            enc.nudo_arranque_id = arq.nudo_id;
            enc.tipo_encepado = TipoEncepado::Encepado2Pilotes;
            enc.pilote_catalogo = cat_pilotes[1]; // PIL-D450
            enc.centro_planta_m = {arq.posicion_m.x, arq.posicion_m.y};
            enc.angulo_giro_deg = arq.angulo_giro_deg;
            populate_pile_positions(enc);
            modelo.encepados.push_back(enc);

            obra.add_elemento_cimentacion(
                model::TipoElementoCimentacion::Encepado,
                arq.nudo_id, enc.ancho_x_m, enc.ancho_y_m, enc.canto_m);
        } else {
            ZapataCimentacion zp;
            zp.id = next_id++;
            zp.referencia = "Z" + std::to_string(zp.id);
            zp.tipo_material = model::TipoElementoCimentacion::ZapataHormigonArmado;
            zp.subtipo = SubtipoZapata::CuadradaCentrada;
            zp.nudos_arranque_ids = {arq.nudo_id};
            zp.centro_planta_m = {arq.posicion_m.x, arq.posicion_m.y};
            zp.ancho_x_m = 1.40;
            zp.ancho_y_m = 1.40;
            zp.canto_total_m = 0.50;
            zp.angulo_giro_deg = arq.angulo_giro_deg;

            if (!tabla_zp.empty()) {
                zp.parrilla_inferior.diametro_x_mm = tabla_zp[2].diametro_mm;
                zp.parrilla_inferior.separacion_x_m = tabla_zp[2].separacion_m;
                zp.parrilla_inferior.area_dispuesta_x_cm2_m = tabla_zp[2].cuantia_cm2_m;
                zp.parrilla_inferior.diametro_y_mm = tabla_zp[2].diametro_mm;
                zp.parrilla_inferior.separacion_y_m = tabla_zp[2].separacion_m;
                zp.parrilla_inferior.area_dispuesta_y_cm2_m = tabla_zp[2].cuantia_cm2_m;
            }

            // Si existen límites poligonales, recortar contorno de la zapata (plclimites_recorta_contorno)
            auto poly = build_footing_rectangle_polygon(zp);
            for (const auto& lim : modelo.limites) {
                poly = clip_polygon_against_limit(poly, lim);
            }
            zp.contorno_recortado_planta_m = std::move(poly);

            modelo.zapatas.push_back(zp);
            obra.add_elemento_cimentacion(
                model::TipoElementoCimentacion::ZapataHormigonArmado,
                arq.nudo_id, zp.ancho_x_m, zp.ancho_y_m, zp.canto_total_m);
        }
    }

    // Generar vigas de atado / centradoras entre arranques cercanos en planta
    if (generar_vigas_perimetrales && modelo.arranques.size() >= 2) {
        std::uint32_t viga_idx = 1;
        for (std::size_t i = 0; i < modelo.arranques.size(); ++i) {
            for (std::size_t j = i + 1; j < modelo.arranques.size(); ++j) {
                const auto& a = modelo.arranques[i];
                const auto& b = modelo.arranques[j];
                const double dx = b.posicion_m.x - a.posicion_m.x;
                const double dy = b.posicion_m.y - a.posicion_m.y;
                const double dist = std::hypot(dx, dy);

                // Conectar soportes alineados ortogonalmente o a distancia razonable (1.5 m .. 12.0 m)
                const bool ortogonal = (std::abs(dx) < 0.25 || std::abs(dy) < 0.25);
                if (dist >= 1.20 && dist <= 12.0 && (ortogonal || modelo.arranques.size() <= 4)) {
                    VigaCimentacion vc;
                    vc.id = next_id++;
                    vc.nudo_ini_id = a.nudo_id;
                    vc.nudo_fin_id = b.nudo_id;
                    vc.longitud_eje_a_eje_m = dist;
                    vc.longitud_libre_m = std::max(0.50, dist - 1.20);
                    vc.tipo = usar_encepados
                        ? model::TipoElementoCimentacion::VigaCentradora
                        : model::TipoElementoCimentacion::VigaAtado;
                    vc.referencia = (vc.tipo == model::TipoElementoCimentacion::VigaCentradora ? "VC" : "VA") +
                                    std::to_string(viga_idx++);
                    vc.seccion_armado = (vc.tipo == model::TipoElementoCimentacion::VigaCentradora)
                        ? cat_vigas[3]
                        : cat_vigas[1];
                    modelo.vigas.push_back(vc);

                    obra.add_elemento_cimentacion(
                        vc.tipo, a.nudo_id,
                        vc.seccion_armado.ancho_b_m, vc.longitud_eje_a_eje_m, vc.seccion_armado.canto_h_m);
                }
            }
        }
    }

    check_all_foundation(modelo, obra.datos_generales());
    modelo.medicion = compute_foundation_takeoff(modelo);
    return modelo;
}

std::size_t FoundationEngine::resolve_overlapping_footings(ModeloCompletoCimentacion& modelo) {
    // Equivalente a "eliminar_solapes" (elimsolap en planocim.dll / unir_elemento unirzp2)
    std::size_t uniones_realizadas = 0;
    for (std::size_t i = 0; i < modelo.zapatas.size(); ++i) {
        for (std::size_t j = i + 1; j < modelo.zapatas.size();) {
            auto& za = modelo.zapatas[i];
            const auto& zb = modelo.zapatas[j];

            const double dx = std::abs(za.centro_planta_m.x - zb.centro_planta_m.x);
            const double dy = std::abs(za.centro_planta_m.y - zb.centro_planta_m.y);
            const double lim_x = 0.5 * (za.ancho_x_m + zb.ancho_x_m);
            const double lim_y = 0.5 * (za.ancho_y_m + zb.ancho_y_m);

            if (dx < lim_x - 1.0e-3 && dy < lim_y - 1.0e-3) {
                // Fusionar zb dentro de za creando una zapata combinada
                const double min_x = std::min(za.centro_planta_m.x - 0.5 * za.ancho_x_m, zb.centro_planta_m.x - 0.5 * zb.ancho_x_m);
                const double max_x = std::max(za.centro_planta_m.x + 0.5 * za.ancho_x_m, zb.centro_planta_m.x + 0.5 * zb.ancho_x_m);
                const double min_y = std::min(za.centro_planta_m.y - 0.5 * za.ancho_y_m, zb.centro_planta_m.y - 0.5 * zb.ancho_y_m);
                const double max_y = std::max(za.centro_planta_m.y + 0.5 * za.ancho_y_m, zb.centro_planta_m.y + 0.5 * zb.ancho_y_m);

                za.centro_planta_m = {0.5 * (min_x + max_x), 0.5 * (min_y + max_y)};
                za.ancho_x_m = max_x - min_x;
                za.ancho_y_m = max_y - min_y;
                za.canto_total_m = std::max(za.canto_total_m, zb.canto_total_m);
                za.subtipo = SubtipoZapata::CombinadaMultipleArranques;
                za.tiene_parrilla_superior = true;
                za.parrilla_superior = za.parrilla_inferior;
                for (std::uint32_t nid : zb.nudos_arranque_ids) {
                    if (std::find(za.nudos_arranque_ids.begin(), za.nudos_arranque_ids.end(), nid) == za.nudos_arranque_ids.end()) {
                        za.nudos_arranque_ids.push_back(nid);
                    }
                }
                za.contorno_recortado_planta_m = build_footing_rectangle_polygon(za);

                modelo.zapatas.erase(modelo.zapatas.begin() + static_cast<std::ptrdiff_t>(j));
                ++uniones_realizadas;
            } else {
                ++j;
            }
        }
    }
    return uniones_realizadas;
}

void FoundationEngine::check_all_foundation(
    ModeloCompletoCimentacion& modelo,
    const model::DatosGeneralesObra& datos_generales) {
    modelo.resultados_zapatas.clear();
    for (const auto& zp : modelo.zapatas) {
        modelo.resultados_zapatas.push_back(check_footing(zp, modelo.arranques, datos_generales, modelo.opciones));
    }

    modelo.resultados_encepados.clear();
    for (const auto& enc : modelo.encepados) {
        const auto* arq = find_starter_by_node(modelo.arranques, enc.nudo_arranque_id);
        const ArranqueCimentacion fallback{};
        modelo.resultados_encepados.push_back(check_pile_cap(
            enc, arq != nullptr ? *arq : fallback, datos_generales, modelo.opciones));
    }

    modelo.resultados_vigas.clear();
    for (const auto& vg : modelo.vigas) {
        const auto* a_ini = find_starter_by_node(modelo.arranques, vg.nudo_ini_id);
        const auto* a_fin = find_starter_by_node(modelo.arranques, vg.nudo_fin_id);
        const ArranqueCimentacion fallback{};
        modelo.resultados_vigas.push_back(check_foundation_beam(
            vg,
            a_ini != nullptr ? *a_ini : fallback,
            a_fin != nullptr ? *a_fin : fallback,
            datos_generales,
            modelo.opciones));
    }

    modelo.medicion = compute_foundation_takeoff(modelo);
}

void FoundationEngine::design_all_foundation(
    model::Obra& obra,
    ModeloCompletoCimentacion& modelo) {
    modelo.resultados_zapatas.clear();
    for (auto& zp : modelo.zapatas) {
        modelo.resultados_zapatas.push_back(
            design_footing(zp, modelo.arranques, obra.datos_generales(), modelo.opciones));
    }

    modelo.resultados_encepados.clear();
    for (auto& enc : modelo.encepados) {
        const auto* arq = find_starter_by_node(modelo.arranques, enc.nudo_arranque_id);
        const ArranqueCimentacion fallback{};
        modelo.resultados_encepados.push_back(
            design_pile_cap(enc, arq != nullptr ? *arq : fallback, obra.datos_generales(), modelo.opciones));
    }

    modelo.resultados_vigas.clear();
    for (auto& vg : modelo.vigas) {
        const auto* a_ini = find_starter_by_node(modelo.arranques, vg.nudo_ini_id);
        const auto* a_fin = find_starter_by_node(modelo.arranques, vg.nudo_fin_id);
        const ArranqueCimentacion fallback{};
        modelo.resultados_vigas.push_back(
            design_foundation_beam(
                vg,
                a_ini != nullptr ? *a_ini : fallback,
                a_fin != nullptr ? *a_fin : fallback,
                obra.datos_generales(),
                modelo.opciones));
    }

    // Sincronizar dimensiones dimensionadas en obra.cimentacion().elementos
    auto& elems_obra = obra.cimentacion_mut().elementos;
    for (std::size_t i = 0; i < modelo.zapatas.size() && i < elems_obra.size(); ++i) {
        elems_obra[i].ancho_x_m = modelo.zapatas[i].ancho_x_m;
        elems_obra[i].ancho_y_m = modelo.zapatas[i].ancho_y_m;
        elems_obra[i].canto_m = modelo.zapatas[i].canto_total_m;
    }

    modelo.medicion = compute_foundation_takeoff(modelo);
}

MedicionCimentacion FoundationEngine::compute_foundation_takeoff(const ModeloCompletoCimentacion& modelo) {
    MedicionCimentacion med;
    for (const auto& zp : modelo.zapatas) {
        const double area = (!zp.contorno_recortado_planta_m.empty())
            ? compute_polygon_area_m2(zp.contorno_recortado_planta_m)
            : (zp.ancho_x_m * zp.ancho_y_m);
        med.volumen_hormigon_limpieza_m3 += area * modelo.opciones.espesor_hormigon_limpieza_m;
        med.volumen_hormigon_zapatas_m3 += area * zp.canto_total_m;

        if (zp.tipo_material == model::TipoElementoCimentacion::ZapataHormigonMasa) {
            ++med.num_zapatas_hm;
        } else {
            ++med.num_zapatas_ha;
            const double peso_x = zp.parrilla_inferior.num_barras_x *
                                  (zp.parrilla_inferior.longitud_barra_x_m + 2.0 * zp.parrilla_inferior.patilla_x_m) *
                                  rebar_weight_kg_per_m(zp.parrilla_inferior.diametro_x_mm);
            const double peso_y = zp.parrilla_inferior.num_barras_y *
                                  (zp.parrilla_inferior.longitud_barra_y_m + 2.0 * zp.parrilla_inferior.patilla_y_m) *
                                  rebar_weight_kg_per_m(zp.parrilla_inferior.diametro_y_mm);
            med.peso_acero_zapatas_kg += (peso_x + peso_y) * (zp.tiene_parrilla_superior ? 1.85 : 1.0);
        }
    }

    for (const auto& enc : modelo.encepados) {
        ++med.num_encepados;
        med.num_pilotes_totales += enc.pilotes.size();
        med.longitud_total_pilotes_m += static_cast<double>(enc.pilotes.size()) * enc.pilote_catalogo.longitud_m;
        med.volumen_hormigon_limpieza_m3 += enc.ancho_x_m * enc.ancho_y_m * modelo.opciones.espesor_hormigon_limpieza_m;
        med.volumen_hormigon_encepados_m3 += enc.ancho_x_m * enc.ancho_y_m * enc.canto_m;

        const double peso_tirante = enc.num_barras_tirante_ppal * (enc.ancho_x_m + 0.30) *
                                    rebar_weight_kg_per_m(enc.diametro_tirante_ppal_mm) * 2.0;
        med.peso_acero_encepados_kg += peso_tirante + 35.0;
    }

    for (const auto& vg : modelo.vigas) {
        if (vg.tipo == model::TipoElementoCimentacion::VigaCentradora) {
            ++med.num_vigas_centradoras;
        } else {
            ++med.num_vigas_atado;
        }
        med.volumen_hormigon_vigas_m3 +=
            vg.seccion_armado.ancho_b_m * vg.seccion_armado.canto_h_m * vg.longitud_libre_m;

        const double l_barra = vg.longitud_libre_m + vg.longitud_anclaje_extremo_ini_m + vg.longitud_anclaje_extremo_fin_m;
        const double peso_long =
            (vg.seccion_armado.num_barras_sup * rebar_weight_kg_per_m(vg.seccion_armado.diametro_sup_mm) +
             vg.seccion_armado.num_barras_inf * rebar_weight_kg_per_m(vg.seccion_armado.diametro_inf_mm) +
             vg.seccion_armado.num_barras_piel * rebar_weight_kg_per_m(vg.seccion_armado.diametro_piel_mm)) * l_barra;
        const double n_estribos = std::ceil(vg.longitud_libre_m / std::max(0.10, vg.seccion_armado.separacion_estribos_m));
        const double perim_estribo = 2.0 * (vg.seccion_armado.ancho_b_m + vg.seccion_armado.canto_h_m - 0.12);
        const double peso_est = n_estribos * perim_estribo * rebar_weight_kg_per_m(vg.seccion_armado.diametro_estribos_mm);
        med.peso_acero_vigas_kg += (peso_long + peso_est);
    }

    med.volumen_hormigon_total_m3 =
        med.volumen_hormigon_zapatas_m3 + med.volumen_hormigon_encepados_m3 + med.volumen_hormigon_vigas_m3;
    const double factor_mermas = 1.0 + 0.01 * modelo.opciones.porcentaje_mermas_acero;
    med.peso_acero_total_con_mermas_kg =
        (med.peso_acero_zapatas_kg + med.peso_acero_encepados_kg + med.peso_acero_vigas_kg) * factor_mermas;

    return med;
}

Escena3dCimentacion FoundationEngine::build_3d_scene(const ModeloCompletoCimentacion& modelo) {
    // Equivalente a vista_3D (plcplano_* en planocim.dll)
    Escena3dCimentacion escena;

    for (const auto& zp : modelo.zapatas) {
        BloqueHormigon3dCimentacion blk;
        blk.etiqueta = zp.referencia;
        blk.color_rgba = 0x9099A2FF;
        blk.centro_m = {zp.centro_planta_m.x, zp.centro_planta_m.y, -0.5 * zp.canto_total_m};
        blk.dimensiones_m = {zp.ancho_x_m, zp.ancho_y_m, zp.canto_total_m};
        escena.bloques_hormigon.push_back(blk);

        if (zp.tipo_material == model::TipoElementoCimentacion::ZapataHormigonArmado) {
            const double z_bar = -zp.canto_total_m + 0.06;
            const std::uint32_t nx = std::max(2u, zp.parrilla_inferior.num_barras_x);
            for (std::uint32_t i = 0; i < nx; ++i) {
                const double t = (nx > 1) ? (static_cast<double>(i) / static_cast<double>(nx - 1) - 0.5) : 0.0;
                const double y = zp.centro_planta_m.y + t * (zp.ancho_y_m - 0.12);
                const double x0 = zp.centro_planta_m.x - 0.5 * zp.ancho_x_m + 0.06;
                const double x1 = zp.centro_planta_m.x + 0.5 * zp.ancho_x_m - 0.06;
                BarraArmadura3dCimentacion bar;
                bar.etiqueta_elemento = zp.referencia;
                bar.posicion = "Parrilla Inf X";
                bar.diametro_mm = zp.parrilla_inferior.diametro_x_mm;
                bar.polilinea_m = {
                    {x0, y, z_bar + zp.parrilla_inferior.patilla_x_m},
                    {x0, y, z_bar},
                    {x1, y, z_bar},
                    {x1, y, z_bar + zp.parrilla_inferior.patilla_x_m}
                };
                escena.barras_armadura_3d.push_back(std::move(bar));
            }
        }
    }

    for (const auto& enc : modelo.encepados) {
        BloqueHormigon3dCimentacion blk;
        blk.etiqueta = enc.referencia;
        blk.color_rgba = 0x85909CFF;
        blk.centro_m = {enc.centro_planta_m.x, enc.centro_planta_m.y, -0.5 * enc.canto_m};
        blk.dimensiones_m = {enc.ancho_x_m, enc.ancho_y_m, enc.canto_m};
        escena.bloques_hormigon.push_back(blk);

        for (const auto& p : enc.pilotes) {
            CilindroPilote3dCimentacion cil;
            cil.etiqueta = enc.referencia + "_P" + std::to_string(p.indice_pilote);
            cil.color_rgba = 0x6E7885FF;
            cil.radio_m = 0.5 * enc.pilote_catalogo.diametro_o_lado_m;
            cil.p_ini_m = {enc.centro_planta_m.x + p.coord_relativa_m.x, enc.centro_planta_m.y + p.coord_relativa_m.y, -enc.canto_m};
            cil.p_fin_m = {cil.p_ini_m.x, cil.p_ini_m.y, -enc.canto_m - enc.pilote_catalogo.longitud_m};
            escena.cilindros_pilotes.push_back(cil);
        }
    }

    for (const auto& vg : modelo.vigas) {
        const auto* a = find_starter_by_node(modelo.arranques, vg.nudo_ini_id);
        const auto* b = find_starter_by_node(modelo.arranques, vg.nudo_fin_id);
        if (a != nullptr && b != nullptr) {
            BloqueHormigon3dCimentacion blk;
            blk.etiqueta = vg.referencia;
            blk.color_rgba = 0x9AA3ADFF;
            blk.centro_m = {
                0.5 * (a->posicion_m.x + b->posicion_m.x),
                0.5 * (a->posicion_m.y + b->posicion_m.y),
                -0.5 * vg.seccion_armado.canto_h_m
            };
            blk.dimensiones_m = {vg.longitud_libre_m, vg.seccion_armado.ancho_b_m, vg.seccion_armado.canto_h_m};
            escena.bloques_hormigon.push_back(blk);
        }
    }

    return escena;
}

} // namespace cypeclon::foundation
