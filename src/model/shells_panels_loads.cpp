#include "model/shells_panels_loads.hpp"
#include "model/obra.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cypeclon::model {

// ============================================================================
// P-07.1: ShellElementEngine (`calcmef.dll`, `calcmallad3d.dll`, `visesf3d.dll`)
// ============================================================================

core::LocalAxes3d ShellElementEngine::calcular_ejes_locales_lamina(
    const std::vector<core::Vec3d>& vertices,
    std::uint32_t arista_eje_x_index,
    double angulo_giro_ejes_rad,
    bool invertir_eje_z
) noexcept {
    if (vertices.size() < 3) {
        return core::LocalAxes3d{};
    }

    const std::size_t n = vertices.size();
    const std::size_t idx0 = static_cast<std::size_t>(arista_eje_x_index) % n;
    const std::size_t idx1 = (idx0 + 1) % n;

    core::Vec3d ex = (vertices[idx1] - vertices[idx0]).normalized();
    if (ex.norm_sq() < 1e-12) {
        ex = {1.0, 0.0, 0.0};
    }

    // Normal de Newell al polígono 3D
    core::Vec3d normal{0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < n; ++i) {
        const auto& cur = vertices[i];
        const auto& nxt = vertices[(i + 1) % n];
        normal.x += (cur.y - nxt.y) * (cur.z + nxt.z);
        normal.y += (cur.z - nxt.z) * (cur.x + nxt.x);
        normal.z += (cur.x - nxt.x) * (cur.y + nxt.y);
    }
    core::Vec3d ez = normal.normalized();
    if (ez.norm_sq() < 1e-12) {
        ez = {0.0, 0.0, 1.0};
    }
    if (invertir_eje_z) {
        ez = ez * -1.0;
    }

    core::Vec3d ey = ez.cross(ex).normalized();
    ex = ey.cross(ez).normalized();

    if (std::abs(angulo_giro_ejes_rad) > 1e-9) {
        const double c = std::cos(angulo_giro_ejes_rad);
        const double s = std::sin(angulo_giro_ejes_rad);
        const core::Vec3d ex_rot = ex * c + ey * s;
        const core::Vec3d ey_rot = ey * c - ex * s;
        ex = ex_rot.normalized();
        ey = ey_rot.normalized();
    }

    core::LocalAxes3d axes{};
    axes.origin = vertices.front();
    axes.u = ex;
    axes.v = ey;
    axes.w = ez;
    return axes;
}

double ShellElementEngine::calcular_area_poligono_3d(const std::vector<core::Vec3d>& vertices) noexcept {
    if (vertices.size() < 3) return 0.0;
    core::Vec3d sum_cross{0.0, 0.0, 0.0};
    const core::Vec3d& p0 = vertices.front();
    for (std::size_t i = 1; i + 1 < vertices.size(); ++i) {
        sum_cross = sum_cross + (vertices[i] - p0).cross(vertices[i + 1] - p0);
    }
    return 0.5 * sum_cross.norm();
}

namespace {
bool point_in_polygon_2d(const core::Vec2d& pt, const std::vector<core::Vec2d>& poly) noexcept {
    if (poly.size() < 3) return false;
    bool inside = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const bool intersect = ((poly[i].y > pt.y) != (poly[j].y > pt.y)) &&
            (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y) / ((poly[j].y - poly[i].y) + 1e-15) + poly[i].x);
        if (intersect) inside = !inside;
    }
    return inside;
}

double distance_point_to_segment_2d(const core::Vec2d& p, const core::Vec2d& a, const core::Vec2d& b) noexcept {
    const core::Vec2d ab = b - a;
    const double len_sq = ab.norm_sq();
    if (len_sq < 1e-12) return p.distance_to(a);
    const double t = std::clamp((p - a).dot(ab) / len_sq, 0.0, 1.0);
    return p.distance_to(a + ab * t);
}
} // namespace

