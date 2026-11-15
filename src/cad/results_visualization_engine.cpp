#include "cad/results_visualization_engine.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <unordered_map>

namespace cypeclon::cad {

namespace {

std::uint32_t pack_rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    return (static_cast<std::uint32_t>(r) << 16) |
           (static_cast<std::uint32_t>(g) << 8) |
           static_cast<std::uint32_t>(b);
}

std::uint32_t interpolate_jet_color(double t) noexcept {
    t = std::clamp(t, 0.0, 1.0);
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    if (t < 0.25) {
        const double u = t / 0.25;
        r = 0.0;
        g = u;
        b = 1.0;
    } else if (t < 0.50) {
        const double u = (t - 0.25) / 0.25;
        r = 0.0;
        g = 1.0;
        b = 1.0 - u;
    } else if (t < 0.75) {
        const double u = (t - 0.50) / 0.25;
        r = u;
        g = 1.0;
        b = 0.0;
    } else {
        const double u = (t - 0.75) / 0.25;
        r = 1.0;
        g = 1.0 - u;
        b = 0.0;
    }
    return pack_rgb(
        static_cast<std::uint8_t>(std::round(r * 255.0)),
        static_cast<std::uint8_t>(std::round(g * 255.0)),
        static_cast<std::uint8_t>(std::round(b * 255.0))
    );
}

std::string fmt_val(double v, int prec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

const solver::ResultadoHipotesisOCombinacion* find_resultado_caso(
    const solver::SolverSummary& solver_results,
    std::uint32_t id,
    bool es_combinacion
) noexcept {
    if (es_combinacion) {
        for (const auto& c : solver_results.resultados_combinaciones) {
            if (c.id == id) {
                return &c;
            }
        }
        if (!solver_results.resultados_combinaciones.empty()) {
            return &solver_results.resultados_combinaciones.front();
        }
    }
    for (const auto& h : solver_results.resultados_hipotesis) {
        if (h.id == id) {
            return &h;
        }
    }
    if (!solver_results.resultados_hipotesis.empty()) {
        return &solver_results.resultados_hipotesis.front();
    }
    return nullptr;
}

double extraer_magnitud_triangulo_lamina(
    const solver::EsfuerzosElementoLaminaResultado& elem_res,
    MagnitudIsovalorLamina mag,
    const core::Vec3d& desp_baricentro_m
) noexcept {
    switch (mag) {
        case MagnitudIsovalorLamina::Desplazamiento_Ux:
            return desp_baricentro_m.x * 1000.0;
        case MagnitudIsovalorLamina::Desplazamiento_Uy:
            return desp_baricentro_m.y * 1000.0;
        case MagnitudIsovalorLamina::Desplazamiento_Uz:
            return desp_baricentro_m.z * 1000.0;
        case MagnitudIsovalorLamina::Desplazamiento_Total:
            return desp_baricentro_m.norm() * 1000.0;
        case MagnitudIsovalorLamina::Esfuerzo_Nx:
            return elem_res.esfuerzos.nx_kn_m;
        case MagnitudIsovalorLamina::Esfuerzo_Ny:
            return elem_res.esfuerzos.ny_kn_m;
        case MagnitudIsovalorLamina::Esfuerzo_Nxy:
            return elem_res.esfuerzos.nxy_kn_m;
        case MagnitudIsovalorLamina::Momento_Mx:
            return elem_res.esfuerzos.mx_knm_m;
        case MagnitudIsovalorLamina::Momento_My:
            return elem_res.esfuerzos.my_knm_m;
        case MagnitudIsovalorLamina::Momento_Mxy:
            return elem_res.esfuerzos.mxy_knm_m;
        case MagnitudIsovalorLamina::Cortante_Qx:
            return elem_res.esfuerzos.qx_kn_m;
        case MagnitudIsovalorLamina::Cortante_Qy:
            return elem_res.esfuerzos.qy_kn_m;
        case MagnitudIsovalorLamina::Tension_VonMises_CaraSup:
            return elem_res.von_mises_cara_sup_mpa;
        case MagnitudIsovalorLamina::Tension_VonMises_CaraInf:
            return elem_res.von_mises_cara_inf_mpa;
        case MagnitudIsovalorLamina::Tension_VonMises_Max:
            return std::max(elem_res.von_mises_cara_sup_mpa, elem_res.von_mises_cara_inf_mpa);
        case MagnitudIsovalorLamina::Cuantia_WoodArmer_SupX:
            return elem_res.cuantias_wood_armer.asx_sup_cm2_m;
        case MagnitudIsovalorLamina::Cuantia_WoodArmer_SupY:
            return elem_res.cuantias_wood_armer.asy_sup_cm2_m;
        case MagnitudIsovalorLamina::Cuantia_WoodArmer_InfX:
            return elem_res.cuantias_wood_armer.asx_inf_cm2_m;
        case MagnitudIsovalorLamina::Cuantia_WoodArmer_InfY:
            return elem_res.cuantias_wood_armer.asy_inf_cm2_m;
    }
    return 0.0;
}

} // namespace

// ==============================================================================
// 1. LeyendaIsovalores (`isolin.dll`: `mapcolor_*`, `isolins_*`)
// ==============================================================================

std::uint32_t LeyendaIsovalores::evaluar_color_rgb(double valor) const noexcept {
    if (tipo_escala == TipoEscalaMapaColor::ValoresNegativosEnGris && valor < 0.0) {
        return 0x808080; // Gris para valores negativos (`mapcolor_dibujar_triangulo_isovalor_valores_negativos_en_gris`)
    }
    const double span = valor_maximo - valor_minimo;
    if (std::abs(span) < 1e-12) {
        return 0x22C55E;
    }
    double t = std::clamp((valor - valor_minimo) / span, 0.0, 1.0);
    switch (tipo_escala) {
        case TipoEscalaMapaColor::AzulARojo_Jet:
            return interpolate_jet_color(t);
        case TipoEscalaMapaColor::BlancoValorMaximo:
            if (t >= 0.96) {
                return 0xFFFFFF;
            }
            return interpolate_jet_color(t);
        case TipoEscalaMapaColor::RojoValorMinimo:
            return interpolate_jet_color(1.0 - t);
        case TipoEscalaMapaColor::RojoValorMaximoAunqueSeaNegativo: {
            const double max_abs = std::max(std::abs(valor_minimo), std::abs(valor_maximo));
            const double u = (max_abs > 1e-12) ? std::clamp(std::abs(valor) / max_abs, 0.0, 1.0) : 0.0;
            return interpolate_jet_color(u);
        }
        case TipoEscalaMapaColor::ValoresNegativosEnGris: {
            const double u = (valor_maximo > 1e-12) ? std::clamp(valor / valor_maximo, 0.0, 1.0) : 0.0;
            return interpolate_jet_color(u);
        }
    }
    return interpolate_jet_color(t);
}

std::string LeyendaIsovalores::generar_texto_leyenda() const {
    std::ostringstream oss;
    oss << "=== LEYENDA DE ISOVALORES: " << magnitud_titulo << " [" << unidad << "] ===\n";
    oss << "  Rango: [" << fmt_val(valor_minimo, 3) << ", " << fmt_val(valor_maximo, 3) << "] " << unidad << "\n";
    for (const auto& b : bandas) {
        oss << "  - " << b.etiqueta_rango << " -> RGB(#"
            << std::hex << std::uppercase << std::setw(6) << std::setfill('0') << b.color_rgb
            << std::dec << std::setfill(' ') << ")\n";
    }
    return oss.str();
}

LeyendaIsovalores ResultsVisualizationEngine::construir_leyenda_isovalores(
    double valor_min,
    double valor_max,
    std::string_view titulo,
    std::string_view unidad,
    TipoEscalaMapaColor tipo_escala,
    std::size_t num_bandas
) {
    LeyendaIsovalores ley{};
    ley.tipo_escala = tipo_escala;
    ley.valor_minimo = std::min(valor_min, valor_max);
    ley.valor_maximo = std::max(valor_min, valor_max);
    ley.magnitud_titulo = std::string(titulo);
    ley.unidad = std::string(unidad);

    const std::size_t n = std::max<std::size_t>(2, num_bandas);
    const double span = ley.valor_maximo - ley.valor_minimo;
    ley.valores_constantes = (span < 1e-9);
    ley.incremento_isolineas = ley.valores_constantes ? 1.0 : (span / static_cast<double>(n));

    for (std::size_t i = 0; i < n; ++i) {
        const double v0 = ley.valor_minimo + static_cast<double>(i) * ley.incremento_isolineas;
        const double v1 = ley.valor_minimo + static_cast<double>(i + 1) * ley.incremento_isolineas;
        const double vm = 0.5 * (v0 + v1);
        BandaLeyendaIsovalor b{};
        b.valor_inferior = v0;
        b.valor_superior = v1;
        b.valor_medio = vm;
        b.color_rgb = ley.evaluar_color_rgb(vm);
        b.etiqueta_rango = "[" + fmt_val(v0, 2) + " .. " + fmt_val(v1, 2) + "] " + ley.unidad;
        ley.bandas.push_back(b);
        if (i > 0) {
            ley.niveles_isolineas.push_back(v0);
        }
    }
    return ley;
}

// ==============================================================================
// 2. Deformada 3D y Animación (`visesf3d.dll`: `ve3animdef_*`)
// ==============================================================================

EscenaDeformada3d EscenaDeformada3d::generar_fotograma_animacion(double fase_rad) const {
    EscenaDeformada3d frame = *this;
    frame.fase_animacion_rad = fase_rad;
    const double factor_armonico = std::sin(fase_rad);

    for (auto& cb : frame.barras_deformadas) {
        for (auto& pt : cb.puntos) {
            pt.pos_deformada_escalada_m = pt.pos_indeformada_m +
                                          pt.desp_global_m * (factor_escala_desplazamientos * factor_armonico);
            const double val_inst = pt.valor_escalar_mm_o_mrad * std::abs(factor_armonico);
            pt.color_rgb = frame.leyenda.evaluar_color_rgb(val_inst);
        }
    }

    for (auto& tri : frame.triangulos_laminas_deformados) {
        for (std::size_t k = 0; k < 3; ++k) {
            tri.vertices_deformados_escalados_m[k] =
                tri.vertices_indeformados_m[k] +
                tri.desplazamientos_globales_m[k] * (factor_escala_desplazamientos * factor_armonico);
            const double val_inst = tri.valores_escalares_mm[k] * std::abs(factor_armonico);
            tri.colores_vertices_rgb[k] = frame.leyenda.evaluar_color_rgb(val_inst);
        }
    }

    return frame;
}

EscenaDeformada3d ResultsVisualizationEngine::generar_deformada_3d(
    const model::Obra& obra,
    const solver::SolverSummary& solver_results,
    ModoOrigenDeformada origen,
    std::uint32_t caso_o_modo_id,
    ComponenteColorDeformada componente_color,
    double factor_escala,
    std::size_t subdivisiones_por_barra
) {
    EscenaDeformada3d out{};
    out.origen = origen;
    out.caso_o_modo_id = caso_o_modo_id;
    out.componente_color = componente_color;

    const std::vector<solver::DesplazamientoNudoResultado>* desp_nudos = nullptr;
    const solver::ResultadoHipotesisOCombinacion* caso_estatico = nullptr;

    if (origen == ModoOrigenDeformada::HipotesisSimple) {
        caso_estatico = find_resultado_caso(solver_results, caso_o_modo_id, false);
        if (caso_estatico) {
            out.titulo_caso = "Hipótesis " + caso_estatico->codigo + " (" + caso_estatico->descripcion + ")";
            desp_nudos = &caso_estatico->desplazamientos_nudos;
        }
    } else if (origen == ModoOrigenDeformada::Combinacion) {
        caso_estatico = find_resultado_caso(solver_results, caso_o_modo_id, true);
        if (caso_estatico) {
            out.titulo_caso = "Combinación " + caso_estatico->descripcion;
            desp_nudos = &caso_estatico->desplazamientos_nudos;
        }
    } else if (origen == ModoOrigenDeformada::ModoVibracionSismico && solver_results.analisis_modal.has_value()) {
        const auto& mod_res = *solver_results.analisis_modal;
        if (!mod_res.modos.empty()) {
            const std::size_t idx = std::min<std::size_t>(
                (caso_o_modo_id > 0 ? caso_o_modo_id - 1 : 0),
                mod_res.modos.size() - 1
            );
            const auto& m = mod_res.modos[idx];
            out.titulo_caso = "Modo de vibración " + std::to_string(m.modo_index) +
                              " (T = " + fmt_val(m.periodo_s, 3) + " s, f = " + fmt_val(m.frecuencia_hz, 2) + " Hz)";
            desp_nudos = &m.forma_modal_nudos;
        }
    } else if (origen == ModoOrigenDeformada::ModoPandeoGlobal && solver_results.analisis_pandeo_global.has_value()) {
        const auto& pan_res = *solver_results.analisis_pandeo_global;
        if (!pan_res.modos_pandeo.empty()) {
            const std::size_t idx = std::min<std::size_t>(
                (caso_o_modo_id > 0 ? caso_o_modo_id - 1 : 0),
                pan_res.modos_pandeo.size() - 1
            );
            const auto& mp = pan_res.modos_pandeo[idx];
            out.titulo_caso = "Modo de pandeo " + std::to_string(mp.modo_index) +
                              " (lambda_cr = " + fmt_val(mp.factor_carga_critica_lambda_cr, 2) + ")";
            desp_nudos = &mp.forma_pandeo_nudos;
        }
    }

    std::unordered_map<std::uint32_t, solver::DesplazamientoNudoResultado> map_desp;
    double max_u_m = 0.0;
    if (desp_nudos) {
        for (const auto& dn : *desp_nudos) {
            map_desp[dn.nudo_id] = dn;
            const double u_tot = dn.desplazamiento_total_m();
            if (u_tot >= max_u_m) {
                max_u_m = u_tot;
                out.nudo_desplazamiento_maximo_id = dn.nudo_id;
            }
        }
    }
    out.desplazamiento_maximo_mm = max_u_m * 1000.0;

    if (factor_escala > 0.0) {
        out.factor_escala_desplazamientos = factor_escala;
    } else {
        const auto bbox = obra.bounding_box();
        const double diag = std::max(2.0, bbox.min_pt.distance_to(bbox.max_pt));
        out.factor_escala_desplazamientos = (max_u_m > 1e-7) ? std::clamp((0.08 * diag) / max_u_m, 1.0, 2000.0) : 50.0;
    }

    auto eval_scalar = [&](const core::Vec3d& u_m, const solver::DesplazamientoNudoResultado& g_rad) -> double {
        switch (componente_color) {
            case ComponenteColorDeformada::DesplazamientoTotal_U:
                return u_m.norm() * 1000.0;
            case ComponenteColorDeformada::Desplazamiento_Ux:
                return u_m.x * 1000.0;
            case ComponenteColorDeformada::Desplazamiento_Uy:
                return u_m.y * 1000.0;
            case ComponenteColorDeformada::Desplazamiento_Uz:
                return u_m.z * 1000.0;
            case ComponenteColorDeformada::Giro_Rx:
                return g_rad.rx_rad * 1000.0;
            case ComponenteColorDeformada::Giro_Ry:
                return g_rad.ry_rad * 1000.0;
            case ComponenteColorDeformada::Giro_Rz:
                return g_rad.rz_rad * 1000.0;
        }
        return u_m.norm() * 1000.0;
    };

    double min_val = 1e18;
    double max_val = -1e18;
    const std::size_t n_sub = std::max<std::size_t>(2, subdivisiones_por_barra);

    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (!ni || !nf) {
            continue;
        }
        const auto di = map_desp[b.nudo_inicial_id];
        const auto df = map_desp[b.nudo_final_id];
        const core::Vec3d ui_g{di.ux_m, di.uy_m, di.uz_m};
        const core::Vec3d uf_g{df.ux_m, df.uy_m, df.uz_m};

        const auto props = solver::TimoshenkoBarElement3d::extraer_propiedades_barra(obra, b);
        const solver::EsfuerzosBarraResultado* esf_b =
            (caso_estatico != nullptr) ? caso_estatico->find_esfuerzos_barra(b.id) : nullptr;

        CurvaDeformadaBarra3d curva{};
        curva.barra_id = b.id;
        curva.perfil_referencia = b.perfil;
        curva.puntos.reserve(n_sub + 1);

        for (std::size_t k = 0; k <= n_sub; ++k) {
            const double xi = static_cast<double>(k) / static_cast<double>(n_sub);
            PuntoDeformadaBarra3d pt{};
            pt.x_local_m = xi * props.longitud_nudo_a_nudo_m;
            pt.pos_indeformada_m = ni->pos + (nf->pos - ni->pos) * xi;

            // Interpolación elástica local + traslación de extremos
            core::Vec3d u_interp = ui_g * (1.0 - xi) + uf_g * xi;
            if (esf_b && !esf_b->puntos.empty()) {
                const double pos_f = xi * static_cast<double>(esf_b->puntos.size() - 1);
                const std::size_t i0 = std::min<std::size_t>(static_cast<std::size_t>(std::floor(pos_f)), esf_b->puntos.size() - 1);
                const std::size_t i1 = std::min<std::size_t>(i0 + 1, esf_b->puntos.size() - 1);
                const double alpha = pos_f - static_cast<double>(i0);
                const double uy_loc = (1.0 - alpha) * esf_b->puntos[i0].uy_local_m + alpha * esf_b->puntos[i1].uy_local_m;
                const double uz_loc = (1.0 - alpha) * esf_b->puntos[i0].uz_local_m + alpha * esf_b->puntos[i1].uz_local_m;

                const double uy_chord = ((1.0 - xi) * esf_b->puntos.front().uy_local_m + xi * esf_b->puntos.back().uy_local_m);
                const double uz_chord = ((1.0 - xi) * esf_b->puntos.front().uz_local_m + xi * esf_b->puntos.back().uz_local_m);
                const core::Vec3d flecha_rel_local{0.0, uy_loc - uy_chord, uz_loc - uz_chord};
                u_interp = u_interp + (props.ejes_locales.u * flecha_rel_local.x +
                                       props.ejes_locales.v * flecha_rel_local.y +
                                       props.ejes_locales.w * flecha_rel_local.z);
            }

            pt.desp_global_m = u_interp;
            pt.pos_deformada_escalada_m = pt.pos_indeformada_m + pt.desp_global_m * out.factor_escala_desplazamientos;

            solver::DesplazamientoNudoResultado g_interp{};
            g_interp.rx_rad = (1.0 - xi) * di.rx_rad + xi * df.rx_rad;
            g_interp.ry_rad = (1.0 - xi) * di.ry_rad + xi * df.ry_rad;
            g_interp.rz_rad = (1.0 - xi) * di.rz_rad + xi * df.rz_rad;

            pt.valor_escalar_mm_o_mrad = eval_scalar(pt.desp_global_m, g_interp);
            curva.desplazamiento_max_barra_mm = std::max(curva.desplazamiento_max_barra_mm, pt.desp_global_m.norm() * 1000.0);
            min_val = std::min(min_val, pt.valor_escalar_mm_o_mrad);
            max_val = std::max(max_val, pt.valor_escalar_mm_o_mrad);
            curva.puntos.push_back(pt);
        }
        out.barras_deformadas.push_back(std::move(curva));
    }

