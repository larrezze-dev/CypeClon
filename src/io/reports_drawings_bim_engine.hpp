#pragma once

#include "cad/results_visualization_engine.hpp"
#include "codes/design_check_engine.hpp"
#include "connections/connections_engine.hpp"
#include "foundation/foundation_engine.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::io {

// =============================================================================
// Fase 13 (P-14.1, P-14.2, P-14.3 / T-019 / A-032, A-033, A-034):
//   1. Generador de Listados de Obra y Mediciones (`listados.dll`, `plalist.dll`, `cypedocu.dll`, `mppzperf.dll`, `csvlib.dll`)
//   2. Generador de Planos 2D/3D, Capas DXF y Biblioteca de Detalles Constructivos (`enplanos.dll`, `dibbase.dll`, `undxf.dll`)
//   3. Interoperabilidad CAD/BIM: Importación DXF/TXT y Exportación IFC2x3/IFC4, glTF 2.0, CIS/2, Tekla y TecnoMETAL
//      (`imporstr.dll`, `bimifc.dll`, `cytekla.dll`, `cypecis2.dll`, `cytecnometal.dll`, `bimcenter.dll`)
// =============================================================================

// -----------------------------------------------------------------------------
// 1. P-14.1 (A-032): Generador de Listados de Obra y Mediciones (`mppzperf.dll`, `listados.dll`, `cypedocu.dll`, `csvlib.dll`)
// -----------------------------------------------------------------------------

enum class CapituloListadoObra : std::uint8_t {
    DatosGeneralesYNormativa = 0,
    NudosYVinculaciones,
    BarrasYPerfiles,
    LaminasYPanos,
    HipotesisYCombinaciones,
    CargasIntroducidas,
    DesplazamientosYReacciones,
    EsfuerzosEnBarrasYLaminas,
    ComprobacionesELU_ELS_Fuego,
    UnionesYPlacasAnclaje,
    CimentacionGeotecniaYEstructura,
    MedicionPerfilesYMateriales
};

enum class FormatoExportacionListado : std::uint8_t {
    TXT = 0,
    HTML,
    RTF,
    DOCX_XML,
    PDF,
    CSV
};

/**
 * @brief Medición desglosada de piezas metálicas, madera y placas de anclaje (`mppzperf.dll`).
 */
struct PartidaMedicionPerfil {
    std::string serie_o_familia;
    std::string nombre_perfil;
    std::string material;
    std::uint32_t num_piezas{0};
    double longitud_total_m{0.0};
    double superficie_pintura_m2{0.0};
    double volumen_m3{0.0};
    double peso_total_kg{0.0};
};

struct ResumenMedicionEstructura {
    // Equivalente a mppzperf_append_pieza_acero_laminado_*, mppzperf_append_pieza_acero_conformado_*, mppzperf_append_pieza_madera_*
    std::vector<PartidaMedicionPerfil> partidas_acero_laminado;
    std::vector<PartidaMedicionPerfil> partidas_acero_conformado;
    std::vector<PartidaMedicionPerfil> partidas_madera;
    std::vector<PartidaMedicionPerfil> partidas_hormigon_y_otros;

    std::size_t num_placas_pernos_soldados{0};
    std::size_t num_placas_pernos_atornillados{0};
    double peso_total_placas_y_pernos_kg{0.0};

    double longitud_total_barras_m{0.0};
    double superficie_total_pintura_m2{0.0};
    double peso_total_acero_laminado_kg{0.0};
    double peso_total_acero_conformado_kg{0.0};
    double volumen_total_madera_m3{0.0};
    double volumen_total_hormigon_m3{0.0};
};

struct SeccionCapituloListado {
    CapituloListadoObra capitulo{CapituloListadoObra::DatosGeneralesYNormativa};
    std::string codigo_capitulo;
    std::string titulo;
    std::vector<std::string> parrafos;
    std::vector<std::string> cabeceras_tabla;
    std::vector<std::vector<std::string>> filas_tabla;
};

struct DocumentoListadoObra {
    std::string titulo_obra;
    std::string clave_obra;
    std::string fecha_generacion{"2026-11-15"};
    std::vector<SeccionCapituloListado> capitulos;
    ResumenMedicionEstructura medicion_estructura{};
};