bool ShellElementEngine::es_punto_interior_lamina(
    const Lamina3d& lamina,
    const core::Vec3d& punto_3d,
    double tolerancia_m
) noexcept {
    if (lamina.vertices.size() < 3) return false;
    const auto axes = calcular_ejes_locales_lamina(
        lamina.vertices, lamina.arista_eje_x_index, lamina.angulo_giro_ejes_rad, lamina.invertir_eje_z);
    const core::Vec3d origin = lamina.vertices.front();

    const core::Vec3d rel = punto_3d - origin;
    if (std::abs(rel.dot(axes.w)) > std::max(0.05, tolerancia_m * 10.0)) {
        return false;
    }

    auto to_2d = [&](const core::Vec3d& v) noexcept -> core::Vec2d {
        const core::Vec3d d = v - origin;
        return {d.dot(axes.u), d.dot(axes.v)};
    };

    const core::Vec2d pt2d = to_2d(punto_3d);
    std::vector<core::Vec2d> outer_2d;
    outer_2d.reserve(lamina.vertices.size());
    for (const auto& v : lamina.vertices) {
        outer_2d.push_back(to_2d(v));
    }

    bool in_outer = point_in_polygon_2d(pt2d, outer_2d);
    if (!in_outer) {
        // Comprobar si está sobre alguna arista exterior dentro de la tolerancia
        for (std::size_t i = 0; i < outer_2d.size(); ++i) {
            if (distance_point_to_segment_2d(pt2d, outer_2d[i], outer_2d[(i + 1) % outer_2d.size()]) <= tolerancia_m) {
                in_outer = true;
                break;
            }
        }
    }
    if (!in_outer) return false;

    // Excluir puntos estrictamente interiores a cualquier hueco de la lámina (`cm3dmalla_crea_hueco_lamina`)
    for (const auto& hueco : lamina.huecos) {
        if (hueco.vertices.size() < 3) continue;
        std::vector<core::Vec2d> hole_2d;
        hole_2d.reserve(hueco.vertices.size());
        for (const auto& hv : hueco.vertices) {
            hole_2d.push_back(to_2d(hv));
        }
        if (point_in_polygon_2d(pt2d, hole_2d)) {
            return false;
        }
    }
    return true;
}

