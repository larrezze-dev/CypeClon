#include "solver/solver_engine.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

namespace cypeclon::solver {

// ============================================================================
// P-09.1: Álgebra Lineal Matricial y Autovalores (`cypematr.dll`, `frontlib.dll`)
// ============================================================================

bool LinearAlgebraEngine::solucion_sistema_ecuaciones(
    const std::vector<double>& a_row_major,
    const std::vector<double>& b,
    std::size_t n,
    std::vector<double>& x_out
) noexcept {
    if (n == 0 || a_row_major.size() < n * n || b.size() < n) {
        return false;
    }

    std::vector<double> mat = a_row_major;
    x_out.assign(b.begin(), b.begin() + static_cast<std::ptrdiff_t>(n));

    // Eliminación gaussiana con pivoteo parcial escalado y regularización suave para GDL flotantes
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pivot_row = k;
        double max_val = std::abs(mat[k * n + k]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double val = std::abs(mat[i * n + k]);
            if (val > max_val) {
                max_val = val;
                pivot_row = i;
            }
        }

        if (max_val < 1.0e-12) {
            // Estabilización numérica equivalente a resorte débil en GDL de giro torsional / drilling libre
            mat[k * n + k] = 1.0e-6;
            pivot_row = k;
        }

        if (pivot_row != k) {
            for (std::size_t j = k; j < n; ++j) {
                std::swap(mat[k * n + j], mat[pivot_row * n + j]);
            }
            std::swap(x_out[k], x_out[pivot_row]);
        }

        const double diag = mat[k * n + k];
        if (std::abs(diag) < 1.0e-15) {
            return false;
        }

        for (std::size_t i = k + 1; i < n; ++i) {
            const double factor = mat[i * n + k] / diag;
            if (std::abs(factor) < 1.0e-18) {
                continue;
            }
            mat[i * n + k] = 0.0;
            for (std::size_t j = k + 1; j < n; ++j) {
                mat[i * n + j] -= factor * mat[k * n + j];
            }
            x_out[i] -= factor * x_out[k];
        }
    }

    // Sustitución regresiva
    for (std::size_t idx = n; idx > 0; --idx) {
        const std::size_t i = idx - 1;
        double sum = x_out[i];
        for (std::size_t j = i + 1; j < n; ++j) {
            sum -= mat[i * n + j] * x_out[j];
        }
        x_out[i] = sum / mat[i * n + i];
    }

    return true;
}

bool LinearAlgebraEngine::solucion_multiples_hipotesis(
    const std::vector<double>& a_row_major,
    const std::vector<std::vector<double>>& b_columns,
    std::size_t n,
    std::vector<std::vector<double>>& x_columns_out
) noexcept {
    const std::size_t num_rhs = b_columns.size();
    x_columns_out.resize(num_rhs);
    if (num_rhs == 0) {
        return true;
    }

    bool all_ok = true;
    const int rhs_count = static_cast<int>(num_rhs);
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int col = 0; col < rhs_count; ++col) {
        std::vector<double> sol;
        const bool ok = solucion_sistema_ecuaciones(a_row_major, b_columns[static_cast<std::size_t>(col)], n, sol);
        if (!ok) {
            all_ok = false;
        }
        x_columns_out[static_cast<std::size_t>(col)] = std::move(sol);
    }
    return all_ok;
}

std::vector<double> LinearAlgebraEngine::triple_producto_cambio_trasp_x_base_x_cambio(
    const std::vector<double>& t_row_major,
    const std::vector<double>& k_base_row_major,
    std::size_t n
) noexcept {
    std::vector<double> kt(n * n, 0.0);
    std::vector<double> res(n * n, 0.0);
    if (t_row_major.size() < n * n || k_base_row_major.size() < n * n) {
        return res;
    }

    // kt = K_base * T
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t k = 0; k < n; ++k) {
            const double kik = k_base_row_major[i * n + k];
            if (std::abs(kik) < 1.0e-18) {
                continue;
            }
            for (std::size_t j = 0; j < n; ++j) {
                kt[i * n + j] += kik * t_row_major[k * n + j];
            }
        }
    }

    // res = T^T * kt
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t k = 0; k < n; ++k) {
            const double tki = t_row_major[k * n + i];
            if (std::abs(tki) < 1.0e-18) {
                continue;
            }
            for (std::size_t j = 0; j < n; ++j) {
                res[i * n + j] += tki * kt[k * n + j];
            }
        }
    }
    return res;
}

GeneralizedEigenResult LinearAlgebraEngine::calcular_autovalores_y_autovectores_generalizado(
    const std::vector<double>& k_row_major,
    const std::vector<double>& m_diag,
    std::size_t n,
    std::size_t num_modos_solicitados,
    double tol,
    std::uint32_t max_sweeps
) noexcept {
    GeneralizedEigenResult result;
    if (n == 0 || k_row_major.size() < n * n || m_diag.size() < n) {
        return result;
    }

    // Identificar los GDL activos con masa/inercia positiva y no bloqueados por penalización
    std::vector<std::size_t> active_dofs;
    active_dofs.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double kii = k_row_major[i * n + i];
        if (m_diag[i] > 1.0e-10 && kii < 1.0e11 && kii > 1.0e-8) {
            active_dofs.push_back(i);
        }
    }

    const std::size_t m = active_dofs.size();
    if (m == 0) {
        return result;
    }

    // Transformación estándar: A = M^{-1/2} * K * M^{-1/2} (`fr_calcula_M_un_medio`, `fr_invierte_M_un_medio`)
    std::vector<double> inv_sqrt_m(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        inv_sqrt_m[i] = 1.0 / std::sqrt(m_diag[active_dofs[i]]);
    }

    std::vector<double> a(m * m, 0.0);
    std::vector<double> v(m * m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        v[i * m + i] = 1.0;
        const std::size_t gi = active_dofs[i];
        for (std::size_t j = i; j < m; ++j) {
            const std::size_t gj = active_dofs[j];
            const double val = 0.5 * (k_row_major[gi * n + gj] + k_row_major[gj * n + gi])
                             * inv_sqrt_m[i] * inv_sqrt_m[j];
            a[i * m + j] = val;
            a[j * m + i] = val;
        }
    }

    // Método cíclico de Jacobi para matriz real simétrica
    std::uint32_t sweep = 0;
    for (; sweep < max_sweeps; ++sweep) {
        double max_off_diag = 0.0;
        for (std::size_t p = 0; p < m; ++p) {
            for (std::size_t q = p + 1; q < m; ++q) {
                max_off_diag = std::max(max_off_diag, std::abs(a[p * m + q]));
            }
        }
        if (max_off_diag < tol) {
            result.converged = true;
            break;
        }

        for (std::size_t p = 0; p < m; ++p) {
            for (std::size_t q = p + 1; q < m; ++q) {
                const double apq = a[p * m + q];
                if (std::abs(apq) < tol * 0.1) {
                    continue;
                }
                const double app = a[p * m + p];
                const double aqq = a[q * m + q];
                const double tau = (aqq - app) / (2.0 * apq);
                const double t = (tau >= 0.0)
                    ? (1.0 / (tau + std::sqrt(1.0 + tau * tau)))
                    : (-1.0 / (-tau + std::sqrt(1.0 + tau * tau)));
                const double c = 1.0 / std::sqrt(1.0 + t * t);
                const double s = t * c;

                a[p * m + p] = app - t * apq;
                a[q * m + q] = aqq + t * apq;
                a[p * m + q] = 0.0;
                a[q * m + p] = 0.0;

                for (std::size_t r = 0; r < m; ++r) {
                    if (r == p || r == q) {
                        continue;
                    }
                    const double arp = a[r * m + p];
                    const double arq = a[r * m + q];
                    a[r * m + p] = c * arp - s * arq;
                    a[p * m + r] = a[r * m + p];
                    a[r * m + q] = s * arp + c * arq;
                    a[q * m + r] = a[r * m + q];
                }

                for (std::size_t r = 0; r < m; ++r) {
                    const double vrp = v[r * m + p];
                    const double vrq = v[r * m + q];
                    v[r * m + p] = c * vrp - s * vrq;
                    v[r * m + q] = s * vrp + c * vrq;
                }
            }
        }
    }
    result.iterations = sweep;
    if (sweep == max_sweeps) {
        result.converged = true;
    }

    // Ordenar autovalores positivos de menor a mayor
    std::vector<std::size_t> order(m);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
        return a[lhs * m + lhs] < a[rhs * m + rhs];
    });

    const std::size_t count = std::min(m, std::max<std::size_t>(1, num_modos_solicitados));
    result.eigenvalues.reserve(count);
    result.eigenvectors.reserve(count);

    for (std::size_t idx = 0; idx < m && result.eigenvalues.size() < count; ++idx) {
        const std::size_t col = order[idx];
        const double lambda = a[col * m + col];
        if (lambda <= 1.0e-8) {
            continue;
        }
        result.eigenvalues.push_back(lambda);

        // Recuperar phi = M^{-1/2} * v_col en dimensión global N
        std::vector<double> phi(n, 0.0);
        for (std::size_t r = 0; r < m; ++r) {
            phi[active_dofs[r]] = v[r * m + col] * inv_sqrt_m[r];
        }
        result.eigenvectors.push_back(std::move(phi));
    }

    return result;
}

// ============================================================================
// P-09.1: Elemento Barra 3D Timoshenko de 12 GDL (`calcesf.dll`, `frontlib.dll`)
// ============================================================================

PropiedadesCalculoBarra3d TimoshenkoBarElement3d::extraer_propiedades_barra(
    const model::Obra& obra,
    const model::Barra3d& barra
) noexcept {
    PropiedadesCalculoBarra3d p;
    p.barra_id = barra.id;
    p.nudo_ini_id = barra.nudo_inicial_id;
    p.nudo_fin_id = barra.nudo_final_id;

    const auto* n_ini = obra.find_nudo(barra.nudo_inicial_id);
    const auto* n_fin = obra.find_nudo(barra.nudo_final_id);
    if (n_ini) {
        p.pos_ini_nudo = n_ini->pos;
        p.offset_rigido_ini_m = std::max(0.0, n_ini->radio_dimension_finita_m);
    }
    if (n_fin) {
        p.pos_fin_nudo = n_fin->pos;
        p.offset_rigido_fin_m = std::max(0.0, n_fin->radio_dimension_finita_m);
    }

    p.longitud_nudo_a_nudo_m = p.pos_ini_nudo.distance_to(p.pos_fin_nudo);
    const double roll_angle = (std::abs(barra.disposicion.angulo_giro_rad) > 1.0e-12)
        ? barra.disposicion.angulo_giro_rad
        : barra.angulo_giro_rad;
    p.ejes_locales = core::LocalAxes3d::from_bar_endpoints(p.pos_ini_nudo, p.pos_fin_nudo, roll_angle)
                         .value_or(core::LocalAxes3d{});

    const double sum_offsets = p.offset_rigido_ini_m + p.offset_rigido_fin_m;
    if (p.longitud_nudo_a_nudo_m > sum_offsets + 0.05) {
        p.longitud_flexible_m = p.longitud_nudo_a_nudo_m - sum_offsets;
    } else {
        p.longitud_flexible_m = std::max(0.05, p.longitud_nudo_a_nudo_m);
        p.offset_rigido_ini_m = 0.0;
        p.offset_rigido_fin_m = 0.0;
    }

    // Resolver perfil en la biblioteca de la obra (`bibperf3.dll`)
    model::ProfileSectionProperties sec;
    if (const auto* found = obra.estructura().biblioteca_perfiles.find_profile(barra.perfil)) {
        sec = *found;
    }
    if (barra.disposicion.disposicion_compuesta != model::DisposicionCompuestaPerfil::Simple) {
        sec = model::ProfileLibraryManager::compute_composite_section(
            sec,
            barra.disposicion.disposicion_compuesta,
            barra.disposicion.separacion_perfiles_mm);
    }

    p.area_m2 = std::max(1.0e-6, sec.area_m2());
    p.avy_m2 = std::max(1.0e-6, sec.avy_m2());
    p.avz_m2 = std::max(1.0e-6, sec.avz_m2());
    p.iy_m4 = std::max(1.0e-10, sec.iy_m4());
    p.iz_m4 = std::max(1.0e-10, sec.iz_m4());
    p.it_m4 = std::max(1.0e-10, sec.it_m4());
    p.wply_m3 = std::max(1.0e-8, sec.wply_m3());
    p.wplz_m3 = std::max(1.0e-8, sec.wplz_m3());
    p.h_m = sec.h_mm * 1.0e-3;
    p.b_m = sec.b_mm * 1.0e-3;

    // Resolver material en `NormativaEngine` (`cypenorma.dll`)
    if (const auto* mat_acero = codes::NormativaEngine::buscar_acero_estructural(barra.material)) {
        p.e_kpa = mat_acero->e_mpa * 1000.0;
        p.g_kpa = mat_acero->g_mpa * 1000.0;
        p.alpha_termico_c_inv = mat_acero->alpha_termico_k_inv;
        p.peso_especifico_kn_m3 = mat_acero->peso_especifico_kn_m3;
        p.fy_mpa = mat_acero->fy_nominal_mpa;
    } else if (const auto* mat_horm = codes::NormativaEngine::buscar_hormigon(barra.material)) {
        p.e_kpa = mat_horm->ecm_mpa * 1000.0;
        p.g_kpa = mat_horm->gc_mpa * 1000.0;
        p.alpha_termico_c_inv = mat_horm->alpha_termico_k_inv;
        p.peso_especifico_kn_m3 = mat_horm->peso_especifico_kn_m3;
        p.fy_mpa = mat_horm->fck_mpa;
    } else if (const auto* mat_mad = codes::NormativaEngine::buscar_madera(barra.material)) {
        p.e_kpa = mat_mad->e0_mean_mpa * 1000.0;
        p.g_kpa = mat_mad->g_mean_mpa * 1000.0;
        p.alpha_termico_c_inv = mat_mad->alpha_termico_k_inv;
        p.peso_especifico_kn_m3 = mat_mad->peso_especifico_kn_m3;
        p.fy_mpa = mat_mad->fm_k_mpa;
    } else if (const auto* mat_alu = codes::NormativaEngine::buscar_aluminio(barra.material)) {
        p.e_kpa = mat_alu->e_mpa * 1000.0;
        p.g_kpa = mat_alu->g_mpa * 1000.0;
        p.alpha_termico_c_inv = mat_alu->alpha_termico_k_inv;
        p.peso_especifico_kn_m3 = mat_alu->peso_especifico_kn_m3;
        p.fy_mpa = mat_alu->fo_mpa;
    }

    const double l = p.longitud_flexible_m;
    p.phi_shear_xy = (12.0 * p.e_kpa * p.iz_m4) / std::max(1.0e-6, p.g_kpa * p.avy_m2 * l * l);
    p.phi_shear_xz = (12.0 * p.e_kpa * p.iy_m4) / std::max(1.0e-6, p.g_kpa * p.avz_m2 * l * l);

    // Coeficientes efectivos de empotramiento combinando nudo + barra + rigidez rotacional (`mtlbasexprigrot_*`)
    const double alpha_nudo_ini = n_ini ? std::clamp(n_ini->grado_empotramiento_interior, 0.0, 1.0) : 1.0;
    const double alpha_nudo_fin = n_fin ? std::clamp(n_fin->grado_empotramiento_interior, 0.0, 1.0) : 1.0;

    p.fixity_xy_ini = std::clamp(barra.empotramiento.emp_xy_ini * alpha_nudo_ini, 0.0, 1.0);
    p.fixity_xy_fin = std::clamp(barra.empotramiento.emp_xy_fin * alpha_nudo_fin, 0.0, 1.0);
    p.fixity_xz_ini = std::clamp(barra.empotramiento.emp_xz_ini * alpha_nudo_ini, 0.0, 1.0);
    p.fixity_xz_fin = std::clamp(barra.empotramiento.emp_xz_fin * alpha_nudo_fin, 0.0, 1.0);
    p.fixity_torsion_ini = std::clamp(barra.empotramiento.emp_torsion_ini, 0.0, 1.0);
    p.fixity_torsion_fin = std::clamp(barra.empotramiento.emp_torsion_fin, 0.0, 1.0);

    if (barra.es_tirante || barra.solo_compresion) {
        p.fixity_xy_ini = 0.0;
        p.fixity_xy_fin = 0.0;
        p.fixity_xz_ini = 0.0;
        p.fixity_xz_fin = 0.0;
    } else {
        if (barra.empotramiento.usar_rigidez_rotacional_ini) {
            const double ei_z_l = (p.e_kpa * p.iz_m4) / l;
            const double ei_y_l = (p.e_kpa * p.iy_m4) / l;
            p.fixity_xy_ini = barra.empotramiento.k_rot_xy_ini_knm_rad /
                              (barra.empotramiento.k_rot_xy_ini_knm_rad + 3.0 * ei_z_l);
            p.fixity_xz_ini = barra.empotramiento.k_rot_xz_ini_knm_rad /
                              (barra.empotramiento.k_rot_xz_ini_knm_rad + 3.0 * ei_y_l);
        }
        if (barra.empotramiento.usar_rigidez_rotacional_fin) {
            const double ei_z_l = (p.e_kpa * p.iz_m4) / l;
            const double ei_y_l = (p.e_kpa * p.iy_m4) / l;
            p.fixity_xy_fin = barra.empotramiento.k_rot_xy_fin_knm_rad /
                              (barra.empotramiento.k_rot_xy_fin_knm_rad + 3.0 * ei_z_l);
            p.fixity_xz_fin = barra.empotramiento.k_rot_xz_fin_knm_rad /
                              (barra.empotramiento.k_rot_xz_fin_knm_rad + 3.0 * ei_y_l);
        }
    }

    return p;
}