    // Triángulos deformados de láminas MEF (`ve3lamef_dibuja_mallado_deformado_lamina`)
    for (const auto& lam : obra.estructura().laminas) {
        const auto malla = (!lam.malla.triangulos.empty())
            ? lam.malla
            : model::ShellElementEngine::generar_mallado_lamina(lam, lam.tamano_discretizacion_m);
        std::unordered_map<std::uint32_t, core::Vec3d> map_pos_malla;
        for (const auto& nm : malla.nodos) {
            map_pos_malla[nm.id] = nm.pos;
        }
        for (const auto& tri : malla.triangulos) {
            TrianguloDeformadoLamina3d td{};
            td.lamina_id = lam.id;
            td.triangulo_id = tri.id;
            for (std::size_t k = 0; k < 3; ++k) {
                const core::Vec3d p_v = map_pos_malla[tri.nodo_ids[k]];
                td.vertices_indeformados_m[k] = p_v;

                // Interpolar desplazamiento del nudo estructural más próximo
                core::Vec3d u_v{0.0, 0.0, 0.0};
                double best_d = 1e18;
                for (const auto& n : obra.estructura().nudos) {
                    const double d = n.pos.distance_to(p_v);
                    if (d < best_d) {
                        best_d = d;
                        const auto dn = map_desp[n.id];
                        u_v = {dn.ux_m, dn.uy_m, dn.uz_m};
                    }
                }
                td.desplazamientos_globales_m[k] = u_v;
                td.vertices_deformados_escalados_m[k] = p_v + u_v * out.factor_escala_desplazamientos;
                td.valores_escalares_mm[k] = u_v.norm() * 1000.0;
                min_val = std::min(min_val, td.valores_escalares_mm[k]);
                max_val = std::max(max_val, td.valores_escalares_mm[k]);
            }
            out.triangulos_laminas_deformados.push_back(td);
        }
    }

