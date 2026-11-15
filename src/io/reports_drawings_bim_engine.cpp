#include "io/reports_drawings_bim_engine.hpp"

#include "core/string_utils.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

namespace cypeclon::io {

namespace {

std::string fmt_num(double v, int decimals = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << v;
    return oss.str();
}

bool should_include_chapter(
    const std::vector<CapituloListadoObra>& filter,
    CapituloListadoObra cap) noexcept {
    if (filter.empty()) {
        return true;
    }
    return std::find(filter.begin(), filter.end(), cap) != filter.end();
}

void get_sheet_dimensions_mm(FormatoHojaPlano fmt, double& w_mm, double& h_mm) noexcept {
    switch (fmt) {
        case FormatoHojaPlano::ISO_A4: w_mm = 297.0;  h_mm = 210.0; break;
        case FormatoHojaPlano::ISO_A3: w_mm = 420.0;  h_mm = 297.0; break;
        case FormatoHojaPlano::ISO_A2: w_mm = 594.0;  h_mm = 420.0; break;
        case FormatoHojaPlano::ISO_A0: w_mm = 1189.0; h_mm = 841.0; break;
        case FormatoHojaPlano::ISO_A1:
        default:                       w_mm = 841.0;  h_mm = 594.0; break;
    }
}

std::uint32_t find_or_create_node_3d(
    model::Obra& obra,
    const core::Vec3d& pt,
    double tol_m,
    std::size_t& created_count) {
    for (const auto& n : obra.estructura().nudos) {
        if ((n.pos - pt).norm() <= tol_m) {
            return n.id;
        }
    }
    const auto vinc = ( std::abs(pt.z) <= tol_m )
        ? model::VinculacionExterior::Empotrado
        : model::VinculacionExterior::Libre;
    ++created_count;
    return obra.add_nudo(pt, vinc);
}

} // namespace

// =============================================================================
// 1. P-14.1 (A-032): Medición (`mppzperf.dll`) y Listados (`listados.dll`, `cypedocu.dll`, `csvlib.dll`)
// =============================================================================

ResumenMedicionEstructura ReportsDrawingsBimEngine::compute_structure_takeoff(
    const model::Obra& obra,
    const std::vector<connections::PlacaAnclaje3d>* baseplates) {
    // Equivalente a mppzperf_append_pieza_acero_laminado_*, mppzperf_append_pieza_acero_conformado_*, mppzperf_append_pieza_madera_*
    ResumenMedicionEstructura res;
    static const model::ProfileLibraryManager profile_lib;

    struct Key {
        std::string perfil;
        std::string material;
        bool operator<(const Key& o) const noexcept {
            if (perfil != o.perfil) return perfil < o.perfil;
            return material < o.material;
        }
    };

    std::map<Key, PartidaMedicionPerfil> map_lam;
    std::map<Key, PartidaMedicionPerfil> map_conf;
    std::map<Key, PartidaMedicionPerfil> map_mad;
    std::map<Key, PartidaMedicionPerfil> map_otros;

    for (const auto& b : obra.estructura().barras) {
        const double l_m = std::max(0.05, obra.longitud_barra_m(b.id));
        res.longitud_total_barras_m += l_m;

        const auto* p = profile_lib.find_profile(b.perfil);
        const double area_m2 = (p != nullptr) ? (p->area_cm2 * 1.0e-4) : 0.00285;
        const double perim_m = (p != nullptr)
            ? (2.0 * (p->h_mm + p->b_mm) * 1.0e-3)
            : 0.60;
        model::FamiliaMaterialPerfil fam = model::FamiliaMaterialPerfil::AceroLaminado;
        std::string serie = "IPE";
        if (p != nullptr) {
            fam = p->familia;
            serie = p->serie;
        } else if (b.perfil.rfind("ZF", 0) == 0 || b.perfil.rfind("CF", 0) == 0 ||
                   b.perfil.rfind("UF", 0) == 0 || b.perfil.rfind("OMEGA", 0) == 0) {
            fam = model::FamiliaMaterialPerfil::AceroConformado;
            serie = b.perfil.substr(0, b.perfil.find(' '));
        } else if (b.perfil.rfind("RECT", 0) == 0 || b.perfil.rfind("GL", 0) == 0 ||
                   b.perfil.rfind("CIRC", 0) == 0 || b.material.rfind("C", 0) == 0) {
            fam = model::FamiliaMaterialPerfil::Madera;
            serie = b.perfil.substr(0, b.perfil.find(' '));
        } else if (b.perfil.rfind("ALU", 0) == 0) {
            fam = model::FamiliaMaterialPerfil::Aluminio;
            serie = "ALU";
        } else if (b.perfil.rfind("HA", 0) == 0) {
            fam = model::FamiliaMaterialPerfil::Hormigon;
            serie = "HA";
        }

        double rho_kg_m3 = 7850.0;
        if (fam == model::FamiliaMaterialPerfil::Madera) {
            rho_kg_m3 = 450.0;
        } else if (fam == model::FamiliaMaterialPerfil::Aluminio) {
            rho_kg_m3 = 2700.0;
        } else if (fam == model::FamiliaMaterialPerfil::Hormigon) {
            rho_kg_m3 = 2500.0;
        }

        const double vol_m3 = area_m2 * l_m;
        const double sup_m2 = perim_m * l_m;
        const double peso_kg = vol_m3 * rho_kg_m3;

        res.superficie_total_pintura_m2 += sup_m2;

        std::map<Key, PartidaMedicionPerfil>* target_map = &map_lam;
        if (fam == model::FamiliaMaterialPerfil::AceroConformado) {
            target_map = &map_conf;
            res.peso_total_acero_conformado_kg += peso_kg;
        } else if (fam == model::FamiliaMaterialPerfil::Madera) {
            target_map = &map_mad;
            res.volumen_total_madera_m3 += vol_m3;
        } else if (fam == model::FamiliaMaterialPerfil::AceroLaminado || fam == model::FamiliaMaterialPerfil::AceroArmado) {
            target_map = &map_lam;
            res.peso_total_acero_laminado_kg += peso_kg;
        } else {
            target_map = &map_otros;
            if (fam == model::FamiliaMaterialPerfil::Hormigon) {
                res.volumen_total_hormigon_m3 += vol_m3;
            }
        }

        auto& entry = (*target_map)[{b.perfil, b.material}];
        entry.serie_o_familia = serie;
        entry.nombre_perfil = b.perfil;
        entry.material = b.material;
        entry.num_piezas += 1;
        entry.longitud_total_m += l_m;
        entry.superficie_pintura_m2 += sup_m2;
        entry.volumen_m3 += vol_m3;
        entry.peso_total_kg += peso_kg;
    }

    for (const auto& [k, v] : map_lam)   res.partidas_acero_laminado.push_back(v);
    for (const auto& [k, v] : map_conf)  res.partidas_acero_conformado.push_back(v);
    for (const auto& [k, v] : map_mad)   res.partidas_madera.push_back(v);
    for (const auto& [k, v] : map_otros) res.partidas_hormigon_y_otros.push_back(v);

    if (baseplates != nullptr) {
        for (const auto& bp : *baseplates) {
            const bool soldados = (!bp.pernos.empty() && bp.pernos.front().montaje == connections::TipoMontajePernoPlaca::SoldadoAPlaca);
            if (soldados) {
                ++res.num_placas_pernos_soldados;
            } else {
                ++res.num_placas_pernos_atornillados;
            }
            const double vol_placa_m3 = (bp.ancho_lx_mm * 1.0e-3) * (bp.largo_ly_mm * 1.0e-3) * (bp.espesor_e_mm * 1.0e-3);
            double peso_pernos = 0.0;
            for (const auto& perno : bp.pernos) {
                peso_pernos += perno.peso_unitario_kg;
            }
            res.peso_total_placas_y_pernos_kg += (vol_placa_m3 * 7850.0 + peso_pernos);
        }
    }

    return res;
}

DocumentoListadoObra ReportsDrawingsBimEngine::generate_project_report(
    const model::Obra& obra,
    const solver::SolverSummary* solver_summary,
    const codes::ResumenComprobacionObra* design_summary,
    const std::vector<connections::PlacaAnclaje3d>* baseplates,
    const std::vector<connections::UnionEstructural3d>* connections,
    const foundation::ModeloCompletoCimentacion* foundation_model,
    const std::vector<CapituloListadoObra>& capitulos_incluidos) {
    // Equivalente a listados.dll / plalist.dll / cypedocu.dll
    DocumentoListadoObra doc;
    doc.clave_obra = obra.datos_generales().clave;
    doc.titulo_obra = obra.datos_generales().descripcion.empty()
        ? ("Proyecto Estructural CYPE 3D - " + obra.datos_generales().clave)
        : obra.datos_generales().descripcion;
    doc.medicion_estructura = compute_structure_takeoff(obra, baseplates);

    const auto& dg = obra.datos_generales();

    // Capítulo 1: Datos Generales y Normativa
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::DatosGeneralesYNormativa)) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::DatosGeneralesYNormativa;
        sec.codigo_capitulo = "1";
        sec.titulo = "Datos generales de la obra, Normativa y Materiales";
        sec.parrafos = {
            "Obra: " + doc.titulo_obra + " (" + doc.clave_obra + ".ed3)",
            "Resumen de normas aplicadas: " + dg.normas_resumen,
            "Tensión admisible del terreno (situaciones persistentes): " + fmt_num(dg.tension_terreno_persistente_mpa, 3) + " MPa",
            "Tensión admisible del terreno (situaciones accidentales/sísmicas): " + fmt_num(dg.tension_terreno_accidental_mpa, 3) + " MPa"
        };
        sec.cabeceras_tabla = {"Familia Estructural", "Norma Aplicada", "Material por Defecto"};
        sec.filas_tabla = {
            {"Hormigón Estructural", dg.norma_hormigon, dg.hormigon_estructural + " / " + dg.acero_barras},
            {"Acero Laminado y Armado", dg.norma_acero_laminado, dg.acero_laminado},
            {"Acero Conformado", dg.norma_acero_conformado, dg.acero_conformado},
            {"Madera", dg.norma_madera, dg.madera},
            {"Aluminio Extruido", dg.norma_aluminio, dg.aluminio},
            {"Cimentación", dg.norma_cimentacion, dg.hormigon_cimentacion}
        };
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 2: Nudos y Vinculaciones
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::NudosYVinculaciones)) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::NudosYVinculaciones;
        sec.codigo_capitulo = "2";
        sec.titulo = "Geometría de Nudos y Vinculaciones Exteriores/Interiores";
        sec.cabeceras_tabla = {"Nudo", "X (m)", "Y (m)", "Z (m)", "Vinculación Exterior"};
        for (const auto& n : obra.estructura().nudos) {
            std::string vinc = "Libre";
            if (n.vinc_ext == model::VinculacionExterior::Empotrado) vinc = "Empotramiento";
            else if (n.vinc_ext == model::VinculacionExterior::ArticuladoFijo) vinc = "Articulado fijo";
            else if (n.vinc_ext != model::VinculacionExterior::Libre) vinc = "Coacción / Apoyo elástico";
            sec.filas_tabla.push_back({
                "N" + std::to_string(n.id),
                fmt_num(n.pos.x, 3),
                fmt_num(n.pos.y, 3),
                fmt_num(n.pos.z, 3),
                vinc
            });
        }
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 3: Barras y Perfiles
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::BarrasYPerfiles)) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::BarrasYPerfiles;
        sec.codigo_capitulo = "3";
        sec.titulo = "Descripción de Barras, Perfiles, Materiales y Coeficientes de Pandeo";
        sec.cabeceras_tabla = {"Barra", "Nudo Ini", "Nudo Fin", "Longitud (m)", "Perfil", "Material", "beta_xy", "beta_xz"};
        for (const auto& b : obra.estructura().barras) {
            sec.filas_tabla.push_back({
                "B" + std::to_string(b.id),
                "N" + std::to_string(b.nudo_inicial_id),
                "N" + std::to_string(b.nudo_final_id),
                fmt_num(obra.longitud_barra_m(b.id), 3),
                b.perfil,
                b.material,
                fmt_num(b.pandeo.beta_xy, 2),
                fmt_num(b.pandeo.beta_xz, 2)
            });
        }
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 4: Hipótesis y Combinaciones
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::HipotesisYCombinaciones)) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::HipotesisYCombinaciones;
        sec.codigo_capitulo = "4";
        sec.titulo = "Hipótesis de Carga y Estados Límite";
        sec.cabeceras_tabla = {"ID", "Código", "Descripción", "Naturaleza"};
        for (const auto& h : obra.estructura().hipotesis) {
            sec.filas_tabla.push_back({
                std::to_string(h.id),
                h.codigo,
                h.descripcion,
                std::to_string(h.naturaleza)
            });
        }
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 5: Desplazamientos y Reacciones
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::DesplazamientosYReacciones) &&
        solver_summary != nullptr && !solver_summary->resultados_hipotesis.empty()) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::DesplazamientosYReacciones;
        sec.codigo_capitulo = "5";
        sec.titulo = "Desplazamientos en Nudos y Reacciones en Apoyos";
        sec.cabeceras_tabla = {"Hipótesis", "Nudo", "Rx (kN)", "Ry (kN)", "Rz (kN)", "Mx (kN·m)", "My (kN·m)", "Mz (kN·m)"};
        for (const auto& caso : solver_summary->resultados_hipotesis) {
            for (const auto& r : caso.reacciones_apoyos) {
                sec.filas_tabla.push_back({
                    caso.codigo,
                    "N" + std::to_string(r.nudo_id),
                    fmt_num(r.fx_kn, 2),
                    fmt_num(r.fy_kn, 2),
                    fmt_num(r.fz_kn, 2),
                    fmt_num(r.mx_knm, 2),
                    fmt_num(r.my_knm, 2),
                    fmt_num(r.mz_knm, 2)
                });
            }
        }
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 6: Esfuerzos en Barras
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::EsfuerzosEnBarrasYLaminas) &&
        solver_summary != nullptr && !solver_summary->envolventes_barras.empty()) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::EsfuerzosEnBarrasYLaminas;
        sec.codigo_capitulo = "6";
        sec.titulo = "Envolventes de Esfuerzos en Barras (N, Vy, Vz, Mt, My, Mz)";
        sec.cabeceras_tabla = {"Barra", "N_min (kN)", "N_max (kN)", "Vz_max (kN)", "My_min (kN·m)", "My_max (kN·m)"};
        for (const auto& env : solver_summary->envolventes_barras) {
            sec.filas_tabla.push_back({
                "B" + std::to_string(env.barra_id),
                fmt_num(env.n_max_compresion_kn, 2),
                fmt_num(env.n_max_traccion_kn, 2),
                fmt_num(env.vz_max_abs_kn, 2),
                fmt_num(env.my_min_neg_knm, 2),
                fmt_num(env.my_max_pos_knm, 2)
            });
        }
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 7: Comprobaciones ELU / ELS / Fuego
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::ComprobacionesELU_ELS_Fuego) &&
        design_summary != nullptr && !design_summary->comprobaciones_barras.empty()) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::ComprobacionesELU_ELS_Fuego;
        sec.codigo_capitulo = "7";
        sec.titulo = "Comprobaciones Normativas ELU, ELS (Flecha) y Resistencia al Fuego";
        sec.cabeceras_tabla = {"Barra", "Perfil", "Norma", "ELU (%)", "ELS (%)", "Estado Global"};
        for (const auto& cb : design_summary->comprobaciones_barras) {
            sec.filas_tabla.push_back({
                cb.referencia_barra,
                cb.perfil_referencia,
                cb.norma_aplicada,
                fmt_num(cb.aprovechamiento_elu_max_pct, 1),
                fmt_num(cb.aprovechamiento_els_max_pct, 1),
                cb.cumple_global ? "CUMPLE" : "NO CUMPLE"
            });
        }
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 8: Uniones y Placas de Anclaje
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::UnionesYPlacasAnclaje) &&
        ((baseplates != nullptr && !baseplates->empty()) || (connections != nullptr && !connections->empty()))) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::UnionesYPlacasAnclaje;
        sec.codigo_capitulo = "8";
        sec.titulo = "Comprobación y Despiece de Uniones y Placas de Anclaje";
        sec.cabeceras_tabla = {"Elemento", "Nudo", "Dimensiones / Tipología", "Aprovechamiento (%)", "Estado"};
        if (baseplates != nullptr) {
            for (const auto& bp : *baseplates) {
                sec.filas_tabla.push_back({
                    "Placa PB-" + std::to_string(bp.id),
                    "N" + std::to_string(bp.nudo_id),
                    fmt_num(bp.ancho_lx_mm, 0) + "x" + fmt_num(bp.largo_ly_mm, 0) + "x" + fmt_num(bp.espesor_e_mm, 0) + " mm",
                    fmt_num(bp.ultima_comprobacion.aprovechamiento_max_pct, 1),
                    bp.ultima_comprobacion.cumple_global ? "CUMPLE" : "NO CUMPLE"
                });
            }
        }
        if (connections != nullptr) {
            for (const auto& u : *connections) {
                const std::string desc_tipo = u.plano_detalle.titulo_lamina.empty()
                    ? (u.es_atornillada ? "Unión atornillada" : "Unión soldada")
                    : u.plano_detalle.titulo_lamina;
                sec.filas_tabla.push_back({
                    u.referencia,
                    "N" + std::to_string(u.nudo_id),
                    desc_tipo,
                    fmt_num(u.aprovechamiento_max_pct, 1),
                    u.cumple_global ? "CUMPLE" : "NO CUMPLE"
                });
            }
        }
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 9: Cimentación
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::CimentacionGeotecniaYEstructura) &&
        foundation_model != nullptr &&
        (!foundation_model->zapatas.empty() || !foundation_model->encepados.empty() || !foundation_model->vigas.empty())) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::CimentacionGeotecniaYEstructura;
        sec.codigo_capitulo = "9";
        sec.titulo = "Cimentación: Zapatas, Encepados y Vigas Centradoras / de Atado";
        sec.cabeceras_tabla = {"Referencia", "Tipo", "Geometría (m)", "Aprovechamiento (%)", "Estado"};
        for (std::size_t i = 0; i < foundation_model->zapatas.size(); ++i) {
            const auto& zp = foundation_model->zapatas[i];
            const double aprov = (i < foundation_model->resultados_zapatas.size())
                ? (foundation_model->resultados_zapatas[i].aprovechamiento_maximo * 100.0)
                : 0.0;
            const bool ok = (i < foundation_model->resultados_zapatas.size())
                ? foundation_model->resultados_zapatas[i].cumple_global
                : true;
            sec.filas_tabla.push_back({
                zp.referencia,
                "Zapata",
                fmt_num(zp.ancho_x_m, 2) + "x" + fmt_num(zp.ancho_y_m, 2) + "x" + fmt_num(zp.canto_total_m, 2),
                fmt_num(aprov, 1),
                ok ? "CUMPLE" : "NO CUMPLE"
            });
        }
        for (std::size_t i = 0; i < foundation_model->vigas.size(); ++i) {
            const auto& vg = foundation_model->vigas[i];
            const double aprov = (i < foundation_model->resultados_vigas.size())
                ? (foundation_model->resultados_vigas[i].aprovechamiento_maximo * 100.0)
                : 0.0;
            const bool ok = (i < foundation_model->resultados_vigas.size())
                ? foundation_model->resultados_vigas[i].cumple_global
                : true;
            sec.filas_tabla.push_back({
                vg.referencia,
                vg.seccion_armado.codigo,
                fmt_num(vg.seccion_armado.ancho_b_m, 2) + "x" + fmt_num(vg.seccion_armado.canto_h_m, 2) + " L=" + fmt_num(vg.longitud_eje_a_eje_m, 2),
                fmt_num(aprov, 1),
                ok ? "CUMPLE" : "NO CUMPLE"
            });
        }
        doc.capitulos.push_back(std::move(sec));
    }

    // Capítulo 10: Medición de Perfiles, Superficie de Pintura y Materiales (mppzperf.dll)
    if (should_include_chapter(capitulos_incluidos, CapituloListadoObra::MedicionPerfilesYMateriales)) {
        SeccionCapituloListado sec;
        sec.capitulo = CapituloListadoObra::MedicionPerfilesYMateriales;
        sec.codigo_capitulo = "10";
        sec.titulo = "Medición de Perfiles, Superficie de Pintura y Resumen de Materiales";
        sec.parrafos = {
            "Longitud total de barras: " + fmt_num(doc.medicion_estructura.longitud_total_barras_m, 2) + " m",
            "Superficie total de pintura: " + fmt_num(doc.medicion_estructura.superficie_total_pintura_m2, 2) + " m2",
            "Peso total de acero laminado/armado: " + fmt_num(doc.medicion_estructura.peso_total_acero_laminado_kg, 2) + " kg",
            "Peso total de placas de anclaje y pernos: " + fmt_num(doc.medicion_estructura.peso_total_placas_y_pernos_kg, 2) + " kg"
        };
        sec.cabeceras_tabla = {"Serie", "Perfil", "Material", "Piezas", "Longitud (m)", "Sup. Pintura (m2)", "Peso (kg)"};
        auto append_rows = [&](const std::vector<PartidaMedicionPerfil>& vec) {
            for (const auto& p : vec) {
                sec.filas_tabla.push_back({
                    p.serie_o_familia,
                    p.nombre_perfil,
                    p.material,
                    std::to_string(p.num_piezas),
                    fmt_num(p.longitud_total_m, 2),
                    fmt_num(p.superficie_pintura_m2, 2),
                    fmt_num(p.peso_total_kg, 2)
                });
            }
        };
        append_rows(doc.medicion_estructura.partidas_acero_laminado);
        append_rows(doc.medicion_estructura.partidas_acero_conformado);
        append_rows(doc.medicion_estructura.partidas_madera);
        append_rows(doc.medicion_estructura.partidas_hormigon_y_otros);
        doc.capitulos.push_back(std::move(sec));
    }

    return doc;
}