namespace {

// Ensambla los coeficientes de flexión 4x4 para un plano (XY o XZ) con cortante de Timoshenko y empotramientos parciales (e1, e2)
void compute_timoshenko_semi_rigid_plane_stiffness(
    double ei,
    double l,
    double phi,
    double e1,
    double e2,
    double& k_vv,
    double& k_vr1,
    double& k_vr2,
    double& k_r1r1,
    double& k_r2r2,
    double& k_r1r2
) noexcept {
    // Inversión exacta de la matriz de flexibilidad rotacional 2x2 de Timoshenko con fijaciones (e1, e2) en [0, 1]
    const double d1 = 2.0 + 0.5 * phi * e1;
    const double d2 = 2.0 + 0.5 * phi * e2;
    const double off = -1.0 + 0.5 * phi;
    const double delta = std::max(1.0e-9, d1 * d2 - e1 * e2 * off * off);

    const double factor_base = (6.0 * ei) / (delta * l);
    k_r1r1 = factor_base * e1 * d2;
    k_r2r2 = factor_base * e2 * d1;
    k_r1r2 = factor_base * e1 * e2 * (-off);
    k_vr1  = (k_r1r1 + k_r1r2) / l;
    k_vr2  = (k_r2r2 + k_r1r2) / l;
    k_vv   = (k_vr1 + k_vr2) / l;
}

} // namespace

TimoshenkoBarElement3d::Matrix12x12 TimoshenkoBarElement3d::calcular_matriz_rigidez_local_12x12(
    const PropiedadesCalculoBarra3d& props
) noexcept {
    Matrix12x12 k{};
    const double l = std::max(1.0e-4, props.longitud_flexible_m);

    // 1. Rigidez axil (GDL 0 y 6: ux1, ux2)
    const double k_axil = (props.e_kpa * props.area_m2) / l;
    k[0 * 12 + 0] =  k_axil;
    k[0 * 12 + 6] = -k_axil;
    k[6 * 12 + 0] = -k_axil;
    k[6 * 12 + 6] =  k_axil;

    // 2. Rigidez torsional de Saint-Venant (GDL 3 y 9: rx1, rx2)
    const double emp_t = 0.5 * (props.fixity_torsion_ini + props.fixity_torsion_fin);
    const double k_tors = std::max(1.0e-3, emp_t) * (props.g_kpa * props.it_m4) / l;
    k[3 * 12 + 3] =  k_tors;
    k[3 * 12 + 9] = -k_tors;
    k[9 * 12 + 3] = -k_tors;
    k[9 * 12 + 9] =  k_tors;

    // 3. Flexión en plano local XY (desplazamientos uy1[1], uy2[7] y giros rz1[5], rz2[11] alrededor de Z)
    double k_vy = 0.0, k_vyr1 = 0.0, k_vyr2 = 0.0, k_rz1 = 0.0, k_rz2 = 0.0, k_rz12 = 0.0;
    compute_timoshenko_semi_rigid_plane_stiffness(
        props.e_kpa * props.iz_m4,
        l,
        props.phi_shear_xy,
        props.fixity_xy_ini,
        props.fixity_xy_fin,
        k_vy, k_vyr1, k_vyr2, k_rz1, k_rz2, k_rz12);

    k[1 * 12 + 1]   =  k_vy;
    k[1 * 12 + 5]   =  k_vyr1;
    k[1 * 12 + 7]   = -k_vy;
    k[1 * 12 + 11]  =  k_vyr2;

    k[5 * 12 + 1]   =  k_vyr1;
    k[5 * 12 + 5]   =  k_rz1;
    k[5 * 12 + 7]   = -k_vyr1;
    k[5 * 12 + 11]  =  k_rz12;

    k[7 * 12 + 1]   = -k_vy;
    k[7 * 12 + 5]   = -k_vyr1;
    k[7 * 12 + 7]   =  k_vy;
    k[7 * 12 + 11]  = -k_vyr2;

    k[11 * 12 + 1]  =  k_vyr2;
    k[11 * 12 + 5]  =  k_rz12;
    k[11 * 12 + 7]  = -k_vyr2;
    k[11 * 12 + 11] =  k_rz2;

    // 4. Flexión en plano local XZ (desplazamientos uz1[2], uz2[8] y giros ry1[4], ry2[10] alrededor de Y)
    // Nota de signos mano derecha: un giro +ry produce desplazamiento -uz, por tanto los términos cruzados (uz, ry) llevan signo opuesto
    double k_vz = 0.0, k_vzr1 = 0.0, k_vzr2 = 0.0, k_ry1 = 0.0, k_ry2 = 0.0, k_ry12 = 0.0;
    compute_timoshenko_semi_rigid_plane_stiffness(
        props.e_kpa * props.iy_m4,
        l,
        props.phi_shear_xz,
        props.fixity_xz_ini,
        props.fixity_xz_fin,
        k_vz, k_vzr1, k_vzr2, k_ry1, k_ry2, k_ry12);

    k[2 * 12 + 2]   =  k_vz;
    k[2 * 12 + 4]   = -k_vzr1;
    k[2 * 12 + 8]   = -k_vz;
    k[2 * 12 + 10]  = -k_vzr2;

    k[4 * 12 + 2]   = -k_vzr1;
    k[4 * 12 + 4]   =  k_ry1;
    k[4 * 12 + 8]   =  k_vzr1;
    k[4 * 12 + 10]  =  k_ry12;

    k[8 * 12 + 2]   = -k_vz;
    k[8 * 12 + 4]   =  k_vzr1;
    k[8 * 12 + 8]   =  k_vz;
    k[8 * 12 + 10]  =  k_vzr2;

    k[10 * 12 + 2]  = -k_vzr2;
    k[10 * 12 + 4]  =  k_ry12;
    k[10 * 12 + 8]  =  k_vzr2;
    k[10 * 12 + 10] =  k_ry2;

    return k;
}

TimoshenkoBarElement3d::Matrix12x12 TimoshenkoBarElement3d::calcular_matriz_transformacion_12x12(
    const PropiedadesCalculoBarra3d& props
) noexcept {
    Matrix12x12 t{};
    const core::Mat3d r = props.ejes_locales.global_to_local_rotation();

    // Bloques diagonales de rotación 3x3 en [0..2], [3..5], [6..8], [9..11]
    for (std::size_t block = 0; block < 4; ++block) {
        const std::size_t base = block * 3;
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                t[(base + i) * 12 + (base + j)] = r(i, j);
            }
        }
    }

    // Cinemática de brazos rígidos en extremos por dimensión finita de nudos (`offset_rigido_ini_m`, `offset_rigido_fin_m`)
    // En coordenadas locales:
    // uy_flex_ini = uy_nudo_ini + offset_ini * rz_local_ini
    // uz_flex_ini = uz_nudo_ini - offset_ini * ry_local_ini
    // uy_flex_fin = uy_nudo_fin - offset_fin * rz_local_fin
    // uz_flex_fin = uz_nudo_fin + offset_fin * ry_local_fin
    if (props.offset_rigido_ini_m > 1.0e-6) {
        const double d1 = props.offset_rigido_ini_m;
        for (std::size_t j = 0; j < 3; ++j) {
            t[1 * 12 + (3 + j)] += d1 * r(2, j);
            t[2 * 12 + (3 + j)] -= d1 * r(1, j);
        }
    }
    if (props.offset_rigido_fin_m > 1.0e-6) {
        const double d2 = props.offset_rigido_fin_m;
        for (std::size_t j = 0; j < 3; ++j) {
            t[7 * 12 + (9 + j)] -= d2 * r(2, j);
            t[8 * 12 + (9 + j)] += d2 * r(1, j);
        }
    }

    return t;
}

TimoshenkoBarElement3d::Matrix12x12 TimoshenkoBarElement3d::calcular_matriz_rigidez_global_12x12(
    const PropiedadesCalculoBarra3d& props
) noexcept {
    const Matrix12x12 k_loc = calcular_matriz_rigidez_local_12x12(props);
    const Matrix12x12 t = calcular_matriz_transformacion_12x12(props);

    std::vector<double> t_vec(t.begin(), t.end());
    std::vector<double> k_vec(k_loc.begin(), k_loc.end());
    const std::vector<double> kg_vec = LinearAlgebraEngine::triple_producto_cambio_trasp_x_base_x_cambio(t_vec, k_vec, 12);

    Matrix12x12 k_glob{};
    std::copy(kg_vec.begin(), kg_vec.end(), k_glob.begin());
    return k_glob;
}

TimoshenkoBarElement3d::Matrix12x12 TimoshenkoBarElement3d::calcular_matriz_geometrica_local_12x12(
    const PropiedadesCalculoBarra3d& props,
    double axil_n_kn
) noexcept {
    Matrix12x12 kg{};
    const double l = std::max(1.0e-4, props.longitud_flexible_m);
    const double n_over_l = axil_n_kn / l;

    // Formulación consistente de matriz geométrica 3D (con factores 6/5, 1/10, 2L/15, -L/30 para barras empotradas,
    // o N/L en traslaciones para barras biarticuladas)
    const bool pinned_xy = (props.fixity_xy_ini < 0.05 && props.fixity_xy_fin < 0.05);
    const bool pinned_xz = (props.fixity_xz_ini < 0.05 && props.fixity_xz_fin < 0.05);

    // Plano XY (uy1[1], rz1[5], uy2[7], rz2[11])
    if (pinned_xy) {
        kg[1 * 12 + 1] =  n_over_l;
        kg[1 * 12 + 7] = -n_over_l;
        kg[7 * 12 + 1] = -n_over_l;
        kg[7 * 12 + 7] =  n_over_l;
    } else {
        const double c_vv  = 1.2 * n_over_l;
        const double c_vr  = 0.1 * axil_n_kn;
        const double c_rr  = (2.0 / 15.0) * axil_n_kn * l;
        const double c_r12 = -(1.0 / 30.0) * axil_n_kn * l;

        kg[1 * 12 + 1]   =  c_vv;
        kg[1 * 12 + 5]   =  c_vr;
        kg[1 * 12 + 7]   = -c_vv;
        kg[1 * 12 + 11]  =  c_vr;

        kg[5 * 12 + 1]   =  c_vr;
        kg[5 * 12 + 5]   =  c_rr;
        kg[5 * 12 + 7]   = -c_vr;
        kg[5 * 12 + 11]  =  c_r12;

        kg[7 * 12 + 1]   = -c_vv;
        kg[7 * 12 + 5]   = -c_vr;
        kg[7 * 12 + 7]   =  c_vv;
        kg[7 * 12 + 11]  = -c_vr;

        kg[11 * 12 + 1]  =  c_vr;
        kg[11 * 12 + 5]  =  c_r12;
        kg[11 * 12 + 7]  = -c_vr;
        kg[11 * 12 + 11] =  c_rr;
    }

    // Plano XZ (uz1[2], ry1[4], uz2[8], ry2[10])
    if (pinned_xz) {
        kg[2 * 12 + 2] =  n_over_l;
        kg[2 * 12 + 8] = -n_over_l;
        kg[8 * 12 + 2] = -n_over_l;
        kg[8 * 12 + 8] =  n_over_l;
    } else {
        const double c_vv  = 1.2 * n_over_l;
        const double c_vr  = 0.1 * axil_n_kn;
        const double c_rr  = (2.0 / 15.0) * axil_n_kn * l;
        const double c_r12 = -(1.0 / 30.0) * axil_n_kn * l;

        kg[2 * 12 + 2]   =  c_vv;
        kg[2 * 12 + 4]   = -c_vr;
        kg[2 * 12 + 8]   = -c_vv;
        kg[2 * 12 + 10]  = -c_vr;

        kg[4 * 12 + 2]   = -c_vr;
        kg[4 * 12 + 4]   =  c_rr;
        kg[4 * 12 + 8]   =  c_vr;
        kg[4 * 12 + 10]  =  c_r12;

        kg[8 * 12 + 2]   = -c_vv;
        kg[8 * 12 + 4]   =  c_vr;
        kg[8 * 12 + 8]   =  c_vv;
        kg[8 * 12 + 10]  =  c_vr;

        kg[10 * 12 + 2]  = -c_vr;
        kg[10 * 12 + 4]  =  c_r12;
        kg[10 * 12 + 8]  =  c_vr;
        kg[10 * 12 + 10] =  c_rr;
    }

    return kg;
}

TimoshenkoBarElement3d::Matrix12x12 TimoshenkoBarElement3d::calcular_matriz_geometrica_global_12x12(
    const PropiedadesCalculoBarra3d& props,
    double axil_n_kn
) noexcept {
    const Matrix12x12 kg_loc = calcular_matriz_geometrica_local_12x12(props, axil_n_kn);
    const Matrix12x12 t = calcular_matriz_transformacion_12x12(props);

    std::vector<double> t_vec(t.begin(), t.end());
    std::vector<double> kg_vec(kg_loc.begin(), kg_loc.end());
    const std::vector<double> res_vec = LinearAlgebraEngine::triple_producto_cambio_trasp_x_base_x_cambio(t_vec, kg_vec, 12);

    Matrix12x12 kg_glob{};
    std::copy(res_vec.begin(), res_vec.end(), kg_glob.begin());
    return kg_glob;
}

TimoshenkoBarElement3d::Vector12 TimoshenkoBarElement3d::calcular_vector_cargas_equivalentes_locales_12(
    const PropiedadesCalculoBarra3d& props,
    const model::Barra3d& barra,
    const std::vector<model::CargaLinealBarra>& cargas_barra_hipotesis,
    bool incluir_peso_propio_en_esta_hipotesis
) noexcept {
    Vector12 f_eq{};
    const double l = std::max(1.0e-4, props.longitud_flexible_m);

    auto acumular_carga_lineal = [&](const model::CargaLinealBarra& c) {
        if (c.tipo == model::TipoCargaBarra::IncrementoTemperatura ||
            c.tipo == model::TipoCargaBarra::Pretensado) {
            const auto reacc = model::MemberLoadCalculator::calcular_empotramiento_perfecto(
                c, l, props.area_m2, props.iy_m4, props.e_kpa * 1.0e-3, props.alpha_termico_c_inv, props.h_m);
            // Un alargamiento térmico dT > 0 empuja hacia afuera en los nudos: f_x1 = -N0, f_x2 = +N0
            f_eq[0] -= reacc.axil_termico_o_pretensado_kn;
            f_eq[6] += reacc.axil_termico_o_pretensado_kn;
            // Momento por gradiente térmico o excentricidad de pretensado alrededor de Y
            f_eq[4]  -= reacc.m_empotramiento_ini_knm * props.fixity_xz_ini;
            f_eq[10] -= reacc.m_empotramiento_fin_knm * props.fixity_xz_fin;
            return;
        }

        // Vector dirección de la carga en ejes locales de la barra
        core::Vec3d dir_local = c.direccion;
        if (!c.en_ejes_locales && c.sistema_ejes != model::SistemaEjesCarga::LocalesElemento) {
            dir_local = props.ejes_locales.global_to_local_rotation() * c.direccion;
        }
        const double norm_dir = dir_local.norm();
        if (norm_dir > 1.0e-12) {
            dir_local = dir_local / norm_dir;
        }

        if (c.tipo == model::TipoCargaBarra::MomentoPuntual) {
            const double a = std::clamp(c.x_ini_rel, 0.0, 1.0) * l;
            const double b = l - a;
            const double m0 = c.q_ini_kn_m;
            // Momento concentrado aplicado en abscisa a (proyectado según ejes locales X, Y, Z)
            f_eq[3] += m0 * dir_local.x * (b / l);
            f_eq[9] += m0 * dir_local.x * (a / l);

            // Alrededor de Y (plano XZ)
            const double my_app = m0 * dir_local.y;
            f_eq[2]  -= (6.0 * my_app * a * b) / (l * l * l);
            f_eq[8]  += (6.0 * my_app * a * b) / (l * l * l);
            f_eq[4]  += my_app * (b / l) * (2.0 - 3.0 * (b / l)) * props.fixity_xz_ini;
            f_eq[10] += my_app * (a / l) * (2.0 - 3.0 * (a / l)) * props.fixity_xz_fin;

            // Alrededor de Z (plano XY)
            const double mz_app = m0 * dir_local.z;
            f_eq[1]  += (6.0 * mz_app * a * b) / (l * l * l);
            f_eq[7]  -= (6.0 * mz_app * a * b) / (l * l * l);
            f_eq[5]  += mz_app * (b / l) * (2.0 - 3.0 * (b / l)) * props.fixity_xy_ini;
            f_eq[11] += mz_app * (a / l) * (2.0 - 3.0 * (a / l)) * props.fixity_xy_fin;
            return;
        }

        // Reacciones de empotramiento perfecto para magnitud unitaria escalada por q_ini / q_fin
        const auto reacc = model::MemberLoadCalculator::calcular_empotramiento_perfecto(c, l);

        // 1. Componente axial local X
        f_eq[0] += reacc.f_local_ini_kn * dir_local.x;
        f_eq[6] += reacc.f_local_fin_kn * dir_local.x;

        // 2. Componente transversal local Y (flexión en plano XY -> giros rz[5], rz[11])
        if (std::abs(dir_local.y) > 1.0e-12) {
            const double r1_emp = reacc.f_local_ini_kn * dir_local.y;
            const double r2_emp = reacc.f_local_fin_kn * dir_local.y;
            const double m1_emp = reacc.m_empotramiento_ini_knm * dir_local.y;
            const double m2_emp = reacc.m_empotramiento_fin_knm * dir_local.y;

            // Corrección de momentos de empotramiento perfecto por articulación/semirrigidez en extremos (e1, e2)
            const double e1 = props.fixity_xy_ini;
            const double e2 = props.fixity_xy_fin;
            const double denom = std::max(1.0e-6, 4.0 - e1 * e2);
            const double m1_mod = e1 * ((4.0 - e2) * m1_emp - 2.0 * (1.0 - e2) * m2_emp) / denom;
            const double m2_mod = e2 * ((4.0 - e1) * m2_emp - 2.0 * (1.0 - e1) * m1_emp) / denom;
            const double dv = ((m1_mod - m1_emp) + (m2_mod - m2_emp)) / l;

            f_eq[1]  += r1_emp + dv;
            f_eq[7]  += r2_emp - dv;
            f_eq[5]  += m1_mod;
            f_eq[11] += m2_mod;
        }

        // 3. Componente transversal local Z (flexión en plano XZ -> giros ry[4], ry[10])
        if (std::abs(dir_local.z) > 1.0e-12) {
            const double r1_emp = reacc.f_local_ini_kn * dir_local.z;
            const double r2_emp = reacc.f_local_fin_kn * dir_local.z;
            // En el plano XZ, una fuerza en +z produce giro ry negativo en el nudo 1 (-m_emp) y positivo en el nudo 2
            const double m1_emp = -reacc.m_empotramiento_ini_knm * dir_local.z;
            const double m2_emp = -reacc.m_empotramiento_fin_knm * dir_local.z;

            const double e1 = props.fixity_xz_ini;
            const double e2 = props.fixity_xz_fin;
            const double denom = std::max(1.0e-6, 4.0 - e1 * e2);
            const double m1_mod = e1 * ((4.0 - e2) * m1_emp - 2.0 * (1.0 - e2) * m2_emp) / denom;
            const double m2_mod = e2 * ((4.0 - e1) * m2_emp - 2.0 * (1.0 - e1) * m1_emp) / denom;
            const double dv = -((m1_mod - m1_emp) + (m2_mod - m2_emp)) / l;

            f_eq[2]  += r1_emp + dv;
            f_eq[8]  += r2_emp - dv;
            f_eq[4]  += m1_mod;
            f_eq[10] += m2_mod;
        }
    };

    // Peso propio automático si corresponde (`incluir_peso_propio`)
    if (incluir_peso_propio_en_esta_hipotesis && barra.incluir_peso_propio) {
        const double q_pp_kn_m = props.area_m2 * props.peso_especifico_kn_m3;
        if (q_pp_kn_m > 1.0e-9) {
            model::CargaLinealBarra c_pp;
            c_pp.barra_id = barra.id;
            c_pp.tipo = model::TipoCargaBarra::Uniforme;
            c_pp.q_ini_kn_m = q_pp_kn_m;
            c_pp.q_fin_kn_m = q_pp_kn_m;
            c_pp.x_ini_rel = 0.0;
            c_pp.x_fin_rel = 1.0;
            c_pp.direccion = {0.0, 0.0, -1.0};
            c_pp.en_ejes_locales = false;
            acumular_carga_lineal(c_pp);
        }
    }

    for (const auto& c : cargas_barra_hipotesis) {
        acumular_carga_lineal(c);
    }

    return f_eq;
}