    if (min_val > max_val) {
        min_val = 0.0;
        max_val = 0.0;
    }
    const std::string unid = (componente_color >= ComponenteColorDeformada::Giro_Rx) ? "mrad" : "mm";
    out.leyenda = construir_leyenda_isovalores(min_val, max_val, "Deformada 3D", unid, TipoEscalaMapaColor::AzulARojo_Jet, 10);

    for (auto& cb : out.barras_deformadas) {
        for (auto& pt : cb.puntos) {
            pt.color_rgb = out.leyenda.evaluar_color_rgb(pt.valor_escalar_mm_o_mrad);
        }
    }
    for (auto& td : out.triangulos_laminas_deformados) {
        for (std::size_t k = 0; k < 3; ++k) {
            td.colores_vertices_rgb[k] = out.leyenda.evaluar_color_rgb(td.valores_escalares_mm[k]);
        }
    }

    return out;
}

// ==============================================================================
// 3. Diagramas de Esfuerzos en Barras, Envolventes y Cursores (`modanres3d.dll`: `mar3dbarra_*`)
// ==============================================================================

std::vector<DiagramaEsfuerzoBarra3d> ResultsVisualizationEngine::generar_diagramas_esfuerzos_barras(
    const model::Obra& obra,
    const solver::SolverSummary& solver_results,
    ComponenteEsfuerzoBarra componente,
    bool usar_envolvente,
    std::uint32_t hipotesis_o_combinacion_id,
    bool es_combinacion,
    double escala_dibujo
) {
    std::vector<DiagramaEsfuerzoBarra3d> out;
    out.reserve(obra.estructura().barras.size());

    const auto* caso = find_resultado_caso(solver_results, hipotesis_o_combinacion_id, es_combinacion);

    auto extraer_comp = [&](const solver::PuntoEsfuerzosBarra& p) -> double {
        switch (componente) {
            case ComponenteEsfuerzoBarra::Axil_N:
                return p.n_kn;
            case ComponenteEsfuerzoBarra::Cortante_Ty:
                return p.vy_kn;
            case ComponenteEsfuerzoBarra::Cortante_Tz:
                return p.vz_kn;
            case ComponenteEsfuerzoBarra::Torsor_Mt:
                return p.mt_knm;
            case ComponenteEsfuerzoBarra::Flector_My:
                return p.my_knm;
            case ComponenteEsfuerzoBarra::Flector_Mz:
                return p.mz_knm;
            case ComponenteEsfuerzoBarra::Flecha_XY:
                return p.uy_local_m * 1000.0;
            case ComponenteEsfuerzoBarra::Flecha_XZ:
                return p.uz_local_m * 1000.0;
        }
        return 0.0;
    };

    double max_abs_global = 0.0;
    for (const auto& c : solver_results.resultados_combinaciones) {
        for (const auto& eb : c.esfuerzos_barras) {
            for (const auto& p : eb.puntos) {
                max_abs_global = std::max(max_abs_global, std::abs(extraer_comp(p)));
            }
        }
    }
    if (max_abs_global < 1e-6 && caso != nullptr) {
        for (const auto& eb : caso->esfuerzos_barras) {
            for (const auto& p : eb.puntos) {
                max_abs_global = std::max(max_abs_global, std::abs(extraer_comp(p)));
            }
        }
    }

    const double scale = (escala_dibujo > 0.0)
        ? escala_dibujo
        : ((max_abs_global > 1e-6) ? (0.80 / max_abs_global) : 0.02);

    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (!ni || !nf) {
            continue;
        }
        const auto props = solver::TimoshenkoBarElement3d::extraer_propiedades_barra(obra, b);
        const core::Vec3d dir_ley = (componente == ComponenteEsfuerzoBarra::Cortante_Ty ||
                                     componente == ComponenteEsfuerzoBarra::Flector_Mz ||
                                     componente == ComponenteEsfuerzoBarra::Flecha_XY)
            ? props.ejes_locales.v
            : props.ejes_locales.w;

        DiagramaEsfuerzoBarra3d diag{};
        diag.barra_id = b.id;
        diag.componente = componente;
        diag.es_envolvente = usar_envolvente;
        diag.longitud_barra_m = props.longitud_nudo_a_nudo_m;
        diag.escala_dibujo_m_por_unidad = scale;
        diag.unidad = (componente == ComponenteEsfuerzoBarra::Torsor_Mt ||
                       componente == ComponenteEsfuerzoBarra::Flector_My ||
                       componente == ComponenteEsfuerzoBarra::Flector_Mz)
            ? "kN·m"
            : ((componente == ComponenteEsfuerzoBarra::Flecha_XY || componente == ComponenteEsfuerzoBarra::Flecha_XZ) ? "mm" : "kN");

        diag.valor_maximo = -1e18;
        diag.valor_minimo = 1e18;

        const auto& lista_casos = (!solver_results.resultados_combinaciones.empty())
            ? solver_results.resultados_combinaciones
            : solver_results.resultados_hipotesis;

        if (usar_envolvente && !lista_casos.empty()) {
            const auto* eb_ref = lista_casos.front().find_esfuerzos_barra(b.id);
            const std::size_t n_pts = (eb_ref && !eb_ref->puntos.empty()) ? eb_ref->puntos.size() : 11;

            for (std::size_t idx = 0; idx < n_pts; ++idx) {
                const double x_rel = (n_pts > 1) ? static_cast<double>(idx) / static_cast<double>(n_pts - 1) : 0.0;
                const double x_m = x_rel * props.longitud_nudo_a_nudo_m;
                const core::Vec3d p_dir = ni->pos + (nf->pos - ni->pos) * x_rel;

                double v_max = -1e18;
                double v_min = 1e18;
                for (const auto& c : lista_casos) {
                    if (const auto* eb = c.find_esfuerzos_barra(b.id)) {
                        if (idx < eb->puntos.size()) {
                            const double v = extraer_comp(eb->puntos[idx]);
                            v_max = std::max(v_max, v);
                            v_min = std::min(v_min, v);
                        }
                    }
                }
                if (v_max < v_min) {
                    v_max = 0.0;
                    v_min = 0.0;
                }

                if (v_max >= diag.valor_maximo) {
                    diag.valor_maximo = v_max;
                    diag.abscisa_maximo_m = x_m;
                }
                if (v_min <= diag.valor_minimo) {
                    diag.valor_minimo = v_min;
                    diag.abscisa_minimo_m = x_m;
                }

                PuntoDiagramaEsfuerzo3d pt_max{};
                pt_max.x_m = x_m;
                pt_max.x_rel = x_rel;
                pt_max.punto_directriz_3d = p_dir;
                pt_max.punto_diagrama_3d = p_dir + dir_ley * (v_max * scale);
                pt_max.valor = v_max;
                pt_max.color_rgb = (v_max >= 0.0) ? 0x22C55E : 0xEF4444;
                diag.curva_principal_o_max.push_back(pt_max);

                PuntoDiagramaEsfuerzo3d pt_min{};
                pt_min.x_m = x_m;
                pt_min.x_rel = x_rel;
                pt_min.punto_directriz_3d = p_dir;
                pt_min.punto_diagrama_3d = p_dir + dir_ley * (v_min * scale);
                pt_min.valor = v_min;
                pt_min.color_rgb = (v_min >= 0.0) ? 0x22C55E : 0xEF4444;
                diag.curva_envolvente_min.push_back(pt_min);
            }
        } else if (caso != nullptr) {
            if (const auto* eb = caso->find_esfuerzos_barra(b.id)) {
                for (const auto& p : eb->puntos) {
                    const double v = extraer_comp(p);
                    const core::Vec3d p_dir = ni->pos + (nf->pos - ni->pos) * p.x_rel;
                    if (v >= diag.valor_maximo) {
                        diag.valor_maximo = v;
                        diag.abscisa_maximo_m = p.x_m;
                    }
                    if (v <= diag.valor_minimo) {
                        diag.valor_minimo = v;
                        diag.abscisa_minimo_m = p.x_m;
                    }

                    PuntoDiagramaEsfuerzo3d pt{};
                    pt.x_m = p.x_m;
                    pt.x_rel = p.x_rel;
                    pt.punto_directriz_3d = p_dir;
                    pt.punto_diagrama_3d = p_dir + dir_ley * (v * scale);
                    pt.valor = v;
                    pt.color_rgb = (v >= 0.0) ? 0x38BDF8 : 0xF97316;
                    diag.curva_principal_o_max.push_back(pt);
                }
            }
        }

        if (diag.valor_maximo < diag.valor_minimo) {
            diag.valor_maximo = 0.0;
            diag.valor_minimo = 0.0;
        }
        out.push_back(std::move(diag));
    }

    return out;
}