std::string ReportsDrawingsBimEngine::export_report(
    const DocumentoListadoObra& doc,
    FormatoExportacionListado formato) {
    std::ostringstream oss;

    switch (formato) {
        case FormatoExportacionListado::TXT: {
            oss << "================================================================================\n";
            oss << "LISTADO DE OBRA CYPE 3D (CypeClon C++20) - " << doc.titulo_obra << "\n";
            oss << "Clave: " << doc.clave_obra << " | Fecha: " << doc.fecha_generacion << "\n";
            oss << "================================================================================\n\n";
            for (const auto& cap : doc.capitulos) {
                oss << cap.codigo_capitulo << ". " << cap.titulo << "\n";
                oss << "--------------------------------------------------------------------------------\n";
                for (const auto& p : cap.parrafos) {
                    oss << "  " << p << "\n";
                }
                if (!cap.cabeceras_tabla.empty()) {
                    for (std::size_t i = 0; i < cap.cabeceras_tabla.size(); ++i) {
                        oss << (i ? " | " : "  ") << cap.cabeceras_tabla[i];
                    }
                    oss << "\n";
                    for (const auto& row : cap.filas_tabla) {
                        for (std::size_t i = 0; i < row.size(); ++i) {
                            oss << (i ? " | " : "  ") << row[i];
                        }
                        oss << "\n";
                    }
                }
                oss << "\n";
            }
            break;
        }

        case FormatoExportacionListado::CSV: {
            // Equivalente a csvlib.dll (Exportacion_A_CSV)
            oss << "Capitulo;Titulo;";
            for (const auto& cap : doc.capitulos) {
                if (!cap.cabeceras_tabla.empty()) {
                    for (const auto& h : cap.cabeceras_tabla) {
                        oss << h << ";";
                    }
                    oss << "\n";
                    for (const auto& row : cap.filas_tabla) {
                        oss << cap.codigo_capitulo << ";" << cap.titulo << ";";
                        for (const auto& cell : row) {
                            oss << cell << ";";
                        }
                        oss << "\n";
                    }
                }
            }
            break;
        }

        case FormatoExportacionListado::HTML: {
            oss << "<!DOCTYPE html>\n<html lang=\"es\">\n<head><meta charset=\"UTF-8\"><title>"
                << doc.titulo_obra << "</title>\n"
                << "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;}table{border-collapse:collapse;width:100%;margin:12px 0;}th,td{border:1px solid #cbd5e1;padding:6px 10px;text-align:left;}th{background:#f1f5f9;}</style>\n"
                << "</head>\n<body>\n";
            oss << "<h1>" << doc.titulo_obra << " (" << doc.clave_obra << ")</h1>\n";
            for (const auto& cap : doc.capitulos) {
                oss << "<h2>" << cap.codigo_capitulo << ". " << cap.titulo << "</h2>\n";
                for (const auto& p : cap.parrafos) {
                    oss << "<p>" << p << "</p>\n";
                }
                if (!cap.cabeceras_tabla.empty()) {
                    oss << "<table><thead><tr>";
                    for (const auto& h : cap.cabeceras_tabla) oss << "<th>" << h << "</th>";
                    oss << "</tr></thead><tbody>\n";
                    for (const auto& row : cap.filas_tabla) {
                        oss << "<tr>";
                        for (const auto& c : row) oss << "<td>" << c << "</td>";
                        oss << "</tr>\n";
                    }
                    oss << "</tbody></table>\n";
                }
            }
            oss << "</body>\n</html>\n";
            break;
        }

        case FormatoExportacionListado::RTF: {
            oss << "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Arial;}}\n";
            oss << "\\fs28\\b " << doc.titulo_obra << " (" << doc.clave_obra << ")\\b0\\fs20\\par\\par\n";
            for (const auto& cap : doc.capitulos) {
                oss << "\\fs24\\b " << cap.codigo_capitulo << ". " << cap.titulo << "\\b0\\fs20\\par\n";
                for (const auto& p : cap.parrafos) {
                    oss << p << "\\par\n";
                }
                for (const auto& row : cap.filas_tabla) {
                    for (std::size_t i = 0; i < row.size(); ++i) {
                        oss << (i ? "\\tab " : "") << row[i];
                    }
                    oss << "\\par\n";
                }
                oss << "\\par\n";
            }
            oss << "}\n";
            break;
        }

        case FormatoExportacionListado::DOCX_XML: {
            oss << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
            oss << "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n<w:body>\n";
            oss << "<w:p><w:r><w:t>" << doc.titulo_obra << " (" << doc.clave_obra << ")</w:t></w:r></w:p>\n";
            for (const auto& cap : doc.capitulos) {
                oss << "<w:p><w:r><w:t>" << cap.codigo_capitulo << ". " << cap.titulo << "</w:t></w:r></w:p>\n";
                for (const auto& p : cap.parrafos) {
                    oss << "<w:p><w:r><w:t>" << p << "</w:t></w:r></w:p>\n";
                }
                if (!cap.filas_tabla.empty()) {
                    oss << "<w:tbl>\n";
                    for (const auto& row : cap.filas_tabla) {
                        oss << "<w:tr>";
                        for (const auto& c : row) {
                            oss << "<w:tc><w:p><w:r><w:t>" << c << "</w:t></w:r></w:p></w:tc>";
                        }
                        oss << "</w:tr>\n";
                    }
                    oss << "</w:tbl>\n";
                }
            }
            oss << "</w:body>\n</w:document>\n";
            break;
        }

        case FormatoExportacionListado::PDF:
        default: {
            oss << "%PDF-1.4\n";
            oss << "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n";
            oss << "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n";
            oss << "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R >> endobj\n";
            std::ostringstream stream_txt;
            stream_txt << "BT /F1 12 Tf 40 800 Td (" << doc.titulo_obra << " - " << doc.clave_obra << ") Tj ET\n";
            int y = 780;
            for (const auto& cap : doc.capitulos) {
                stream_txt << "BT /F1 10 Tf 40 " << y << " Td (" << cap.codigo_capitulo << ". " << cap.titulo << ") Tj ET\n";
                y -= 16;
                if (y < 60) break;
            }
            const std::string content = stream_txt.str();
            oss << "4 0 obj << /Length " << content.size() << " >>\nstream\n" << content << "endstream\nendobj\n";
            oss << "xref\n0 5\n0000000000 65535 f \ntrailer << /Size 5 /Root 1 0 R >>\nstartxref\n0\n%%EOF\n";
            break;
        }
    }

    return oss.str();
}