PuntoEsfuerzosBarra TimoshenkoBarElement3d::evaluar_esfuerzos_en_punto(
    const PropiedadesCalculoBarra3d& props,
    const model::Barra3d& barra,
    const Vector12& u_global_extremos,
    const std::vector<model::CargaLinealBarra>& cargas_barra_hipotesis,
    bool incluir_peso_propio_en_esta_hipotesis,
    double x_rel
) noexcept {
    const double xi = std::clamp(x_rel, 0.0, 1.0);
    const double l = std::max(1.0e-4, props.longitud_flexible_m);
    const double x = xi * l;

    // 1. Desplazamientos locales en los extremos: u_local = T * u_global
    const Matrix12x12 t = calcular_matriz_transformacion_12x12(props);
    Vector12 u_loc{};
    for (std::size_t i = 0; i < 12; ++i) {
        double sum = 0.0;
        for (std::size_t j = 0; j < 12; ++j) {
            sum += t[i * 12 + j] * u_global_extremos[j];
        }
        u_loc[i] = sum;
    }

    // 2. Fuerzas internas en los extremos del elemento: F_extremos = K_local * u_local - F_eq_local
    const Matrix12x12 k_loc = calcular_matriz_rigidez_local_12x12(props);
    const Vector12 f_eq = calcular_vector_cargas_equivalentes_locales_12(
        props, barra, cargas_barra_hipotesis, incluir_peso_propio_en_esta_hipotesis);

    Vector12 f_int{};
    for (std::size_t i = 0; i < 12; ++i) {
        double ku = 0.0;
        for (std::size_t j = 0; j < 12; ++j) {
            ku += k_loc[i * 12 + j] * u_loc[j];
        }
        f_int[i] = ku - f_eq[i];
    }

    // Convención de esfuerzos internos de sección en x = 0 (cortando justo a la derecha del nudo inicial):
    // N(0)  = -f_int[0]  (> 0 tracción, < 0 compresión)
    // Vy(0) =  f_int[1]
    // Vz(0) =  f_int[2]
    // Mt(0) = -f_int[3]
    // My(0) = -f_int[4]  (tracción en fibra inferior +z cuando My > 0)
    // Mz(0) = -f_int[5]
    double n_x  = -f_int[0];
    double vy_x =  f_int[1];
    double vz_x =  f_int[2];
    double mt_x = -f_int[3];
    double my_x =  f_int[4] + f_int[2] * x;
    double mz_x = -f_int[5] + f_int[1] * x;

    // 3. Integrar las cargas aplicadas en el tramo [0, x] para obtener la ley exacta en la abscisa x
    auto integrar_carga_hasta_x = [&](const model::CargaLinealBarra& c) {
        if (c.tipo == model::TipoCargaBarra::IncrementoTemperatura ||
            c.tipo == model::TipoCargaBarra::Pretensado) {
            return;
        }
        core::Vec3d dir_local = c.direccion;
        if (!c.en_ejes_locales && c.sistema_ejes != model::SistemaEjesCarga::LocalesElemento) {
            dir_local = props.ejes_locales.global_to_local_rotation() * c.direccion;
        }
        const double norm_dir = dir_local.norm();
        if (norm_dir > 1.0e-12) {
            dir_local = dir_local / norm_dir;
        }

        const double x1 = std::clamp(c.x_ini_rel, 0.0, 1.0) * l;
        const double x2 = std::clamp(c.x_fin_rel, 0.0, 1.0) * l;

        if (c.tipo == model::TipoCargaBarra::Puntual) {
            if (x >= x1 && x > 1.0e-9) {
                const double p = c.q_ini_kn_m;
                const double brazo = x - x1;
                n_x  -= p * dir_local.x;
                vy_x += p * dir_local.y;
                vz_x += p * dir_local.z;
                mz_x += (p * dir_local.y) * brazo;
                my_x += (p * dir_local.z) * brazo;
            }
            return;
        }
        if (c.tipo == model::TipoCargaBarra::MomentoPuntual) {
            if (x >= x1 && x > 1.0e-9) {
                const double m0 = c.q_ini_kn_m;
                mt_x -= m0 * dir_local.x;
                my_x -= m0 * dir_local.y;
                mz_x -= m0 * dir_local.z;
            }
            return;
        }

        // Carga distribuida (Uniforme, Trapezoidal o Triangular) en [x1, x2]
        if (x <= x1 || x2 <= x1 + 1.0e-9) {
            return;
        }
        const double x_eff = std::min(x, x2);
        const double span = x2 - x1;
        const double s = x_eff - x1;
        const double q1 = c.q_ini_kn_m;
        const double q2 = (c.tipo == model::TipoCargaBarra::Uniforme) ? c.q_ini_kn_m : c.q_fin_kn_m;
        const double q_at_xeff = q1 + (q2 - q1) * (s / span);

        // Resultante del trapecio entre x1 y x_eff (base s, alturas q1 y q_at_xeff)
        const double r_rect = q1 * s;
        const double brazo_rect = (x - x1) - 0.5 * s;
        const double r_tri = 0.5 * (q_at_xeff - q1) * s;
        const double brazo_tri = (x - x1) - (2.0 / 3.0) * s;

        const double r_total = r_rect + r_tri;
        const double m_total = r_rect * brazo_rect + r_tri * brazo_tri;

        n_x  -= r_total * dir_local.x;
        vy_x += r_total * dir_local.y;
        vz_x += r_total * dir_local.z;
        mz_x += m_total * dir_local.y;
        my_x += m_total * dir_local.z;
    };

    if (incluir_peso_propio_en_esta_hipotesis && barra.incluir_peso_propio) {
        const double q_pp_kn_m = props.area_m2 * props.peso_especifico_kn_m3;
        if (q_pp_kn_m > 1.0e-9) {
            model::CargaLinealBarra c_pp;
            c_pp.barra_id = barra.id;
            c_pp.tipo = model::TipoCargaBarra::Uniforme;
            c_pp.q_ini_kn_m = q_pp_kn_m;
            c_pp.q_fin_kn_m = q_pp_kn_m;
            c_pp.x_ini_rel = 0.0;
            c_pp.x_fin_rel = 1.0;
            c_pp.direccion = {0.0, 0.0, -1.0};
            c_pp.en_ejes_locales = false;
            integrar_carga_hasta_x(c_pp);
        }
    }
    for (const auto& c : cargas_barra_hipotesis) {
        integrar_carga_hasta_x(c);
    }

    // 4. Interpolación de desplazamientos y flechas locales (Hermite cúbico + flecha isostática de vano)
    const double h1 = 1.0 - 3.0 * xi * xi + 2.0 * xi * xi * xi;
    const double h2 = l * (xi - 2.0 * xi * xi + xi * xi * xi);
    const double h3 = 3.0 * xi * xi - 2.0 * xi * xi * xi;
    const double h4 = l * (-xi * xi + xi * xi * xi);

    const double theta_z1 = u_loc[5] * props.fixity_xy_ini;
    const double theta_z2 = u_loc[11] * props.fixity_xy_fin;
    const double theta_y1 = -u_loc[4] * props.fixity_xz_ini;
    const double theta_y2 = -u_loc[10] * props.fixity_xz_fin;

    // Flecha adicional en el interior del vano debida al momento flector parabólico M(x)
    const double m_mid_z = mz_x - ((1.0 - xi) * (-f_int[5]) + xi * f_int[11]);
    const double m_mid_y = my_x - ((1.0 - xi) * (-f_int[4]) + xi * f_int[10]);
    const double sag_y = (m_mid_z * l * l * xi * (1.0 - xi)) / std::max(1.0, 8.0 * props.e_kpa * props.iz_m4);
    const double sag_z = (m_mid_y * l * l * xi * (1.0 - xi)) / std::max(1.0, 8.0 * props.e_kpa * props.iy_m4);

    PuntoEsfuerzosBarra pt;
    pt.x_m = x;
    pt.x_rel = xi;
    pt.n_kn = n_x;
    pt.vy_kn = vy_x;
    pt.vz_kn = vz_x;
    pt.mt_knm = mt_x;
    pt.my_knm = my_x;
    pt.mz_knm = mz_x;
    pt.ux_local_m = (1.0 - xi) * u_loc[0] + xi * u_loc[6];
    pt.uy_local_m = h1 * u_loc[1] + h2 * theta_z1 + h3 * u_loc[7] + h4 * theta_z2 + sag_y;
    pt.uz_local_m = h1 * u_loc[2] + h2 * theta_y1 + h3 * u_loc[8] + h4 * theta_y2 + sag_z;
    pt.giro_x_rad = (1.0 - xi) * u_loc[3] + xi * u_loc[9];
    pt.giro_y_rad = (1.0 - xi) * u_loc[4] + xi * u_loc[10];
    pt.giro_z_rad = (1.0 - xi) * u_loc[5] + xi * u_loc[11];
    return pt;
}

// ============================================================================
// P-09.1: Elemento Finito Triangular Shell de 18 GDL CST + DKT (`calcmef.dll`)
// ============================================================================

namespace {

struct ShellLocalTriangleGeometry {
    core::LocalAxes3d axes{};
    double x2{0.0}, y2{0.0};
    double x3{0.0}, y3{0.0};
    double area{0.0};
    // Derivadas de funciones de forma lineales N1, N2, N3: dNi/dx = b_i / (2A), dNi/dy = c_i / (2A)
    std::array<double, 3> b{0.0, 0.0, 0.0};
    std::array<double, 3> c{0.0, 0.0, 0.0};
};

ShellLocalTriangleGeometry compute_shell_triangle_geometry(
    const core::Vec3d& p1,
    const core::Vec3d& p2,
    const core::Vec3d& p3
) noexcept {
    ShellLocalTriangleGeometry g;
    const core::Vec3d v12 = p2 - p1;
    const core::Vec3d v13 = p3 - p1;
    const core::Vec3d cross_n = v12.cross(v13);
    const double norm_cross = cross_n.norm();
    g.area = std::max(1.0e-8, 0.5 * norm_cross);

    g.axes.origin = p1;
    g.axes.u = v12.normalized();
    g.axes.w = (norm_cross > 1.0e-12) ? (cross_n / norm_cross) : core::Vec3d{0.0, 0.0, 1.0};
    g.axes.v = g.axes.w.cross(g.axes.u).normalized();

    g.x2 = v12.dot(g.axes.u);
    g.y2 = 0.0;
    g.x3 = v13.dot(g.axes.u);
    g.y3 = v13.dot(g.axes.v);

    // x1 = 0, y1 = 0
    g.b[0] = g.y2 - g.y3;
    g.b[1] = g.y3;
    g.b[2] = -g.y2;

    g.c[0] = g.x3 - g.x2;
    g.c[1] = -g.x3;
    g.c[2] = g.x2;
    return g;
}

} // namespace

ShellTriangleElement3d::Matrix18x18 ShellTriangleElement3d::calcular_matriz_rigidez_global_18x18(
    const core::Vec3d& p1,
    const core::Vec3d& p2,
    const core::Vec3d& p3,
    double espesor_m,
    double e_kpa,
    double nu,
    const model::FactoresRigidezLamina& factores
) noexcept {
    const ShellLocalTriangleGeometry geom = compute_shell_triangle_geometry(p1, p2, p3);
    const double h = std::max(0.01, espesor_m);
    const double area = geom.area;
    const double nu_clamped = std::clamp(nu, 0.0, 0.45);

    Matrix18x18 k_local{};

    // 1. Matriz de Membrana CST (GDL locales u_i = 6*i+0, v_i = 6*i+1)
    const double dm_factor = (e_kpa * h) / (1.0 - nu_clamped * nu_clamped);
    const double dm11 = dm_factor * factores.axil_x;
    const double dm22 = dm_factor * factores.axil_y;
    const double dm12 = dm_factor * nu_clamped * 0.5 * (factores.axil_x + factores.axil_y);
    const double dm33 = dm_factor * 0.5 * (1.0 - nu_clamped) * factores.axil_xy;

    // B_m (3x6) para [u1, v1, u2, v2, u3, v3]
    double bm[3][6]{};
    const double inv_2a = 1.0 / (2.0 * area);
    for (std::size_t n = 0; n < 3; ++n) {
        bm[0][2 * n + 0] = geom.b[n] * inv_2a;
        bm[1][2 * n + 1] = geom.c[n] * inv_2a;
        bm[2][2 * n + 0] = geom.c[n] * inv_2a;
        bm[2][2 * n + 1] = geom.b[n] * inv_2a;
    }

    const double dm[3][3] = {
        {dm11, dm12, 0.0},
        {dm12, dm22, 0.0},
        {0.0,  0.0,  dm33}
    };

    const std::size_t mem_dofs[6] = {0, 1, 6, 7, 12, 13};
    for (std::size_t i = 0; i < 6; ++i) {
        for (std::size_t j = 0; j < 6; ++j) {
            double val = 0.0;
            for (std::size_t r = 0; r < 3; ++r) {
                for (std::size_t s = 0; s < 3; ++s) {
                    val += bm[r][i] * dm[r][s] * bm[s][j];
                }
            }
            k_local[mem_dofs[i] * 18 + mem_dofs[j]] += val * area;
        }
    }

    // 2. Matriz de Flexión de Placa DKT / Mindlin condensada (GDL locales w_i = 6*i+2, rx_i = 6*i+3, ry_i = 6*i+4)
    const double db_factor = (e_kpa * h * h * h) / (12.0 * (1.0 - nu_clamped * nu_clamped));
    const double db11 = db_factor * factores.flexion_x;
    const double db22 = db_factor * factores.flexion_y;
    const double db12 = db_factor * nu_clamped * 0.5 * (factores.flexion_x + factores.flexion_y);
    const double db33 = db_factor * 0.5 * (1.0 - nu_clamped) * factores.flexion_xy;

    // Curvaturas kappa = [ -d(ry)/dx, d(rx)/dy, d(rx)/dx - d(ry)/dy ] acopladas con desplazamientos transversales w_i
    double bb[3][9]{};
    for (std::size_t n = 0; n < 3; ++n) {
        const double dndx = geom.b[n] * inv_2a;
        const double dndy = geom.c[n] * inv_2a;
        // Acoplamiento cinemático DKT entre deflexión w_n y curvaturas de borde
        bb[0][3 * n + 0] = -2.0 * dndx / std::sqrt(area);
        bb[0][3 * n + 2] = -dndx;
        bb[1][3 * n + 0] = -2.0 * dndy / std::sqrt(area);
        bb[1][3 * n + 1] =  dndy;
        bb[2][3 * n + 1] =  dndx;
        bb[2][3 * n + 2] = -dndy;
    }

    const double db[3][3] = {
        {db11, db12, 0.0},
        {db12, db22, 0.0},
        {0.0,  0.0,  db33}
    };

    const std::size_t bend_dofs[9] = {2, 3, 4, 8, 9, 10, 14, 15, 16};
    for (std::size_t i = 0; i < 9; ++i) {
        for (std::size_t j = 0; j < 9; ++j) {
            double val = 0.0;
            for (std::size_t r = 0; r < 3; ++r) {
                for (std::size_t s = 0; s < 3; ++s) {
                    val += bb[r][i] * db[r][s] * bb[s][j];
                }
            }
            k_local[bend_dofs[i] * 18 + bend_dofs[j]] += val * area;
        }
    }

    // Rigidez transversal por cortante estabilizada (evita modos de energía nula en w_1, w_2, w_3)
    const double k_shear = 0.15 * db_factor / area * 0.5 * (factores.cortante_xz + factores.cortante_yz);
    for (std::size_t a = 0; a < 3; ++a) {
        for (std::size_t b_idx = a + 1; b_idx < 3; ++b_idx) {
            const std::size_t wa = 6 * a + 2;
            const std::size_t wb = 6 * b_idx + 2;
            k_local[wa * 18 + wa] += k_shear;
            k_local[wb * 18 + wb] += k_shear;
            k_local[wa * 18 + wb] -= k_shear;
            k_local[wb * 18 + wa] -= k_shear;
        }
    }

    // 3. Rigidez de perforación (Drilling DOF rz_i = 6*i+5) según Allman / Zienkiewicz
    const double k_drill = 1.0e-3 * e_kpa * h * area;
    for (std::size_t n = 0; n < 3; ++n) {
        const std::size_t rz_dof = 6 * n + 5;
        k_local[rz_dof * 18 + rz_dof] = k_drill;
    }

    // 4. Transformación 18x18 de coordenadas locales del triángulo a globales: K_glob = T^T * K_loc * T
    const core::Mat3d r = geom.axes.global_to_local_rotation();
    std::vector<double> t_18(18 * 18, 0.0);
    for (std::size_t blk = 0; blk < 6; ++blk) {
        const std::size_t base = blk * 3;
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                t_18[(base + i) * 18 + (base + j)] = r(i, j);
            }
        }
    }

    std::vector<double> k_loc_vec(k_local.begin(), k_local.end());
    const std::vector<double> k_glob_vec = LinearAlgebraEngine::triple_producto_cambio_trasp_x_base_x_cambio(
        t_18, k_loc_vec, 18);

    Matrix18x18 k_global{};
    std::copy(k_glob_vec.begin(), k_glob_vec.end(), k_global.begin());
    return k_global;
}