LecturaCursorBarra3d ResultsVisualizationEngine::consultar_cursor_barra_en_abscisa(
    const model::Obra& obra,
    const solver::SolverSummary& solver_results,
    std::uint32_t barra_id,
    double abscisa_x_m,
    std::uint32_t hipotesis_o_combinacion_id,
    bool es_combinacion
) {
    LecturaCursorBarra3d cur{};
    cur.barra_id = barra_id;

    const auto* b = obra.find_barra(barra_id);
    if (!b) {
        return cur;
    }
    const auto* ni = obra.find_nudo(b->nudo_inicial_id);
    const auto* nf = obra.find_nudo(b->nudo_final_id);
    if (!ni || !nf) {
        return cur;
    }

    const auto props = solver::TimoshenkoBarElement3d::extraer_propiedades_barra(obra, *b);
    cur.abscisa_x_m = std::clamp(abscisa_x_m, 0.0, props.longitud_nudo_a_nudo_m);
    cur.x_rel = (props.longitud_nudo_a_nudo_m > 1e-9) ? (cur.abscisa_x_m / props.longitud_nudo_a_nudo_m) : 0.0;
    cur.coordenada_3d_m = ni->pos + (nf->pos - ni->pos) * cur.x_rel;
    cur.plano_seccion_transversal = core::Plane3d::from_point_and_normal(cur.coordenada_3d_m, props.ejes_locales.u);

    const auto* caso = find_resultado_caso(solver_results, hipotesis_o_combinacion_id, es_combinacion);
    if (!caso) {
        return cur;
    }
    const auto* eb = caso->find_esfuerzos_barra(barra_id);
    if (!eb || eb->puntos.empty()) {
        return cur;
    }

    const double pos_f = cur.x_rel * static_cast<double>(eb->puntos.size() - 1);
    const std::size_t i0 = std::min<std::size_t>(static_cast<std::size_t>(std::floor(pos_f)), eb->puntos.size() - 1);
    const std::size_t i1 = std::min<std::size_t>(i0 + 1, eb->puntos.size() - 1);
    const double a = pos_f - static_cast<double>(i0);
    const auto& p0 = eb->puntos[i0];
    const auto& p1 = eb->puntos[i1];

    cur.n_kn = (1.0 - a) * p0.n_kn + a * p1.n_kn;
    cur.ty_kn = (1.0 - a) * p0.vy_kn + a * p1.vy_kn;
    cur.tz_kn = (1.0 - a) * p0.vz_kn + a * p1.vz_kn;
    cur.mt_knm = (1.0 - a) * p0.mt_knm + a * p1.mt_knm;
    cur.my_knm = (1.0 - a) * p0.my_knm + a * p1.my_knm;
    cur.mz_knm = (1.0 - a) * p0.mz_knm + a * p1.mz_knm;

    const core::Vec3d u_loc_m{
        (1.0 - a) * p0.ux_local_m + a * p1.ux_local_m,
        (1.0 - a) * p0.uy_local_m + a * p1.uy_local_m,
        (1.0 - a) * p0.uz_local_m + a * p1.uz_local_m
    };
    cur.desp_local_mm = u_loc_m * 1000.0;
    cur.desp_global_mm = (props.ejes_locales.u * u_loc_m.x +
                          props.ejes_locales.v * u_loc_m.y +
                          props.ejes_locales.w * u_loc_m.z) * 1000.0;

    return cur;
}