// =============================================================================
// 2. P-14.2 (A-033): Planos 2D/3D (`enplanos.dll`), Capas DXF (`undxf.dll`) y Detalles (`det_*.dll`)
// =============================================================================

std::vector<CapaDxfPlano> ReportsDrawingsBimEngine::build_standard_dxf_drawing_layers() {
    // Equivalente exacto a las 16 capas principales de ubcpdxf_capa_dxf_* en undxf.dll
    return {
        {"EJES",                    1, 0.13, true,  true},
        {"PERFILES",                3, 0.35, false, true},
        {"HORMIGON",                4, 0.30, false, true},
        {"ARMADURAS",               5, 0.40, false, true},
        {"CHAPAS",                  2, 0.35, false, true},
        {"RIGIDIZADORES",           6, 0.30, false, true},
        {"PERNOS",                  1, 0.25, false, true},
        {"SOLDADURAS",              1, 0.20, false, true},
        {"ANGULARES_UNION",         3, 0.25, false, true},
        {"ARANDELAS",               7, 0.18, false, true},
        {"LINEAS_OCULTAS",          8, 0.15, true,  true},
        {"COTAS",                   7, 0.18, false, true},
        {"TEXTOS",                  7, 0.20, false, true},
        {"TABLAS",                  7, 0.25, false, true},
        {"MARCO_Y_CAJETIN",         7, 0.50, false, true},
        {"DETALLES_CONSTRUCTIVOS",  6, 0.25, false, true}
    };
}