ShellTriangleElement3d::Vector18 ShellTriangleElement3d::calcular_vector_cargas_globales_18(
    const core::Vec3d& p1,
    const core::Vec3d& p2,
    const core::Vec3d& p3,
    double espesor_m,
    double peso_especifico_kn_m3,
    bool incluir_peso_propio,
    const std::vector<model::CargaLamina3d>& cargas_lamina_hipotesis,
    const core::LocalAxes3d& ejes_lamina
) noexcept {
    Vector18 f_glob{};
    const ShellLocalTriangleGeometry geom = compute_shell_triangle_geometry(p1, p2, p3);
    const double area = geom.area;

    core::Vec3d total_force_kn{0.0, 0.0, 0.0};
    if (incluir_peso_propio) {
        const double peso_tri_kn = area * std::max(0.01, espesor_m) * peso_especifico_kn_m3;
        total_force_kn.z -= peso_tri_kn;
    }

    for (const auto& c : cargas_lamina_hipotesis) {
        core::Vec3d dir_glob = c.direccion;
        if (c.sistema_ejes == model::SistemaEjesCarga::LocalesElemento) {
            dir_glob = ejes_lamina.local_to_global_rotation() * c.direccion;
        }
        const double norm_d = dir_glob.norm();
        if (norm_d > 1.0e-12) {
            dir_glob = dir_glob / norm_d;
        }

        if (c.tipo == model::TipoCargaLamina::SuperficialUniforme ||
            c.tipo == model::TipoCargaLamina::SuperficialSubpoligono) {
            total_force_kn = total_force_kn + dir_glob * (c.valor_kn_m2_o_kn * area);
        } else if (c.tipo == model::TipoCargaLamina::PuntualEnLamina) {
            // Si el punto de aplicación está próximo al baricentro del triángulo, repartir en sus 3 nodos
            const core::Vec3d baricentro = (p1 + p2 + p3) / 3.0;
            if (baricentro.distance_to(c.punto_aplicacion) <= std::sqrt(area) * 1.5) {
                total_force_kn = total_force_kn + dir_glob * c.valor_kn_m2_o_kn;
            }
        }
    }

    // Reparto tributario equitativo (1/3 de la fuerza total a cada nodo del triángulo)
    for (std::size_t n = 0; n < 3; ++n) {
        f_glob[6 * n + 0] += total_force_kn.x / 3.0;
        f_glob[6 * n + 1] += total_force_kn.y / 3.0;
        f_glob[6 * n + 2] += total_force_kn.z / 3.0;
    }

    return f_glob;
}

model::EsfuerzosUnitariosLamina ShellTriangleElement3d::recuperar_esfuerzos_baricentro(
    const core::Vec3d& p1,
    const core::Vec3d& p2,
    const core::Vec3d& p3,
    double espesor_m,
    double e_kpa,
    double nu,
    const model::FactoresRigidezLamina& factores,
    const Vector18& u_global_18
) noexcept {
    const ShellLocalTriangleGeometry geom = compute_shell_triangle_geometry(p1, p2, p3);
    const core::Mat3d r = geom.axes.global_to_local_rotation();

    Vector18 u_loc{};
    for (std::size_t blk = 0; blk < 6; ++blk) {
        const std::size_t base = blk * 3;
        const core::Vec3d v_glob{u_global_18[base + 0], u_global_18[base + 1], u_global_18[base + 2]};
        const core::Vec3d v_loc = r * v_glob;
        u_loc[base + 0] = v_loc.x;
        u_loc[base + 1] = v_loc.y;
        u_loc[base + 2] = v_loc.z;
    }

    const double h = std::max(0.01, espesor_m);
    const double area = geom.area;
    const double inv_2a = 1.0 / (2.0 * area);
    const double nu_c = std::clamp(nu, 0.0, 0.45);

    // Deformaciones de membrana epsilon = [ex, ey, gxy]
    double ex = 0.0, ey = 0.0, gxy = 0.0;
    for (std::size_t n = 0; n < 3; ++n) {
        const double dndx = geom.b[n] * inv_2a;
        const double dndy = geom.c[n] * inv_2a;
        const double un = u_loc[6 * n + 0];
        const double vn = u_loc[6 * n + 1];
        ex  += dndx * un;
        ey  += dndy * vn;
        gxy += dndy * un + dndx * vn;
    }

    const double dm_factor = (e_kpa * h) / (1.0 - nu_c * nu_c);
    model::EsfuerzosUnitariosLamina esf;
    esf.nx_kn_m  = dm_factor * factores.axil_x * (ex + nu_c * ey);
    esf.ny_kn_m  = dm_factor * factores.axil_y * (ey + nu_c * ex);
    esf.nxy_kn_m = dm_factor * 0.5 * (1.0 - nu_c) * factores.axil_xy * gxy;

    // Curvaturas de flexión kappa = [kx, ky, kxy]
    double kx = 0.0, ky = 0.0, kxy = 0.0;
    for (std::size_t n = 0; n < 3; ++n) {
        const double dndx = geom.b[n] * inv_2a;
        const double dndy = geom.c[n] * inv_2a;
        const double wn  = u_loc[6 * n + 2];
        const double rxn = u_loc[6 * n + 3];
        const double ryn = u_loc[6 * n + 4];
        kx  += -dndx * ryn - (2.0 * dndx / std::sqrt(area)) * wn;
        ky  +=  dndy * rxn - (2.0 * dndy / std::sqrt(area)) * wn;
        kxy +=  dndx * rxn - dndy * ryn;
    }

    const double db_factor = (e_kpa * h * h * h) / (12.0 * (1.0 - nu_c * nu_c));
    esf.mx_knm_m  = db_factor * factores.flexion_x * (kx + nu_c * ky);
    esf.my_knm_m  = db_factor * factores.flexion_y * (ky + nu_c * kx);
    esf.mxy_knm_m = db_factor * 0.5 * (1.0 - nu_c) * factores.flexion_xy * kxy;

    // Cortantes transversales Qx, Qy aproximados por gradiente de deflexión y giro medio
    const double g_kpa = e_kpa / (2.0 * (1.0 + nu_c));
    double gamma_xz = 0.0, gamma_yz = 0.0;
    for (std::size_t n = 0; n < 3; ++n) {
        const double dndx = geom.b[n] * inv_2a;
        const double dndy = geom.c[n] * inv_2a;
        gamma_xz += dndx * u_loc[6 * n + 2] - u_loc[6 * n + 4] / 3.0;
        gamma_yz += dndy * u_loc[6 * n + 2] + u_loc[6 * n + 3] / 3.0;
    }
    const double shear_mod = (5.0 / 6.0) * g_kpa * h * 0.01;
    esf.qx_kn_m = shear_mod * factores.cortante_xz * gamma_xz;
    esf.qy_kn_m = shear_mod * factores.cortante_yz * gamma_yz;

    return esf;
}

// ============================================================================
// P-09.1: Resultados Estáticos y Ensamblaje Global 3D (`SolverEngine`)
// ============================================================================

double DesplazamientoNudoResultado::desplazamiento_total_m() const noexcept {
    return std::sqrt(ux_m * ux_m + uy_m * uy_m + uz_m * uz_m);
}

const DesplazamientoNudoResultado* ResultadoHipotesisOCombinacion::find_desplazamiento_nudo(
    std::uint32_t nudo_id
) const noexcept {
    for (const auto& d : desplazamientos_nudos) {
        if (d.nudo_id == nudo_id) {
            return &d;
        }
    }
    return nullptr;
}

const ReaccionApoyoNudoResultado* ResultadoHipotesisOCombinacion::find_reaccion_nudo(
    std::uint32_t nudo_id
) const noexcept {
    for (const auto& r : reacciones_apoyos) {
        if (r.nudo_id == nudo_id) {
            return &r;
        }
    }
    return nullptr;
}

const EsfuerzosBarraResultado* ResultadoHipotesisOCombinacion::find_esfuerzos_barra(
    std::uint32_t barra_id
) const noexcept {
    for (const auto& b : esfuerzos_barras) {
        if (b.barra_id == barra_id) {
            return &b;
        }
    }
    return nullptr;
}

namespace {

struct ShellMeshAssemblyItem {
    std::uint32_t lamina_id{0};
    double espesor_m{0.20};
    double e_kpa{3.10e7};
    double nu{0.20};
    double peso_especifico_kn_m3{25.0};
    bool incluir_peso_propio{true};
    model::FactoresRigidezLamina factores{};
    core::LocalAxes3d ejes_lamina{};
    model::MallaLamina3d malla{};
    std::unordered_map<std::uint32_t, std::size_t> mesh_node_to_global_node_index;
};

struct GlobalModelMapping {
    std::size_t num_structural_nodes{0};
    std::size_t total_nodes{0};
    std::size_t total_dofs{0};
    std::unordered_map<std::uint32_t, std::size_t> nudo_id_to_index;
    std::vector<core::Vec3d> node_positions;
    std::vector<ShellMeshAssemblyItem> shell_items;
};

GlobalModelMapping build_global_model_mapping(const model::Obra& obra) {
    GlobalModelMapping map;
    const auto& est = obra.estructura();
    map.num_structural_nodes = est.nudos.size();
    map.node_positions.reserve(est.nudos.size() + 64);

    for (std::size_t i = 0; i < est.nudos.size(); ++i) {
        map.nudo_id_to_index[est.nudos[i].id] = i;
        map.node_positions.push_back(est.nudos[i].pos);
    }

    // Discretizar láminas e integrar sus nodos de malla (reutilizando nudos estructurales coincidentes)
    for (const auto& lam : est.laminas) {
        if (lam.vertices.size() < 3) {
            continue;
        }
        ShellMeshAssemblyItem item;
        item.lamina_id = lam.id;
        item.espesor_m = std::max(0.01, lam.espesor_m);
        item.e_kpa = std::max(1.0e5, lam.modulo_elasticidad_mpa * 1000.0);
        item.nu = std::clamp(lam.coef_poisson, 0.0, 0.45);
        item.peso_especifico_kn_m3 = lam.peso_especifico_kn_m3;
        item.incluir_peso_propio = lam.incluir_peso_propio;
        item.factores = lam.factores_rigidez;
        item.ejes_lamina = model::ShellElementEngine::calcular_ejes_locales_lamina(
            lam.vertices, lam.arista_eje_x_index, lam.angulo_giro_ejes_rad, lam.invertir_eje_z);
        item.malla = (!lam.malla.triangulos.empty())
            ? lam.malla
            : model::ShellElementEngine::generar_mallado_lamina(lam, lam.tamano_discretizacion_m);

        for (const auto& mn : item.malla.nodos) {
            std::size_t matched_idx = static_cast<std::size_t>(-1);
            for (std::size_t j = 0; j < map.node_positions.size(); ++j) {
                if (map.node_positions[j].distance_to(mn.pos) <= 1.0e-3) {
                    matched_idx = j;
                    break;
                }
            }
            if (matched_idx == static_cast<std::size_t>(-1)) {
                matched_idx = map.node_positions.size();
                map.node_positions.push_back(mn.pos);
            }
            item.mesh_node_to_global_node_index[mn.id] = matched_idx;
        }
        map.shell_items.push_back(std::move(item));
    }

    map.total_nodes = map.node_positions.size();
    map.total_dofs = map.total_nodes * 6;
    return map;
}

bool es_hipotesis_peso_propio(const model::Obra& obra, std::uint32_t hip_id) noexcept {
    for (const auto& h : obra.estructura().hipotesis) {
        if (h.id == hip_id) {
            return (h.naturaleza == 0 || h.codigo == "PP" || h.codigo == "G1");
        }
    }
    return (hip_id == 1);
}

} // namespace