// -----------------------------------------------------------------------------
// 2. P-14.2 (A-033): Generador de Planos 2D/3D, Capas DXF (`undxf.dll`) y Detalles Constructivos (`enplanos.dll`)
// -----------------------------------------------------------------------------

enum class FormatoHojaPlano : std::uint8_t {
    ISO_A4 = 0, // 297 x 210 mm
    ISO_A3,     // 420 x 297 mm
    ISO_A2,     // 594 x 420 mm
    ISO_A1,     // 841 x 594 mm
    ISO_A0      // 1189 x 841 mm
};

enum class TipoPlanoObra : std::uint8_t {
    Estructura3D = 0,
    Plano2D,
    PorticosVigasHA,
    CuadroYDespiecePilares,
    UnionesMetalicas,
    PlacasAnclaje,
    PlantaYDespieceCimentacion,
    EsquemaCargas
};

struct CapaDxfPlano {
    // Equivalente a ubcpdxf_capa_dxf_* en undxf.dll y plncfgcapa_* en enplanos.dll
    std::string nombre_capa;
    std::uint16_t color_aci{7};
    double grosor_linea_mm{0.25};
    bool tipo_linea_discontinua{false};
    bool activa_en_plano{true};
};

struct DetalleConstructivoCatalogo {
    // Biblioteca de detalles constructivos (det_*.dll: EAC, EAM, EHU, CSZ, CCP...)
    std::string codigo{"EAC001"};
    std::string categoria{"Estructura de Acero"};
    std::string titulo{"Arranque de pilar metálico sobre placa de anclaje con pernos"};
    double ancho_mm{180.0};
    double alto_mm{140.0};
    std::vector<std::pair<core::Vec2d, core::Vec2d>> lineas_vectoriales_mm;
    std::vector<std::pair<core::Vec2d, std::string>> textos_nota;
};

struct PrimitivaLineaPlano2d {
    std::string capa;
    core::Vec2d p1_mm{0.0, 0.0};
    core::Vec2d p2_mm{0.0, 0.0};
};

struct PrimitivaTextoPlano2d {
    std::string capa;
    core::Vec2d pos_mm{0.0, 0.0};
    double altura_mm{2.5};
    std::string texto;
};

struct CajetinPlano {
    std::string empresa{"CYPE Ingenieros / CypeClon C++20"};
    std::string proyecto;
    std::string titulo_plano;
    std::string escala{"1:100"};
    std::string numero_plano{"PL-01"};
    std::string fecha{"Nov 2026"};
};

struct HojaPlanoCompuesta {
    std::uint32_t id{1};
    FormatoHojaPlano formato{FormatoHojaPlano::ISO_A1};
    double ancho_hoja_mm{841.0};
    double alto_hoja_mm{594.0};
    double denominador_escala{100.0};
    TipoPlanoObra tipo{TipoPlanoObra::Estructura3D};
    CajetinPlano cajetin{};

    std::vector<CapaDxfPlano> capas;
    std::vector<PrimitivaLineaPlano2d> lineas;
    std::vector<PrimitivaTextoPlano2d> textos;
    std::vector<DetalleConstructivoCatalogo> detalles_insertados;
};

// -----------------------------------------------------------------------------
// 3. P-14.3 (A-034): Interoperabilidad CAD/BIM (`imporstr.dll`, `bimifc.dll`, `cytekla.dll`, `cypecis2.dll`, `cytecnometal.dll`)
// -----------------------------------------------------------------------------

enum class VersionEsquemaIfc : std::uint8_t {
    IFC2x3_TC1 = 0,
    IFC4_ADD2_TC1
};

struct ResultadoImportacionCad {
    bool exito{false};
    std::size_t nudos_importados{0};
    std::size_t barras_importadas{0};
    std::vector<std::string> capas_detectadas;
    std::string resumen;
};

/**
 * @brief Motor integral de Fase 13: Listados, Mediciones, Planos, Detalles Constructivos e Interoperabilidad BIM/CAD.
 */
