#include "cad/cad_engine.hpp"
#include "core/math.hpp"
#include "io/cypesave.hpp"
#include "io/undoredo.hpp"
#include "model/obra.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void assert_io(bool cond, const char* expr, int line) {
    if (!cond) {
        std::cerr << "[FAIL test_io_undoredo.cpp:" << line << "] " << expr << "\n";
        std::exit(1);
    }
}
#define IO_ASSERT(expr) assert_io((expr), #expr, __LINE__)
} // namespace

void run_io_undoredo_tests() {
    using namespace cypeclon::core;
    using namespace cypeclon::io;
    using namespace cypeclon::model;
    using namespace cypeclon::cad;

    // 1. Prueba de Transacciones Undo / Redo y Rollback sobre Obra
    Obra obra;
    obra.datos_generales_mut().clave = "NaveIndustrial_01";
    obra.datos_generales_mut().descripcion = "Portico biarticulado de acero S275";

    IO_ASSERT(!obra.undo_redo().can_undo());
    IO_ASSERT(!obra.undo_redo().has_unsaved_changes());

    // Transacción agrupada: crear pórtico (4 nudos y 3 barras)
    obra.undo_redo().begin_transaction("Introducir portico principal");
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({0.0, 0.0, 6.0}, VinculacionExterior::Libre);
    const auto n3 = obra.add_nudo({12.0, 0.0, 6.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({12.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto b1 = obra.add_barra(n1, n2, "HEA 240", "S275");
    const auto b2 = obra.add_barra(n2, n3, "IPE 300", "S275");
    const auto b3 = obra.add_barra(n3, n4, "HEA 240", "S275");
    IO_ASSERT(obra.undo_redo().commit_transaction());

    IO_ASSERT(obra.estructura().nudos.size() == 4);
    IO_ASSERT(obra.estructura().barras.size() == 3);
    IO_ASSERT(obra.undo_redo().can_undo());
    IO_ASSERT(obra.undo_redo().undo_description() == "Introducir portico principal");
    IO_ASSERT(obra.undo_redo().has_unsaved_changes());

    // Probar Rollback de una transacción cancelada
    obra.undo_redo().begin_transaction("Mover cumbrera (cancelado)");
    IO_ASSERT(obra.move_nudo(n2, {0.0, 0.0, 9.5}));
    IO_ASSERT(obra.find_nudo(n2)->pos.equals({0.0, 0.0, 9.5}));
    obra.undo_redo().rollback();
    IO_ASSERT(obra.find_nudo(n2)->pos.equals({0.0, 0.0, 6.0}));

    // Probar Undo y Redo del pórtico completo
    IO_ASSERT(obra.undo_redo().undo());
    IO_ASSERT(obra.estructura().nudos.empty() && obra.estructura().barras.empty());
    IO_ASSERT(obra.undo_redo().can_redo());

    IO_ASSERT(obra.undo_redo().redo());
    IO_ASSERT(obra.estructura().nudos.size() == 4 && obra.estructura().barras.size() == 3);
    (void)b1; (void)b2; (void)b3;

    // 2. Prueba de Serialización Binaria cypesave (con salto de sección desconocida futura)
    BinaryWriter writer({2027, 1, 0});
    obra.write_binary(writer);

    // Añadir una sección futura ficticia para comprobar compatibilidad hacia adelante (skip_section)
    const auto sec_future = writer.begin_section("SECCION_FUTURA_2028", 1);
    writer.write_array_f64(std::vector<double>{1.0, 2.0, 3.0, 4.0});
    writer.end_section(sec_future);

    BinaryReader reader(writer.take_buffer());
    Obra obra_leida;
    IO_ASSERT(obra_leida.read_binary(reader));
    IO_ASSERT(reader.version().major == 2027);
    IO_ASSERT(obra_leida.datos_generales().clave == "NaveIndustrial_01");
    IO_ASSERT(obra_leida.estructura().nudos.size() == 4);
    IO_ASSERT(obra_leida.estructura().barras.size() == 3);
    IO_ASSERT(obra_leida.find_barra(2)->perfil == "IPE 300");
    IO_ASSERT(obra_leida.find_nudo(3)->pos.equals({12.0, 0.0, 6.0}));

    // 3. Prueba de Serialización JSON (ida y vuelta)
    const JsonValue jdoc = obra.to_json();
    const std::string jtext = jdoc.dump(2);
    const auto jparsed = JsonValue::parse(jtext);
    IO_ASSERT(jparsed.has_value());

    Obra obra_json;
    IO_ASSERT(obra_json.from_json(*jparsed));
    IO_ASSERT(obra_json.datos_generales().clave == "NaveIndustrial_01");
    IO_ASSERT(obra_json.estructura().nudos.size() == 4);
    IO_ASSERT(obra_json.estructura().barras.size() == 3);

    // 4. Prueba del Octree 3D (ebaoctree) sobre los nudos de la obra
    Octree3d octree(obra.bounding_box());
    for (const auto& n : obra.estructura().nudos) {
        BBox3d nbox{n.pos - Vec3d{0.15, 0.15, 0.15}, n.pos + Vec3d{0.15, 0.15, 0.15}};
        octree.insert(SpatialItem{EntityKind::Nudo, n.id, nbox});
    }
    const auto ray_hits = octree.query_ray({12.0, -5.0, 6.0}, {0.0, 1.0, 0.0});
    IO_ASSERT(!ray_hits.empty() && ray_hits.front().entity_id == n3);
}