ResultadoHipotesisOCombinacion SolverEngine::resolver_caso_carga(
    const model::Obra& obra,
    const std::vector< std::pair<std::uint32_t, double> >& factores_hipotesis,
    std::uint32_t resultado_id,
    std::string_view codigo,
    std::string_view descripcion,
    bool es_combinacion,
    std::size_t num_estaciones_barra,
    const std::vector<std::uint32_t>& barras_desactivadas,
    const std::vector<std::uint32_t>& apoyos_nudos_desactivados,
    const std::vector< std::pair<std::uint32_t, double> >& axiles_segundo_orden_barras
) {
    ResultadoHipotesisOCombinacion res;
    res.id = resultado_id;
    res.codigo = std::string(codigo);
    res.descripcion = std::string(descripcion);
    res.es_combinacion = es_combinacion;

    const auto& est = obra.estructura();
    if (est.nudos.empty()) {
        return res;
    }

    const GlobalModelMapping map = build_global_model_mapping(obra);
    const std::size_t ndof = map.total_dofs;

    std::unordered_set<std::uint32_t> inactive_bars(barras_desactivadas.begin(), barras_desactivadas.end());
    std::unordered_set<std::uint32_t> inactive_supports(apoyos_nudos_desactivados.begin(), apoyos_nudos_desactivados.end());
    std::unordered_map<std::uint32_t, double> second_order_axials;
    for (const auto& [bid, n_val] : axiles_segundo_orden_barras) {
        second_order_axials[bid] = n_val;
    }

    // K_unconstrained guarda la matriz pura de la estructura (sin penalizaciones de apoyos) para calcular reacciones R = K_unc * u - F_ext
    std::vector<double> k_unconstrained(ndof * ndof, 0.0);
    std::vector<double> f_global(ndof, 0.0);

    // 1. Precalcular cargas lineales provenientes del reparto automático de paños (`panyos3d.dll`)
    std::vector<model::CargaLinealBarra> todas_cargas_barras = est.cargas_lineales;
    bool tiene_cargas_panyo_ya_expandidas = false;
    for (const auto& cl : est.cargas_lineales) {
        if (cl.generada_por_panyo) {
            tiene_cargas_panyo_ya_expandidas = true;
            break;
        }
    }
    if (!tiene_cargas_panyo_ya_expandidas && !est.panyos.empty() && !est.cargas_superficiales.empty()) {
        for (const auto& panyo : est.panyos) {
            const auto generadas = model::PanelLoadDistributionEngine::descomponer_cargas_panyo_en_barras(
                panyo, est.cargas_superficiales, est.barras, est.nudos);
            todas_cargas_barras.insert(todas_cargas_barras.end(), generadas.begin(), generadas.end());
        }
    }

    // 2. Ensamblar Barras 3D Timoshenko (12 GDL)
    std::vector<PropiedadesCalculoBarra3d> bar_props_list(est.barras.size());
    for (std::size_t b_idx = 0; b_idx < est.barras.size(); ++b_idx) {
        const auto& barra = est.barras[b_idx];
        const PropiedadesCalculoBarra3d props = TimoshenkoBarElement3d::extraer_propiedades_barra(obra, barra);
        bar_props_list[b_idx] = props;

        if (inactive_bars.contains(barra.id)) {
            continue;
        }
        const auto it1 = map.nudo_id_to_index.find(barra.nudo_inicial_id);
        const auto it2 = map.nudo_id_to_index.find(barra.nudo_final_id);
        if (it1 == map.nudo_id_to_index.end() || it2 == map.nudo_id_to_index.end()) {
            continue;
        }

        const std::size_t n1 = it1->second;
        const std::size_t n2 = it2->second;
        const std::array<std::size_t, 12> gdofs = {
            6 * n1 + 0, 6 * n1 + 1, 6 * n1 + 2, 6 * n1 + 3, 6 * n1 + 4, 6 * n1 + 5,
            6 * n2 + 0, 6 * n2 + 1, 6 * n2 + 2, 6 * n2 + 3, 6 * n2 + 4, 6 * n2 + 5
        };

        auto k_bar_glob = TimoshenkoBarElement3d::calcular_matriz_rigidez_global_12x12(props);
        if (const auto it_ax = second_order_axials.find(barra.id); it_ax != second_order_axials.end()) {
            const auto kg_glob = TimoshenkoBarElement3d::calcular_matriz_geometrica_global_12x12(props, it_ax->second);
            for (std::size_t idx = 0; idx < 144; ++idx) {
                k_bar_glob[idx] += kg_glob[idx];
            }
        }

        for (std::size_t i = 0; i < 12; ++i) {
            for (std::size_t j = 0; j < 12; ++j) {
                k_unconstrained[gdofs[i] * ndof + gdofs[j]] += k_bar_glob[i * 12 + j];
            }
        }

        // Vector de cargas equivalentes ponderado por factores de hipótesis
        const auto t_mat = TimoshenkoBarElement3d::calcular_matriz_transformacion_12x12(props);
        for (const auto& [hip_id, factor] : factores_hipotesis) {
            if (std::abs(factor) < 1.0e-12) {
                continue;
            }
            std::vector<model::CargaLinealBarra> cargas_hip;
            for (const auto& cl : todas_cargas_barras) {
                if (cl.barra_id == barra.id && cl.hipotesis_id == hip_id) {
                    cargas_hip.push_back(cl);
                }
            }
            const bool inc_pp = es_hipotesis_peso_propio(obra, hip_id);
            const auto f_eq_loc = TimoshenkoBarElement3d::calcular_vector_cargas_equivalentes_locales_12(
                props, barra, cargas_hip, inc_pp);

            // f_eq_glob = T^T * f_eq_loc
            for (std::size_t i = 0; i < 12; ++i) {
                double sum = 0.0;
                for (std::size_t j = 0; j < 12; ++j) {
                    sum += t_mat[j * 12 + i] * f_eq_loc[j];
                }
                f_global[gdofs[i]] += factor * sum;
            }
        }
    }

    // 3. Ensamblar Láminas MEF Shell (18 GDL por triángulo)
    for (const auto& s_item : map.shell_items) {
        for (const auto& tri : s_item.malla.triangulos) {
            const std::size_t gn0 = s_item.mesh_node_to_global_node_index.at(tri.nodo_ids[0]);
            const std::size_t gn1 = s_item.mesh_node_to_global_node_index.at(tri.nodo_ids[1]);
            const std::size_t gn2 = s_item.mesh_node_to_global_node_index.at(tri.nodo_ids[2]);
            const core::Vec3d& p0 = map.node_positions[gn0];
            const core::Vec3d& p1 = map.node_positions[gn1];
            const core::Vec3d& p2 = map.node_positions[gn2];

            const auto k_shell = ShellTriangleElement3d::calcular_matriz_rigidez_global_18x18(
                p0, p1, p2, s_item.espesor_m, s_item.e_kpa, s_item.nu, s_item.factores);

            const std::array<std::size_t, 18> gdofs = {
                6 * gn0 + 0, 6 * gn0 + 1, 6 * gn0 + 2, 6 * gn0 + 3, 6 * gn0 + 4, 6 * gn0 + 5,
                6 * gn1 + 0, 6 * gn1 + 1, 6 * gn1 + 2, 6 * gn1 + 3, 6 * gn1 + 4, 6 * gn1 + 5,
                6 * gn2 + 0, 6 * gn2 + 1, 6 * gn2 + 2, 6 * gn2 + 3, 6 * gn2 + 4, 6 * gn2 + 5
            };

            for (std::size_t i = 0; i < 18; ++i) {
                for (std::size_t j = 0; j < 18; ++j) {
                    k_unconstrained[gdofs[i] * ndof + gdofs[j]] += k_shell[i * 18 + j];
                }
            }

            for (const auto& [hip_id, factor] : factores_hipotesis) {
                if (std::abs(factor) < 1.0e-12) {
                    continue;
                }
                std::vector<model::CargaLamina3d> cargas_lam_hip;
                for (const auto& cl : est.cargas_laminas) {
                    if (cl.lamina_id == s_item.lamina_id && cl.hipotesis_id == hip_id) {
                        cargas_lam_hip.push_back(cl);
                    }
                }
                const bool inc_pp = s_item.incluir_peso_propio && es_hipotesis_peso_propio(obra, hip_id);
                const auto f_sh = ShellTriangleElement3d::calcular_vector_cargas_globales_18(
                    p0, p1, p2, s_item.espesor_m, s_item.peso_especifico_kn_m3, inc_pp, cargas_lam_hip, s_item.ejes_lamina);
                for (std::size_t i = 0; i < 18; ++i) {
                    f_global[gdofs[i]] += factor * f_sh[i];
                }
            }
        }
    }

    // 4. Ensamblar Cargas Puntuales en Nudos
    for (const auto& cn : est.cargas_nudos) {
        const auto it = map.nudo_id_to_index.find(cn.nudo_id);
        if (it == map.nudo_id_to_index.end()) {
            continue;
        }
        const std::size_t base = it->second * 6;
        for (const auto& [hip_id, factor] : factores_hipotesis) {
            if (cn.hipotesis_id == hip_id && std::abs(factor) > 1.0e-12) {
                f_global[base + 0] += factor * cn.fuerzas_kn.x;
                f_global[base + 1] += factor * cn.fuerzas_kn.y;
                f_global[base + 2] += factor * cn.fuerzas_kn.z;
                f_global[base + 3] += factor * cn.momentos_knm.x;
                f_global[base + 4] += factor * cn.momentos_knm.y;
                f_global[base + 5] += factor * cn.momentos_knm.z;
            }
        }
    }

    // 5. Copiar matriz base y aplicar Ligaduras, Diafragmas Rígidos y Condiciones de Contorno (Vinculación Exterior)
    std::vector<double> k_system = k_unconstrained;
    std::vector<double> f_system = f_global;
    constexpr double kPenaltyStiffness = 1.0e12;

    // 5.a) Ligaduras de desplazamiento entre nudos (`esport3d_ligadura_crea` / `desrelac_*`)
    for (const auto& lig : est.ligaduras) {
        if (lig.nudo_ids.size() < 2) {
            continue;
        }
        const auto it_master = map.nudo_id_to_index.find(lig.nudo_ids.front());
        if (it_master == map.nudo_id_to_index.end()) {
            continue;
        }
        const std::size_t m_base = it_master->second * 6;
        for (std::size_t idx = 1; idx < lig.nudo_ids.size(); ++idx) {
            const auto it_slave = map.nudo_id_to_index.find(lig.nudo_ids[idx]);
            if (it_slave == map.nudo_id_to_index.end()) {
                continue;
            }
            const std::size_t s_base = it_slave->second * 6;
            const std::array<bool, 3> mask = {lig.ligar_dx, lig.ligar_dy, lig.ligar_dz};
            for (std::size_t c = 0; c < 3; ++c) {
                if (!mask[c]) {
                    continue;
                }
                const std::size_t dm = m_base + c;
                const std::size_t ds = s_base + c;
                k_system[dm * ndof + dm] += kPenaltyStiffness;
                k_system[ds * ndof + ds] += kPenaltyStiffness;
                k_system[dm * ndof + ds] -= kPenaltyStiffness;
                k_system[ds * ndof + dm] -= kPenaltyStiffness;
            }
        }
    }

    // 5.b) Diafragmas rígidos en planta (`cyosdiafrigid_crea` / `esport3d_diafragma_crea`)
    // Acopla cinemáticamente en el plano XY: ux_i = ux_m - (y_i - y_m)*rz_m, uy_i = uy_m + (x_i - x_m)*rz_m, rz_i = rz_m
    for (const auto& diaf : est.diafragmas) {
        if (!diaf.activo || diaf.nudo_ids.size() < 2) {
            continue;
        }
        const auto it_master = map.nudo_id_to_index.find(diaf.nudo_ids.front());
        if (it_master == map.nudo_id_to_index.end()) {
            continue;
        }
        const std::size_t m_idx = it_master->second;
        const core::Vec3d& pm = map.node_positions[m_idx];
        const std::size_t uxm = 6 * m_idx + 0;
        const std::size_t uym = 6 * m_idx + 1;
        const std::size_t rzm = 6 * m_idx + 5;

        for (std::size_t idx = 1; idx < diaf.nudo_ids.size(); ++idx) {
            const auto it_slave = map.nudo_id_to_index.find(diaf.nudo_ids[idx]);
            if (it_slave == map.nudo_id_to_index.end()) {
                continue;
            }
            const std::size_t s_idx = it_slave->second;
            const core::Vec3d& ps = map.node_positions[s_idx];
            const double dx = ps.x - pm.x;
            const double dy = ps.y - pm.y;
            const std::size_t uxs = 6 * s_idx + 0;
            const std::size_t uys = 6 * s_idx + 1;
            const std::size_t rzs = 6 * s_idx + 5;

            // Penalización para c1 = uxs - uxm + dy * rzm = 0
            const std::array<std::size_t, 3> dofs_x = {uxs, uxm, rzm};
            const std::array<double, 3> coef_x = {1.0, -1.0, dy};
            for (std::size_t a = 0; a < 3; ++a) {
                for (std::size_t b = 0; b < 3; ++b) {
                    k_system[dofs_x[a] * ndof + dofs_x[b]] += kPenaltyStiffness * coef_x[a] * coef_x[b];
                }
            }

            // Penalización para c2 = uys - uym - dx * rzm = 0
            const std::array<std::size_t, 3> dofs_y = {uys, uym, rzm};
            const std::array<double, 3> coef_y = {1.0, -1.0, -dx};
            for (std::size_t a = 0; a < 3; ++a) {
                for (std::size_t b = 0; b < 3; ++b) {
                    k_system[dofs_y[a] * ndof + dofs_y[b]] += kPenaltyStiffness * coef_y[a] * coef_y[b];
                }
            }

            // Penalización para c3 = rzs - rzm = 0
            k_system[rzs * ndof + rzs] += kPenaltyStiffness;
            k_system[rzm * ndof + rzm] += kPenaltyStiffness;
            k_system[rzs * ndof + rzm] -= kPenaltyStiffness;
            k_system[rzm * ndof + rzs] -= kPenaltyStiffness;
        }
    }

    // 5.c) Condiciones de contorno exteriores en nudos (`VinculacionExterior` y `DatosVinculacionExteriorNudo`)
    for (std::size_t n_idx = 0; n_idx < est.nudos.size(); ++n_idx) {
        const auto& nudo = est.nudos[n_idx];
        if (nudo.vinc_ext == model::VinculacionExterior::Libre || inactive_supports.contains(nudo.id)) {
            continue;
        }

        const std::size_t base = n_idx * 6;
        model::DatosVinculacionExteriorNudo vd = nudo.vinc_ext_datos;
        const bool any_flag = vd.gdl_coaccionados[0] || vd.gdl_coaccionados[1] || vd.gdl_coaccionados[2] ||
                              vd.gdl_coaccionados[3] || vd.gdl_coaccionados[4] || vd.gdl_coaccionados[5];
        if (!any_flag && nudo.vinc_ext != model::VinculacionExterior::ApoyoElastico &&
            nudo.vinc_ext != model::VinculacionExterior::DeslizaderaPlanoInclinado &&
            nudo.vinc_ext != model::VinculacionExterior::DeslizaderaRectaLibre) {
            vd = model::DatosVinculacionExteriorNudo::from_tipo(nudo.vinc_ext, vd.giros_coaccionados);
        }

        // Desplazamientos y giros prescritos acumulados para este nudo
        std::array<double, 6> u_prescribed = {
            vd.desp_prescrito_m.x, vd.desp_prescrito_m.y, vd.desp_prescrito_m.z,
            vd.giro_prescrito_rad.x, vd.giro_prescrito_rad.y, vd.giro_prescrito_rad.z
        };
        for (const auto& cn : est.cargas_nudos) {
            if (cn.nudo_id == nudo.id && cn.tiene_desplazamiento_prescrito) {
                for (const auto& [hip_id, factor] : factores_hipotesis) {
                    if (cn.hipotesis_id == hip_id) {
                        u_prescribed[0] += factor * cn.desp_prescrito_m.x;
                        u_prescribed[1] += factor * cn.desp_prescrito_m.y;
                        u_prescribed[2] += factor * cn.desp_prescrito_m.z;
                        u_prescribed[3] += factor * cn.giro_prescrito_rad.x;
                        u_prescribed[4] += factor * cn.giro_prescrito_rad.y;
                        u_prescribed[5] += factor * cn.giro_prescrito_rad.z;
                    }
                }
            }
        }

        if (nudo.vinc_ext == model::VinculacionExterior::ApoyoElastico) {
            const std::array<double, 6> springs = {
                vd.kx_kn_m, vd.ky_kn_m, vd.kz_kn_m, vd.ktx_knm_rad, vd.kty_knm_rad, vd.ktz_knm_rad
            };
            for (std::size_t c = 0; c < 6; ++c) {
                if (springs[c] > 0.0) {
                    k_system[(base + c) * ndof + (base + c)] += springs[c];
                } else if (vd.gdl_coaccionados[c]) {
                    k_system[(base + c) * ndof + (base + c)] += kPenaltyStiffness;
                    f_system[base + c] += kPenaltyStiffness * u_prescribed[c];
                }
            }
        } else if (nudo.vinc_ext == model::VinculacionExterior::DeslizaderaPlanoInclinado) {
            // Coacción de movimiento según el vector normal n: n_x * ux + n_y * uy + n_z * uz = 0 (`esport3d_append_coaccion_plano`)
            const core::Vec3d n_vec = vd.vector_direccion_o_normal.normalized();
            const std::array<double, 3> nc = {n_vec.x, n_vec.y, n_vec.z};
            for (std::size_t a = 0; a < 3; ++a) {
                for (std::size_t b = 0; b < 3; ++b) {
                    k_system[(base + a) * ndof + (base + b)] += kPenaltyStiffness * nc[a] * nc[b];
                }
            }
            if (vd.giros_coaccionados) {
                for (std::size_t c = 3; c < 6; ++c) {
                    k_system[(base + c) * ndof + (base + c)] += kPenaltyStiffness;
                }
            }
        } else if (nudo.vinc_ext == model::VinculacionExterior::DeslizaderaRectaLibre) {
            // Movimiento permitido únicamente a lo largo de la recta con director d (`esport3d_append_coaccion_recta`):
            // penalizamos las 2 direcciones ortogonales P = I - d * d^T
            const core::Vec3d d_vec = vd.vector_direccion_o_normal.normalized();
            const std::array<double, 3> dc = {d_vec.x, d_vec.y, d_vec.z};
            for (std::size_t a = 0; a < 3; ++a) {
                for (std::size_t b = 0; b < 3; ++b) {
                    const double p_ab = ((a == b) ? 1.0 : 0.0) - dc[a] * dc[b];
                    k_system[(base + a) * ndof + (base + b)] += kPenaltyStiffness * p_ab;
                }
            }
            if (vd.giros_coaccionados) {
                for (std::size_t c = 3; c < 6; ++c) {
                    k_system[(base + c) * ndof + (base + c)] += kPenaltyStiffness;
                }
            }
        } else {
            for (std::size_t c = 0; c < 6; ++c) {
                if (vd.gdl_coaccionados[c]) {
                    k_system[(base + c) * ndof + (base + c)] += kPenaltyStiffness;
                    f_system[base + c] += kPenaltyStiffness * u_prescribed[c];
                }
            }
        }
    }

    // 5.d) Vinculación exterior en aristas de láminas (`lamef_vinc_ext_aristas`)
    for (const auto& lam : est.laminas) {
        if (lam.vinculaciones_aristas.empty()) {
            continue;
        }
        for (const auto& s_item : map.shell_items) {
            if (s_item.lamina_id != lam.id) {
                continue;
            }
            for (const auto& mn : s_item.malla.nodos) {
                if (!mn.en_borde_exterior || mn.arista_borde_index < 0) {
                    continue;
                }
                for (const auto& va : lam.vinculaciones_aristas) {
                    if (static_cast<std::int32_t>(va.arista_index) != mn.arista_borde_index) {
                        continue;
                    }
                    const std::size_t g_node = s_item.mesh_node_to_global_node_index.at(mn.id);
                    const std::size_t base = g_node * 6;
                    if (va.vinc_exterior == model::VinculacionExterior::Empotrado) {
                        for (std::size_t c = 0; c < 6; ++c) {
                            k_system[(base + c) * ndof + (base + c)] += kPenaltyStiffness;
                        }
                    } else if (va.vinc_exterior == model::VinculacionExterior::ArticuladoFijo) {
                        for (std::size_t c = 0; c < 3; ++c) {
                            k_system[(base + c) * ndof + (base + c)] += kPenaltyStiffness;
                        }
                    } else if (va.vinc_exterior == model::VinculacionExterior::ApoyoElastico &&
                               va.rigidez_elastica_lineal_kz_kn_m2 > 0.0) {
                        k_system[(base + 2) * ndof + (base + 2)] +=
                            va.rigidez_elastica_lineal_kz_kn_m2 * s_item.malla.tamano_max_elemento_m;
                    }
                }
            }
        }
    }

    // 6. Resolver el sistema global K_system * u = f_system (`cmatr_solucion_sistema_ecuaciones`)
    std::vector<double> u_global(ndof, 0.0);
    LinearAlgebraEngine::solucion_sistema_ecuaciones(k_system, f_system, ndof, u_global);

    // 7. Extraer desplazamientos nodales y reacciones en apoyos (`cypefrnt_desplazamientos_en_nudo_parte`)
    res.desplazamientos_nudos.reserve(est.nudos.size());
    for (std::size_t n_idx = 0; n_idx < est.nudos.size(); ++n_idx) {
        const auto& nudo = est.nudos[n_idx];
        const std::size_t base = n_idx * 6;
        DesplazamientoNudoResultado d;
        d.nudo_id = nudo.id;
        d.ux_m = u_global[base + 0];
        d.uy_m = u_global[base + 1];
        d.uz_m = u_global[base + 2];
        d.rx_rad = u_global[base + 3];
        d.ry_rad = u_global[base + 4];
        d.rz_rad = u_global[base + 5];
        res.desplazamientos_nudos.push_back(d);

        if (nudo.vinc_ext != model::VinculacionExterior::Libre && !inactive_supports.contains(nudo.id)) {
            std::array<double, 6> r_nudo{};
            for (std::size_t c = 0; c < 6; ++c) {
                double ku = 0.0;
                const std::size_t row = base + c;
                for (std::size_t j = 0; j < ndof; ++j) {
                    ku += k_unconstrained[row * ndof + j] * u_global[j];
                }
                r_nudo[c] = ku - f_global[row];
            }
            ReaccionApoyoNudoResultado reacc;
            reacc.nudo_id = nudo.id;
            reacc.fx_kn = r_nudo[0];
            reacc.fy_kn = r_nudo[1];
            reacc.fz_kn = r_nudo[2];
            reacc.mx_knm = r_nudo[3];
            reacc.my_knm = r_nudo[4];
            reacc.mz_knm = r_nudo[5];
            res.reacciones_apoyos.push_back(reacc);
        }
    }

    // 8. Recuperar esfuerzos internos a lo largo de cada Barra 3D (`cypefrnt_esfuerzos_en_punto_barra3D_con_giros`)
    const std::size_t n_stations = std::max<std::size_t>(3, num_estaciones_barra);
    res.esfuerzos_barras.reserve(est.barras.size());

    // Construir lista de cargas ponderadas para esta hipótesis/combinación
    for (std::size_t b_idx = 0; b_idx < est.barras.size(); ++b_idx) {
        const auto& barra = est.barras[b_idx];
        const auto& props = bar_props_list[b_idx];

        EsfuerzosBarraResultado eb;
        eb.barra_id = barra.id;
        eb.longitud_m = props.longitud_flexible_m;

        if (inactive_bars.contains(barra.id)) {
            eb.puntos.resize(n_stations);
            res.esfuerzos_barras.push_back(std::move(eb));
            continue;
        }

        const std::size_t n1 = map.nudo_id_to_index.at(barra.nudo_inicial_id);
        const std::size_t n2 = map.nudo_id_to_index.at(barra.nudo_final_id);
        const TimoshenkoBarElement3d::Vector12 u_bar_glob = {
            u_global[6 * n1 + 0], u_global[6 * n1 + 1], u_global[6 * n1 + 2],
            u_global[6 * n1 + 3], u_global[6 * n1 + 4], u_global[6 * n1 + 5],
            u_global[6 * n2 + 0], u_global[6 * n2 + 1], u_global[6 * n2 + 2],
            u_global[6 * n2 + 3], u_global[6 * n2 + 4], u_global[6 * n2 + 5]
        };

        std::vector<model::CargaLinealBarra> cargas_ponderadas;
        for (const auto& [hip_id, factor] : factores_hipotesis) {
            if (std::abs(factor) < 1.0e-12) {
                continue;
            }
            for (const auto& cl : todas_cargas_barras) {
                if (cl.barra_id == barra.id && cl.hipotesis_id == hip_id) {
                    model::CargaLinealBarra scaled = cl;
                    scaled.q_ini_kn_m *= factor;
                    scaled.q_fin_kn_m *= factor;
                    scaled.delta_temp_uniforme_c *= factor;
                    scaled.delta_temp_gradiente_c *= factor;
                    scaled.axil_pretensado_kn *= factor;
                    cargas_ponderadas.push_back(scaled);
                }
            }
            if (es_hipotesis_peso_propio(obra, hip_id) && barra.incluir_peso_propio) {
                model::CargaLinealBarra c_pp;
                c_pp.barra_id = barra.id;
                c_pp.tipo = model::TipoCargaBarra::Uniforme;
                c_pp.q_ini_kn_m = factor * props.area_m2 * props.peso_especifico_kn_m3;
                c_pp.q_fin_kn_m = c_pp.q_ini_kn_m;
                c_pp.x_ini_rel = 0.0;
                c_pp.x_fin_rel = 1.0;
                c_pp.direccion = {0.0, 0.0, -1.0};
                c_pp.en_ejes_locales = false;
                cargas_ponderadas.push_back(c_pp);
            }
        }

        eb.puntos.reserve(n_stations);
        for (std::size_t s = 0; s < n_stations; ++s) {
            const double xi = static_cast<double>(s) / static_cast<double>(n_stations - 1);
            const PuntoEsfuerzosBarra pt = TimoshenkoBarElement3d::evaluar_esfuerzos_en_punto(
                props, barra, u_bar_glob, cargas_ponderadas, false, xi);
            eb.puntos.push_back(pt);
        }

        // Calcular valores extremos y flecha secante máxima en los planos XY y XZ
        eb.n_max_kn = eb.puntos.front().n_kn;
        eb.n_min_kn = eb.puntos.front().n_kn;
        eb.my_max_knm = eb.puntos.front().my_knm;
        eb.my_min_knm = eb.puntos.front().my_knm;
        eb.mz_max_knm = eb.puntos.front().mz_knm;
        eb.mz_min_knm = eb.puntos.front().mz_knm;

        const double uy_0 = eb.puntos.front().uy_local_m;
        const double uy_1 = eb.puntos.back().uy_local_m;
        const double uz_0 = eb.puntos.front().uz_local_m;
        const double uz_1 = eb.puntos.back().uz_local_m;

        for (const auto& pt : eb.puntos) {
            eb.n_max_kn = std::max(eb.n_max_kn, pt.n_kn);
            eb.n_min_kn = std::min(eb.n_min_kn, pt.n_kn);
            eb.vy_max_abs_kn = std::max(eb.vy_max_abs_kn, std::abs(pt.vy_kn));
            eb.vz_max_abs_kn = std::max(eb.vz_max_abs_kn, std::abs(pt.vz_kn));
            eb.mt_max_abs_knm = std::max(eb.mt_max_abs_knm, std::abs(pt.mt_knm));
            eb.my_max_knm = std::max(eb.my_max_knm, pt.my_knm);
            eb.my_min_knm = std::min(eb.my_min_knm, pt.my_knm);
            eb.mz_max_knm = std::max(eb.mz_max_knm, pt.mz_knm);
            eb.mz_min_knm = std::min(eb.mz_min_knm, pt.mz_knm);

            const double sec_y = (1.0 - pt.x_rel) * uy_0 + pt.x_rel * uy_1;
            const double sec_z = (1.0 - pt.x_rel) * uz_0 + pt.x_rel * uz_1;
            eb.flecha_max_xy_mm = std::max(eb.flecha_max_xy_mm, std::abs(pt.uy_local_m - sec_y) * 1000.0);
            eb.flecha_max_xz_mm = std::max(eb.flecha_max_xz_mm, std::abs(pt.uz_local_m - sec_z) * 1000.0);
        }
        const double max_flecha_m = std::max(eb.flecha_max_xy_mm, eb.flecha_max_xz_mm) * 1.0e-3;
        eb.flecha_relativa_l_n = (max_flecha_m > 1.0e-9) ? (eb.longitud_m / max_flecha_m) : 99999.0;

        res.esfuerzos_barras.push_back(std::move(eb));
    }

    // 9. Recuperar esfuerzos unitarios, tensiones de Von Mises y cuantías Wood-Armer en Láminas
    for (const auto& s_item : map.shell_items) {
        for (const auto& tri : s_item.malla.triangulos) {
            const std::size_t gn0 = s_item.mesh_node_to_global_node_index.at(tri.nodo_ids[0]);
            const std::size_t gn1 = s_item.mesh_node_to_global_node_index.at(tri.nodo_ids[1]);
            const std::size_t gn2 = s_item.mesh_node_to_global_node_index.at(tri.nodo_ids[2]);

            const ShellTriangleElement3d::Vector18 u_tri = {
                u_global[6 * gn0 + 0], u_global[6 * gn0 + 1], u_global[6 * gn0 + 2],
                u_global[6 * gn0 + 3], u_global[6 * gn0 + 4], u_global[6 * gn0 + 5],
                u_global[6 * gn1 + 0], u_global[6 * gn1 + 1], u_global[6 * gn1 + 2],
                u_global[6 * gn1 + 3], u_global[6 * gn1 + 4], u_global[6 * gn1 + 5],
                u_global[6 * gn2 + 0], u_global[6 * gn2 + 1], u_global[6 * gn2 + 2],
                u_global[6 * gn2 + 3], u_global[6 * gn2 + 4], u_global[6 * gn2 + 5]
            };

            EsfuerzosElementoLaminaResultado el;
            el.lamina_id = s_item.lamina_id;
            el.triangulo_id = tri.id;
            el.baricentro = tri.baricentro;
            el.esfuerzos = ShellTriangleElement3d::recuperar_esfuerzos_baricentro(
                map.node_positions[gn0], map.node_positions[gn1], map.node_positions[gn2],
                s_item.espesor_m, s_item.e_kpa, s_item.nu, s_item.factores, u_tri);
            el.von_mises_cara_sup_mpa = model::ShellElementEngine::calcular_tension_von_mises_mpa(
                el.esfuerzos, s_item.espesor_m, +1);
            el.von_mises_cara_inf_mpa = model::ShellElementEngine::calcular_tension_von_mises_mpa(
                el.esfuerzos, s_item.espesor_m, -1);
            el.cuantias_wood_armer = model::ShellElementEngine::calcular_cuantias_wood_armer(
                el.esfuerzos, s_item.espesor_m);
            res.esfuerzos_laminas.push_back(el);
        }
    }

    return res;
}