class ReportsDrawingsBimEngine {
public:
    // =========================================================================
    // 1. P-14.1: Medición de perfiles/materiales (`mppzperf.dll`) y Listados (`listados.dll`, `cypedocu.dll`, `csvlib.dll`)
    // =========================================================================
    static ResumenMedicionEstructura compute_structure_takeoff(
        const model::Obra& obra,
        const std::vector<connections::PlacaAnclaje3d>* baseplates = nullptr);

    static DocumentoListadoObra generate_project_report(
        const model::Obra& obra,
        const solver::SolverSummary* solver_summary = nullptr,
        const codes::ResumenComprobacionObra* design_summary = nullptr,
        const std::vector<connections::PlacaAnclaje3d>* baseplates = nullptr,
        const std::vector<connections::UnionEstructural3d>* connections = nullptr,
        const foundation::ModeloCompletoCimentacion* foundation_model = nullptr,
        const std::vector<CapituloListadoObra>& capitulos_incluidos = {});

    static std::string export_report(
        const DocumentoListadoObra& doc,
        FormatoExportacionListado formato);

    // =========================================================================
    // 2. P-14.2: Planos 2D/3D (`enplanos.dll`), Capas DXF (`undxf.dll`) y Detalles Constructivos (`det_*.dll`)
    // =========================================================================
    static std::vector<CapaDxfPlano> build_standard_dxf_drawing_layers();
    static std::vector<DetalleConstructivoCatalogo> build_constructive_details_catalog();

    static HojaPlanoCompuesta compose_drawing_sheet(
        const model::Obra& obra,
        TipoPlanoObra tipo,
        FormatoHojaPlano formato = FormatoHojaPlano::ISO_A1,
        double denominador_escala = 100.0,
        const std::vector<connections::PlacaAnclaje3d>* baseplates = nullptr,
        const std::vector<connections::UnionEstructural3d>* connections = nullptr,
        const foundation::ModeloCompletoCimentacion* foundation_model = nullptr,
        const std::vector<std::string>& codigos_detalles = {});

    static std::string export_drawing_to_dxf(const HojaPlanoCompuesta& hoja);
    static std::string export_drawing_to_svg_dwg_xml(const HojaPlanoCompuesta& hoja);
    static std::string export_drawing_to_pdf(const HojaPlanoCompuesta& hoja);

    // =========================================================================
    // 3. P-14.3: Interoperabilidad CAD/BIM (`imporstr.dll`, `bimifc.dll`, `cytekla.dll`, `cypecis2.dll`, `cytecnometal.dll`)
    // =========================================================================
    static ResultadoImportacionCad import_geometry_from_dxf(
        model::Obra& obra,
        std::string_view dxf_content,
        std::string_view default_profile = "IPE 200",
        std::string_view default_material = "S275",
        double merge_tolerance_m = 1.0e-3);

    static ResultadoImportacionCad import_geometry_from_text(
        model::Obra& obra,
        std::string_view txt_content,
        std::string_view default_profile = "IPE 200",
        std::string_view default_material = "S275");

    static std::string export_to_ifc(
        const model::Obra& obra,
        VersionEsquemaIfc version = VersionEsquemaIfc::IFC4_ADD2_TC1,
        const foundation::ModeloCompletoCimentacion* foundation_model = nullptr,
        const std::vector<connections::PlacaAnclaje3d>* baseplates = nullptr);

    static std::string export_to_gltf_json(
        const model::Obra& obra,
        const foundation::ModeloCompletoCimentacion* foundation_model = nullptr);

    static std::string export_to_cis2_step(
        const model::Obra& obra,
        const std::vector<connections::UnionEstructural3d>* connections = nullptr,
        const std::vector<connections::PlacaAnclaje3d>* baseplates = nullptr);

    static std::string export_to_tekla(
        const model::Obra& obra,
        const std::vector<connections::UnionEstructural3d>* connections = nullptr,
        const std::vector<connections::PlacaAnclaje3d>* baseplates = nullptr);

    static std::string export_to_tecnometal(
        const model::Obra& obra,
        const std::vector<connections::UnionEstructural3d>* connections = nullptr,
        const std::vector<connections::PlacaAnclaje3d>* baseplates = nullptr);
};

} // namespace cypeclon::io