MallaLamina3d ShellElementEngine::generar_mallado_lamina(
    const Lamina3d& lamina,
    double tamano_max_elemento_m
) {
    MallaLamina3d malla{};
    const double h_elem = std::max(0.10, tamano_max_elemento_m);
    malla.tamano_max_elemento_m = h_elem;

    if (lamina.vertices.size() < 3) {
        return malla;
    }

    const auto axes = calcular_ejes_locales_lamina(
        lamina.vertices, lamina.arista_eje_x_index, lamina.angulo_giro_ejes_rad, lamina.invertir_eje_z);
    const core::Vec3d origin = lamina.vertices.front();

    auto to_2d = [&](const core::Vec3d& v) noexcept -> core::Vec2d {
        const core::Vec3d d = v - origin;
        return {d.dot(axes.u), d.dot(axes.v)};
    };
    auto to_3d = [&](double u, double v) noexcept -> core::Vec3d {
        return origin + axes.u * u + axes.v * v;
    };

    std::vector<core::Vec2d> outer_2d;
    core::BBox2d box2d;
    for (const auto& v : lamina.vertices) {
        const core::Vec2d p2 = to_2d(v);
        outer_2d.push_back(p2);
        box2d.expand(p2);
    }

    const double width_u = std::max(0.10, box2d.max_pt.x - box2d.min_pt.x);
    const double height_v = std::max(0.10, box2d.max_pt.y - box2d.min_pt.y);
    const int nx = std::clamp(static_cast<int>(std::ceil(width_u / h_elem)), 1, 80);
    const int ny = std::clamp(static_cast<int>(std::ceil(height_v / h_elem)), 1, 80);
    const double du = width_u / static_cast<double>(nx);
    const double dv = height_v / static_cast<double>(ny);

    std::vector<std::uint32_t> grid_node_ids(static_cast<std::size_t>((nx + 1) * (ny + 1)), 0);

    auto get_or_create_node = [&](int ix, int iy) -> std::uint32_t {
        const std::size_t idx = static_cast<std::size_t>(iy * (nx + 1) + ix);
        if (grid_node_ids[idx] != 0) {
            return grid_node_ids[idx];
        }
        const double u = box2d.min_pt.x + static_cast<double>(ix) * du;
        const double v = box2d.min_pt.y + static_cast<double>(iy) * dv;
        const core::Vec2d p2{u, v};

        std::int32_t edge_idx = -1;
        for (std::size_t e = 0; e < outer_2d.size(); ++e) {
            if (distance_point_to_segment_2d(p2, outer_2d[e], outer_2d[(e + 1) % outer_2d.size()]) <= 0.15 * h_elem) {
                edge_idx = static_cast<std::int32_t>(e);
                break;
            }
        }

        const std::uint32_t nid = static_cast<std::uint32_t>(malla.nodos.size() + 1);
        malla.nodos.push_back(NodoMallaLamina{
            nid,
            to_3d(u, v),
            edge_idx >= 0,
            edge_idx
        });
        grid_node_ids[idx] = nid;
        return nid;
    };

    std::uint32_t tid = 1;
    for (int iy = 0; iy < ny; ++iy) {
        for (int ix = 0; ix < nx; ++ix) {
            const double u0 = box2d.min_pt.x + ix * du;
            const double v0 = box2d.min_pt.y + iy * dv;
            const double u1 = u0 + du;
            const double v1 = v0 + dv;

            // Triángulo 1: (ix, iy) -> (ix+1, iy) -> (ix+1, iy+1)
            const core::Vec3d c1_3d = to_3d((u0 + u1 + u1) / 3.0, (v0 + v0 + v1) / 3.0);
            if (es_punto_interior_lamina(lamina, c1_3d, 1e-4)) {
                const auto n_a = get_or_create_node(ix, iy);
                const auto n_b = get_or_create_node(ix + 1, iy);
                const auto n_c = get_or_create_node(ix + 1, iy + 1);
                const double area = 0.5 * du * dv;
                malla.triangulos.push_back(TrianguloMefLamina{tid++, {n_a, n_b, n_c}, area, c1_3d});
                malla.area_total_mallada_m2 += area;
            }

            // Triángulo 2: (ix, iy) -> (ix+1, iy+1) -> (ix, iy+1)
            const core::Vec3d c2_3d = to_3d((u0 + u1 + u0) / 3.0, (v0 + v1 + v1) / 3.0);
            if (es_punto_interior_lamina(lamina, c2_3d, 1e-4)) {
                const auto n_a = get_or_create_node(ix, iy);
                const auto n_c = get_or_create_node(ix + 1, iy + 1);
                const auto n_d = get_or_create_node(ix, iy + 1);
                const double area = 0.5 * du * dv;
                malla.triangulos.push_back(TrianguloMefLamina{tid++, {n_a, n_c, n_d}, area, c2_3d});
                malla.area_total_mallada_m2 += area;
            }
        }
    }

    return malla;
}

EsfuerzosUnitariosLamina ShellElementEngine::transformar_esfuerzos_lamina(
    const EsfuerzosUnitariosLamina& esf,
    double angulo_rotacion_rad
) noexcept {
    const double c = std::cos(angulo_rotacion_rad);
    const double s = std::sin(angulo_rotacion_rad);
    const double c2 = c * c;
    const double s2 = s * s;
    const double cs = c * s;

    EsfuerzosUnitariosLamina out{};
    out.nx_kn_m = esf.nx_kn_m * c2 + esf.ny_kn_m * s2 + 2.0 * esf.nxy_kn_m * cs;
    out.ny_kn_m = esf.nx_kn_m * s2 + esf.ny_kn_m * c2 - 2.0 * esf.nxy_kn_m * cs;
    out.nxy_kn_m = (esf.ny_kn_m - esf.nx_kn_m) * cs + esf.nxy_kn_m * (c2 - s2);

    out.mx_knm_m = esf.mx_knm_m * c2 + esf.my_knm_m * s2 + 2.0 * esf.mxy_knm_m * cs;
    out.my_knm_m = esf.mx_knm_m * s2 + esf.my_knm_m * c2 - 2.0 * esf.mxy_knm_m * cs;
    out.mxy_knm_m = (esf.my_knm_m - esf.mx_knm_m) * cs + esf.mxy_knm_m * (c2 - s2);

    out.qx_kn_m = esf.qx_kn_m * c + esf.qy_kn_m * s;
    out.qy_kn_m = -esf.qx_kn_m * s + esf.qy_kn_m * c;
    return out;
}