std::vector<DetalleConstructivoCatalogo> ReportsDrawingsBimEngine::build_constructive_details_catalog() {
    // Biblioteca de detalles constructivos (det_*.dll)
    return {
        {
            "EAC001", "Estructura de Acero",
            "Detalle de placa de anclaje con pernos y mortero de nivelación",
            160.0, 120.0,
            {{{0.0, 0.0}, {160.0, 0.0}}, {{20.0, 20.0}, {140.0, 20.0}}, {{70.0, 20.0}, {70.0, 110.0}}, {{90.0, 20.0}, {90.0, 110.0}}},
            {{{25.0, 105.0}, "EAC001: Placa base S275 + Pernos B500S + Grout 30 mm"}}
        },
        {
            "EAC014", "Estructura de Acero",
            "Nudo rígido viga-pilar con chapa frontal atornillada y cartela inferior",
            160.0, 130.0,
            {{{40.0, 10.0}, {40.0, 120.0}}, {{40.0, 75.0}, {150.0, 95.0}}, {{40.0, 55.0}, {95.0, 75.0}}},
            {{{20.0, 115.0}, "EAC014: Unión atornillada clase 8.8 / 10.9 con cartela rigidizada"}}
        },
        {
            "EAM005", "Estructura de Madera",
            "Apoyo articulado de viga laminada encolada mediante herraje metálico oculto",
            150.0, 110.0,
            {{{15.0, 20.0}, {135.0, 20.0}}, {{35.0, 20.0}, {35.0, 90.0}}, {{115.0, 20.0}, {115.0, 90.0}}},
            {{{20.0, 98.0}, "EAM005: Herraje acero S275 galvanizado + pasadores calibrados"}}
        },
        {
            "CSZ002", "Cimentación Superficial",
            "Zapata aislada de hormigón armado sobre capa de hormigón de limpieza HL-150",
            170.0, 120.0,
            {{{10.0, 15.0}, {160.0, 15.0}}, {{15.0, 25.0}, {155.0, 25.0}}, {{15.0, 70.0}, {155.0, 70.0}}, {{75.0, 70.0}, {75.0, 115.0}}},
            {{{20.0, 108.0}, "CSZ002: Zapata H.A. rec. inf. 50 mm + parrilla ortogonal"}}
        },
        {
            "CCP004", "Cimentación Profunda",
            "Encepado rígido de 2 pilotes in situ con viga centradora y armadura de tirante",
            180.0, 130.0,
            {{{15.0, 50.0}, {165.0, 50.0}}, {{15.0, 95.0}, {165.0, 95.0}}, {{45.0, 5.0}, {45.0, 50.0}}, {{135.0, 5.0}, {135.0, 50.0}}},
            {{{20.0, 118.0}, "CCP004: Encepado sobre pilotes + cercos + tirante inferior"}}
        }
    };
}

