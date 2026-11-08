#include "model/obra.hpp"

#include <algorithm>

namespace cypeclon::model {

Obra::Obra() {
    estructura_.capas.push_back(EtiquetaCapa{1, "Por defecto", true, false, 0x0080FF});
    estructura_.rejillas.push_back(RejillaTrabajo{});
}

std::uint32_t Obra::add_nudo(const core::Vec3d& pos, VinculacionExterior vinc) {
    const std::uint32_t assigned_id = estructura_.next_nudo_id++;
    Nudo3d nudo{assigned_id, pos, "N" + std::to_string(assigned_id), vinc, true};

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