double ShellElementEngine::calcular_tension_von_mises_mpa(
    const EsfuerzosUnitariosLamina& esf,
    double espesor_m,
    int cara_z
) noexcept {
    const double h = std::max(0.005, espesor_m);
    const double sign_z = (cara_z > 0) ? 1.0 : ((cara_z < 0) ? -1.0 : 0.0);

    // Conversión de kN/m a MPa (MN/m^2 = kN/m^2 * 1e-3)
    const double sx_mpa = (esf.nx_kn_m / h + sign_z * (6.0 * esf.mx_knm_m) / (h * h)) * 1.0e-3;
    const double sy_mpa = (esf.ny_kn_m / h + sign_z * (6.0 * esf.my_knm_m) / (h * h)) * 1.0e-3;
    const double txy_mpa = (esf.nxy_kn_m / h + sign_z * (6.0 * esf.mxy_knm_m) / (h * h)) * 1.0e-3;

    return std::sqrt(std::max(0.0, sx_mpa * sx_mpa - sx_mpa * sy_mpa + sy_mpa * sy_mpa + 3.0 * txy_mpa * txy_mpa));
}

EsfuerzosIntegradosBanda ShellElementEngine::integrar_esfuerzos_en_banda(
    const EsfuerzosUnitariosLamina& esf_en_punto,
    const BandaIntegracionLamina& banda
) noexcept {
    const double b_tot = std::max(0.01, banda.ancho_total_m());
    EsfuerzosIntegradosBanda res{};
    res.axil_n_kn = esf_en_punto.nx_kn_m * b_tot;
    res.cortante_vy_kn = esf_en_punto.nxy_kn_m * b_tot;
    res.cortante_vz_kn = esf_en_punto.qx_kn_m * b_tot;
    res.torsor_mt_knm = 2.0 * esf_en_punto.mxy_knm_m * b_tot;
    res.flector_my_knm = esf_en_punto.mx_knm_m * b_tot;
    res.flector_mz_knm = (esf_en_punto.nx_kn_m * (banda.ancho_derecho_m - banda.ancho_izquierdo_m) * 0.5) * b_tot;
    return res;
}

CuantiasArmadoLamina ShellElementEngine::calcular_cuantias_wood_armer(
    const EsfuerzosUnitariosLamina& esf,
    double espesor_m,
    double recubrimiento_m,
    double fcd_mpa,
    double fyd_mpa
) noexcept {
    (void)fcd_mpa;
    CuantiasArmadoLamina res{};
    const double h_m = std::max(0.08, espesor_m);
    const double d_m = std::max(0.05, h_m - recubrimiento_m);
    const double z_m = 0.9 * d_m;

    // Momentos de diseño de Wood-Armer en caras inferior (+M) y superior (-M)
    const double mx_inf = std::max(0.0, esf.mx_knm_m + std::abs(esf.mxy_knm_m));
    const double my_inf = std::max(0.0, esf.my_knm_m + std::abs(esf.mxy_knm_m));
    const double mx_sup = std::max(0.0, -esf.mx_knm_m + std::abs(esf.mxy_knm_m));
    const double my_sup = std::max(0.0, -esf.my_knm_m + std::abs(esf.mxy_knm_m));

    // Tracción membranal concomitante en cada cara
    const double nx_trac = std::max(0.0, esf.nx_kn_m + std::abs(esf.nxy_kn_m));
    const double ny_trac = std::max(0.0, esf.ny_kn_m + std::abs(esf.nxy_kn_m));

    // Cuantía geométrica mínima por cara según EC2 / Código Estructural (0.0009 * Ac en losas)
    const double as_min_cara_cm2_m = 0.0009 * (h_m * 100.0) * 100.0;

    auto calc_as_cm2_m = [&](double m_knm_m, double n_trac_kn_m) noexcept -> double {
        const double f_tens_kn_m = (m_knm_m / z_m) + 0.5 * n_trac_kn_m;
        const double as_nec_cm2_m = (f_tens_kn_m / std::max(100.0, fyd_mpa * 1000.0)) * 1.0e4;
        return std::max(as_min_cara_cm2_m, as_nec_cm2_m);
    };

    res.asx_inf_cm2_m = calc_as_cm2_m(mx_inf, nx_trac);
    res.asy_inf_cm2_m = calc_as_cm2_m(my_inf, ny_trac);
    res.asx_sup_cm2_m = calc_as_cm2_m(mx_sup, nx_trac);
    res.asy_sup_cm2_m = calc_as_cm2_m(my_sup, ny_trac);
    res.cumple_cuantia_minima = true;
    return res;
}

