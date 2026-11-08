#include "model/obra.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace cypeclon::model {

Obra::Obra() {
    estructura_.capas.push_back(EtiquetaCapa{1, "Por defecto", true, false, 0x0080FF});
    estructura_.rejillas.push_back(RejillaTrabajo{});
    add_hipotesis_carga("PP", "Peso propio", 0, 0, 0, 0, true);
    add_hipotesis_carga("CM", "Cargas muertas", 1, 0, 0, 0, true);
    add_hipotesis_carga("Q1", "Sobrecarga de uso", 2, 0, 0, 0, true);
    add_definicion_rotula_plastica(DefinicionRotulaPlastica{
        0, "FEMA356-Acero-Flexion-M3", TipoRotulaPlastica::FlexionEjeFuerteM3, 0.03, 0.010, 0.025, 0.040, 0.050, 0.20
    });
    undo_redo_.clear();
}

std::uint32_t Obra::add_nudo(const core::Vec3d& pos, VinculacionExterior vinc) {
    const std::uint32_t assigned_id = estructura_.next_nudo_id++;
    Nudo3d nudo{assigned_id, pos, "N" + std::to_string(assigned_id), vinc, true};
    nudo.vinc_ext_datos = DatosVinculacionExteriorNudo::from_tipo(vinc);

    undo_redo_.record_action(
        "Nuevo nudo N" + std::to_string(assigned_id),
        [this, nudo]() {
            estructura_.nudos.push_back(nudo);
        },
        [this, assigned_id]() {
            auto& vec = estructura_.nudos;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                                     [assigned_id](const Nudo3d& n) { return n.id == assigned_id; }),
                      vec.end());
        },
        true
    );
    return assigned_id;
}

bool Obra::move_nudo(std::uint32_t nudo_id, const core::Vec3d& new_pos) {
    auto it = std::find_if(estructura_.nudos.begin(), estructura_.nudos.end(),
                           [nudo_id](const Nudo3d& n) { return n.id == nudo_id; });
    if (it == estructura_.nudos.end()) {
        return false;
    }
    const core::Vec3d old_pos = it->pos;
    undo_redo_.record_action(
        "Mover nudo N" + std::to_string(nudo_id),
        [this, nudo_id, new_pos]() {
            for (auto& n : estructura_.nudos) {
                if (n.id == nudo_id) {
                    n.pos = new_pos;
                    break;
                }
            }
        },
        [this, nudo_id, old_pos]() {
            for (auto& n : estructura_.nudos) {
                if (n.id == nudo_id) {
                    n.pos = old_pos;
                    break;
                }
            }
        },
        true
    );
    return true;
}

std::uint32_t Obra::add_barra(std::uint32_t nudo_ini, std::uint32_t nudo_fin, std::string_view perfil, std::string_view material) {
    const std::uint32_t assigned_id = estructura_.next_barra_id++;
    Barra3d barra{assigned_id, nudo_ini, nudo_fin, std::string(perfil), std::string(material), 0.0, false, 1};

    undo_redo_.record_action(
        "Nueva barra B" + std::to_string(assigned_id),
        [this, barra]() {
            estructura_.barras.push_back(barra);
        },
        [this, assigned_id]() {
            auto& vec = estructura_.barras;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                                     [assigned_id](const Barra3d& b) { return b.id == assigned_id; }),
                      vec.end());
        },
        true
    );
    return assigned_id;
}

std::uint32_t Obra::add_elemento_cimentacion(
    TipoElementoCimentacion tipo,
    std::uint32_t nudo_arranque_id,
    double ancho_x,
    double ancho_y,
    double canto
) {
    const std::uint32_t assigned_id = cimentacion_.next_elemento_id++;
    ElementoCimentacion elem{assigned_id, tipo, nudo_arranque_id, ancho_x, ancho_y, canto, 0.0};

    undo_redo_.record_action(
        "Nuevo elemento cimentacion Z" + std::to_string(assigned_id),
        [this, elem]() {
            cimentacion_.elementos.push_back(elem);
        },
        [this, assigned_id]() {
            auto& vec = cimentacion_.elementos;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                                     [assigned_id](const ElementoCimentacion& e) { return e.id == assigned_id; }),
                      vec.end());
        },
        true
    );
    return assigned_id;
}

