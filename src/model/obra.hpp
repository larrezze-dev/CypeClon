#pragma once

#include "core/math.hpp"
#include "io/cypesave.hpp"
#include "io/undoredo.hpp"
#include "units/units.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cypeclon::model {

enum class SolapaActiva : std::uint8_t {
    Estructura = 0,  ///< "METAL3D_ESTRUCTURA" (ed3datos_t / ed3datvi_t)
    Cimentacion = 1  ///< "ID_CIMENTACION" (plcdatos_t / ebadatvi_datos_vista_nulo_t)
};

enum class VinculacionExterior : std::uint8_t {
    Libre = 0,
    ArticuladoFijo,
    Empotrado,
    ApoyoElastico
};

struct Nudo3d {
    std::uint32_t id{0};
    core::Vec3d pos{0.0, 0.0, 0.0};
    std::string referencia;
    VinculacionExterior vinc_ext{VinculacionExterior::Libre};
    bool empotrado_interior{true};
};

struct Barra3d {
    std::uint32_t id{0};
    std::uint32_t nudo_inicial_id{0};
    std::uint32_t nudo_final_id{0};
    std::string perfil{"IPE 200"};
    std::string material{"S275"};
    double angulo_giro_rad{0.0};
    bool es_tirante{false};
    std::uint32_t capa_id{1};
};

struct EtiquetaCapa {
    std::uint32_t id{1};
    std::string nombre{"Por defecto"};
    bool visible{true};
    bool bloqueada{false};
    std::uint32_t color_rgb{0x0080FF};
};

struct DatosGeneralesObra {
    std::string clave{"Obra1"};
    std::string descripcion{"Obra de CYPE 3D"};
    units::UnitSystem sistema_unidades{units::UnitSystem::SI};

    // Normas y materiales por defecto (04_after_wizard_aceptar.png / 14_dialog_datos_generales.png)
    std::string normas_resumen{"Eurocódigo 2, CTE DB SE-A, Código Estructural, CTE DB SE-M y Eurocódigo 9"};
    std::string acero_laminado{"S275 (UNE-EN 10025-2)"};
    std::string acero_conformado{"S235"};
    std::string hormigon_estructural{"C25/30"};
    std::string hormigon_pilares{"C25/30"};
    std::string hormigon_vigas{"C25/30"};
    std::string hormigon_cimentacion{"C25/30"};
    std::string acero_barras{"S-400"};
    std::string madera{"Aserrada C24"};
    std::string aluminio{"EN AW-5083 - F"};

    bool con_sismo_dinamico{false};
    bool verificar_deslizamiento_zapatas{true};
    double tension_terreno_persistente_mpa{0.200};
    double tension_terreno_accidental_mpa{0.300};
};

struct EstructuraDatos {
    // Equivalente a ed3datos_t en ed3dlib.dll
    std::vector<Nudo3d> nudos;
    std::vector<Barra3d> barras;
    std::vector<EtiquetaCapa> capas;
    std::uint32_t next_nudo_id{1};
    std::uint32_t next_barra_id{1};
};

enum class TipoElementoCimentacion : std::uint8_t {
    ZapataHormigonArmado = 0,
    ZapataHormigonMasa,
    Encepado,
    VigaAtado,
    VigaCentradora
};

struct ElementoCimentacion {
    std::uint32_t id{0};
    TipoElementoCimentacion tipo{TipoElementoCimentacion::ZapataHormigonArmado};
    std::uint32_t nudo_arranque_id{0};
    double ancho_x_m{1.20};
    double ancho_y_m{1.20};
    double canto_m{0.50};
};

struct CimentacionDatos {
    // Equivalente a plcdatos_t en planocim.dll
    std::vector<ElementoCimentacion> elementos;
    std::uint32_t next_elemento_id{1};
};

/**
 * @brief Estructura maestra de proyecto `Obra` equivalente a la estructura de obra de e3dcomun.dll.
 *        Integra `ed3datos_t` (Estructura), `plcdatos_t` (Cimentación), `cypesave` y `undoredo`.
 */
class Obra {
public:
    Obra();

    const DatosGeneralesObra& datos_generales() const noexcept { return datos_generales_; }
    DatosGeneralesObra& datos_generales_mut() noexcept { return datos_generales_; }

    const EstructuraDatos& estructura() const noexcept { return estructura_; }
    EstructuraDatos& estructura_mut() noexcept { return estructura_; }

    const CimentacionDatos& cimentacion() const noexcept { return cimentacion_; }
    CimentacionDatos& cimentacion_mut() noexcept { return cimentacion_; }

    SolapaActiva solapa_activa() const noexcept { return solapa_activa_; }
    void set_solapa_activa(SolapaActiva s) noexcept { solapa_activa_ = s; }

    io::UndoRedoManager& undo_redo() noexcept { return undo_redo_; }
    const io::UndoRedoManager& undo_redo() const noexcept { return undo_redo_; }

    // Operaciones de modelo con soporte transaccional Undo/Redo
    std::uint32_t add_nudo(const core::Vec3d& pos, VinculacionExterior vinc = VinculacionExterior::Libre);
    bool move_nudo(std::uint32_t nudo_id, const core::Vec3d& new_pos);
    std::uint32_t add_barra(std::uint32_t nudo_ini, std::uint32_t nudo_fin, std::string_view perfil = "IPE 200", std::string_view material = "S275");

    const Nudo3d* find_nudo(std::uint32_t id) const noexcept;
    const Barra3d* find_barra(std::uint32_t id) const noexcept;
    core::BBox3d bounding_box() const noexcept;

    // Serialización binaria (.ed3 via cypesave) y JSON
    void write_binary(io::BinaryWriter& writer) const;
    bool read_binary(io::BinaryReader& reader);

    bool save_ed3(const std::filesystem::path& path);
    bool load_ed3(const std::filesystem::path& path);

    io::JsonValue to_json() const;
    bool from_json(const io::JsonValue& root);

private:
    DatosGeneralesObra datos_generales_{};
    EstructuraDatos estructura_{};
    CimentacionDatos cimentacion_{};
    SolapaActiva solapa_activa_{SolapaActiva::Estructura};
    io::UndoRedoManager undo_redo_{64};
};

} // namespace cypeclon::model