HojaPlanoCompuesta ReportsDrawingsBimEngine::compose_drawing_sheet(
    const model::Obra& obra,
    TipoPlanoObra tipo,
    FormatoHojaPlano formato,
    double denominador_escala,
    const std::vector<connections::PlacaAnclaje3d>* baseplates,
    const std::vector<connections::UnionEstructural3d>* connections,
    const foundation::ModeloCompletoCimentacion* foundation_model,
    const std::vector<std::string>& codigos_detalles) {
    // Equivalente a plndimhoja_crea y plnediba_* en enplanos.dll
    HojaPlanoCompuesta hoja;
    hoja.formato = formato;
    get_sheet_dimensions_mm(formato, hoja.ancho_hoja_mm, hoja.alto_hoja_mm);
    hoja.denominador_escala = std::max(1.0, denominador_escala);
    hoja.tipo = tipo;
    hoja.capas = build_standard_dxf_drawing_layers();

    hoja.cajetin.proyecto = obra.datos_generales().descripcion.empty()
        ? obra.datos_generales().clave
        : obra.datos_generales().descripcion;
    hoja.cajetin.escala = "1:" + fmt_num(hoja.denominador_escala, 0);
    hoja.cajetin.titulo_plano = "Plano Estructural CYPE 3D";

    // 1. Dibujar recuadro exterior y cajetín normalizado en la esquina inferior derecha
    const double m = 10.0;
    const double w = hoja.ancho_hoja_mm;
    const double h = hoja.alto_hoja_mm;
    hoja.lineas.push_back({"MARCO_Y_CAJETIN", {m, m}, {w - m, m}});
    hoja.lineas.push_back({"MARCO_Y_CAJETIN", {w - m, m}, {w - m, h - m}});
    hoja.lineas.push_back({"MARCO_Y_CAJETIN", {w - m, h - m}, {m, h - m}});
    hoja.lineas.push_back({"MARCO_Y_CAJETIN", {m, h - m}, {m, m}});

    const double cj_w = 180.0;
    const double cj_h = 45.0;
    hoja.lineas.push_back({"MARCO_Y_CAJETIN", {w - m - cj_w, m}, {w - m - cj_w, m + cj_h}});
    hoja.lineas.push_back({"MARCO_Y_CAJETIN", {w - m - cj_w, m + cj_h}, {w - m, m + cj_h}});
    hoja.textos.push_back({"MARCO_Y_CAJETIN", {w - m - cj_w + 6.0, m + 32.0}, 3.5, hoja.cajetin.empresa});
    hoja.textos.push_back({"MARCO_Y_CAJETIN", {w - m - cj_w + 6.0, m + 20.0}, 3.0, "Obra: " + hoja.cajetin.proyecto});
    hoja.textos.push_back({"MARCO_Y_CAJETIN", {w - m - cj_w + 6.0, m + 8.0},  2.5, "Escala " + hoja.cajetin.escala + " | " + hoja.cajetin.numero_plano});

    // 2. Proyectar barras / cimentación / uniones a escala sobre la hoja (factor m -> mm en papel = 1000 / escala)
    const double scale_m_to_mm = 1000.0 / hoja.denominador_escala;
    const double ox_mm = 80.0;
    const double oy_mm = 90.0;

    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (ni == nullptr || nf == nullptr) {
            continue;
        }
        // Proyección axonométrica 2D o planta según el tipo de plano
        core::Vec2d p1_mm;
        core::Vec2d p2_mm;
        if (tipo == TipoPlanoObra::PlantaYDespieceCimentacion || tipo == TipoPlanoObra::Plano2D) {
            p1_mm = {ox_mm + ni->pos.x * scale_m_to_mm, oy_mm + ni->pos.y * scale_m_to_mm};
            p2_mm = {ox_mm + nf->pos.x * scale_m_to_mm, oy_mm + nf->pos.y * scale_m_to_mm};
        } else {
            p1_mm = {ox_mm + (ni->pos.x - 0.35 * ni->pos.y) * scale_m_to_mm,
                     oy_mm + (ni->pos.z + 0.25 * ni->pos.y) * scale_m_to_mm};
            p2_mm = {ox_mm + (nf->pos.x - 0.35 * nf->pos.y) * scale_m_to_mm,
                     oy_mm + (nf->pos.z + 0.25 * nf->pos.y) * scale_m_to_mm};
        }
        hoja.lineas.push_back({"PERFILES", p1_mm, p2_mm});
        hoja.textos.push_back({
            "TEXTOS",
            {0.5 * (p1_mm.x + p2_mm.x), 0.5 * (p1_mm.y + p2_mm.y) + 2.0},
            2.2,
            b.perfil
        });
    }

    if (foundation_model != nullptr) {
        for (const auto& zp : foundation_model->zapatas) {
            const double cx = ox_mm + zp.centro_planta_m.x * scale_m_to_mm;
            const double cy = oy_mm + zp.centro_planta_m.y * scale_m_to_mm;
            const double hx = 0.5 * zp.ancho_x_m * scale_m_to_mm;
            const double hy = 0.5 * zp.ancho_y_m * scale_m_to_mm;
            hoja.lineas.push_back({"HORMIGON", {cx - hx, cy - hy}, {cx + hx, cy - hy}});
            hoja.lineas.push_back({"HORMIGON", {cx + hx, cy - hy}, {cx + hx, cy + hy}});
            hoja.lineas.push_back({"HORMIGON", {cx + hx, cy + hy}, {cx - hx, cy + hy}});
            hoja.lineas.push_back({"HORMIGON", {cx - hx, cy + hy}, {cx - hx, cy - hy}});
            hoja.textos.push_back({"TEXTOS", {cx - hx + 2.0, cy + hy + 2.0}, 2.5, zp.referencia});
        }
    }

    if (baseplates != nullptr) {
        for (const auto& bp : *baseplates) {
            hoja.textos.push_back({
                "CHAPAS",
                {ox_mm + 10.0, h - 40.0 - static_cast<double>(bp.id) * 8.0},
                2.4,
                "Placa PB-" + std::to_string(bp.id) + ": " + fmt_num(bp.ancho_lx_mm, 0) + "x" +
                fmt_num(bp.largo_ly_mm, 0) + "x" + fmt_num(bp.espesor_e_mm, 0) + " mm"
            });
        }
    }

    if (connections != nullptr) {
        for (const auto& u : *connections) {
            const std::string desc_tipo = u.plano_detalle.titulo_lamina.empty()
                ? (u.es_atornillada ? "Unión atornillada" : "Unión soldada")
                : u.plano_detalle.titulo_lamina;
            hoja.textos.push_back({
                "SOLDADURAS",
                {ox_mm + 120.0, h - 40.0 - static_cast<double>(u.id) * 8.0},
                2.4,
                u.referencia + ": " + desc_tipo
            });
        }
    }

    // 3. Insertar detalles constructivos seleccionados (det_*.dll)
    const auto catalogo_det = build_constructive_details_catalog();
    double det_x_offset = w - 210.0;
    double det_y_offset = m + cj_h + 15.0;
    for (const auto& code : codigos_detalles) {
        for (const auto& det : catalogo_det) {
            if (det.codigo == code) {
                hoja.detalles_insertados.push_back(det);
                for (const auto& seg : det.lineas_vectoriales_mm) {
                    hoja.lineas.push_back({
                        "DETALLES_CONSTRUCTIVOS",
                        {det_x_offset + seg.first.x, det_y_offset + seg.first.y},
                        {det_x_offset + seg.second.x, det_y_offset + seg.second.y}
                    });
                }
                for (const auto& txt : det.textos_nota) {
                    hoja.textos.push_back({
                        "DETALLES_CONSTRUCTIVOS",
                        {det_x_offset + txt.first.x, det_y_offset + txt.first.y},
                        2.2,
                        txt.second
                    });
                }
                det_y_offset += det.alto_mm + 10.0;
            }
        }
    }

    return hoja;
}

std::string ReportsDrawingsBimEngine::export_drawing_to_dxf(const HojaPlanoCompuesta& hoja) {
    // Genera un fichero AutoCAD DXF (secciones HEADER, TABLES/LAYER, ENTITIES, EOF) compatible con dxflib.dll / undxf.dll
    std::ostringstream oss;
    oss << "0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n";
    oss << "0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n70\n" << hoja.capas.size() << "\n";
    for (const auto& capa : hoja.capas) {
        oss << "0\nLAYER\n2\n" << capa.nombre_capa << "\n70\n0\n62\n" << capa.color_aci
            << "\n6\n" << (capa.tipo_linea_discontinua ? "DASHED" : "CONTINUOUS") << "\n";
    }
    oss << "0\nENDTAB\n0\nENDSEC\n";

    oss << "0\nSECTION\n2\nENTITIES\n";
    for (const auto& ln : hoja.lineas) {
        oss << "0\nLINE\n8\n" << ln.capa
            << "\n10\n" << fmt_num(ln.p1_mm.x, 3) << "\n20\n" << fmt_num(ln.p1_mm.y, 3) << "\n30\n0.0"
            << "\n11\n" << fmt_num(ln.p2_mm.x, 3) << "\n21\n" << fmt_num(ln.p2_mm.y, 3) << "\n31\n0.0\n";
    }
    for (const auto& tx : hoja.textos) {
        oss << "0\nTEXT\n8\n" << tx.capa
            << "\n10\n" << fmt_num(tx.pos_mm.x, 3) << "\n20\n" << fmt_num(tx.pos_mm.y, 3) << "\n30\n0.0"
            << "\n40\n" << fmt_num(tx.altura_mm, 2) << "\n1\n" << tx.texto << "\n";
    }
    oss << "0\nENDSEC\n0\nEOF\n";
    return oss.str();
}