// ============================================================================
// P-07.2: PanelLoadDistributionEngine (`panyos3d.dll`: `p3dcalsup_nousar_append_cargas_lineales_a_barras_panyo`)
// ============================================================================

std::vector<CargaLinealBarra> PanelLoadDistributionEngine::descomponer_cargas_panyo_en_barras(
    const Panyo3d& panyo,
    const std::vector<CargaSuperficialPanyo>& cargas_panyo,
    const std::vector<Barra3d>& barras_obra,
    const std::vector<Nudo3d>& nudos_obra,
    double tolerancia_coplanar_m
) {
    std::vector<CargaLinealBarra> generadas;
    if (panyo.vertices.size() < 3 || cargas_panyo.empty()) {
        return generadas;
    }

    const auto find_nudo_local = [&](std::uint32_t nid) -> const Nudo3d* {
        for (const auto& n : nudos_obra) {
            if (n.id == nid) return &n;
        }
        return nullptr;
    };

    const auto axes = ShellElementEngine::calcular_ejes_locales_lamina(panyo.vertices);
    const core::Vec3d origin = panyo.vertices.front();

    core::Vec3d dir_rep = panyo.direccion_reparto.normalized();
    if (dir_rep.norm_sq() < 1e-12) {
        dir_rep = axes.u;
    }
    // Dirección perpendicular al reparto dentro del plano del paño (eje de las barras portantes)
    core::Vec3d dir_perp = axes.w.cross(dir_rep).normalized();
    if (dir_perp.norm_sq() < 1e-12) {
        dir_perp = axes.v;
    }

    // Proyección del paño sobre el eje de reparto (s) y perpendicular (t)
    double s_min = 1.0e9, s_max = -1.0e9;
    double t_min = 1.0e9, t_max = -1.0e9;
    for (const auto& v : panyo.vertices) {
        const core::Vec3d rel = v - origin;
        const double s = rel.dot(dir_rep);
        const double t = rel.dot(dir_perp);
        s_min = std::min(s_min, s);
        s_max = std::max(s_max, s);
        t_min = std::min(t_min, t);
        t_max = std::max(t_max, t);
    }
    const double span_reparto_m = std::max(0.20, s_max - s_min);

    // Identificar las barras receptoras contenidas en el plano del paño y no paralelas a la dirección de reparto
    struct CandidateBar {
        std::uint32_t barra_id;
        double s_coord;
    };
    std::vector<CandidateBar> receptoras;

    for (const auto& b : barras_obra) {
        if (b.es_tirante) continue;
        if (!panyo.barras_activas_reparto.empty() &&
            std::find(panyo.barras_activas_reparto.begin(), panyo.barras_activas_reparto.end(), b.id) ==
                panyo.barras_activas_reparto.end()) {
            continue;
        }

        const Nudo3d* n1 = find_nudo_local(b.nudo_inicial_id);
        const Nudo3d* n2 = find_nudo_local(b.nudo_final_id);
        if (!n1 || !n2) continue;

        const double z1 = std::abs((n1->pos - origin).dot(axes.w));
        const double z2 = std::abs((n2->pos - origin).dot(axes.w));
        if (z1 > tolerancia_coplanar_m || z2 > tolerancia_coplanar_m) {
            continue;
        }

        const core::Vec3d mid = (n1->pos + n2->pos) * 0.5 - origin;
        const double s_mid = mid.dot(dir_rep);
        const double t_mid = mid.dot(dir_perp);
        if (s_mid < s_min - tolerancia_coplanar_m || s_mid > s_max + tolerancia_coplanar_m ||
            t_mid < t_min - tolerancia_coplanar_m || t_mid > t_max + tolerancia_coplanar_m) {
            continue;
        }

        // La barra recibe carga si forma cierto ángulo con la dirección de reparto (no es colineal al reparto)
        const core::Vec3d b_dir = (n2->pos - n1->pos).normalized();
        if (std::abs(b_dir.dot(dir_rep)) < 0.92) {
            receptoras.push_back(CandidateBar{b.id, s_mid});
        }
    }

    if (receptoras.empty()) {
        return generadas;
    }

    std::sort(receptoras.begin(), receptoras.end(),
              [](const CandidateBar& a, const CandidateBar& b) { return a.s_coord < b.s_coord; });

    // Calcular el ancho tributario isostático de cada barra receptora según su posición s_coord
    std::vector<double> anchos_tributarios(receptoras.size(), 0.0);
    if (receptoras.size() == 1) {
        anchos_tributarios[0] = span_reparto_m;
    } else {
        for (std::size_t i = 0; i < receptoras.size(); ++i) {
            const double left_bound = (i == 0)
                ? s_min
                : 0.5 * (receptoras[i - 1].s_coord + receptoras[i].s_coord);
            const double right_bound = (i + 1 == receptoras.size())
                ? s_max
                : 0.5 * (receptoras[i].s_coord + receptoras[i + 1].s_coord);
            anchos_tributarios[i] = std::max(0.05, right_bound - left_bound);
        }
    }

    // Generar las cargas lineales equivalentes para cada carga superficial asociada al paño
    for (const auto& csup : cargas_panyo) {
        if (csup.panyo_id != panyo.id) continue;

        double factor_zona = 1.0;
        if (csup.ambito == AmbitoCargaSuperficial::PoligonalEnZona && csup.poligono_zona.size() >= 3) {
            const double area_panyo = std::max(0.10, ShellElementEngine::calcular_area_poligono_3d(panyo.vertices));
            const double area_zona = ShellElementEngine::calcular_area_poligono_3d(csup.poligono_zona);
            factor_zona = std::clamp(area_zona / area_panyo, 0.05, 1.0);
        }

        for (std::size_t i = 0; i < receptoras.size(); ++i) {
            const double b_trib = anchos_tributarios[i] * factor_zona;
            const double q_ini = csup.valor_kn_m2 * b_trib;
            const double q_fin = (csup.es_variable ? csup.valor_q2_kn_m2 : csup.valor_kn_m2) * b_trib;

            CargaLinealBarra clin{};
            clin.barra_id = receptoras[i].barra_id;
            clin.hipotesis_id = csup.hipotesis_id;
            clin.q_ini_kn_m = q_ini;
            clin.q_fin_kn_m = q_fin;
            clin.x_ini_rel = 0.0;
            clin.x_fin_rel = 1.0;
            clin.direccion = csup.direccion;
            clin.en_ejes_locales = csup.en_ejes_locales_panyo;
            clin.tipo = csup.es_variable ? TipoCargaBarra::Trapezoidal : TipoCargaBarra::Uniforme;
            clin.generada_por_panyo = true;
            clin.panyo_origen_id = panyo.id;
            generadas.push_back(clin);
        }
    }

    return generadas;
}