// ==============================================================================
// 4. Mapas de Isovalores e Isolíneas en Láminas MEF (`visesf3d.dll`: `ve3lamef_*`, `isolin.dll`: `isolins_*`)
// ==============================================================================

MapaIsovaloresLamina3d ResultsVisualizationEngine::generar_mapa_isovalores_laminas(
    const model::Obra& obra,
    const solver::SolverSummary& solver_results,
    MagnitudIsovalorLamina magnitud,
    std::uint32_t hipotesis_o_combinacion_id,
    bool es_combinacion,
    bool suavizar_en_nudos,
    TipoEscalaMapaColor tipo_escala,
    std::size_t num_niveles_isolineas
) {
    MapaIsovaloresLamina3d mapa{};
    mapa.magnitud = magnitud;
    mapa.suavizado_en_nudos = suavizar_en_nudos;

    const auto* caso = find_resultado_caso(solver_results, hipotesis_o_combinacion_id, es_combinacion);

    std::unordered_map<std::uint32_t, core::Vec3d> map_desp_nudos;
    if (caso) {
        for (const auto& dn : caso->desplazamientos_nudos) {
            map_desp_nudos[dn.nudo_id] = {dn.ux_m, dn.uy_m, dn.uz_m};
        }
    }

    double v_min_glob = 1e18;
    double v_max_glob = -1e18;

    for (const auto& lam : obra.estructura().laminas) {
        const auto malla = (!lam.malla.triangulos.empty())
            ? lam.malla
            : model::ShellElementEngine::generar_mallado_lamina(lam, lam.tamano_discretizacion_m);

        std::unordered_map<std::uint32_t, core::Vec3d> map_pos;
        for (const auto& nm : malla.nodos) {
            map_pos[nm.id] = nm.pos;
        }

        // Mapa de valores por elemento triangular de esta lámina
        std::unordered_map<std::uint32_t, double> val_por_tri;
        for (const auto& tri : malla.triangulos) {
            solver::EsfuerzosElementoLaminaResultado elem_res{};
            elem_res.lamina_id = lam.id;
            elem_res.triangulo_id = tri.id;
            elem_res.baricentro = tri.baricentro;
            if (caso) {
                for (const auto& er : caso->esfuerzos_laminas) {
                    if (er.lamina_id == lam.id && er.triangulo_id == tri.id) {
                        elem_res = er;
                        break;
                    }
                }
            }
            core::Vec3d desp_bar{0.0, 0.0, 0.0};
            double best_d = 1e18;
            for (const auto& n : obra.estructura().nudos) {
                const double d = n.pos.distance_to(tri.baricentro);
                if (d < best_d) {
                    best_d = d;
                    desp_bar = map_desp_nudos[n.id];
                }
            }
            val_por_tri[tri.id] = extraer_magnitud_triangulo_lamina(elem_res, magnitud, desp_bar);
        }

        // Suavizado nodal promediando los triángulos concurrentes en cada nodo de malla
        std::unordered_map<std::uint32_t, double> suma_nodal;
        std::unordered_map<std::uint32_t, std::uint32_t> cuenta_nodal;
        for (const auto& tri : malla.triangulos) {
            const double vt = val_por_tri[tri.id];
            for (std::uint32_t nid : tri.nodo_ids) {
                suma_nodal[nid] += vt;
                cuenta_nodal[nid] += 1;
            }
        }

        for (const auto& tri : malla.triangulos) {
            TrianguloIsovalorLamina3d ti{};
            ti.lamina_id = lam.id;
            ti.triangulo_id = tri.id;
            ti.nodo_ids = tri.nodo_ids;
            ti.valor_baricentro = val_por_tri[tri.id];

            for (std::size_t k = 0; k < 3; ++k) {
                const std::uint32_t nid = tri.nodo_ids[k];
                ti.vertices_3d[k] = map_pos[nid];
                ti.valores_vertices[k] = suavizar_en_nudos
                    ? (suma_nodal[nid] / static_cast<double>( std::max<std::uint32_t>(1, cuenta_nodal[nid]) ))
                    : ti.valor_baricentro;

                if (ti.valores_vertices[k] <= v_min_glob) {
                    v_min_glob = ti.valores_vertices[k];
                }
                if (ti.valores_vertices[k] >= v_max_glob) {
                    v_max_glob = ti.valores_vertices[k];
                    mapa.punto_maximo_3d = ti.vertices_3d[k];
                }
            }
            mapa.triangulos.push_back(ti);
        }
    }

    if (v_min_glob > v_max_glob) {
        v_min_glob = 0.0;
        v_max_glob = 0.0;
    }
    mapa.valor_minimo_global = v_min_glob;
    mapa.valor_maximo_global = v_max_glob;
    mapa.leyenda = construir_leyenda_isovalores(
        v_min_glob, v_max_glob, "Isovalores en Láminas", "ud", tipo_escala, std::max<std::size_t>(2, num_niveles_isolineas));

    for (auto& ti : mapa.triangulos) {
        for (std::size_t k = 0; k < 3; ++k) {
            ti.colores_vertices_rgb[k] = mapa.leyenda.evaluar_color_rgb(ti.valores_vertices[k]);
        }
    }

    // Extracción de isolíneas sobre los triángulos (`isolins_append_isolineas_triangulo`)
    for (double nivel : mapa.leyenda.niveles_isolineas) {
        for (const auto& ti : mapa.triangulos) {
            std::vector<core::Vec3d> cruces;
            for (std::size_t e = 0; e < 3; ++e) {
                const std::size_t i0 = e;
                const std::size_t i1 = (e + 1) % 3;
                const double v0 = ti.valores_vertices[i0];
                const double v1 = ti.valores_vertices[i1];
                if ((v0 <= nivel && nivel <= v1) || (v1 <= nivel && nivel <= v0)) {
                    const double dv = v1 - v0;
                    if (std::abs(dv) > 1e-9) {
                        const double t = std::clamp((nivel - v0) / dv, 0.0, 1.0);
                        cruces.push_back(ti.vertices_3d[i0] + (ti.vertices_3d[i1] - ti.vertices_3d[i0]) * t);
                    }
                }
            }
            if (cruces.size() >= 2 && cruces[0].distance_to(cruces[1]) > 1e-5) {
                CurvaIsolinea3d iso{};
                iso.lamina_id = ti.lamina_id;
                iso.valor_nivel = nivel;
                iso.color_rgb = mapa.leyenda.evaluar_color_rgb(nivel);
                iso.puntos_3d = {cruces[0], cruces[1]};
                mapa.isolineas.push_back(std::move(iso));
            }
        }
    }

    return mapa;
}