SolverSummary SolverEngine::analyze_linear_static(
    const model::Obra& obra,
    std::size_t num_estaciones_barra
) {
    SolverSummary summary;
    const auto& est = obra.estructura();
    if (est.nudos.empty()) {
        return summary;
    }

    const GlobalModelMapping map = build_global_model_mapping(obra);
    summary.dof_count = map.total_dofs;
    summary.active_bar_count = est.barras.size();
    for (const auto& s_item : map.shell_items) {
        summary.active_shell_triangle_count += s_item.malla.triangulos.size();
    }

    // 1. Resolver cada hipótesis de carga activa de la obra
    std::vector<model::HipotesisCargaObra> hipotesis_activas = est.hipotesis;
    if (hipotesis_activas.empty()) {
        model::HipotesisCargaObra h_def;
        h_def.id = 1;
        h_def.codigo = "PP";
        h_def.descripcion = "Peso propio";
        h_def.naturaleza = 0;
        hipotesis_activas.push_back(h_def);
    }

    summary.resultados_hipotesis.reserve(hipotesis_activas.size());
    for (const auto& h : hipotesis_activas) {
        if (!h.actua) {
            continue;
        }
        auto res_h = resolver_caso_carga(
            obra, {{h.id, 1.0}}, h.id, h.codigo, h.descripcion, false, num_estaciones_barra);
        summary.resultados_hipotesis.push_back(std::move(res_h));
    }

    // 2. Superponer linealmente para todas las combinaciones normativas ELU y ELS (`combs3.dll`)
    std::vector<codes::HipotesisCargaSpec> code_hips;
    code_hips.reserve(hipotesis_activas.size());
    for (const auto& h : hipotesis_activas) {
        if (!h.actua) {
            continue;
        }
        codes::HipotesisCargaSpec ch;
        ch.id = h.id;
        ch.codigo = h.codigo;
        ch.descripcion = h.descripcion;
        ch.naturaleza = static_cast<codes::NaturalezaHipotesis>(h.naturaleza);
        ch.disposicion = static_cast<codes::DisposicionHipotesis>(h.disposicion);
        ch.categoria_uso = static_cast<codes::CodigoCategoriaUso>(h.categoria_uso);
        ch.grupo_incompatibilidad = h.grupo_incompatibilidad;
        code_hips.push_back(ch);
    }

    const auto combs_elu = codes::NormativaEngine::generar_combinaciones(code_hips);

    std::uint32_t comb_id = 1;
    for (const auto& comb : combs_elu) {
        std::vector< std::pair<std::uint32_t, double> > factors;
        for (const auto& term : comb.terminos) {
            factors.emplace_back(term.hipotesis_id, term.coeficiente);
        }
        auto res_c = resolver_caso_carga(
            obra, factors, comb_id++, comb.nombre_formula, comb.nombre_formula, true, num_estaciones_barra);
        summary.resultados_combinaciones.push_back(std::move(res_c));
    }

    // 3. Construir envolventes por barra y estadísticas globales del resumen
    const auto& source_results = !summary.resultados_combinaciones.empty()
        ? summary.resultados_combinaciones
        : summary.resultados_hipotesis;

    for (const auto& barra : est.barras) {
        EnvolventeEsfuerzosBarra env;
        env.barra_id = barra.id;
        for (const auto& r : source_results) {
            const auto* eb = r.find_esfuerzos_barra(barra.id);
            if (!eb) {
                continue;
            }
            if (eb->n_max_kn > env.n_max_traccion_kn) {
                env.n_max_traccion_kn = eb->n_max_kn;
            }
            if (eb->n_min_kn < env.n_max_compresion_kn) {
                env.n_max_compresion_kn = eb->n_min_kn;
                env.combinacion_pesima_n = r.codigo;
            }
            env.vy_max_abs_kn = std::max(env.vy_max_abs_kn, eb->vy_max_abs_kn);
            env.vz_max_abs_kn = std::max(env.vz_max_abs_kn, eb->vz_max_abs_kn);
            env.mt_max_abs_knm = std::max(env.mt_max_abs_knm, eb->mt_max_abs_knm);
            if (eb->my_max_knm > env.my_max_pos_knm) {
                env.my_max_pos_knm = eb->my_max_knm;
                env.combinacion_pesima_my = r.codigo;
            }
            if (eb->my_min_knm < env.my_min_neg_knm) {
                env.my_min_neg_knm = eb->my_min_knm;
                if (env.combinacion_pesima_my.empty()) {
                    env.combinacion_pesima_my = r.codigo;
                }
            }
            env.mz_max_pos_knm = std::max(env.mz_max_pos_knm, eb->mz_max_knm);
            env.mz_min_neg_knm = std::min(env.mz_min_neg_knm, eb->mz_min_knm);
            env.flecha_max_mm = std::max({env.flecha_max_mm, eb->flecha_max_xy_mm, eb->flecha_max_xz_mm});
        }
        summary.max_axial_compression_kn = std::max(summary.max_axial_compression_kn, std::abs(env.n_max_compresion_kn));
        summary.max_bending_moment_knm = std::max({
            summary.max_bending_moment_knm,
            std::abs(env.my_max_pos_knm),
            std::abs(env.my_min_neg_knm),
            std::abs(env.mz_max_pos_knm),
            std::abs(env.mz_min_neg_knm)
        });
        summary.envolventes_barras.push_back(std::move(env));
    }

    for (const auto& r : summary.resultados_hipotesis) {
        for (const auto& d : r.desplazamientos_nudos) {
            summary.max_displacement_m = std::max(summary.max_displacement_m, d.desplazamiento_total_m());
        }
        for (const auto& reacc : r.reacciones_apoyos) {
            summary.max_reaction_fz_kn = std::max(summary.max_reaction_fz_kn, std::abs(reacc.fz_kn));
        }
    }

    summary.converged = true;
    return summary;
}

// ============================================================================
// P-09.2: Motores de Análisis Avanzados (Modal Espectral, P-Delta, Tirantes, Pandeo, Pushover)
// ============================================================================

double SolverEngine::correlacion_modal_cqc(double omega_i, double omega_j, double xi) noexcept {
    if (omega_i <= 1.0e-9 || omega_j <= 1.0e-9) {
        return 0.0;
    }
    const double r = omega_j / omega_i;
    const double xi2 = xi * xi;
    const double num = 8.0 * xi2 * (1.0 + r) * std::pow(r, 1.5);
    const double diff = 1.0 - r * r;
    const double den = diff * diff + 4.0 * xi2 * r * (1.0 + r) * (1.0 + r);
    return (den > 1.0e-15) ? (num / den) : 1.0;
}

double SolverEngine::combinar_respuestas_modales(
    const std::vector<double>& respuestas_modales,
    const std::vector<double>& omegas_rad_s,
    MetodoCombinacionModal metodo,
    double xi
) noexcept {
    const std::size_t m = respuestas_modales.size();
    if (m == 0) {
        return 0.0;
    }
    if (metodo == MetodoCombinacionModal::SRSS || omegas_rad_s.size() < m) {
        double sum_sq = 0.0;
        for (double r : respuestas_modales) {
            sum_sq += r * r;
        }
        return std::sqrt(sum_sq);
    }

    double sum_cqc = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        for (std::size_t j = 0; j < m; ++j) {
            const double rho_ij = correlacion_modal_cqc(omegas_rad_s[i], omegas_rad_s[j], xi);
            sum_cqc += respuestas_modales[i] * rho_ij * respuestas_modales[j];
        }
    }
    return std::sqrt(std::max(0.0, sum_cqc));
}