const Nudo3d* Obra::find_nudo(std::uint32_t id) const noexcept {
    for (const auto& n : estructura_.nudos) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

Nudo3d* Obra::find_nudo_mut(std::uint32_t id) noexcept {
    for (auto& n : estructura_.nudos) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const Barra3d* Obra::find_barra(std::uint32_t id) const noexcept {
    for (const auto& b : estructura_.barras) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

Barra3d* Obra::find_barra_mut(std::uint32_t id) noexcept {
    for (auto& b : estructura_.barras) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

const EtiquetaCapa* Obra::find_capa(std::uint32_t id) const noexcept {
    for (const auto& c : estructura_.capas) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

EtiquetaCapa* Obra::find_capa_mut(std::uint32_t id) noexcept {
    for (auto& c : estructura_.capas) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

std::uint32_t Obra::add_lamina(const std::vector<core::Vec3d>& vertices, double espesor_m, std::string_view material, std::uint32_t capa_id) {
    const std::uint32_t assigned_id = estructura_.next_lamina_id++;
    Lamina3d lam{assigned_id, vertices, espesor_m, std::string(material), capa_id};
    estructura_.laminas.push_back(std::move(lam));
    return assigned_id;
}

std::uint32_t Obra::add_panyo(const std::vector<core::Vec3d>& vertices, const core::Vec3d& dir_reparto, std::uint32_t capa_id) {
    const std::uint32_t assigned_id = estructura_.next_panyo_id++;
    Panyo3d pan{assigned_id, vertices, dir_reparto, capa_id};
    estructura_.panyos.push_back(std::move(pan));
    return assigned_id;
}

std::uint32_t Obra::add_nivel_planta(std::string_view nombre, double cota_z_m, double altura_m, std::uint32_t grupo_id) {
    const std::uint32_t assigned_id = estructura_.next_nivel_id++;
    NivelPlanta niv{assigned_id, std::string(nombre), cota_z_m, altura_m, grupo_id, true};
    estructura_.niveles.push_back(std::move(niv));
    return assigned_id;
}

std::uint32_t Obra::add_etiqueta_capa(std::string_view nombre, std::uint32_t color_rgb, bool visible, bool bloqueada) {
    const std::uint32_t assigned_id = estructura_.next_capa_id++;
    EtiquetaCapa cap{assigned_id, std::string(nombre), visible, bloqueada, color_rgb};
    estructura_.capas.push_back(std::move(cap));
    return assigned_id;
}

std::uint32_t Obra::add_hipotesis_carga(
    std::string_view codigo,
    std::string_view descripcion,
    std::uint8_t naturaleza,
    std::uint8_t disposicion,
    std::uint8_t categoria_uso,
    std::uint32_t grupo_incompatibilidad,
    bool es_automatica
) {
    const std::uint32_t assigned_id = estructura_.next_hipotesis_id++;
    HipotesisCargaObra hip{
        assigned_id,
        std::string(codigo),
        std::string(descripcion),
        naturaleza,
        disposicion,
        categoria_uso,
        grupo_incompatibilidad,
        es_automatica,
        true
    };
    estructura_.hipotesis.push_back(std::move(hip));
    return assigned_id;
}

std::uint32_t Obra::add_carga_superficial_panyo(
    std::uint32_t panyo_id,
    std::uint32_t hipotesis_id,
    double valor_kn_m2,
    const core::Vec3d& direccion,
    bool en_ejes_locales_panyo,
    std::string_view descripcion_zona
) {
    const std::uint32_t assigned_id = estructura_.next_carga_sup_id++;
    CargaSuperficialPanyo csup{
        assigned_id,
        panyo_id,
        hipotesis_id,
        valor_kn_m2,
        direccion,
        en_ejes_locales_panyo,
        std::string(descripcion_zona)
    };
    estructura_.cargas_superficiales.push_back(std::move(csup));
    return assigned_id;
}

std::uint32_t Obra::add_carga_lineal_barra(
    std::uint32_t barra_id,
    std::uint32_t hipotesis_id,
    double q_ini_kn_m,
    double q_fin_kn_m,
    const core::Vec3d& direccion,
    bool en_ejes_locales
) {
    const std::uint32_t assigned_id = estructura_.next_carga_lin_id++;
    CargaLinealBarra clin{
        assigned_id,
        barra_id,
        hipotesis_id,
        q_ini_kn_m,
        q_fin_kn_m,
        0.0,
        1.0,
        direccion,
        en_ejes_locales
    };
    estructura_.cargas_lineales.push_back(std::move(clin));
    return assigned_id;
}

std::uint32_t Obra::add_carga_puntual_nudo(
    std::uint32_t nudo_id,
    std::uint32_t hipotesis_id,
    const core::Vec3d& fuerzas_kn,
    const core::Vec3d& momentos_knm
) {
    const std::uint32_t assigned_id = estructura_.next_carga_nudo_id++;
    CargaPuntualNudo cnud{
        assigned_id,
        nudo_id,
        hipotesis_id,
        fuerzas_kn,
        momentos_knm
    };
    estructura_.cargas_nudos.push_back(std::move(cnud));
    return assigned_id;
}

core::BBox3d Obra::bounding_box() const noexcept {
    core::BBox3d box;
    for (const auto& n : estructura_.nudos) {
        box.expand(n.pos);
    }
    for (const auto& lam : estructura_.laminas) {
        for (const auto& v : lam.vertices) {
            box.expand(v);
        }
    }
    for (const auto& pan : estructura_.panyos) {
        for (const auto& v : pan.vertices) {
            box.expand(v);
        }
    }
    return box;
}

// ============================================================================
// Fase 5 (P-06.1): Operaciones sobre Nudos (`ed3dlib.dll`, `calcesf.dll`)
// ============================================================================

bool Obra::delete_nudo(std::uint32_t nudo_id, bool eliminar_barras_incidentes) {
    auto it = std::find_if(estructura_.nudos.begin(), estructura_.nudos.end(),
                           [nudo_id](const Nudo3d& n) { return n.id == nudo_id; });
    if (it == estructura_.nudos.end()) {
        return false;
    }

    const Nudo3d saved_nudo = *it;
    std::vector<Barra3d> removed_bars;
    if (eliminar_barras_incidentes) {
        for (const auto& b : estructura_.barras) {
            if (b.nudo_inicial_id == nudo_id || b.nudo_final_id == nudo_id) {
                removed_bars.push_back(b);
            }
        }
    }

    undo_redo_.record_action(
        "Borrar nudo N" + std::to_string(nudo_id),
        [this, nudo_id, eliminar_barras_incidentes]() {
            if (eliminar_barras_incidentes) {
                auto& bvec = estructura_.barras;
                bvec.erase(std::remove_if(bvec.begin(), bvec.end(),
                                          [nudo_id](const Barra3d& b) {
                                              return b.nudo_inicial_id == nudo_id || b.nudo_final_id == nudo_id;
                                          }),
                           bvec.end());
            }
            auto& nvec = estructura_.nudos;
            nvec.erase(std::remove_if(nvec.begin(), nvec.end(),
                                      [nudo_id](const Nudo3d& n) { return n.id == nudo_id; }),
                       nvec.end());
        },
        [this, saved_nudo, removed_bars]() {
            estructura_.nudos.push_back(saved_nudo);
            for (const auto& b : removed_bars) {
                estructura_.barras.push_back(b);
            }
        },
        true
    );
    return true;
}

bool Obra::vincular_empatar_nudos(std::uint32_t nudo_destino_id, std::uint32_t nudo_absorbido_id) {
    if (nudo_destino_id == nudo_absorbido_id) return false;
    const Nudo3d* dst = find_nudo(nudo_destino_id);
    const Nudo3d* abs = find_nudo(nudo_absorbido_id);
    if (!dst || !abs) return false;

    const auto saved_nudos = estructura_.nudos;
    const auto saved_barras = estructura_.barras;

    undo_redo_.record_action(
        "Vincular nudos N" + std::to_string(nudo_destino_id) + " <- N" + std::to_string(nudo_absorbido_id),
        [this, nudo_destino_id, nudo_absorbido_id]() {
            for (auto& b : estructura_.barras) {
                if (b.nudo_inicial_id == nudo_absorbido_id) b.nudo_inicial_id = nudo_destino_id;
                if (b.nudo_final_id == nudo_absorbido_id) b.nudo_final_id = nudo_destino_id;
            }
            // Eliminar posibles barras degeneradas (nudo_ini == nudo_fin)
            estructura_.barras.erase(
                std::remove_if(estructura_.barras.begin(), estructura_.barras.end(),
                               [](const Barra3d& b) { return b.nudo_inicial_id == b.nudo_final_id; }),
                estructura_.barras.end());

            estructura_.nudos.erase(
                std::remove_if(estructura_.nudos.begin(), estructura_.nudos.end(),
                               [nudo_absorbido_id](const Nudo3d& n) { return n.id == nudo_absorbido_id; }),
                estructura_.nudos.end());
        },
        [this, saved_nudos, saved_barras]() {
            estructura_.nudos = saved_nudos;
            estructura_.barras = saved_barras;
        },
        true
    );
    return true;
}

std::uint32_t Obra::desempatar_nudo(std::uint32_t nudo_id, std::uint32_t barra_id) {
    const Nudo3d* orig = find_nudo(nudo_id);
    Barra3d* b = find_barra_mut(barra_id);
    if (!orig || !b) return 0;
    if (b->nudo_inicial_id != nudo_id && b->nudo_final_id != nudo_id) return 0;

    const core::Vec3d pos_copy = orig->pos;
    const VinculacionExterior vinc_copy = orig->vinc_ext;
    const std::uint32_t new_nudo_id = add_nudo(pos_copy, vinc_copy);

    b = find_barra_mut(barra_id);
    if (b->nudo_inicial_id == nudo_id) {
        b->nudo_inicial_id = new_nudo_id;
    } else {
        b->nudo_final_id = new_nudo_id;
    }
    return new_nudo_id;
}

bool Obra::set_vinculacion_exterior_nudo(
    std::uint32_t nudo_id,
    VinculacionExterior tipo,
    const DatosVinculacionExteriorNudo& datos
) {
    Nudo3d* n = find_nudo_mut(nudo_id);
    if (!n) return false;
    n->vinc_ext = tipo;
    n->vinc_ext_datos = datos;
    return true;
}

bool Obra::set_vinculacion_interior_nudo(
    std::uint32_t nudo_id,
    TipoVinculacionInterior tipo,
    double grado_empotramiento
) {
    Nudo3d* n = find_nudo_mut(nudo_id);
    if (!n) return false;
    n->tipo_vinc_int = tipo;
    if (tipo == TipoVinculacionInterior::Empotrado) {
        n->empotrado_interior = true;
        n->grado_empotramiento_interior = 1.0;
    } else if (tipo == TipoVinculacionInterior::Articulado) {
        n->empotrado_interior = false;
        n->grado_empotramiento_interior = 0.0;
    } else {
        n->empotrado_interior = (grado_empotramiento >= 0.5);
        n->grado_empotramiento_interior = std::clamp(grado_empotramiento, 0.0, 1.0);
    }
    return true;
}

std::uint32_t Obra::add_ligadura_nudos(
    std::string_view nombre,
    const std::vector<std::uint32_t>& nudo_ids,
    bool ligar_dx,
    bool ligar_dy,
    bool ligar_dz
) {
    const std::uint32_t id = estructura_.next_ligadura_id++;
    LigaduraDesplazamientoNudos lig{
        id,
        nombre.empty() ? ("Ligadura " + std::to_string(id)) : std::string(nombre),
        nudo_ids,
        ligar_dx,
        ligar_dy,
        ligar_dz
    };
    estructura_.ligaduras.push_back(std::move(lig));
    return id;
}

std::size_t Obra::generar_diafragmas_por_niveles(double tolerancia_z_m) {
    estructura_.diafragmas.clear();
    for (auto& n : estructura_.nudos) {
        n.diafragma_id = 0;
    }

    std::size_t count = 0;
    for (const auto& niv : estructura_.niveles) {
        if (std::abs(niv.cota_z_m) < 1e-4) {
            continue; // La cota de cimentación Z=0 no lleva diafragma de piso por defecto
        }
        std::vector<std::uint32_t> nudos_en_planta;
        for (const auto& n : estructura_.nudos) {
            if (std::abs(n.pos.z - niv.cota_z_m) <= tolerancia_z_m) {
                nudos_en_planta.push_back(n.id);
            }
        }
        if (nudos_en_planta.size() >= 2) {
            const std::uint32_t did = estructura_.next_diafragma_id++;
            for (const auto nid : nudos_en_planta) {
                if (auto* nm = find_nudo_mut(nid)) {
                    nm->diafragma_id = did;
                }
            }
            estructura_.diafragmas.push_back(DiafragmaRigidoPlanta{
                did,
                "Diafragma " + niv.nombre,
                niv.cota_z_m,
                std::move(nudos_en_planta),
                true
            });
            ++count;
        }
    }
    return count;
}

void Obra::reiniciar_numeracion_nudos_y_barras() {
    std::uint32_t nid = 1;
    for (auto& n : estructura_.nudos) {
        n.referencia = "N" + std::to_string(nid++);
    }
}

// ============================================================================
// Fase 5 (P-06.2): Operaciones sobre Barras, Piezas y Agrupaciones
// ============================================================================

bool Obra::delete_barra(std::uint32_t barra_id) {
    auto it = std::find_if(estructura_.barras.begin(), estructura_.barras.end(),
                           [barra_id](const Barra3d& b) { return b.id == barra_id; });
    if (it == estructura_.barras.end()) return false;

    const Barra3d saved_barra = *it;
    undo_redo_.record_action(
        "Borrar barra B" + std::to_string(barra_id),
        [this, barra_id]() {
            auto& bvec = estructura_.barras;
            bvec.erase(std::remove_if(bvec.begin(), bvec.end(),
                                      [barra_id](const Barra3d& b) { return b.id == barra_id; }),
                       bvec.end());
        },
        [this, saved_barra]() {
            estructura_.barras.push_back(saved_barra);
        },
        true
    );
    return true;
}

bool Obra::move_extremo_barra(std::uint32_t barra_id, bool extremo_inicial, std::uint32_t nuevo_nudo_id) {
    if (!find_nudo(nuevo_nudo_id)) return false;
    Barra3d* b = find_barra_mut(barra_id);
    if (!b) return false;

    const std::uint32_t otro_nudo = extremo_inicial ? b->nudo_final_id : b->nudo_inicial_id;
    if (otro_nudo == nuevo_nudo_id) return false;

    if (extremo_inicial) {
        b->nudo_inicial_id = nuevo_nudo_id;
    } else {
        b->nudo_final_id = nuevo_nudo_id;
    }
    return true;
}

bool Obra::invertir_sentido_eje_x_barra(std::uint32_t barra_id) {
    Barra3d* b = find_barra_mut(barra_id);
    if (!b) return false;
    std::swap(b->nudo_inicial_id, b->nudo_final_id);
    std::swap(b->disposicion.desp_y_ini_m, b->disposicion.desp_y_fin_m);
    std::swap(b->disposicion.desp_z_ini_m, b->disposicion.desp_z_fin_m);
    std::swap(b->empotramiento.emp_xy_ini, b->empotramiento.emp_xy_fin);
    std::swap(b->empotramiento.emp_xz_ini, b->empotramiento.emp_xz_fin);
    return true;
}

double Obra::longitud_barra_m(std::uint32_t barra_id) const noexcept {
    const Barra3d* b = find_barra(barra_id);
    if (!b) return 0.0;
    const Nudo3d* n1 = find_nudo(b->nudo_inicial_id);
    const Nudo3d* n2 = find_nudo(b->nudo_final_id);
    if (!n1 || !n2) return 0.0;
    return n1->pos.distance_to(n2->pos);
}

namespace {
bool segment_segment_intersection_3d(
    const core::Vec3d& p1, const core::Vec3d& p2,
    const core::Vec3d& p3, const core::Vec3d& p4,
    double tol_m,
    core::Vec3d& out_point
) noexcept {
    const core::Vec3d d1 = p2 - p1;
    const core::Vec3d d2 = p4 - p3;
    const core::Vec3d r = p1 - p3;
    const double a = d1.dot(d1);
    const double e = d2.dot(d2);
    if (a < 1e-9 || e < 1e-9) return false;

    const double f = d2.dot(r);
    const double c = d1.dot(r);
    const double b = d1.dot(d2);
    const double denom = a * e - b * b;

    if (std::abs(denom) < 1e-9) return false; // Segmentos paralelos o colineales

    const double s = (b * f - c * e) / denom;
    const double t = (a * f - b * c) / denom;

    // Intersección estrictamente interior en al menos uno de los dos segmentos
    if (s < 0.02 || s > 0.98 || t < 0.02 || t > 0.98) {
        return false;
    }

    const core::Vec3d c1 = p1 + d1 * s;
    const core::Vec3d c2 = p3 + d2 * t;
    if (c1.distance_to(c2) > tol_m) {
        return false;
    }
    out_point = (c1 + c2) * 0.5;
    return true;
}
} // namespace

std::size_t Obra::generar_nudos_y_dividir_barras_en_cruces(double tolerancia_m) {
    std::size_t intersections_created = 0;
    bool found_any = true;
    int max_passes = 32;

    while (found_any && max_passes-- > 0) {
        found_any = false;
        const std::size_t n_bars = estructura_.barras.size();
        for (std::size_t i = 0; i < n_bars && !found_any; ++i) {
            for (std::size_t j = i + 1; j < n_bars && !found_any; ++j) {
                const Barra3d b1 = estructura_.barras[i];
                const Barra3d b2 = estructura_.barras[j];
                if (b1.nudo_inicial_id == b2.nudo_inicial_id ||
                    b1.nudo_inicial_id == b2.nudo_final_id ||
                    b1.nudo_final_id == b2.nudo_inicial_id ||
                    b1.nudo_final_id == b2.nudo_final_id) {
                    continue;
                }

                const Nudo3d* n1a = find_nudo(b1.nudo_inicial_id);
                const Nudo3d* n1b = find_nudo(b1.nudo_final_id);
                const Nudo3d* n2a = find_nudo(b2.nudo_inicial_id);
                const Nudo3d* n2b = find_nudo(b2.nudo_final_id);
                if (!n1a || !n1b || !n2a || !n2b) continue;

                core::Vec3d pt_inter;
                if (segment_segment_intersection_3d(n1a->pos, n1b->pos, n2a->pos, n2b->pos, tolerancia_m, pt_inter)) {
                    const std::uint32_t nid_inter = add_nudo(pt_inter, VinculacionExterior::Libre);

                    // Subdividir b1 (n1a -> nid_inter y nid_inter -> n1b) y b2 (n2a -> nid_inter y nid_inter -> n2b)
                    estructura_.barras[i].nudo_final_id = nid_inter;
                    estructura_.barras[j].nudo_final_id = nid_inter;

                    const std::uint32_t new_b1 = add_barra(nid_inter, b1.nudo_final_id, b1.perfil, b1.material);
                    if (auto* nb1 = find_barra_mut(new_b1)) {
                        nb1->angulo_giro_rad = b1.angulo_giro_rad;
                        nb1->es_tirante = b1.es_tirante;
                        nb1->capa_id = b1.capa_id;
                        nb1->tipo_estructural = b1.tipo_estructural;
                    }

                    const std::uint32_t new_b2 = add_barra(nid_inter, b2.nudo_final_id, b2.perfil, b2.material);
                    if (auto* nb2 = find_barra_mut(new_b2)) {
                        nb2->angulo_giro_rad = b2.angulo_giro_rad;
                        nb2->es_tirante = b2.es_tirante;
                        nb2->capa_id = b2.capa_id;
                        nb2->tipo_estructural = b2.tipo_estructural;
                    }

                    ++intersections_created;
                    found_any = true;
                }
            }
        }
    }
    return intersections_created;
}

std::uint32_t Obra::crear_pieza(const std::vector<std::uint32_t>& barra_ids, std::string_view nombre) {
    if (barra_ids.empty()) return 0;

    double len_total = 0.0;
    std::uint32_t n_ini = 0;
    std::uint32_t n_fin = 0;

    for (std::size_t idx = 0; idx < barra_ids.size(); ++idx) {
        const Barra3d* b = find_barra(barra_ids[idx]);
        if (!b) return 0;
        if (idx == 0) n_ini = b->nudo_inicial_id;
        if (idx + 1 == barra_ids.size()) n_fin = b->nudo_final_id;
        len_total += longitud_barra_m(b->id);
    }

    const std::uint32_t pid = estructura_.next_pieza_id++;
    for (const auto bid : barra_ids) {
        if (auto* bm = find_barra_mut(bid)) {
            bm->pieza_id = pid;
        }
    }

    estructura_.piezas.push_back(PiezaEstructural{
        pid,
        nombre.empty() ? ("Pieza " + std::to_string(pid)) : std::string(nombre),
        barra_ids,
        n_ini,
        n_fin,
        len_total
    });
    return pid;
}

bool Obra::dividir_pieza(std::uint32_t pieza_id) {
    auto it = std::find_if(estructura_.piezas.begin(), estructura_.piezas.end(),
                           [pieza_id](const PiezaEstructural& p) { return p.id == pieza_id; });
    if (it == estructura_.piezas.end()) return false;

    for (const auto bid : it->barra_ids) {
        if (auto* bm = find_barra_mut(bid)) {
            bm->pieza_id = 0;
        }
    }
    estructura_.piezas.erase(it);
    return true;
}

std::uint32_t Obra::agrupar_barras(const std::vector<std::uint32_t>& barra_ids, std::string_view nombre) {
    if (barra_ids.empty()) return 0;
    const Barra3d* b0 = find_barra(barra_ids.front());
    if (!b0) return 0;

    const std::uint32_t aid = estructura_.next_agrupacion_id++;
    const std::string perf = b0->perfil;
    const std::string mat = b0->material;

    for (const auto bid : barra_ids) {
        if (auto* bm = find_barra_mut(bid)) {
            bm->agrupacion_id = aid;
            bm->perfil = perf;
            bm->material = mat;
        }
    }

    estructura_.agrupaciones.push_back(AgrupacionBarras{
        aid,
        nombre.empty() ? ("Agrupación " + std::to_string(aid)) : std::string(nombre),
        barra_ids,
        perf,
        mat
    });
    return aid;
}

bool Obra::desagrupar_barras(std::uint32_t agrupacion_id) {
    auto it = std::find_if(estructura_.agrupaciones.begin(), estructura_.agrupaciones.end(),
                           [agrupacion_id](const AgrupacionBarras& a) { return a.id == agrupacion_id; });
    if (it == estructura_.agrupaciones.end()) return false;

    for (const auto bid : it->barra_ids) {
        if (auto* bm = find_barra_mut(bid)) {
            bm->agrupacion_id = 0;
        }
    }
    estructura_.agrupaciones.erase(it);
    return true;
}

// ============================================================================
// Fase 5 (P-06.3): Grupos de Flecha, Tirantes en X, Pandeo y Rótulas Plásticas
// ============================================================================

std::uint32_t Obra::crear_grupo_flecha(
    const std::vector<std::uint32_t>& barra_ids,
    TipoMedicionFlecha tipo
) {
    if (barra_ids.empty()) return 0;
    double len_ref = 0.0;
    std::uint32_t na = 0;
    std::uint32_t nb = 0;

    for (std::size_t i = 0; i < barra_ids.size(); ++i) {
        const Barra3d* b = find_barra(barra_ids[i]);
        if (!b) return 0;
        if (i == 0) na = b->nudo_inicial_id;
        if (i + 1 == barra_ids.size()) nb = b->nudo_final_id;
        len_ref += longitud_barra_m(b->id);
    }

    const std::uint32_t gfid = estructura_.next_grupo_flecha_id++;
    for (const auto bid : barra_ids) {
        if (auto* bm = find_barra_mut(bid)) {
            bm->flecha.grupo_flecha_id = gfid;
            bm->flecha.tipo_medicion = tipo;
        }
    }

    estructura_.grupos_flecha.push_back(GrupoFlecha{
        gfid,
        "Grupo de flecha " + std::to_string(gfid),
        barra_ids,
        na,
        nb,
        len_ref,
        tipo
    });
    return gfid;
}

CuadroArriostramientoTirantes Obra::emparejar_tirantes_recuadro(
    std::uint32_t barra_diag1_id,
    std::uint32_t barra_diag2_id,
    double tol_coplanar_m
) {
    CuadroArriostramientoTirantes cuadro{};
    cuadro.id = estructura_.next_cuadro_tirantes_id++;
    cuadro.barra_diagonal_1_id = barra_diag1_id;
    cuadro.barra_diagonal_2_id = barra_diag2_id;

    Barra3d* b1 = find_barra_mut(barra_diag1_id);
    Barra3d* b2 = find_barra_mut(barra_diag2_id);
    if (!b1 || !b2 || barra_diag1_id == barra_diag2_id) {
        cuadro.geometricamente_valido = false;
        cuadro.mensaje_validacion = "Barras diagonales no encontradas o idénticas.";
        return cuadro;
    }

    const Nudo3d* n1a = find_nudo(b1->nudo_inicial_id);
    const Nudo3d* n1b = find_nudo(b1->nudo_final_id);
    const Nudo3d* n2a = find_nudo(b2->nudo_inicial_id);
    const Nudo3d* n2b = find_nudo(b2->nudo_final_id);
    if (!n1a || !n1b || !n2a || !n2b) {
        cuadro.geometricamente_valido = false;
        cuadro.mensaje_validacion = "Nudos extremos de las diagonales no válidos.";
        return cuadro;
    }

    cuadro.nudos_recuadro = {n1a->id, n1b->id, n2a->id, n2b->id};

    // Validación de coplanaridad de los 4 vértices del recuadro (`esp3dcmb_son_tirantes_geometricamente_compatibles_para_formar_cuadro_arriostramiento`)
    const core::Vec3d v1 = n1b->pos - n1a->pos;
    const core::Vec3d v2 = n2b->pos - n2a->pos;
    const core::Vec3d normal = v1.cross(v2);
    const double norm_len = normal.norm();
    if (norm_len < 1e-6) {
        cuadro.geometricamente_valido = false;
        cuadro.mensaje_validacion = "Las diagonales son paralelas; no forman una cruz de San Andrés.";
        return cuadro;
    }

    const core::Vec3d unit_normal = normal * (1.0 / norm_len);
    const double dist_coplanar = std::abs((n2a->pos - n1a->pos).dot(unit_normal));
    if (dist_coplanar > tol_coplanar_m) {
        cuadro.geometricamente_valido = false;
        cuadro.mensaje_validacion = "Los 4 nudos de los tirantes no son coplanares dentro de la tolerancia.";
        return cuadro;
    }

    const double cos_ang = std::clamp(std::abs(v1.normalized().dot(v2.normalized())), 0.0, 1.0);
    cuadro.angulo_diagonal_deg = 0.5 * std::acos(cos_ang) * (180.0 / std::numbers::pi);
    cuadro.geometricamente_valido = true;
    cuadro.mensaje_validacion = "Recuadro arriostrado en X geométricamente compatible.";

    b1->es_tirante = true;
    b1->tipo_estructural = TipoBarraEstructural::Tirante;
    b1->empotramiento = EmpotramientoExtremosBarra::biarticulado();

    b2->es_tirante = true;
    b2->tipo_estructural = TipoBarraEstructural::Tirante;
    b2->empotramiento = EmpotramientoExtremosBarra::biarticulado();

    estructura_.cuadros_tirantes.push_back(cuadro);
    return cuadro;
}

std::size_t Obra::calcular_pandeo_automatico_estructura(
    bool estructura_traslacional_xy,
    bool estructura_traslacional_xz
) {
    std::size_t updated = 0;

    for (auto& b : estructura_.barras) {
        if (b.es_tirante) {
            b.pandeo.modo_xy = ModoCalculoPandeo::SinPandeo;
            b.pandeo.modo_xz = ModoCalculoPandeo::SinPandeo;
            b.pandeo.beta_xy = 0.0;
            b.pandeo.beta_xz = 0.0;
            continue;
        }

        const Nudo3d* na = find_nudo(b.nudo_inicial_id);
        const Nudo3d* nb = find_nudo(b.nudo_final_id);
        if (!na || !nb) continue;

        auto node_flexibility = [&](const Nudo3d* n, double emp_barra) noexcept -> double {
            if (n->vinc_ext == VinculacionExterior::Empotrado) return 0.05;
            if (n->vinc_ext == VinculacionExterior::ArticuladoFijo) return 0.95;
            if (!n->empotrado_interior || emp_barra < 0.05) return 0.95;
            return std::clamp(1.0 - 0.70 * emp_barra * n->grado_empotramiento_interior, 0.10, 0.95);
        };

        const double ga_xy = node_flexibility(na, b.empotramiento.emp_xy_ini);
        const double gb_xy = node_flexibility(nb, b.empotramiento.emp_xy_fin);
        const double ga_xz = node_flexibility(na, b.empotramiento.emp_xz_ini);
        const double gb_xz = node_flexibility(nb, b.empotramiento.emp_xz_fin);

        b.pandeo.modo_xy = estructura_traslacional_xy
            ? ModoCalculoPandeo::AutoTraslacional
            : ModoCalculoPandeo::AutoIntraslacional;
        b.pandeo.beta_xy = estructura_traslacional_xy
            ? DatosPandeoBarra::vpandeo_beta_traslacional(ga_xy, gb_xy)
            : DatosPandeoBarra::vpandeo_beta_intraslacional(ga_xy, gb_xy);

        b.pandeo.modo_xz = estructura_traslacional_xz
            ? ModoCalculoPandeo::AutoTraslacional
            : ModoCalculoPandeo::AutoIntraslacional;
        b.pandeo.beta_xz = estructura_traslacional_xz
            ? DatosPandeoBarra::vpandeo_beta_traslacional(ga_xz, gb_xz)
            : DatosPandeoBarra::vpandeo_beta_intraslacional(ga_xz, gb_xz);

        ++updated;
    }
    return updated;
}

std::uint32_t Obra::add_definicion_rotula_plastica(const DefinicionRotulaPlastica& def) {
    DefinicionRotulaPlastica copy = def;
    copy.id = estructura_.next_rotula_id++;
    estructura_.catalogo_rotulas_plasticas.push_back(copy);
    return copy.id;
}

// ============================================================================
// Fase 5 (P-06.4): Pilares y Vigas de Hormigón Armado / Mixtos
// ============================================================================

std::uint32_t Obra::add_pilar_estructural(const PilarEstructural& pilar, bool generar_barras_3d) {
    PilarEstructural copy = pilar;
    copy.id = estructura_.next_pilar_id++;
    if (copy.referencia.empty()) {
        copy.referencia = "P" + std::to_string(copy.id);
    }

    if (generar_barras_3d && !copy.tramos.empty()) {
        std::uint32_t prev_nudo_id = 0;
        for (std::size_t idx = 0; idx < copy.tramos.size(); ++idx) {
            auto& tramo = copy.tramos[idx];
            if (idx == 0) {
                const VinculacionExterior v_arranque = copy.con_vinculacion_exterior
                    ? VinculacionExterior::Empotrado
                    : VinculacionExterior::Libre;
                prev_nudo_id = add_nudo({copy.pos_planta_xy.x, copy.pos_planta_xy.y, tramo.cota_inf_m}, v_arranque);
            }
            const std::uint32_t top_nudo_id = add_nudo(
                {copy.pos_planta_xy.x, copy.pos_planta_xy.y, tramo.cota_sup_m},
                VinculacionExterior::Libre
            );

            const std::string desc_perfil = (tramo.tipo_seccion == TipoSeccionPilar::HormigonRectangular)
                ? ("HA " + std::to_string(static_cast<int>(tramo.ancho_x_m * 100.0)) + "x" +
                           std::to_string(static_cast<int>(tramo.ancho_y_m * 100.0)))
                : tramo.perfil_metalico;

            tramo.barra_3d_id = add_barra(prev_nudo_id, top_nudo_id, desc_perfil, tramo.hormigon);
            if (auto* bm = find_barra_mut(tramo.barra_3d_id)) {
                bm->tipo_estructural = TipoBarraEstructural::ColumnaPilar;
                bm->pandeo.beta_xy = tramo.beta_pandeo_x;
                bm->pandeo.beta_xz = tramo.beta_pandeo_y;
                bm->empotramiento.emp_xy_ini = tramo.coef_empotramiento_pie;
                bm->empotramiento.emp_xz_ini = tramo.coef_empotramiento_pie;
                bm->empotramiento.emp_xy_fin = tramo.coef_empotramiento_cabeza;
                bm->empotramiento.emp_xz_fin = tramo.coef_empotramiento_cabeza;
            }
            prev_nudo_id = top_nudo_id;
        }
    }

    estructura_.pilares.push_back(std::move(copy));
    return estructura_.pilares.back().id;
}

std::uint32_t Obra::add_portico_vigas(const PorticoVigasEstructural& portico) {
    PorticoVigasEstructural copy = portico;
    copy.id = estructura_.next_portico_id++;
    if (copy.referencia.empty()) {
        copy.referencia = "Pórtico " + std::to_string(copy.id);
    }
    estructura_.porticos_vigas.push_back(std::move(copy));
    return estructura_.porticos_vigas.back().id;
}

// ============================================================================
// Fase 6 (P-07.1): Subsistema de Láminas MEF Shell (`calcmef`, `calcmallad3d`, `visesf3d`)
// ============================================================================

const Lamina3d* Obra::find_lamina(std::uint32_t id) const noexcept {
    for (const auto& lam : estructura_.laminas) {
        if (lam.id == id) return &lam;
    }
    return nullptr;
}

Lamina3d* Obra::find_lamina_mut(std::uint32_t id) noexcept {
    for (auto& lam : estructura_.laminas) {
        if (lam.id == id) return &lam;
    }
    return nullptr;
}

bool Obra::delete_lamina(std::uint32_t lamina_id) {
    auto it = std::find_if(estructura_.laminas.begin(), estructura_.laminas.end(),
                           [lamina_id](const Lamina3d& l) { return l.id == lamina_id; });
    if (it == estructura_.laminas.end()) return false;

    const Lamina3d saved = *it;
    undo_redo_.record_action(
        "Borrar lámina L" + std::to_string(lamina_id),
        [this, lamina_id]() {
            estructura_.laminas.erase(
                std::remove_if(estructura_.laminas.begin(), estructura_.laminas.end(),
                               [lamina_id](const Lamina3d& l) { return l.id == lamina_id; }),
                estructura_.laminas.end());
            estructura_.cargas_laminas.erase(
                std::remove_if(estructura_.cargas_laminas.begin(), estructura_.cargas_laminas.end(),
                               [lamina_id](const CargaLamina3d& cl) { return cl.lamina_id == lamina_id; }),
                estructura_.cargas_laminas.end());
        },
        [this, saved]() {
            estructura_.laminas.push_back(saved);
        },
        true
    );
    return true;
}

bool Obra::move_lamina(std::uint32_t lamina_id, const core::Vec3d& delta) {
    Lamina3d* lam = find_lamina_mut(lamina_id);
    if (!lam) return false;
    for (auto& v : lam->vertices) {
        v = v + delta;
    }
    for (auto& h : lam->huecos) {
        for (auto& hv : h.vertices) {
            hv = hv + delta;
        }
    }
    return true;
}

std::uint32_t Obra::add_hueco_lamina(std::uint32_t lamina_id, const std::vector<core::Vec3d>& vertices_hueco) {
    Lamina3d* lam = find_lamina_mut(lamina_id);
    if (!lam || vertices_hueco.size() < 3) return 0;
    const std::uint32_t hid = static_cast<std::uint32_t>(lam->huecos.size() + 1);
    lam->huecos.push_back(HuecoLamina3d{hid, vertices_hueco});
    return hid;
}

std::uint32_t Obra::add_banda_integracion_lamina(std::uint32_t lamina_id, const BandaIntegracionLamina& banda) {
    Lamina3d* lam = find_lamina_mut(lamina_id);
    if (!lam) return 0;
    BandaIntegracionLamina copy = banda;
    copy.id = static_cast<std::uint32_t>(lam->bandas_integracion.size() + 1);
    if (copy.nombre.empty()) {
        copy.nombre = "Banda " + std::to_string(copy.id);
    }
    lam->bandas_integracion.push_back(std::move(copy));
    return lam->bandas_integracion.back().id;
}

bool Obra::set_vinculacion_arista_lamina(std::uint32_t lamina_id, const VinculacionAristaLamina& vinc) {
    Lamina3d* lam = find_lamina_mut(lamina_id);
    if (!lam) return false;
    for (auto& existing : lam->vinculaciones_aristas) {
        if (existing.arista_index == vinc.arista_index) {
            existing = vinc;
            return true;
        }
    }
    lam->vinculaciones_aristas.push_back(vinc);
    return true;
}

std::uint32_t Obra::unir_laminas(std::uint32_t lamina_a_id, std::uint32_t lamina_b_id) {
    if (lamina_a_id == lamina_b_id) return 0;
    const Lamina3d* la = find_lamina(lamina_a_id);
    const Lamina3d* lb = find_lamina(lamina_b_id);
    if (!la || !lb || la->vertices.size() < 3 || lb->vertices.size() < 3) return 0;

    // Construir envolvente convexa 2D proyectada sobre el plano de la lámina A
    const auto axes = ShellElementEngine::calcular_ejes_locales_lamina(la->vertices);
    const core::Vec3d origin = la->vertices.front();

    std::vector<core::Vec3d> all_pts = la->vertices;
    for (const auto& v : lb->vertices) {
        bool dup = false;
        for (const auto& existing : all_pts) {
            if (existing.distance_to(v) < 1e-3) {
                dup = true;
                break;
            }
        }
        if (!dup) all_pts.push_back(v);
    }

    // Ordenar vértices angularmente alrededor del centroide proyectado
    core::Vec3d centroid{0.0, 0.0, 0.0};
    for (const auto& p : all_pts) centroid = centroid + p;
    centroid = centroid * (1.0 / static_cast<double>(all_pts.size()));

    std::sort(all_pts.begin(), all_pts.end(), [&](const core::Vec3d& p1, const core::Vec3d& p2) {
        const core::Vec3d d1 = p1 - centroid;
        const core::Vec3d d2 = p2 - centroid;
        const double a1 = std::atan2(d1.dot(axes.v), d1.dot(axes.u));
        const double a2 = std::atan2(d2.dot(axes.v), d2.dot(axes.u));
        return a1 < a2;
    });
    (void)origin;

    const double espesor = la->espesor_m;
    const std::string mat = la->material;
    const std::uint32_t capa = la->capa_id;
    delete_lamina(lamina_a_id);
    delete_lamina(lamina_b_id);
    return add_lamina(all_pts, espesor, mat, capa);
}

std::pair<std::uint32_t, std::uint32_t> Obra::dividir_lamina(
    std::uint32_t lamina_id,
    std::size_t idx_vertice_a,
    std::size_t idx_vertice_b
) {
    const Lamina3d* lam = find_lamina(lamina_id);
    if (!lam || lam->vertices.size() < 4) return {0, 0};
    if (idx_vertice_a >= lam->vertices.size() || idx_vertice_b >= lam->vertices.size()) return {0, 0};
    if (idx_vertice_a > idx_vertice_b) std::swap(idx_vertice_a, idx_vertice_b);
    if (idx_vertice_b - idx_vertice_a < 2) return {0, 0};

    std::vector<core::Vec3d> poly1;
    for (std::size_t i = idx_vertice_a; i <= idx_vertice_b; ++i) {
        poly1.push_back(lam->vertices[i]);
    }

    std::vector<core::Vec3d> poly2;
    for (std::size_t i = idx_vertice_b; i < lam->vertices.size(); ++i) {
        poly2.push_back(lam->vertices[i]);
    }
    for (std::size_t i = 0; i <= idx_vertice_a; ++i) {
        poly2.push_back(lam->vertices[i]);
    }

    const double esp = lam->espesor_m;
    const std::string mat = lam->material;
    const std::uint32_t capa = lam->capa_id;
    delete_lamina(lamina_id);
    const auto id1 = add_lamina(poly1, esp, mat, capa);
    const auto id2 = add_lamina(poly2, esp, mat, capa);
    return {id1, id2};
}

std::size_t Obra::discretizar_todas_las_laminas(double tamano_defecto_m) {
    std::size_t total_triangulos = 0;
    for (auto& lam : estructura_.laminas) {
        const double h_elem = (lam.tamano_discretizacion_m > 0.05) ? lam.tamano_discretizacion_m : tamano_defecto_m;
        lam.malla = ShellElementEngine::generar_mallado_lamina(lam, h_elem);
        total_triangulos += lam.malla.triangulos.size();
    }
    return total_triangulos;
}

// ============================================================================
// Fase 6 (P-07.2): Subsistema de Paños y Reparto de Cargas (`panyos3d.dll`)
// ============================================================================

const Panyo3d* Obra::find_panyo(std::uint32_t id) const noexcept {
    for (const auto& p : estructura_.panyos) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

Panyo3d* Obra::find_panyo_mut(std::uint32_t id) noexcept {
    for (auto& p : estructura_.panyos) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

bool Obra::delete_panyo(std::uint32_t panyo_id) {
    auto it = std::find_if(estructura_.panyos.begin(), estructura_.panyos.end(),
                           [panyo_id](const Panyo3d& p) { return p.id == panyo_id; });
    if (it == estructura_.panyos.end()) return false;

    estructura_.panyos.erase(it);
    estructura_.cargas_superficiales.erase(
        std::remove_if(estructura_.cargas_superficiales.begin(), estructura_.cargas_superficiales.end(),
                       [panyo_id](const CargaSuperficialPanyo& c) { return c.panyo_id == panyo_id; }),
        estructura_.cargas_superficiales.end());
    estructura_.cargas_lineales.erase(
        std::remove_if(estructura_.cargas_lineales.begin(), estructura_.cargas_lineales.end(),
                       [panyo_id](const CargaLinealBarra& cl) {
                           return cl.generada_por_panyo && cl.panyo_origen_id == panyo_id;
                       }),
        estructura_.cargas_lineales.end());
    return true;
}

bool Obra::move_panyo(std::uint32_t panyo_id, const core::Vec3d& delta) {
    Panyo3d* p = find_panyo_mut(panyo_id);
    if (!p) return false;
    for (auto& v : p->vertices) {
        v = v + delta;
    }
    return true;
}

bool Obra::set_direccion_reparto_panyo(std::uint32_t panyo_id, const core::Vec3d& direccion) {
    Panyo3d* p = find_panyo_mut(panyo_id);
    if (!p || direccion.norm_sq() < 1e-12) return false;
    p->direccion_reparto = direccion.normalized();
    return true;
}

std::pair<std::uint32_t, std::uint32_t> Obra::dividir_panyo(
    std::uint32_t panyo_id,
    std::size_t idx_vertice_a,
    std::size_t idx_vertice_b
) {
    const Panyo3d* p = find_panyo(panyo_id);
    if (!p || p->vertices.size() < 4) return {0, 0};
    if (idx_vertice_a >= p->vertices.size() || idx_vertice_b >= p->vertices.size()) return {0, 0};
    if (idx_vertice_a > idx_vertice_b) std::swap(idx_vertice_a, idx_vertice_b);
    if (idx_vertice_b - idx_vertice_a < 2) return {0, 0};

    std::vector<core::Vec3d> poly1;
    for (std::size_t i = idx_vertice_a; i <= idx_vertice_b; ++i) {
        poly1.push_back(p->vertices[i]);
    }
    std::vector<core::Vec3d> poly2;
    for (std::size_t i = idx_vertice_b; i < p->vertices.size(); ++i) {
        poly2.push_back(p->vertices[i]);
    }
    for (std::size_t i = 0; i <= idx_vertice_a; ++i) {
        poly2.push_back(p->vertices[i]);
    }

    const core::Vec3d dir = p->direccion_reparto;
    const std::uint32_t capa = p->capa_id;
    delete_panyo(panyo_id);
    const auto id1 = add_panyo(poly1, dir, capa);
    const auto id2 = add_panyo(poly2, dir, capa);
    return {id1, id2};
}

std::size_t Obra::actualizar_reparto_cargas_panyos_a_barras() {
    // Limpiar cargas lineales previamente generadas automáticamente por paños
    estructura_.cargas_lineales.erase(
        std::remove_if(estructura_.cargas_lineales.begin(), estructura_.cargas_lineales.end(),
                       [](const CargaLinealBarra& cl) { return cl.generada_por_panyo; }),
        estructura_.cargas_lineales.end());

    std::size_t generadas_count = 0;
    for (const auto& panyo : estructura_.panyos) {
        const auto nuevas = PanelLoadDistributionEngine::descomponer_cargas_panyo_en_barras(
            panyo, estructura_.cargas_superficiales, estructura_.barras, estructura_.nudos);
        for (auto cl : nuevas) {
            cl.id = estructura_.next_carga_lin_id++;
            estructura_.cargas_lineales.push_back(std::move(cl));
            ++generadas_count;
        }
    }
    return generadas_count;
}

// ============================================================================
// Fase 7 (P-08.1): Gestión de Cargas en Nudos, Barras, Láminas y Paños (`ed3dlib.dll`, `calcesf.dll`)
// ============================================================================

bool Obra::editar_carga_nudo(std::uint32_t carga_id, const core::Vec3d& fuerzas_kn, const core::Vec3d& momentos_knm) {
    for (auto& cn : estructura_.cargas_nudos) {
        if (cn.id == carga_id) {
            cn.fuerzas_kn = fuerzas_kn;
            cn.momentos_knm = momentos_knm;
            return true;
        }
    }
    return false;
}

bool Obra::delete_carga_nudo(std::uint32_t carga_id) {
    auto it = std::find_if(estructura_.cargas_nudos.begin(), estructura_.cargas_nudos.end(),
                           [carga_id](const CargaPuntualNudo& c) { return c.id == carga_id; });
    if (it == estructura_.cargas_nudos.end()) return false;
    estructura_.cargas_nudos.erase(it);
    return true;
}

bool Obra::cambiar_hipotesis_carga_nudo(std::uint32_t carga_id, std::uint32_t nueva_hipotesis_id) {
    for (auto& cn : estructura_.cargas_nudos) {
        if (cn.id == carga_id) {
            cn.hipotesis_id = nueva_hipotesis_id;
            return true;
        }
    }
    return false;
}

std::uint32_t Obra::add_carga_especial_barra(const CargaLinealBarra& carga) {
    CargaLinealBarra copy = carga;
    copy.id = estructura_.next_carga_lin_id++;
    estructura_.cargas_lineales.push_back(std::move(copy));
    return estructura_.cargas_lineales.back().id;
}

bool Obra::delete_carga_barra(std::uint32_t carga_id) {
    auto it = std::find_if(estructura_.cargas_lineales.begin(), estructura_.cargas_lineales.end(),
                           [carga_id](const CargaLinealBarra& c) { return c.id == carga_id; });
    if (it == estructura_.cargas_lineales.end()) return false;
    estructura_.cargas_lineales.erase(it);
    return true;
}

bool Obra::cambiar_hipotesis_carga_barra(std::uint32_t carga_id, std::uint32_t nueva_hipotesis_id) {
    for (auto& cb : estructura_.cargas_lineales) {
        if (cb.id == carga_id) {
            cb.hipotesis_id = nueva_hipotesis_id;
            return true;
        }
    }
    return false;
}

std::uint32_t Obra::add_carga_lamina(const CargaLamina3d& carga) {
    CargaLamina3d copy = carga;
    copy.id = estructura_.next_carga_lamina_id++;
    estructura_.cargas_laminas.push_back(std::move(copy));
    return estructura_.cargas_laminas.back().id;
}

bool Obra::delete_carga_lamina(std::uint32_t carga_id) {
    auto it = std::find_if(estructura_.cargas_laminas.begin(), estructura_.cargas_laminas.end(),
                           [carga_id](const CargaLamina3d& c) { return c.id == carga_id; });
    if (it == estructura_.cargas_laminas.end()) return false;
    estructura_.cargas_laminas.erase(it);
    return true;
}

bool Obra::cambiar_hipotesis_carga_lamina(std::uint32_t carga_id, std::uint32_t nueva_hipotesis_id) {
    for (auto& cl : estructura_.cargas_laminas) {
        if (cl.id == carga_id) {
            cl.hipotesis_id = nueva_hipotesis_id;
            return true;
        }
    }
    return false;
}

bool Obra::delete_carga_superficial_panyo(std::uint32_t carga_id) {
    auto it = std::find_if(estructura_.cargas_superficiales.begin(), estructura_.cargas_superficiales.end(),
                           [carga_id](const CargaSuperficialPanyo& c) { return c.id == carga_id; });
    if (it == estructura_.cargas_superficiales.end()) return false;
    estructura_.cargas_superficiales.erase(it);
    return true;
}

bool Obra::cambiar_hipotesis_carga_superficial_panyo(std::uint32_t carga_id, std::uint32_t nueva_hipotesis_id) {
    for (auto& cs : estructura_.cargas_superficiales) {
        if (cs.id == carga_id) {
            cs.hipotesis_id = nueva_hipotesis_id;
            return true;
        }
    }
    return false;
}

void Obra::write_binary(io::BinaryWriter& writer) const {
    writer.write_header();

    const auto sec_dg = writer.begin_section("DATOS_GENERALES", 2);
    writer.write_string_utf8(datos_generales_.ruta_directorio);
    writer.write_string_utf8(datos_generales_.clave);
    writer.write_string_utf8(datos_generales_.descripcion);
    writer.write_u8(static_cast<std::uint8_t>(datos_generales_.sistema_unidades));
    writer.write_string_utf8(datos_generales_.normas_resumen);
    writer.write_string_utf8(datos_generales_.acero_laminado);
    writer.write_string_utf8(datos_generales_.acero_conformado);
    writer.write_string_utf8(datos_generales_.madera);
    writer.write_string_utf8(datos_generales_.aluminio);
    writer.write_string_utf8(datos_generales_.hormigon_estructural);
    writer.write_string_utf8(datos_generales_.hormigon_pilares);
    writer.write_string_utf8(datos_generales_.hormigon_vigas);
    writer.write_string_utf8(datos_generales_.hormigon_cimentacion);
    writer.write_string_utf8(datos_generales_.acero_barras);
    writer.write_string_utf8(datos_generales_.caracteristicas_arido);
    writer.write_string_utf8(datos_generales_.ambiente_vigas);
    writer.write_bool(datos_generales_.con_sismo_dinamico);
    writer.write_bool(datos_generales_.verificar_deslizamiento_zapatas);
    writer.write_f64(datos_generales_.tension_terreno_persistente_mpa);
    writer.write_f64(datos_generales_.tension_terreno_accidental_mpa);
    writer.write_f64(datos_generales_.pilotes_hormigon.excentricidad_momento_minimo_m);
    writer.write_f64(datos_generales_.pilotes_hormigon.factor_pilotes_punta);
    writer.write_f64(datos_generales_.pilotes_hormigon.coeficiente_pandeo);
    writer.end_section(sec_dg);

    const auto sec_ed3 = writer.begin_section("METAL3D_ESTRUCTURA", 1);
    writer.write_u32(estructura_.next_nudo_id);
    writer.write_u32(estructura_.next_barra_id);
    writer.write_u32(static_cast<std::uint32_t>(estructura_.nudos.size()));
    for (const auto& n : estructura_.nudos) {
        const auto blk = writer.begin_block(101, 1);
        writer.write_u32(n.id);
        writer.write_vec3d(n.pos);
        writer.write_string_utf8(n.referencia);
        writer.write_u8(static_cast<std::uint8_t>(n.vinc_ext));
        writer.write_bool(n.empotrado_interior);
        writer.end_block(blk);
    }
    writer.write_u32(static_cast<std::uint32_t>(estructura_.barras.size()));
    for (const auto& b : estructura_.barras) {
        const auto blk = writer.begin_block(102, 1);
        writer.write_u32(b.id);
        writer.write_u32(b.nudo_inicial_id);
        writer.write_u32(b.nudo_final_id);
        writer.write_string_utf8(b.perfil);
        writer.write_string_utf8(b.material);
        writer.write_f64(b.angulo_giro_rad);
        writer.write_bool(b.es_tirante);
        writer.write_u32(b.capa_id);
        writer.end_block(blk);
    }
    writer.end_section(sec_ed3);

    const auto sec_plc = writer.begin_section("ID_CIMENTACION", 1);
    writer.write_u32(cimentacion_.next_elemento_id);
    writer.write_u32(static_cast<std::uint32_t>(cimentacion_.elementos.size()));
    for (const auto& el : cimentacion_.elementos) {
        const auto blk = writer.begin_block(201, 1);
        writer.write_u32(el.id);
        writer.write_u8(static_cast<std::uint8_t>(el.tipo));
        writer.write_u32(el.nudo_arranque_id);
        writer.write_f64(el.ancho_x_m);
        writer.write_f64(el.ancho_y_m);
        writer.write_f64(el.canto_m);
        writer.end_block(blk);
    }
    writer.end_section(sec_plc);
}

bool Obra::read_binary(io::BinaryReader& reader) {
    if (!reader.read_and_verify_header()) {
        return false;
    }

    estructura_.nudos.clear();
    estructura_.barras.clear();
    cimentacion_.elementos.clear();

    while (!reader.eof()) {
        const auto sec = reader.begin_section();
        if (sec.name == "DATOS_GENERALES") {
            if (sec.version >= 2) {
                datos_generales_.ruta_directorio = reader.read_string_utf8();
            }
            datos_generales_.clave = reader.read_string_utf8();
            datos_generales_.descripcion = reader.read_string_utf8();
            datos_generales_.sistema_unidades = static_cast<units::UnitSystem>(reader.read_u8());
            datos_generales_.normas_resumen = reader.read_string_utf8();
            datos_generales_.acero_laminado = reader.read_string_utf8();
            datos_generales_.acero_conformado = reader.read_string_utf8();
            if (sec.version >= 2) {
                datos_generales_.madera = reader.read_string_utf8();
                datos_generales_.aluminio = reader.read_string_utf8();
            }
            datos_generales_.hormigon_estructural = reader.read_string_utf8();
            if (sec.version >= 2) {
                datos_generales_.hormigon_pilares = reader.read_string_utf8();
                datos_generales_.hormigon_vigas = reader.read_string_utf8();
                datos_generales_.hormigon_cimentacion = reader.read_string_utf8();
                datos_generales_.acero_barras = reader.read_string_utf8();
                datos_generales_.caracteristicas_arido = reader.read_string_utf8();
                datos_generales_.ambiente_vigas = reader.read_string_utf8();
                datos_generales_.con_sismo_dinamico = reader.read_bool();
                datos_generales_.verificar_deslizamiento_zapatas = reader.read_bool();
            }
            datos_generales_.tension_terreno_persistente_mpa = reader.read_f64();
            datos_generales_.tension_terreno_accidental_mpa = reader.read_f64();
            if (sec.version >= 2) {
                datos_generales_.pilotes_hormigon.excentricidad_momento_minimo_m = reader.read_f64();
                datos_generales_.pilotes_hormigon.factor_pilotes_punta = reader.read_f64();
                datos_generales_.pilotes_hormigon.coeficiente_pandeo = reader.read_f64();
            }
            reader.end_section(sec);
        } else if (sec.name == "METAL3D_ESTRUCTURA") {
            estructura_.next_nudo_id = reader.read_u32();
            estructura_.next_barra_id = reader.read_u32();
            const std::uint32_t num_nudos = reader.read_u32();
            estructura_.nudos.reserve(num_nudos);
            for (std::uint32_t i = 0; i < num_nudos; ++i) {
                const auto blk = reader.begin_block();
                Nudo3d n;
                n.id = reader.read_u32();
                n.pos = reader.read_vec3d();
                n.referencia = reader.read_string_utf8();
                n.vinc_ext = static_cast<VinculacionExterior>(reader.read_u8());
                n.empotrado_interior = reader.read_bool();
                reader.end_block(blk);
                estructura_.nudos.push_back(std::move(n));
            }
            const std::uint32_t num_barras = reader.read_u32();
            estructura_.barras.reserve(num_barras);
            for (std::uint32_t i = 0; i < num_barras; ++i) {
                const auto blk = reader.begin_block();
                Barra3d b;
                b.id = reader.read_u32();
                b.nudo_inicial_id = reader.read_u32();
                b.nudo_final_id = reader.read_u32();
                b.perfil = reader.read_string_utf8();
                b.material = reader.read_string_utf8();
                b.angulo_giro_rad = reader.read_f64();
                b.es_tirante = reader.read_bool();
                b.capa_id = reader.read_u32();
                reader.end_block(blk);
                estructura_.barras.push_back(std::move(b));
            }
            reader.end_section(sec);
        } else if (sec.name == "ID_CIMENTACION") {
            cimentacion_.next_elemento_id = reader.read_u32();
            const std::uint32_t num_elem = reader.read_u32();
            cimentacion_.elementos.reserve(num_elem);
            for (std::uint32_t i = 0; i < num_elem; ++i) {
                const auto blk = reader.begin_block();
                ElementoCimentacion el;
                el.id = reader.read_u32();
                el.tipo = static_cast<TipoElementoCimentacion>(reader.read_u8());
                el.nudo_arranque_id = reader.read_u32();
                el.ancho_x_m = reader.read_f64();
                el.ancho_y_m = reader.read_f64();
                el.canto_m = reader.read_f64();
                reader.end_block(blk);
                cimentacion_.elementos.push_back(el);
            }
            reader.end_section(sec);
        } else {
            reader.skip_section(sec);
        }
    }

    abierta_ = true;
    undo_redo_.clear();
    return true;
}

bool Obra::save_ed3(const std::filesystem::path& path) {
    io::BinaryWriter writer;
    write_binary(writer);
    if (writer.save_to_file(path)) {
        undo_redo_.mark_saved();
        return true;
    }
    return false;
}

bool Obra::load_ed3(const std::filesystem::path& path) {
    auto reader = io::BinaryReader::load_from_file(path);
    if (!reader) return false;
    return read_binary(*reader);
}

io::JsonValue Obra::to_json() const {
    io::JsonValue root = io::JsonValue::make_object();
    root["schema"] = "CypeClon.ed3.json";
    root["version"] = 2027;

    io::JsonValue dg = io::JsonValue::make_object();
    dg["clave"] = datos_generales_.clave;
    dg["descripcion"] = datos_generales_.descripcion;
    dg["sistema_unidades"] = static_cast<int>(datos_generales_.sistema_unidades);
    dg["acero_laminado"] = datos_generales_.acero_laminado;
    dg["hormigon_estructural"] = datos_generales_.hormigon_estructural;
    dg["tension_terreno_persistente_mpa"] = datos_generales_.tension_terreno_persistente_mpa;
    root["datos_generales"] = std::move(dg);

    io::JsonValue nudos_arr = io::JsonValue::make_array();
    for (const auto& n : estructura_.nudos) {
        io::JsonValue jn = io::JsonValue::make_object();
        jn["id"] = n.id;
        jn["x"] = n.pos.x;
        jn["y"] = n.pos.y;
        jn["z"] = n.pos.z;
        jn["referencia"] = n.referencia;
        jn["vinc_ext"] = static_cast<int>(n.vinc_ext);
        nudos_arr.as_array_mut().push_back(std::move(jn));
    }
    root["nudos"] = std::move(nudos_arr);

    io::JsonValue barras_arr = io::JsonValue::make_array();
    for (const auto& b : estructura_.barras) {
        io::JsonValue jb = io::JsonValue::make_object();
        jb["id"] = b.id;
        jb["nudo_inicial_id"] = b.nudo_inicial_id;
        jb["nudo_final_id"] = b.nudo_final_id;
        jb["perfil"] = b.perfil;
        jb["material"] = b.material;
        jb["angulo_giro_rad"] = b.angulo_giro_rad;
        barras_arr.as_array_mut().push_back(std::move(jb));
    }
    root["barras"] = std::move(barras_arr);
    return root;
}

bool Obra::from_json(const io::JsonValue& root) {
    if (!root.is_object()) return false;
    const auto& dg = root.get("datos_generales");
    if (dg.is_object()) {
        datos_generales_.clave = dg.get("clave").as_string();
        datos_generales_.descripcion = dg.get("descripcion").as_string();
        datos_generales_.sistema_unidades = static_cast<units::UnitSystem>(dg.get("sistema_unidades").as_int64(0));
        datos_generales_.acero_laminado = dg.get("acero_laminado").as_string();
        datos_generales_.hormigon_estructural = dg.get("hormigon_estructural").as_string();
        datos_generales_.tension_terreno_persistente_mpa = dg.get("tension_terreno_persistente_mpa").as_number(0.2);
    }

    estructura_.nudos.clear();
    estructura_.barras.clear();
    std::uint32_t max_nid = 0;
    for (const auto& jn : root.get("nudos").as_array()) {
        Nudo3d n;
        n.id = static_cast<std::uint32_t>(jn.get("id").as_int64());
        n.pos = {jn.get("x").as_number(), jn.get("y").as_number(), jn.get("z").as_number()};
        n.referencia = jn.get("referencia").as_string();
        n.vinc_ext = static_cast<VinculacionExterior>(jn.get("vinc_ext").as_int64(0));
        max_nid = std::max(max_nid, n.id);
        estructura_.nudos.push_back(std::move(n));
    }
    estructura_.next_nudo_id = max_nid + 1;

    std::uint32_t max_bid = 0;
    for (const auto& jb : root.get("barras").as_array()) {
        Barra3d b;
        b.id = static_cast<std::uint32_t>(jb.get("id").as_int64());
        b.nudo_inicial_id = static_cast<std::uint32_t>(jb.get("nudo_inicial_id").as_int64());
        b.nudo_final_id = static_cast<std::uint32_t>(jb.get("nudo_final_id").as_int64());
        b.perfil = jb.get("perfil").as_string();
        b.material = jb.get("material").as_string();
        b.angulo_giro_rad = jb.get("angulo_giro_rad").as_number();
        max_bid = std::max(max_bid, b.id);
        estructura_.barras.push_back(std::move(b));
    }
    estructura_.next_barra_id = max_bid + 1;
    abierta_ = true;
    undo_redo_.clear();
    return true;
}

} // namespace cypeclon::model