// ==============================================================================
// 5. Reacciones en Apoyos y Coloreado por Aprovechamiento (`mar3dnudreacc_*`, `ve3datos_*`)
// ==============================================================================

std::vector<VectorReaccionApoyo3d> ResultsVisualizationEngine::generar_vectores_reacciones_apoyos(
    const model::Obra& obra,
    const solver::SolverSummary& solver_results,
    std::uint32_t hipotesis_o_combinacion_id,
    bool es_combinacion,
    double longitud_flecha_max_m
) {
    std::vector<VectorReaccionApoyo3d> out;
    const auto* caso = find_resultado_caso(solver_results, hipotesis_o_combinacion_id, es_combinacion);
    if (!caso) {
        return out;
    }

    double max_r_kn = 0.0;
    for (const auto& r : caso->reacciones_apoyos) {
        const core::Vec3d f{r.fx_kn, r.fy_kn, r.fz_kn};
        max_r_kn = std::max(max_r_kn, f.norm());
    }

    for (const auto& r : caso->reacciones_apoyos) {
        const auto* n = obra.find_nudo(r.nudo_id);
        if (!n) {
            continue;
        }
        VectorReaccionApoyo3d vr{};
        vr.nudo_id = r.nudo_id;
        vr.posicion_nudo_3d = n->pos;
        vr.fuerza_reaccion_kn = {r.fx_kn, r.fy_kn, r.fz_kn};
        vr.momento_reaccion_knm = {r.mx_knm, r.my_knm, r.mz_knm};
        vr.resultante_fuerza_kn = vr.fuerza_reaccion_kn.norm();
        vr.resultante_momento_knm = vr.momento_reaccion_knm.norm();

        const double len = (max_r_kn > 1e-6)
            ? (longitud_flecha_max_m * (vr.resultante_fuerza_kn / max_r_kn))
            : 0.0;
        vr.extremo_flecha_fuerza_3d = (vr.resultante_fuerza_kn > 1e-6)
            ? (n->pos + vr.fuerza_reaccion_kn.normalized() * len)
            : n->pos;

        vr.etiqueta_texto = "N" + std::to_string(r.nudo_id) + ": R=(" +
                            fmt_val(r.fx_kn, 1) + ", " + fmt_val(r.fy_kn, 1) + ", " + fmt_val(r.fz_kn, 1) + ") kN";
        out.push_back(std::move(vr));
    }

    return out;
}