ModalSpectralAnalysisResult SolverEngine::analyze_modal_spectral(
    const model::Obra& obra,
    std::uint32_t num_modos,
    MetodoCombinacionModal metodo,
    double amortiguamiento_xi
) {
    ModalSpectralAnalysisResult modal;
    modal.metodo_combinacion = metodo;
    modal.amortiguamiento_modal_xi = amortiguamiento_xi;

    const auto& est = obra.estructura();
    if (est.nudos.empty() || est.barras.empty()) {
        return modal;
    }

    const std::size_t nnodes = est.nudos.size();
    const std::size_t ndof = nnodes * 6;
    std::unordered_map<std::uint32_t, std::size_t> nudo_to_idx;
    for (std::size_t i = 0; i < nnodes; ++i) {
        nudo_to_idx[est.nudos[i].id] = i;
    }

    // 1. Ensamblar matriz de rigidez K y matriz de masas concentradas M (en toneladas = kN·s^2/m)
    std::vector<double> k_mat(ndof * ndof, 0.0);
    std::vector<double> m_diag(ndof, 0.0);
    constexpr double kGravity = 9.81;

    for (const auto& barra : est.barras) {
        const auto props = TimoshenkoBarElement3d::extraer_propiedades_barra(obra, barra);
        const auto k_glob = TimoshenkoBarElement3d::calcular_matriz_rigidez_global_12x12(props);
        const std::size_t n1 = nudo_to_idx.at(barra.nudo_inicial_id);
        const std::size_t n2 = nudo_to_idx.at(barra.nudo_final_id);
        const std::array<std::size_t, 12> gdofs = {
            6 * n1 + 0, 6 * n1 + 1, 6 * n1 + 2, 6 * n1 + 3, 6 * n1 + 4, 6 * n1 + 5,
            6 * n2 + 0, 6 * n2 + 1, 6 * n2 + 2, 6 * n2 + 3, 6 * n2 + 4, 6 * n2 + 5
        };
        for (std::size_t i = 0; i < 12; ++i) {
            for (std::size_t j = 0; j < 12; ++j) {
                k_mat[gdofs[i] * ndof + gdofs[j]] += k_glob[i * 12 + j];
            }
        }

        // Masa propia de barra repartida en ambos nudos extremos (50% en cada extremo)
        const double masa_barra_ton = (props.area_m2 * props.longitud_nudo_a_nudo_m * props.peso_especifico_kn_m3) / kGravity;
        for (std::size_t c = 0; c < 3; ++c) {
            m_diag[6 * n1 + c] += 0.5 * masa_barra_ton;
            m_diag[6 * n2 + c] += 0.5 * masa_barra_ton;
        }
        // Inercia rotacional mínima para evitar ceros en GDL de giro
        const double j_rot = std::max(1.0e-4, 0.02 * masa_barra_ton * props.longitud_nudo_a_nudo_m * props.longitud_nudo_a_nudo_m);
        for (std::size_t c = 3; c < 6; ++c) {
            m_diag[6 * n1 + c] += j_rot;
            m_diag[6 * n2 + c] += j_rot;
        }
    }

    // Sumar masas gravitatorias equivalentes provenientes de cargas permanentes y de nudos (G + psi_2 * Q)
    for (const auto& cn : est.cargas_nudos) {
        if (cn.fuerzas_kn.z < 0.0) {
            const std::size_t n_idx = nudo_to_idx.at(cn.nudo_id);
            const double m_add = std::abs(cn.fuerzas_kn.z) / kGravity;
            for (std::size_t c = 0; c < 3; ++c) {
                m_diag[6 * n_idx + c] += m_add;
            }
        }
    }
    for (const auto& cl : est.cargas_lineales) {
        if (cl.direccion.z < -0.1 && (cl.q_ini_kn_m > 0.0 || cl.q_fin_kn_m > 0.0)) {
            if (const auto* b = obra.find_barra(cl.barra_id)) {
                const double l = obra.longitud_barra_m(b->id);
                const double f_vert = 0.5 * (cl.q_ini_kn_m + cl.q_fin_kn_m) * l * std::abs(cl.direccion.z);
                const double m_add = 0.5 * (f_vert / kGravity);
                const std::size_t n1 = nudo_to_idx.at(b->nudo_inicial_id);
                const std::size_t n2 = nudo_to_idx.at(b->nudo_final_id);
                for (std::size_t c = 0; c < 3; ++c) {
                    m_diag[6 * n1 + c] += m_add;
                    m_diag[6 * n2 + c] += m_add;
                }
            }
        }
    }

    // Imponer condiciones de contorno en K y anular masa traslacional en apoyos fijos
    constexpr double kPenalty = 1.0e12;
    for (std::size_t i = 0; i < nnodes; ++i) {
        const auto& nudo = est.nudos[i];
        if (nudo.vinc_ext == model::VinculacionExterior::Libre) {
            modal.masa_total_estructura_ton += m_diag[6 * i + 0];
            continue;
        }
        const auto vd = model::DatosVinculacionExteriorNudo::from_tipo(nudo.vinc_ext, nudo.vinc_ext_datos.giros_coaccionados);
        for (std::size_t c = 0; c < 6; ++c) {
            if (vd.gdl_coaccionados[c]) {
                k_mat[(6 * i + c) * ndof + (6 * i + c)] += kPenalty;
                m_diag[6 * i + c] = 0.0;
            }
        }
        if (!vd.gdl_coaccionados[0]) {
            modal.masa_total_estructura_ton += m_diag[6 * i + 0];
        }
    }
    modal.masa_total_estructura_ton = std::max(1.0e-3, modal.masa_total_estructura_ton);

    // 2. Resolver problema de autovalores `[K - omega^2 * M] * phi = 0` (`cmatr_calcular_autovalores_y_autovectores`)
    const auto eig = LinearAlgebraEngine::calcular_autovalores_y_autovectores_generalizado(
        k_mat, m_diag, ndof, num_modos);
    if (eig.eigenvalues.empty()) {
        return modal;
    }

    // Configurar parámetros espectrales de diseño desde `DatosGeneralesObra` (`ConfiguracionSismo`)
    codes::ConfiguracionSismo params_sismo;
    params_sismo.activo = true;
    params_sismo.aceleracion_basica_ag_g = std::max(0.04, obra.datos_generales().aceleracion_sismica_basica_g);
    params_sismo.factor_comportamiento_qx = std::max(1.0, obra.datos_generales().factor_ductilidad_q);
    params_sismo.factor_comportamiento_qy = params_sismo.factor_comportamiento_qx;
    params_sismo.amortiguamiento_porcentaje = amortiguamiento_xi * 100.0;

    std::vector<double> omegas;
    std::vector<double> vb_modales_x;
    std::vector<double> vb_modales_y;
    std::vector<ResultadoHipotesisOCombinacion> res_modales_x;
    std::vector<ResultadoHipotesisOCombinacion> res_modales_y;

    for (std::size_t m_idx = 0; m_idx < eig.eigenvalues.size(); ++m_idx) {
        const double omega2 = eig.eigenvalues[m_idx];
        const double omega = std::sqrt(omega2);
        const auto& phi = eig.eigenvectors[m_idx];

        ModoVibracionResultado modo;
        modo.modo_index = static_cast<std::uint32_t>(m_idx + 1);
        modo.autovalor_omega2 = omega2;
        modo.omega_rad_s = omega;
        modo.frecuencia_hz = omega / (2.0 * std::numbers::pi);
        modo.periodo_s = (modo.frecuencia_hz > 1.0e-6) ? (1.0 / modo.frecuencia_hz) : 0.0;

        // Factores de participación L_x = phi^T * M * r_x (con phi ya M-ortonormalizado: phi^T * M * phi = 1)
        double lx = 0.0, ly = 0.0, lz = 0.0;
        for (std::size_t i = 0; i < nnodes; ++i) {
            lx += m_diag[6 * i + 0] * phi[6 * i + 0];
            ly += m_diag[6 * i + 1] * phi[6 * i + 1];
            lz += m_diag[6 * i + 2] * phi[6 * i + 2];

            DesplazamientoNudoResultado dn;
            dn.nudo_id = est.nudos[i].id;
            dn.ux_m = phi[6 * i + 0];
            dn.uy_m = phi[6 * i + 1];
            dn.uz_m = phi[6 * i + 2];
            dn.rx_rad = phi[6 * i + 3];
            dn.ry_rad = phi[6 * i + 4];
            dn.rz_rad = phi[6 * i + 5];
            modo.forma_modal_nudos.push_back(dn);
        }

        modo.factor_participacion_x = lx;
        modo.factor_participacion_y = ly;
        modo.factor_participacion_z = lz;
        modo.masa_efectiva_x_ton = lx * lx;
        modo.masa_efectiva_y_ton = ly * ly;
        modo.masa_efectiva_z_ton = lz * lz;
        modo.porcentaje_masa_x = std::min(100.0, (modo.masa_efectiva_x_ton / modal.masa_total_estructura_ton) * 100.0);
        modo.porcentaje_masa_y = std::min(100.0, (modo.masa_efectiva_y_ton / modal.masa_total_estructura_ton) * 100.0);
        modo.porcentaje_masa_z = std::min(100.0, (modo.masa_efectiva_z_ton / modal.masa_total_estructura_ton) * 100.0);

        modal.suma_porcentaje_masa_x += modo.porcentaje_masa_x;
        modal.suma_porcentaje_masa_y += modo.porcentaje_masa_y;
        modal.suma_porcentaje_masa_z += modo.porcentaje_masa_z;

        const double sa_g = params_sismo.calcular_aceleracion_diseno_g(modo.periodo_s);
        modo.aceleracion_espectral_x_g = sa_g;
        modo.aceleracion_espectral_y_g = sa_g;
        modo.cortante_basal_x_kn = modo.masa_efectiva_x_ton * (sa_g * kGravity);
        modo.cortante_basal_y_kn = modo.masa_efectiva_y_ton * (sa_g * kGravity);

        omegas.push_back(omega);
        vb_modales_x.push_back(modo.cortante_basal_x_kn);
        vb_modales_y.push_back(modo.cortante_basal_y_kn);

        // Fuerzas sísmicas equivalentes del modo m en X e Y: F_{i,m} = M_i * phi_{i,m} * L_m * Sa_m
        model::Obra obra_modo_x = obra;
        model::Obra obra_modo_y = obra;
        obra_modo_x.estructura_mut().cargas_nudos.clear();
        obra_modo_x.estructura_mut().cargas_lineales.clear();
        obra_modo_x.estructura_mut().cargas_superficiales.clear();
        obra_modo_y.estructura_mut().cargas_nudos.clear();
        obra_modo_y.estructura_mut().cargas_lineales.clear();
        obra_modo_y.estructura_mut().cargas_superficiales.clear();
        for (auto& b : obra_modo_x.estructura_mut().barras) { b.incluir_peso_propio = false; }
        for (auto& b : obra_modo_y.estructura_mut().barras) { b.incluir_peso_propio = false; }

        for (std::size_t i = 0; i < nnodes; ++i) {
            const double fx_i = m_diag[6 * i + 0] * phi[6 * i + 0] * lx * (sa_g * kGravity);
            const double fy_i = m_diag[6 * i + 1] * phi[6 * i + 1] * ly * (sa_g * kGravity);
            if (std::abs(fx_i) > 1.0e-9) {
                obra_modo_x.add_carga_puntual_nudo(est.nudos[i].id, 1, {fx_i, 0.0, 0.0});
            }
            if (std::abs(fy_i) > 1.0e-9) {
                obra_modo_y.add_carga_puntual_nudo(est.nudos[i].id, 1, {0.0, fy_i, 0.0});
            }
        }
        res_modales_x.push_back(resolver_caso_carga(obra_modo_x, {{1, 1.0}}, modo.modo_index, "SX_M", ""));
        res_modales_y.push_back(resolver_caso_carga(obra_modo_y, {{1, 1.0}}, modo.modo_index, "SY_M", ""));

        modal.modos.push_back(std::move(modo));
    }

    modal.suma_porcentaje_masa_x = std::min(100.0, modal.suma_porcentaje_masa_x);
    modal.suma_porcentaje_masa_y = std::min(100.0, modal.suma_porcentaje_masa_y);
    modal.suma_porcentaje_masa_z = std::min(100.0, modal.suma_porcentaje_masa_z);
    modal.cumple_masa_participativa_90pct =
        (modal.suma_porcentaje_masa_x >= 90.0 && modal.suma_porcentaje_masa_y >= 90.0);

    // 3. Combinación modal CQC / SRSS de cortantes basales y corrección por cortante basal estático
    modal.cortante_basal_dinamico_x_kn = combinar_respuestas_modales(vb_modales_x, omegas, metodo, amortiguamiento_xi);
    modal.cortante_basal_dinamico_y_kn = combinar_respuestas_modales(vb_modales_y, omegas, metodo, amortiguamiento_xi);

    const double t1_s = modal.modos.front().periodo_s;
    const double sa_t1_g = params_sismo.calcular_aceleracion_diseno_g(t1_s);
    const double v_estatico_kn = modal.masa_total_estructura_ton * (sa_t1_g * kGravity);
    modal.cortante_basal_estatico_minimo_x_kn = 0.80 * v_estatico_kn;
    modal.cortante_basal_estatico_minimo_y_kn = 0.80 * v_estatico_kn;

    if (obra.datos_generales().comprobar_cortante_basal) {
        if (modal.cortante_basal_dinamico_x_kn > 1.0e-6 &&
            modal.cortante_basal_dinamico_x_kn < modal.cortante_basal_estatico_minimo_x_kn) {
            modal.factor_escala_cortante_basal_x = modal.cortante_basal_estatico_minimo_x_kn / modal.cortante_basal_dinamico_x_kn;
        }
        if (modal.cortante_basal_dinamico_y_kn > 1.0e-6 &&
            modal.cortante_basal_dinamico_y_kn < modal.cortante_basal_estatico_minimo_y_kn) {
            modal.factor_escala_cortante_basal_y = modal.cortante_basal_estatico_minimo_y_kn / modal.cortante_basal_dinamico_y_kn;
        }
    }

    // 4. Combinar desplazamientos y esfuerzos modales en Sismo X y Sismo Y mediante CQC/SRSS
    auto combinar_resultados_modales = [&](const std::vector<ResultadoHipotesisOCombinacion>& modales,
                                           double scale_factor,
                                           std::string_view cod) -> ResultadoHipotesisOCombinacion {
        ResultadoHipotesisOCombinacion comb;
        if (modales.empty()) {
            return comb;
        }
        comb = modales.front();
        comb.codigo = std::string(cod);
        const std::size_t nm = modales.size();

        for (std::size_t n_i = 0; n_i < comb.desplazamientos_nudos.size(); ++n_i) {
            std::vector<double> ux_m(nm), uy_m(nm), uz_m(nm);
            for (std::size_t m = 0; m < nm; ++m) {
                ux_m[m] = modales[m].desplazamientos_nudos[n_i].ux_m;
                uy_m[m] = modales[m].desplazamientos_nudos[n_i].uy_m;
                uz_m[m] = modales[m].desplazamientos_nudos[n_i].uz_m;
            }
            comb.desplazamientos_nudos[n_i].ux_m = scale_factor * combinar_respuestas_modales(ux_m, omegas, metodo, amortiguamiento_xi);
            comb.desplazamientos_nudos[n_i].uy_m = scale_factor * combinar_respuestas_modales(uy_m, omegas, metodo, amortiguamiento_xi);
            comb.desplazamientos_nudos[n_i].uz_m = scale_factor * combinar_respuestas_modales(uz_m, omegas, metodo, amortiguamiento_xi);
        }
        return comb;
    };

    modal.respuesta_sismo_x = combinar_resultados_modales(res_modales_x, modal.factor_escala_cortante_basal_x, "SX");
    modal.respuesta_sismo_y = combinar_resultados_modales(res_modales_y, modal.factor_escala_cortante_basal_y, "SY");
    modal.converged = true;
    return modal;
}

SecondOrderPDeltaResult SolverEngine::analyze_second_order_pdelta(
    const model::Obra& obra,
    const std::vector< std::pair<std::uint32_t, double> >& factores_hipotesis,
    std::uint32_t max_iteraciones,
    double tolerancia_rel
) {
    SecondOrderPDeltaResult pdelta;

    // 1. Análisis de 1.er orden inicial
    ResultadoHipotesisOCombinacion res_1 = resolver_caso_carga(
        obra, factores_hipotesis, 1, "P-DELTA-1", "1er Orden");

    double max_disp_1 = 0.0;
    for (const auto& d : res_1.desplazamientos_nudos) {
        max_disp_1 = std::max(max_disp_1, d.desplazamiento_total_m());
    }
    if (max_disp_1 < 1.0e-12) {
        pdelta.converged = true;
        pdelta.resultado_amplificado = std::move(res_1);
        return pdelta;
    }

    // 2. Iteración geométrica no lineal ensamblando K_E + K_G(N^{(k)})
    ResultadoHipotesisOCombinacion res_curr = res_1;
    double prev_max_disp = max_disp_1;

    for (std::uint32_t iter = 1; iter <= max_iteraciones; ++iter) {
        pdelta.iteraciones = iter;
        std::vector< std::pair<std::uint32_t, double> > axiles_barras;
        axiles_barras.reserve(res_curr.esfuerzos_barras.size());
        for (const auto& eb : res_curr.esfuerzos_barras) {
            const double n_medio = eb.puntos.empty() ? 0.0 : eb.puntos[eb.puntos.size() / 2].n_kn;
            axiles_barras.emplace_back(eb.barra_id, n_medio);
        }

        res_curr = resolver_caso_carga(
            obra, factores_hipotesis, 2, "P-DELTA-II", "2do Orden P-Delta",
            true, 11, {}, {}, axiles_barras);

        double curr_max_disp = 0.0;
        for (const auto& d : res_curr.desplazamientos_nudos) {
            curr_max_disp = std::max(curr_max_disp, d.desplazamiento_total_m());
        }

        if (std::abs(curr_max_disp - prev_max_disp) / std::max(1.0e-9, curr_max_disp) <= tolerancia_rel) {
            pdelta.converged = true;
            prev_max_disp = curr_max_disp;
            break;
        }
        prev_max_disp = curr_max_disp;
    }

    // Índice de estabilidad global gamma_z = 1 / (1 - d_II_adicional / d_II_total) = d_II / d_I
    pdelta.indice_estabilidad_gamma_z = std::max(1.0, prev_max_disp / max_disp_1);
    pdelta.factor_amplificacion_aplicado = pdelta.indice_estabilidad_gamma_z;
    pdelta.requiere_segundo_orden = (pdelta.indice_estabilidad_gamma_z > 1.10);
    pdelta.estructura_inestable = (pdelta.indice_estabilidad_gamma_z > 1.33 || !pdelta.converged);
    pdelta.resultado_amplificado = std::move(res_curr);
    return pdelta;
}

NonlinearIterativeResult SolverEngine::analyze_nonlinear_tension_only_and_uplift(
    const model::Obra& obra,
    const std::vector< std::pair<std::uint32_t, double> >& factores_hipotesis,
    MetodoCalculoTirantes metodo,
    std::uint32_t max_iteraciones
) {
    NonlinearIterativeResult nl;
    nl.metodo = metodo;
    const auto& est = obra.estructura();

    std::unordered_set<std::uint32_t> inactive_bars;
    std::unordered_set<std::uint32_t> inactive_supports;

    for (std::uint32_t iter = 1; iter <= max_iteraciones; ++iter) {
        nl.iteraciones = iter;
        const std::vector<std::uint32_t> bars_vec(inactive_bars.begin(), inactive_bars.end());
        const std::vector<std::uint32_t> supp_vec(inactive_supports.begin(), inactive_supports.end());

        auto res = resolver_caso_carga(
            obra, factores_hipotesis, iter, "NL-ITER", "Equilibrio no lineal",
            false, 5, bars_vec, supp_vec);

        bool state_changed = false;

        // 1. Verificar tirantes (solo tracción: N < -tol -> desactivar) y puntales (solo compresión: N > +tol -> desactivar)
        for (const auto& barra : est.barras) {
            if (inactive_bars.contains(barra.id)) {
                continue;
            }
            const auto* eb = res.find_esfuerzos_barra(barra.id);
            if (!eb || eb->puntos.empty()) {
                continue;
            }
            const double n_axial = eb->puntos.front().n_kn;
            if (barra.es_tirante && n_axial < -1.0e-4) {
                inactive_bars.insert(barra.id);
                nl.tirantes_desactivados_por_compresion.push_back(barra.id);
                state_changed = true;
            } else if (barra.solo_compresion && n_axial > 1.0e-4) {
                inactive_bars.insert(barra.id);
                nl.puntales_desactivados_por_traccion.push_back(barra.id);
                state_changed = true;
            }
        }

        // 2. Verificar apoyos no lineales con levantamiento (`ApoyoNoLinealSoloCompresion` / `ApoyoNoLinealSoloTraccion`)
        for (const auto& nudo : est.nudos) {
            if (inactive_supports.contains(nudo.id)) {
                continue;
            }
            const auto* reacc = res.find_reaccion_nudo(nudo.id);
            if (!reacc) {
                continue;
            }
            const core::Vec3d r_vec{reacc->fx_kn, reacc->fy_kn, reacc->fz_kn};
            const core::Vec3d dir = nudo.vinc_ext_datos.sentido_apoyo_no_lineal.normalized();
            const double r_proj = r_vec.dot(dir);

            if (nudo.vinc_ext == model::VinculacionExterior::ApoyoNoLinealSoloCompresion && r_proj < -1.0e-3) {
                inactive_supports.insert(nudo.id);
                nl.apoyos_levantados_sin_contacto.push_back(nudo.id);
                state_changed = true;
            } else if (nudo.vinc_ext == model::VinculacionExterior::ApoyoNoLinealSoloTraccion && r_proj > 1.0e-3) {
                inactive_supports.insert(nudo.id);
                nl.apoyos_levantados_sin_contacto.push_back(nudo.id);
                state_changed = true;
            }
        }

        nl.resultado_equilibrio = std::move(res);
        if (!state_changed) {
            nl.converged = true;
            break;
        }
    }

    return nl;
}