std::string ReportsDrawingsBimEngine::export_drawing_to_svg_dwg_xml(const HojaPlanoCompuesta& hoja) {
    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
        << fmt_num(hoja.ancho_hoja_mm, 0) << " " << fmt_num(hoja.alto_hoja_mm, 0) << "\">\n";
    for (const auto& ln : hoja.lineas) {
        oss << "  <line data-layer=\"" << ln.capa << "\" x1=\"" << fmt_num(ln.p1_mm.x, 2)
            << "\" y1=\"" << fmt_num(hoja.alto_hoja_mm - ln.p1_mm.y, 2)
            << "\" x2=\"" << fmt_num(ln.p2_mm.x, 2)
            << "\" y2=\"" << fmt_num(hoja.alto_hoja_mm - ln.p2_mm.y, 2)
            << "\" stroke=\"#1e293b\" stroke-width=\"0.35\"/>\n";
    }
    for (const auto& tx : hoja.textos) {
        oss << "  <text data-layer=\"" << tx.capa << "\" x=\"" << fmt_num(tx.pos_mm.x, 2)
            << "\" y=\"" << fmt_num(hoja.alto_hoja_mm - tx.pos_mm.y, 2)
            << "\" font-size=\"" << fmt_num(tx.altura_mm, 1) << "\">" << tx.texto << "</text>\n";
    }
    oss << "</svg>\n";
    return oss.str();
}

std::string ReportsDrawingsBimEngine::export_drawing_to_pdf(const HojaPlanoCompuesta& hoja) {
    std::ostringstream cmds;
    cmds << "0.5 w\n";
    for (const auto& ln : hoja.lineas) {
        cmds << fmt_num(ln.p1_mm.x, 2) << " " << fmt_num(ln.p1_mm.y, 2) << " m "
             << fmt_num(ln.p2_mm.x, 2) << " " << fmt_num(ln.p2_mm.y, 2) << " l S\n";
    }
    for (const auto& tx : hoja.textos) {
        cmds << "BT /F1 8 Tf " << fmt_num(tx.pos_mm.x, 2) << " " << fmt_num(tx.pos_mm.y, 2)
             << " Td (" << tx.texto << ") Tj ET\n";
    }
    const std::string body = cmds.str();

    std::ostringstream pdf;
    pdf << "%PDF-1.4\n";
    pdf << "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n";
    pdf << "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n";
    pdf << "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 "
        << fmt_num(hoja.ancho_hoja_mm, 0) << " " << fmt_num(hoja.alto_hoja_mm, 0)
        << "] /Contents 4 0 R >> endobj\n";
    pdf << "4 0 obj << /Length " << body.size() << " >>\nstream\n" << body << "endstream\nendobj\n";
    pdf << "xref\n0 5\ntrailer << /Size 5 /Root 1 0 R >>\n%%EOF\n";
    return pdf.str();
}

// =============================================================================
// 3. P-14.3 (A-034): Interoperabilidad CAD/BIM (`imporstr`, `bimifc`, `cytekla`, `cypecis2`, `cytecnometal`)
// =============================================================================

ResultadoImportacionCad ReportsDrawingsBimEngine::import_geometry_from_dxf(
    model::Obra& obra,
    std::string_view dxf_content,
    std::string_view default_profile,
    std::string_view default_material,
    double merge_tolerance_m) {
    // Equivalente a impdxf_* en imporstr.dll
    ResultadoImportacionCad res;
    const auto lines = core::StringUtils::split(dxf_content, '\n', true);

    bool in_line_entity = false;
    core::Vec3d p1{0.0, 0.0, 0.0};
    core::Vec3d p2{0.0, 0.0, 0.0};
    std::string layer_name = "0";

    auto flush_line = [&]() {
        if ((p2 - p1).norm() > merge_tolerance_m) {
            const auto n1 = find_or_create_node_3d(obra, p1, merge_tolerance_m, res.nudos_importados);
            const auto n2 = find_or_create_node_3d(obra, p2, merge_tolerance_m, res.nudos_importados);
            if (n1 != n2) {
                obra.add_barra(n1, n2, default_profile, default_material);
                ++res.barras_importadas;
                if (std::find(res.capas_detectadas.begin(), res.capas_detectadas.end(), layer_name) == res.capas_detectadas.end()) {
                    res.capas_detectadas.push_back(layer_name);
                }
            }
        }
        p1 = {0.0, 0.0, 0.0};
        p2 = {0.0, 0.0, 0.0};
    };

    for (std::size_t i = 0; i + 1 < lines.size(); i += 2) {
        const std::string code = core::StringUtils::trim(lines[i]);
        const std::string val = core::StringUtils::trim(lines[i + 1]);

        if (code == "0") {
            if (in_line_entity) {
                flush_line();
            }
            in_line_entity = (val == "LINE");
            continue;
        }
        if (!in_line_entity) {
            continue;
        }
        if (code == "8")  layer_name = val;
        if (code == "10") p1.x = std::stod(val);
        if (code == "20") p1.y = std::stod(val);
        if (code == "30") p1.z = std::stod(val);
        if (code == "11") p2.x = std::stod(val);
        if (code == "21") p2.y = std::stod(val);
        if (code == "31") p2.z = std::stod(val);
    }
    if (in_line_entity) {
        flush_line();
    }

    res.exito = (res.barras_importadas > 0);
    res.resumen = "Importación DXF completada: " + std::to_string(res.nudos_importados) + " nudos nuevos y " +
                  std::to_string(res.barras_importadas) + " barras generadas.";
    return res;
}

ResultadoImportacionCad ReportsDrawingsBimEngine::import_geometry_from_text(
    model::Obra& obra,
    std::string_view txt_content,
    std::string_view default_profile,
    std::string_view default_material) {
    // Formato de texto de líneas/barras: x1 y1 z1 x2 y2 z2 [perfil]
    ResultadoImportacionCad res;
    const auto lines = core::StringUtils::split(txt_content, '\n', true);
    for (const auto& raw : lines) {
        const std::string trimmed = core::StringUtils::trim(raw);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        std::istringstream iss(trimmed);
        core::Vec3d p1{}, p2{};
        if (iss >> p1.x >> p1.y >> p1.z >> p2.x >> p2.y >> p2.z) {
            const auto n1 = find_or_create_node_3d(obra, p1, 1.0e-3, res.nudos_importados);
            const auto n2 = find_or_create_node_3d(obra, p2, 1.0e-3, res.nudos_importados);
            if (n1 != n2) {
                obra.add_barra(n1, n2, default_profile, default_material);
                ++res.barras_importadas;
            }
        }
    }
    res.exito = (res.barras_importadas > 0);
    res.resumen = "Importación TXT completada: " + std::to_string(res.nudos_importados) + " nudos y " +
                  std::to_string(res.barras_importadas) + " barras.";
    return res;
}