std::vector<ColorAprovechamientoBarra3d> ResultsVisualizationEngine::generar_mapa_colores_aprovechamiento_barras(
    const model::Obra& obra,
    const codes::ResumenComprobacionObra& design_summary
) {
    std::vector<ColorAprovechamientoBarra3d> out;
    out.reserve(obra.estructura().barras.size());

    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (!ni || !nf) {
            continue;
        }

        ColorAprovechamientoBarra3d item{};
        item.barra_id = b.id;
        item.perfil_referencia = b.perfil;
        item.p_ini_3d = ni->pos;
        item.p_fin_3d = nf->pos;

        if (const auto* comp = design_summary.find_comprobacion_barra(b.id)) {
            item.aprovechamiento_pct = comp->aprovechamiento_global_max_pct;
            item.cumple = comp->cumple_global;
            if (!comp->cumple_global || item.aprovechamiento_pct > 100.0) {
                item.rango = RangoColorAprovechamiento::Superado_Mayor100Pct;
                item.color_rgb = 0xEF4444; // Rojo
            } else if (item.aprovechamiento_pct > 90.0) {
                item.rango = RangoColorAprovechamiento::Ajustado_90a100Pct;
                item.color_rgb = 0xF97316; // Ámbar / Naranja
            } else if (item.aprovechamiento_pct > 50.0) {
                item.rango = RangoColorAprovechamiento::Moderado_50a90Pct;
                item.color_rgb = 0xEAB308; // Amarillo
            } else {
                item.rango = RangoColorAprovechamiento::Holgado_Menor50Pct;
                item.color_rgb = 0x22C55E; // Verde
            }
            item.etiqueta_barra = b.perfil + " (" + fmt_val(item.aprovechamiento_pct, 1) + "%)";
        } else {
            item.rango = RangoColorAprovechamiento::SinComprobar;
            item.color_rgb = 0x94A3B8; // Gris
            item.etiqueta_barra = b.perfil + " (Sin comprobar)";
        }

        out.push_back(std::move(item));
    }

    return out;
}

} // namespace cypeclon::cad