// ============================================================================
// P-08.1: MemberLoadCalculator (`calcesf.dll`)
// ============================================================================

ReaccionesEmpotramientoCargaBarra MemberLoadCalculator::calcular_empotramiento_perfecto(
    const CargaLinealBarra& carga,
    double longitud_barra_m,
    double area_seccion_m2,
    double inercia_m4,
    double canto_h_m,
    double e_mpa,
    double alpha_termico
) noexcept {
    ReaccionesEmpotramientoCargaBarra r{};
    const double L = std::max(0.05, longitud_barra_m);

    switch (carga.tipo) {
        case TipoCargaBarra::Uniforme: {
            const double q = carga.q_ini_kn_m;
            r.f_local_ini_kn = 0.5 * q * L;
            r.f_local_fin_kn = 0.5 * q * L;
            r.m_empotramiento_ini_knm = (q * L * L) / 12.0;
            r.m_empotramiento_fin_knm = -(q * L * L) / 12.0;
            break;
        }
        case TipoCargaBarra::Puntual: {
            const double P = carga.q_ini_kn_m;
            const double a = std::clamp(carga.x_ini_rel, 0.0, 1.0) * L;
            const double b = L - a;
            r.f_local_ini_kn = P * (b * b * (3.0 * a + b)) / (L * L * L);
            r.f_local_fin_kn = P * (a * a * (a + 3.0 * b)) / (L * L * L);
            r.m_empotramiento_ini_knm = (P * a * b * b) / (L * L);
            r.m_empotramiento_fin_knm = -(P * a * a * b) / (L * L);
            break;
        }
        case TipoCargaBarra::Trapezoidal:
        case TipoCargaBarra::Triangular: {
            const double q1 = carga.q_ini_kn_m;
            const double q2 = (carga.tipo == TipoCargaBarra::Triangular && std::abs(carga.q_fin_kn_m) < 1e-9)
                ? 0.0
                : carga.q_fin_kn_m;
            const double frac = std::clamp(carga.x_fin_rel - carga.x_ini_rel, 0.05, 1.0);
            const double Leff = L * frac;
            r.f_local_ini_kn = (7.0 * q1 + 3.0 * q2) * Leff / 20.0;
            r.f_local_fin_kn = (3.0 * q1 + 7.0 * q2) * Leff / 20.0;
            r.m_empotramiento_ini_knm = (3.0 * q1 + 2.0 * q2) * Leff * Leff / 60.0;
            r.m_empotramiento_fin_knm = -(2.0 * q1 + 3.0 * q2) * Leff * Leff / 60.0;
            break;
        }
        case TipoCargaBarra::MomentoPuntual: {
            const double M = carga.q_ini_kn_m;
            const double a = std::clamp(carga.x_ini_rel, 0.0, 1.0) * L;
            const double b = L - a;
            r.f_local_ini_kn = -(6.0 * M * a * b) / (L * L * L);
            r.f_local_fin_kn = +(6.0 * M * a * b) / (L * L * L);
            r.m_empotramiento_ini_knm = M * b * (2.0 * a - b) / (L * L);
            r.m_empotramiento_fin_knm = M * a * (2.0 * b - a) / (L * L);
            break;
        }
        case TipoCargaBarra::IncrementoTemperatura: {
            const double e_kn_m2 = e_mpa * 1000.0;
            r.axil_termico_o_pretensado_kn = e_kn_m2 * area_seccion_m2 * alpha_termico * carga.delta_temp_uniforme_c;
            const double m_grad = e_kn_m2 * inercia_m4 * alpha_termico * (carga.delta_temp_gradiente_c / std::max(0.02, canto_h_m));
            r.m_empotramiento_ini_knm = m_grad;
            r.m_empotramiento_fin_knm = -m_grad;
            break;
        }
        case TipoCargaBarra::Pretensado: {
            r.axil_termico_o_pretensado_kn = -carga.axil_pretensado_kn;
            const double m_pret = carga.axil_pretensado_kn * carga.excentricidad_pretensado_m;
            r.m_empotramiento_ini_knm = -m_pret;
            r.m_empotramiento_fin_knm = +m_pret;
            break;
        }
    }
    return r;
}

} // namespace cypeclon::model