std::string ReportsDrawingsBimEngine::export_to_ifc(
    const model::Obra& obra,
    VersionEsquemaIfc version,
    const foundation::ModeloCompletoCimentacion* foundation_model,
    const std::vector<connections::PlacaAnclaje3d>* baseplates) {
    // Equivalente a bimifc.dll + bsifcpsets.dll (ISO-10303-21 STEP IFC2X3 / IFC4)
    const std::string schema = (version == VersionEsquemaIfc::IFC2x3_TC1) ? "IFC2X3" : "IFC4";
    std::ostringstream oss;
    oss << "ISO-10303-21;\n";
    oss << "HEADER;\n";
    oss << "FILE_DESCRIPTION(('ViewDefinition [CoordinationView_V2.0]'),'2;1');\n";
    oss << "FILE_NAME('" << obra.datos_generales().clave << ".ifc','2026-11-15T12:00:00',('CypeClon'),('CYPE Ingenieros / CypeClon C++20'),'bimifc.dll','CYPE 3D v2027','');\n";
    oss << "FILE_SCHEMA(('" << schema << "'));\n";
    oss << "ENDSEC;\n";
    oss << "DATA;\n";
    oss << "#1=IFCPROJECT('0001_PRJ',$,'" << obra.datos_generales().clave << "','" << obra.datos_generales().descripcion << "',$,$,$,$,$);\n";
    oss << "#2=IFCSITE('0002_SITE',$,'Parcela',$,$,$,$,$,.ELEMENT.,$,$,$,$,$);\n";
    oss << "#3=IFCBUILDING('0003_BLD',$,'Edificio',$,$,$,$,$,.ELEMENT.,$,$,$);\n";
    oss << "#4=IFCBUILDINGSTOREY('0004_STR',$,'Planta General',$,$,$,$,$,.ELEMENT.,0.0);\n";

    std::uint32_t next_step = 100;
    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (ni == nullptr || nf == nullptr) continue;

        const bool es_pilar = (std::abs(nf->pos.z - ni->pos.z) > 0.70 * obra.longitud_barra_m(b.id));
        const std::string ent_type = es_pilar ? "IFCCOLUMN" : "IFCBEAM";
        const std::uint32_t id_pt1 = next_step++;
        const std::uint32_t id_pt2 = next_step++;
        const std::uint32_t id_elem = next_step++;
        const std::uint32_t id_pset = next_step++;

        oss << "#" << id_pt1 << "=IFCCARTESIANPOINT((" << fmt_num(ni->pos.x, 3) << "," << fmt_num(ni->pos.y, 3) << "," << fmt_num(ni->pos.z, 3) << "));\n";
        oss << "#" << id_pt2 << "=IFCCARTESIANPOINT((" << fmt_num(nf->pos.x, 3) << "," << fmt_num(nf->pos.y, 3) << "," << fmt_num(nf->pos.z, 3) << "));\n";
        oss << "#" << id_elem << "=" << ent_type << "('BAR_" << b.id << "',$,'B" << b.id << "','" << b.perfil << "','" << b.material << "',#" << id_pt1 << ",#" << id_pt2 << ",'B" << b.id << "',.STANDARD.);\n";
        oss << "#" << id_pset << "=IFCPROPERTYSET('PSET_B" << b.id << "',$,'Pset_Cype3dMember',$,('" << b.perfil << "','" << b.material << "'));\n";
    }

    for (const auto& lam : obra.estructura().laminas) {
        const std::uint32_t id_slab = next_step++;
        oss << "#" << id_slab << "=IFCSLAB('SLAB_" << lam.id << "',$,'Lamina " << lam.id << "','e=" << fmt_num(lam.espesor_m, 2) << " m','" << lam.material << "',$,$,'L" << lam.id << "',.FLOOR.);\n";
    }

    if (foundation_model != nullptr) {
        for (const auto& zp : foundation_model->zapatas) {
            const std::uint32_t id_ftg = next_step++;
            oss << "#" << id_ftg << "=IFCFOOTING('FTG_" << zp.id << "',$,'" << zp.referencia << "','"
                << fmt_num(zp.ancho_x_m, 2) << "x" << fmt_num(zp.ancho_y_m, 2) << "x" << fmt_num(zp.canto_total_m, 2)
                << "',$,$,$,'" << zp.referencia << "',.PAD_FOOTING.);\n";
        }
        for (const auto& enc : foundation_model->encepados) {
            const std::uint32_t id_cap = next_step++;
            oss << "#" << id_cap << "=IFCFOOTING('CAP_" << enc.id << "',$,'" << enc.referencia << "','Encepado',$,$,$,'" << enc.referencia << "',.PILE_CAP.);\n";
            for (const auto& p : enc.pilotes) {
                const std::uint32_t id_pile = next_step++;
                oss << "#" << id_pile << "=IFCPILE('PILE_" << enc.id << "_" << p.indice_pilote << "',$,'" << enc.pilote_catalogo.codigo << "',$,$,$,$,$,.SUPPORT.,.CAST_IN_PLACE.);\n";
            }
        }
    }

    if (baseplates != nullptr) {
        for (const auto& bp : *baseplates) {
            const std::uint32_t id_bp = next_step++;
            oss << "#" << id_bp << "=IFCPLATE('BP_" << bp.id << "',$,'Placa PB-" << bp.id << "','" << bp.acero_placa << "',$,$,$,'PB-" << bp.id << "',.BASE_PLATE.);\n";
        }
    }

    oss << "ENDSEC;\n";
    oss << "END-ISO-10303-21;\n";
    return oss.str();
}

std::string ReportsDrawingsBimEngine::export_to_gltf_json(
    const model::Obra& obra,
    const foundation::ModeloCompletoCimentacion* foundation_model) {
    // Equivalente a la exportación glTF 2.0 para BIMserver.center (bimcenter.dll)
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"asset\": { \"version\": \"2.0\", \"generator\": \"CYPE 3D v2027 / CypeClon C++20 (bimcenter.dll)\" },\n";
    oss << "  \"scene\": 0,\n";
    oss << "  \"scenes\": [{ \"name\": \"" << obra.datos_generales().clave << "\", \"nodes\": [0] }],\n";
    oss << "  \"materials\": [\n";
    oss << "    { \"name\": \"Acero_S275\", \"pbrMetallicRoughness\": { \"baseColorFactor\": [0.28, 0.52, 0.78, 1.0], \"metallicFactor\": 0.8, \"roughnessFactor\": 0.3 } },\n";
    oss << "    { \"name\": \"Hormigon_C25_30\", \"pbrMetallicRoughness\": { \"baseColorFactor\": [0.62, 0.65, 0.68, 1.0], \"metallicFactor\": 0.1, \"roughnessFactor\": 0.85 } }\n";
    oss << "  ],\n";
    oss << "  \"nodes\": [\n";
    oss << "    { \"name\": \"Estructura_CYPE3D\", \"children\": [";
    const std::size_t n_bars = obra.estructura().barras.size();
    const std::size_t n_ftgs = (foundation_model != nullptr) ? foundation_model->zapatas.size() : 0;
    const std::size_t total_children = n_bars + n_ftgs;
    for (std::size_t i = 0; i < total_children; ++i) {
        oss << (i ? ", " : "") << (i + 1);
    }
    oss << "] }";

    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const core::Vec3d p = (ni != nullptr) ? ni->pos : core::Vec3d{0.0, 0.0, 0.0};
        oss << ",\n    { \"name\": \"Barra_B" << b.id << "_" << b.perfil << "\", \"translation\": ["
            << fmt_num(p.x, 3) << ", " << fmt_num(p.z, 3) << ", " << fmt_num(p.y, 3) << "] }";
    }
    if (foundation_model != nullptr) {
        for (const auto& zp : foundation_model->zapatas) {
            oss << ",\n    { \"name\": \"Zapata_" << zp.referencia << "\", \"translation\": ["
                << fmt_num(zp.centro_planta_m.x, 3) << ", " << fmt_num(-0.5 * zp.canto_total_m, 3) << ", "
                << fmt_num(zp.centro_planta_m.y, 3) << "] }";
        }
    }
    oss << "\n  ]\n}\n";
    return oss.str();
}