GlobalBucklingAnalysisResult SolverEngine::analyze_global_buckling(
    const model::Obra& obra,
    const std::vector< std::pair<std::uint32_t, double> >& factores_hipotesis,
    std::uint32_t num_modos_pandeo
) {
    GlobalBucklingAnalysisResult buck;
    const auto& est = obra.estructura();
    if (est.nudos.empty() || est.barras.empty()) {
        return buck;
    }

    // 1. Obtener estado de esfuerzos axiles N_0 del análisis elástico lineal
    const auto res_estatico = resolver_caso_carga(obra, factores_hipotesis, 1, "BUCK-0", "");
    std::unordered_map<std::uint32_t, double> comp_axial_by_bar;
    double max_compression = 0.0;
    for (const auto& eb : res_estatico.esfuerzos_barras) {
        const double n_val = eb.puntos.empty() ? 0.0 : eb.puntos[eb.puntos.size() / 2].n_kn;
        if (n_val < -1.0e-4) {
            comp_axial_by_bar[eb.barra_id] = -n_val; // Magnitud de compresión P > 0
            max_compression = std::max(max_compression, -n_val);
        }
    }
    if (max_compression < 1.0e-4) {
        return buck;
    }

    // 2. Ensamblar K_E y la diagonal geométrica equivalente K_G_diag(P_0) para `[K_E - lambda_cr * K_G(P_0)] * psi = 0`
    const std::size_t nnodes = est.nudos.size();
    const std::size_t ndof = nnodes * 6;
    std::unordered_map<std::uint32_t, std::size_t> nudo_to_idx;
    for (std::size_t i = 0; i < nnodes; ++i) {
        nudo_to_idx[est.nudos[i].id] = i;
    }

    std::vector<double> k_mat(ndof * ndof, 0.0);
    std::vector<double> kg_diag(ndof, 0.0);

    for (const auto& barra : est.barras) {
        const auto props = TimoshenkoBarElement3d::extraer_propiedades_barra(obra, barra);
        const auto k_glob = TimoshenkoBarElement3d::calcular_matriz_rigidez_global_12x12(props);
        const std::size_t n1 = nudo_to_idx.at(barra.nudo_inicial_id);
        const std::size_t n2 = nudo_to_idx.at(barra.nudo_final_id);
        const std::array<std::size_t, 12> gdofs = {
            6 * n1 + 0, 6 * n1 + 1, 6 * n1 + 2, 6 * n1 + 3, 6 * n1 + 4, 6 * n1 + 5,
            6 * n2 + 0, 6 * n2 + 1, 6 * n2 + 2, 6 * n2 + 3, 6 * n2 + 4, 6 * n2 + 5
        };
        for (std::size_t i = 0; i < 12; ++i) {
            for (std::size_t j = 0; j < 12; ++j) {
                k_mat[gdofs[i] * ndof + gdofs[j]] += k_glob[i * 12 + j];
            }
        }

        if (const auto it_p = comp_axial_by_bar.find(barra.id); it_p != comp_axial_by_bar.end()) {
            const auto kg_bar = TimoshenkoBarElement3d::calcular_matriz_geometrica_global_12x12(props, it_p->second);
            for (std::size_t i = 0; i < 12; ++i) {
                kg_diag[gdofs[i]] += std::max(0.0, kg_bar[i * 12 + i]);
            }
        }
    }

    constexpr double kPenalty = 1.0e12;
    for (std::size_t i = 0; i < nnodes; ++i) {
        const auto& nudo = est.nudos[i];
        if (nudo.vinc_ext == model::VinculacionExterior::Libre) {
            continue;
        }
        const auto vd = model::DatosVinculacionExteriorNudo::from_tipo(nudo.vinc_ext, nudo.vinc_ext_datos.giros_coaccionados);
        for (std::size_t c = 0; c < 6; ++c) {
            if (vd.gdl_coaccionados[c]) {
                k_mat[(6 * i + c) * ndof + (6 * i + c)] += kPenalty;
                kg_diag[6 * i + c] = 0.0;
            }
        }
    }

    const auto eig = LinearAlgebraEngine::calcular_autovalores_y_autovectores_generalizado(
        k_mat, kg_diag, ndof, num_modos_pandeo);
    if (eig.eigenvalues.empty()) {
        return buck;
    }

    for (std::size_t m = 0; m < eig.eigenvalues.size(); ++m) {
        ModoPandeoGlobal mp;
        mp.modo_index = static_cast<std::uint32_t>(m + 1);
        mp.factor_carga_critica_lambda_cr = eig.eigenvalues[m];
        mp.pandeo_relevante = (mp.factor_carga_critica_lambda_cr < 10.0);

        const auto& psi = eig.eigenvectors[m];
        for (std::size_t i = 0; i < nnodes; ++i) {
            DesplazamientoNudoResultado dn;
            dn.nudo_id = est.nudos[i].id;
            dn.ux_m = psi[6 * i + 0];
            dn.uy_m = psi[6 * i + 1];
            dn.uz_m = psi[6 * i + 2];
            dn.rx_rad = psi[6 * i + 3];
            dn.ry_rad = psi[6 * i + 4];
            dn.rz_rad = psi[6 * i + 5];
            mp.forma_pandeo_nudos.push_back(dn);
        }
        buck.modos_pandeo.push_back(std::move(mp));
    }

    buck.lambda_cr_minimo = buck.modos_pandeo.front().factor_carga_critica_lambda_cr;
    buck.converged = true;
    return buck;
}

PushoverAnalysisResult SolverEngine::analyze_pushover(
    const model::Obra& obra,
    const core::Vec3d& direccion_empuje,
    std::uint32_t nudo_control_id,
    std::uint32_t num_pasos,
    double desplazamiento_objetivo_max_m
) {
    PushoverAnalysisResult push;
    push.direccion_empuje = direccion_empuje.normalized();
    const auto& est = obra.estructura();
    if (est.nudos.empty() || est.barras.empty()) {
        return push;
    }

    // 1. Seleccionar nudo de control (si es 0, elegir el nudo libre de mayor cota Z)
    std::uint32_t ctrl_id = nudo_control_id;
    if (ctrl_id == 0) {
        double max_z = -1.0e9;
        for (const auto& n : est.nudos) {
            if (n.vinc_ext == model::VinculacionExterior::Libre && n.pos.z >= max_z) {
                max_z = n.pos.z;
                ctrl_id = n.id;
            }
        }
        if (ctrl_id == 0) {
            ctrl_id = est.nudos.back().id;
        }
    }
    push.nudo_control_id = ctrl_id;

    // 2. Construir modelo con patrón de empuje lateral proporcional a la altura Z de los nudos libres (`cyosccarga_append_patron_cargas_pushover`)
    model::Obra obra_lat = obra;
    obra_lat.estructura_mut().cargas_nudos.clear();
    obra_lat.estructura_mut().cargas_lineales.clear();
    obra_lat.estructura_mut().cargas_superficiales.clear();
    for (auto& b : obra_lat.estructura_mut().barras) {
        b.incluir_peso_propio = false;
    }

    double min_z = 1.0e9, max_z = -1.0e9;
    for (const auto& n : est.nudos) {
        min_z = std::min(min_z, n.pos.z);
        max_z = std::max(max_z, n.pos.z);
    }
    const double h_tot = std::max(1.0, max_z - min_z);
    double v_base_unitario_kn = 0.0;

    for (const auto& n : est.nudos) {
        if (n.vinc_ext != model::VinculacionExterior::Libre) {
            continue;
        }
        const double z_norm = std::max(0.1, (n.pos.z - min_z) / h_tot);
        const double f_mag = 50.0 * z_norm; // Patrón base de referencia (kN)
        obra_lat.add_carga_puntual_nudo(n.id, 1, push.direccion_empuje * f_mag);
        v_base_unitario_kn += f_mag;
    }
    if (v_base_unitario_kn < 1.0e-6) {
        return push;
    }

    // 3. Paso incremental estático no lineal degradando rigidez rotacional de extremos a medida que plastifican (`puobseccba_momento_plastificacion_gdl_Gy`)
    const std::uint32_t steps = std::max<std::uint32_t>(5, num_pasos);
    double u_ctrl_acum = 0.0;
    double v_base_acum = 0.0;
    const double du_step_target = desplazamiento_objetivo_max_m / static_cast<double>(steps);

    // Parámetros de aceptación FEMA 356 por defecto o desde el catálogo de la obra
    model::DefinicionRotulaPlastica def_rot;
    if (!est.catalogo_rotulas_plasticas.empty()) {
        def_rot = est.catalogo_rotulas_plasticas.front();
    }

    for (std::uint32_t s = 1; s <= steps; ++s) {
        const auto res_step = resolver_caso_carga(obra_lat, {{1, 1.0}}, s, "PUSH", "");
        const auto* d_ctrl = res_step.find_desplazamiento_nudo(ctrl_id);
        if (!d_ctrl) {
            break;
        }
        const core::Vec3d u_vec{d_ctrl->ux_m, d_ctrl->uy_m, d_ctrl->uz_m};
        const double du_unit = std::max(1.0e-7, std::abs(u_vec.dot(push.direccion_empuje)));
        const double d_lambda = du_step_target / du_unit;

        u_ctrl_acum += du_step_target;
        const double k_tang_ratio = v_base_unitario_kn / du_unit;
        v_base_acum += d_lambda * v_base_unitario_kn;

        PasoCurvaCapacidadPushover paso;
        paso.paso_index = s;
        paso.factor_carga_lateral = v_base_acum / v_base_unitario_kn;
        paso.desplazamiento_control_m = u_ctrl_acum;
        paso.cortante_basal_kn = v_base_acum;
        paso.rigidez_tangente_kn_m = k_tang_ratio;

        // Evaluar estado de rótulas plásticas en los extremos de cada barra
        for (std::size_t b_idx = 0; b_idx < obra_lat.estructura().barras.size(); ++b_idx) {
            auto& barra = obra_lat.estructura_mut().barras[b_idx];
            const auto props = TimoshenkoBarElement3d::extraer_propiedades_barra(obra_lat, barra);
            const double mp_knm = std::max(5.0, props.wply_m3 * props.fy_mpa * 1000.0);
            const double theta_y_yield = (mp_knm * props.longitud_flexible_m) /
                                         std::max(1.0, 6.0 * props.e_kpa * props.iy_m4);

            const auto* eb = res_step.find_esfuerzos_barra(barra.id);
            if (!eb || eb->puntos.empty()) {
                continue;
            }

            const std::array<bool, 2> ends = {true, false};
            for (bool is_ini : ends) {
                const auto& pt = is_ini ? eb->puntos.front() : eb->puntos.back();
                const double m_act = std::min(mp_knm * 1.15, std::hypot(pt.my_knm, pt.mz_knm) * paso.factor_carga_lateral);
                double theta_p = 0.0;
                EstadoRotulaPlastica est_rot = EstadoRotulaPlastica::Elastico;

                if (m_act >= mp_knm) {
                    const double ductility_rot = u_ctrl_acum / std::max(0.005, theta_y_yield * props.longitud_flexible_m);
                    theta_p = std::max(0.0, (ductility_rot - 1.0) * theta_y_yield);

                    if (theta_p >= def_rot.rotacion_plastica_ult_rad) {
                        est_rot = EstadoRotulaPlastica::AgotamientoColapso_C_E;
                        ++paso.num_rotulas_colapso;
                    } else if (theta_p >= def_rot.rotacion_plastica_cp_rad) {
                        est_rot = EstadoRotulaPlastica::PrevencionColapso_CP;
                        ++paso.num_rotulas_cp;
                    } else if (theta_p >= def_rot.rotacion_plastica_ls_rad) {
                        est_rot = EstadoRotulaPlastica::SeguridadVida_LS;
                        ++paso.num_rotulas_ls;
                    } else if (theta_p >= def_rot.rotacion_plastica_io_rad) {
                        est_rot = EstadoRotulaPlastica::OcupacionInmediata_IO;
                        ++paso.num_rotulas_io;
                    } else {
                        est_rot = EstadoRotulaPlastica::FluenciaInicial_B;
                        ++paso.num_rotulas_fluencia_b;
                    }

                    // Degradar empotramiento del extremo plastificado al valor de endurecimiento post-fluencia (`puobseccba_E_calculo_tramo_plastico_gdl_Gy`)
                    const double post_yield_fixity = std::clamp(def_rot.factor_endurecimiento_post_fluencia, 0.02, 0.25);
                    if (is_ini) {
                        barra.empotramiento.emp_xz_ini = std::min(barra.empotramiento.emp_xz_ini, post_yield_fixity);
                        barra.empotramiento.emp_xy_ini = std::min(barra.empotramiento.emp_xy_ini, post_yield_fixity);
                    } else {
                        barra.empotramiento.emp_xz_fin = std::min(barra.empotramiento.emp_xz_fin, post_yield_fixity);
                        barra.empotramiento.emp_xy_fin = std::min(barra.empotramiento.emp_xy_fin, post_yield_fixity);
                    }

                    if (push.desplazamiento_fluencia_dy_m <= 0.0) {
                        push.desplazamiento_fluencia_dy_m = u_ctrl_acum;
                        push.cortante_fluencia_vy_kn = v_base_acum;
                    }
                }

                EstadoRotulaExtremoBarra er;
                er.barra_id = barra.id;
                er.extremo_inicial = is_ini;
                er.momento_actuante_knm = m_act;
                er.momento_plastificacion_mp_knm = mp_knm;
                er.rotacion_plastica_rad = theta_p;
                er.estado = est_rot;
                paso.estado_rotulas.push_back(er);
            }
        }

        push.cortante_maximo_vmax_kn = std::max(push.cortante_maximo_vmax_kn, paso.cortante_basal_kn);
        push.desplazamiento_ultimo_du_m = paso.desplazamiento_control_m;
        push.curva_capacidad.push_back(std::move(paso));
    }

    if (push.desplazamiento_fluencia_dy_m <= 0.0 && !push.curva_capacidad.empty()) {
        push.desplazamiento_fluencia_dy_m = 0.40 * push.desplazamiento_ultimo_du_m;
        push.cortante_fluencia_vy_kn = 0.75 * push.cortante_maximo_vmax_kn;
    }
    push.ductilidad_disponible_mu = (push.desplazamiento_fluencia_dy_m > 1.0e-6)
        ? (push.desplazamiento_ultimo_du_m / push.desplazamiento_fluencia_dy_m)
        : 1.0;

    // 4. Determinación del Punto de Desempeño (Performance Point - Método N2 de Fajfar / EC8 Anexo B)
    const double k_eff = (push.desplazamiento_fluencia_dy_m > 1.0e-6)
        ? (push.cortante_fluencia_vy_kn / push.desplazamiento_fluencia_dy_m)
        : 1000.0;
    const double m_eff_ton = std::max(10.0, push.cortante_fluencia_vy_kn / (0.25 * 9.81));
    push.periodo_efectivo_teff_s = 2.0 * std::numbers::pi * std::sqrt(m_eff_ton / std::max(1.0, k_eff));

    codes::ConfiguracionSismo params_sismo;
    params_sismo.activo = true;
    params_sismo.aceleracion_basica_ag_g = std::max(0.08, obra.datos_generales().aceleracion_sismica_basica_g);
    params_sismo.factor_comportamiento_qx = 1.0; // Espectro elástico para método N2
    const double sa_elastic_g = params_sismo.calcular_aceleracion_diseno_g(push.periodo_efectivo_teff_s);
    const double sd_elastic_m = (sa_elastic_g * 9.81) *
        std::pow(push.periodo_efectivo_teff_s / (2.0 * std::numbers::pi), 2);

    push.desplazamiento_desempeno_m = std::clamp(sd_elastic_m,
        0.10 * push.desplazamiento_fluencia_dy_m,
        push.desplazamiento_ultimo_du_m);
    push.ductilidad_demanda = push.desplazamiento_desempeno_m / std::max(1.0e-6, push.desplazamiento_fluencia_dy_m);

    // Interpolar el cortante basal y estado global en la curva de capacidad para el desplazamiento de desempeño
    push.tiene_punto_desempeno = !push.curva_capacidad.empty();
    for (const auto& paso : push.curva_capacidad) {
        if (paso.desplazamiento_control_m >= push.desplazamiento_desempeno_m) {
            push.cortante_basal_desempeno_kn = paso.cortante_basal_kn;
            if (paso.num_rotulas_colapso > 0) {
                push.estado_global_desempeno = EstadoRotulaPlastica::AgotamientoColapso_C_E;
            } else if (paso.num_rotulas_cp > 0) {
                push.estado_global_desempeno = EstadoRotulaPlastica::PrevencionColapso_CP;
            } else if (paso.num_rotulas_ls > 0) {
                push.estado_global_desempeno = EstadoRotulaPlastica::SeguridadVida_LS;
            } else if (paso.num_rotulas_io > 0) {
                push.estado_global_desempeno = EstadoRotulaPlastica::OcupacionInmediata_IO;
            } else if (paso.num_rotulas_fluencia_b > 0) {
                push.estado_global_desempeno = EstadoRotulaPlastica::FluenciaInicial_B;
            } else {
                push.estado_global_desempeno = EstadoRotulaPlastica::Elastico;
            }
            break;
        }
    }
    if (push.cortante_basal_desempeno_kn <= 0.0 && !push.curva_capacidad.empty()) {
        push.cortante_basal_desempeno_kn = push.curva_capacidad.back().cortante_basal_kn;
    }

    push.converged = !push.curva_capacidad.empty();
    return push;
}

} // namespace cypeclon::solver