std::string ReportsDrawingsBimEngine::export_to_cis2_step(
    const model::Obra& obra,
    const std::vector<connections::UnionEstructural3d>* connections,
    const std::vector<connections::PlacaAnclaje3d>* baseplates) {
    // Equivalente a cis2modelo_* en cypecis2.dll (CIMsteel Integration Standards CIS/2 LPM6)
    std::ostringstream oss;
    oss << "ISO-10303-21;\n";
    oss << "HEADER;\n";
    oss << "FILE_DESCRIPTION(('CIMsteel Integration Standards CIS/2 - Structural Frame Schema'),'2;1');\n";
    oss << "FILE_NAME('" << obra.datos_generales().clave << ".stp','2026-11-15',('CypeClon'),('CYPE 3D v2027'),'cypecis2.dll','','');\n";
    oss << "FILE_SCHEMA(('STRUCTURAL_FRAME_SCHEMA'));\n";
    oss << "ENDSEC;\n";
    oss << "DATA;\n";

    std::uint32_t eid = 10;
    oss << "#" << eid++ << "=STRUCTURAL_FRAME_PROJECT('" << obra.datos_generales().clave << "','" << obra.datos_generales().descripcion << "');\n";
    oss << "#" << eid++ << "=MATERIAL_ISOTROPIC_STEEL('" << obra.datos_generales().acero_laminado << "',210000.0,0.3,275.0);\n";

    // Exportar barras lineales (cis2modelo_append_elemento_linear)
    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (ni == nullptr || nf == nullptr) continue;

        oss << "#" << eid++ << "=SECTION_PROFILE_I_TYPE('" << b.perfil << "');\n";
        oss << "#" << eid++ << "=LOCATED_PART_LINEAR('B" << b.id << "','" << b.perfil << "','" << b.material << "',("
            << fmt_num(ni->pos.x, 3) << "," << fmt_num(ni->pos.y, 3) << "," << fmt_num(ni->pos.z, 3) << "),("
            << fmt_num(nf->pos.x, 3) << "," << fmt_num(nf->pos.y, 3) << "," << fmt_num(nf->pos.z, 3) << "));\n";
    }

    // Exportar chapas, cartelas y conjuntos de tornillos (cis2modelo_append_elemento_plano, cis2modelo_append_conjunto_tornillo)
    if (baseplates != nullptr) {
        for (const auto& bp : *baseplates) {
            oss << "#" << eid++ << "=LOCATED_PART_PLANAR('PB_" << bp.id << "'," << fmt_num(bp.ancho_lx_mm, 1) << ","
                << fmt_num(bp.largo_ly_mm, 1) << "," << fmt_num(bp.espesor_e_mm, 1) << ",'" << bp.acero_placa << "');\n";
        }
    }
    if (connections != nullptr) {
        for (const auto& u : *connections) {
            const std::string desc_tipo = u.plano_detalle.titulo_lamina.empty()
                ? (u.es_atornillada ? "Unión atornillada" : "Unión soldada")
                : u.plano_detalle.titulo_lamina;
            oss << "#" << eid++ << "=JOINT_SYSTEM_WELDED_OR_BOLTED('" << u.referencia << "',N" << u.nudo_id
                << ",'" << desc_tipo << "',BOLTS=" << u.grupos_tornillos.size()
                << ",WELDS=" << u.soldaduras.size() << ");\n";
        }
    }

    oss << "ENDSEC;\n";
    oss << "END-ISO-10303-21;\n";
    return oss.str();
}

std::string ReportsDrawingsBimEngine::export_to_tekla(
    const model::Obra& obra,
    const std::vector<connections::UnionEstructural3d>* connections,
    const std::vector<connections::PlacaAnclaje3d>* baseplates) {
    // Equivalente a ctkmodelo_* y ctkmaterial_* en cytekla.dll (Exportacion_A_Tekla)
    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<TeklaStructuresModel generator=\"cytekla.dll (CYPE 3D v2027 / CypeClon C++20)\" project=\""
        << obra.datos_generales().clave << "\">\n";
    oss << "  <Materials>\n";
    oss << "    <Material kind=\"ctkmaterial_acero_laminado_armado\" name=\"" << obra.datos_generales().acero_laminado << "\"/>\n";
    oss << "    <Material kind=\"ctkmaterial_acero_conformado\" name=\"" << obra.datos_generales().acero_conformado << "\"/>\n";
    oss << "    <Material kind=\"ctkmaterial_hormigon\" name=\"" << obra.datos_generales().hormigon_estructural << "\"/>\n";
    oss << "    <Material kind=\"ctkmaterial_madera\" name=\"" << obra.datos_generales().madera << "\"/>\n";
    oss << "  </Materials>\n";

    oss << "  <Members>\n";
    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (ni == nullptr || nf == nullptr) continue;

        const bool es_pilar = (std::abs(nf->pos.z - ni->pos.z) > 0.70 * obra.longitud_barra_m(b.id));
        const std::string func = es_pilar ? "ctkmodelo_append_pilar" : "ctkmodelo_append_viga";
        oss << "    <Member id=\"B" << b.id << "\" api=\"" << func << "\" profile=\"" << b.perfil
            << "\" material=\"" << b.material << "\" x1=\"" << fmt_num(ni->pos.x * 1000.0, 1)
            << "\" y1=\"" << fmt_num(ni->pos.y * 1000.0, 1) << "\" z1=\"" << fmt_num(ni->pos.z * 1000.0, 1)
            << "\" x2=\"" << fmt_num(nf->pos.x * 1000.0, 1) << "\" y2=\"" << fmt_num(nf->pos.y * 1000.0, 1)
            << "\" z2=\"" << fmt_num(nf->pos.z * 1000.0, 1) << "\"/>\n";
    }
    oss << "  </Members>\n";

    if (baseplates != nullptr && !baseplates->empty()) {
        oss << "  <BasePlates>\n";
        for (const auto& bp : *baseplates) {
            oss << "    <BasePlate id=\"PB" << bp.id << "\" api=\"ctkmodelo_append_chapa_base_placa_anclaje\" node=\"N"
                << bp.nudo_id << "\" lx_mm=\"" << fmt_num(bp.ancho_lx_mm, 0) << "\" ly_mm=\""
                << fmt_num(bp.largo_ly_mm, 0) << "\" e_mm=\"" << fmt_num(bp.espesor_e_mm, 0)
                << "\" bolts=\"" << bp.pernos.size() << "\" stiffeners=\"" << bp.rigidizadores.size() << "\"/>\n";
        }
        oss << "  </BasePlates>\n";
    }

    if (connections != nullptr && !connections->empty()) {
        oss << "  <Connections>\n";
        for (const auto& u : *connections) {
            oss << "    <Connection code=\"" << u.referencia << "\" api=\"ctkmodelo_append_auto_viga_pilar\" node=\"N"
                << u.nudo_id << "\" plates=\"" << u.chapas_y_rigidizadores.size() << "\" bolts=\"" << u.grupos_tornillos.size()
                << "\" welds=\"" << u.soldaduras.size() << "\"/>\n";
        }
        oss << "  </Connections>\n";
    }

    oss << "</TeklaStructuresModel>\n";
    return oss.str();
}

std::string ReportsDrawingsBimEngine::export_to_tecnometal(
    const model::Obra& obra,
    const std::vector<connections::UnionEstructural3d>* connections,
    const std::vector<connections::PlacaAnclaje3d>* baseplates) {
    // Equivalente a ctmmodelo_* en cytecnometal.dll
    std::ostringstream oss;
    oss << "; ==========================================================================\n";
    oss << "; FICHERO DE INTERCAMBIO TECNOMETAL 4D (cytecnometal.dll - CYPE 3D v2027)\n";
    oss << "; OBRA: " << obra.datos_generales().clave << " - " << obra.datos_generales().descripcion << "\n";
    oss << "; ==========================================================================\n";
    oss << "[MODELO_VACIO]\nctmmodelo_crea_vacio=1\n\n";

    oss << "[PIEZAS]\n";
    for (const auto& b : obra.estructura().barras) {
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (ni == nullptr || nf == nullptr) continue;
        const bool es_pilar = (std::abs(nf->pos.z - ni->pos.z) > 0.70 * obra.longitud_barra_m(b.id));
        const std::string cmd = b.es_tirante
            ? "ctmmodelo_append_tirante"
            : (es_pilar ? "ctmmodelo_append_pilar" : "ctmmodelo_append_viga");
        oss << cmd << "=B" << b.id << ";PERFIL=" << b.perfil << ";MAT=" << b.material
            << ";INI=(" << fmt_num(ni->pos.x * 1000.0, 1) << "," << fmt_num(ni->pos.y * 1000.0, 1) << "," << fmt_num(ni->pos.z * 1000.0, 1) << ")"
            << ";FIN=(" << fmt_num(nf->pos.x * 1000.0, 1) << "," << fmt_num(nf->pos.y * 1000.0, 1) << "," << fmt_num(nf->pos.z * 1000.0, 1) << ")\n";
    }

    if (baseplates != nullptr && !baseplates->empty()) {
        oss << "\n[PLACAS_ANCLAJE]\n";
        for (const auto& bp : *baseplates) {
            oss << "ctmmodelo_append_chapa_anclaje=PB" << bp.id << ";NUDO=N" << bp.nudo_id
                << ";LX=" << fmt_num(bp.ancho_lx_mm, 0) << ";LY=" << fmt_num(bp.largo_ly_mm, 0)
                << ";ESP=" << fmt_num(bp.espesor_e_mm, 0) << ";TALADROS=" << bp.pernos.size() << "\n";
        }
    }

    if (connections != nullptr && !connections->empty()) {
        oss << "\n[UNIONES]\n";
        for (const auto& u : *connections) {
            oss << "ctmmodelo_append_chapa=" << u.referencia << ";NUDO=N" << u.nudo_id
                << ";CHAPAS=" << u.chapas_y_rigidizadores.size() << ";TORNILLOS=" << u.grupos_tornillos.size() << "\n";
        }
    }

    return oss.str();
}

} // namespace cypeclon::io
