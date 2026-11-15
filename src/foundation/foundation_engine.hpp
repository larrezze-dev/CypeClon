#pragma once

#include "connections/connections_engine.hpp"
#include "core/math.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cypeclon::foundation {

// =============================================================================
// Fase 12 (P-13.1 / T-018): Solapa Cimentación
// Basado en la inspección REA de:
//   - planocim.dll (79 exports: plcdatos_*, plclimites_*, plcpaneles_*, plcplano_*)
//   - arranque.dll (84 exports: arqcomb_*, arqdatos_*, cmbelcimci_*, oparranq_*)
//   - zptipos.dll (539 exports: opzapais_*, opzaplin_*, tbzapata_aislada_*, zmcuad_*, zmrec_*)
//   - encepado.dll (157 exports: encepado_*, ezbibmicroplt_*, ezbibplthorarm_*, ezbibterrenos_*)
//   - pilotes.dll (61 exports: pilote_*, plterreno_*, pltubuloe_*)
//   - micropilotes.dll (37 exports: mplcfgcomp_*, mplcomp_*, mplcomphund_*, mpltipos_*)
//   - vigacim.dll (86 exports: opvigata_*, opvigcen_*, tbvigcim_*, vcvigcen_*, vigacim_*)
// =============================================================================

enum class SubtipoZapata : std::uint8_t {
    CuadradaCentrada = 0,
    RectangularCentrada,
    ExcentricaMedianeraX,
    ExcentricaMedianeraY,
    Esquina,
    CombinadaMultipleArranques,
    LinealCorrida
};

enum class GeometriaCantoZapata : std::uint8_t {
    CantoConstante = 0,
    Piramidal
};

enum class TipoEncepado : std::uint8_t {
    Encepado1Pilote = 1,
    Encepado2Pilotes = 2,
    Encepado3Pilotes = 3,
    Encepado4Pilotes = 4,
    EncepadoLinealNPilotes = 5,
    EncepadoRectangularNPilotes = 6
};

enum class FormaPilote : std::uint8_t {
    CircularInSitu = 0,
    CuadradoPrefabricado,
    MicropiloteTubular
};

struct ReaccionCombinacionCimentacion {
    std::string nombre_combinacion;
    bool es_sismica_o_accidental{false};
    bool es_servicio_caracteristica{false};
    // Cargas que transmite el soporte a la cimentación (N > 0 compresión hacia abajo en kN)
    double n_comp_kn{0.0};
    double vx_kn{0.0};
    double vy_kn{0.0};
    double mx_knm{0.0}; // Momento alrededor de eje X (produce excentricidad ey)
    double my_knm{0.0}; // Momento alrededor de eje Y (produce excentricidad ex)
    double t_torsor_knm{0.0};
};

/**
 * @brief Arranque de soporte en cimentación (`arranque.dll`: `arqdatos_*`, `cmbelcimci_*`).
 */
struct ArranqueCimentacion {
    std::uint32_t nudo_id{0};
    core::Vec3d posicion_m{0.0, 0.0, 0.0};
    std::string referencia_soporte{"N1"};
    bool es_soporte_hormigon{false};
    double dim_x_soporte_m{0.30};
    double dim_y_soporte_m{0.30};
    double angulo_giro_deg{0.0};
    double diametro_armadura_arranque_mm{16.0};
    std::uint32_t num_barras_arranque{4};
    bool tiene_placa_anclaje{false};
    double ancho_placa_x_m{0.35};
    double ancho_placa_y_m{0.35};
    double longitud_anclaje_requerida_m{0.40};
    std::vector<core::Vec2d> contorno_punzonamiento_2d_m;
    std::vector<ReaccionCombinacionCimentacion> combinaciones;
};

/**
 * @brief Opciones generales de cálculo y tablas de armado de cimentación (`planocim.dll`: `menu_opciones`).
 */
struct OpcionesCalculoCimentacion {
    double recubrimiento_inferior_mm{50.0};
    double recubrimiento_superior_mm{40.0};
    double recubrimiento_lateral_mm{40.0};
    double espesor_hormigon_limpieza_m{0.10};
    double porcentaje_mermas_acero{10.0};
    bool comprobar_fisuracion{true};
    double abertura_max_fisura_mm{0.30};
    double coef_seguridad_vuelco_persistente{1.80};
    double coef_seguridad_vuelco_accidental{1.50};
    double coef_seguridad_deslizamiento_persistente{1.50};
    double coef_seguridad_deslizamiento_accidental{1.20};
    double angulo_rozamiento_terreno_deg{30.0};
    double peso_especifico_hormigon_armado_kn_m3{25.0};
    double peso_especifico_hormigon_masa_kn_m3{23.0};
};

struct EntradaTablaArmadoZapata {
    double diametro_mm{12.0};
    double separacion_m{0.20};
    double cuantia_cm2_m{5.65};
    std::string etiqueta{"Ø12c/20"};
};

struct DisposicionParrillaZapata {
    double diametro_x_mm{12.0};
    double separacion_x_m{0.20};
    std::uint32_t num_barras_x{6};
    double area_dispuesta_x_cm2_m{5.65};
    double area_necesaria_x_cm2_m{0.0};
    double longitud_barra_x_m{1.10};
    double patilla_x_m{0.15};

    double diametro_y_mm{12.0};
    double separacion_y_m{0.20};
    std::uint32_t num_barras_y{6};
    double area_dispuesta_y_cm2_m{5.65};
    double area_necesaria_y_cm2_m{0.0};
    double longitud_barra_y_m{1.10};
    double patilla_y_m{0.15};
};

/**
 * @brief Definición y resultado de comprobación de Zapata Aislada / Combinada (`zptipos.dll`).
 */
struct ZapataCimentacion {
    std::uint32_t id{0};
    std::string referencia{"Z1"};
    model::TipoElementoCimentacion tipo_material{model::TipoElementoCimentacion::ZapataHormigonArmado};
    SubtipoZapata subtipo{SubtipoZapata::CuadradaCentrada};
    GeometriaCantoZapata geometria_canto{GeometriaCantoZapata::CantoConstante};
    std::vector<std::uint32_t> nudos_arranque_ids;

    core::Vec2d centro_planta_m{0.0, 0.0};
    double ancho_x_m{1.20};
    double ancho_y_m{1.20};
    double canto_total_m{0.50};
    double canto_borde_m{0.50}; // Usado si es piramidal
    double excentricidad_x_m{0.0};
    double excentricidad_y_m{0.0};
    double angulo_giro_deg{0.0};
    std::vector<core::Vec2d> contorno_recortado_planta_m;

    DisposicionParrillaZapata parrilla_inferior{};
    bool tiene_parrilla_superior{false};
    DisposicionParrillaZapata parrilla_superior{};
};

struct ResultadoComprobacionZapata {
    std::uint32_t zapata_id{0};
    std::string referencia;
    bool cumple_global{true};

    // Tensiones sobre el terreno (Meyerhof / ley trapecial)
    double tension_media_pers_mpa{0.0};
    double tension_max_borde_pers_mpa{0.0};
    double tension_adm_pers_mpa{0.200};
    double tension_max_borde_acc_mpa{0.0};
    double tension_adm_acc_mpa{0.300};
    double aprovechamiento_tension{0.0};
    bool cumple_tension_terreno{true};

    // Estabilidad al vuelco y deslizamiento
    double coef_vuelco_x_obtenido{99.0};
    double coef_vuelco_y_obtenido{99.0};
    double coef_vuelco_exigido{1.80};
    bool cumple_vuelco{true};

    double coef_deslizamiento_obtenido{99.0};
    double coef_deslizamiento_exigido{1.50};
    bool cumple_deslizamiento{true};

    // Comprobaciones estructurales (Flexión, Cortante, Punzonamiento, Compresión oblicua, Fisuración, Anclaje)
    double md_x_knm_m{0.0};
    double md_y_knm_m{0.0};
    double mr_x_knm_m{0.0};
    double mr_y_knm_m{0.0};
    double aprovechamiento_flexion{0.0};
    bool cumple_flexion{true};

    double vd_cortante_max_kn_m{0.0};
    double vrd_cortante_kn_m{0.0};
    double aprovechamiento_cortante{0.0};
    bool cumple_cortante{true};

    double tau_punzonamiento_ed_mpa{0.0};
    double tau_punzonamiento_rd_mpa{0.0};
    double aprovechamiento_punzonamiento{0.0};
    bool cumple_punzonamiento{true};

    // Si es Hormigón en Masa (zmcuad_*, zmrec_*)
    double sigma_ct_masa_ed_mpa{0.0};
    double fctd_masa_mpa{0.0};
    bool cumple_traccion_hormigon_masa{true};

    double abertura_fisura_estimada_mm{0.0};
    bool cumple_fisuracion{true};

    double canto_minimo_normativo_m{0.40};
    bool cumple_dimensiones_minimas{true};
    bool cumple_anclaje_arranque{true};

    double aprovechamiento_maximo{0.0};
    std::string listado_justificativo;
};

/**
 * @brief Definición de Pilote / Micropilote y Encepado (`pilotes.dll`, `micropilotes.dll`, `encepado.dll`).
 */
struct EntradaCatalogoPilote {
    std::string codigo{"PIL-D450"};
    FormaPilote forma{FormaPilote::CircularInSitu};
    double diametro_o_lado_m{0.45};
    double longitud_m{12.0};
    double capacidad_portante_persistente_kn{650.0};
    double capacidad_portante_accidental_kn{850.0};
    double resistencia_traccion_kn{150.0};
    double separacion_minima_m{1.35}; // 3 * D por defecto (pilote_separacion_por_defecto)
    double espesor_camisa_mm{0.0};    // Para micropilotes tubulares (mplcfgcomp_*)
    double limite_elastico_camisa_mpa{0.0};
};

struct PosicionPiloteEnEncepado {
    std::uint32_t indice_pilote{1};
    core::Vec2d coord_relativa_m{0.0, 0.0};
    double axil_max_persistente_kn{0.0};
    double axil_max_accidental_kn{0.0};
    double axil_min_traccion_kn{0.0};
};

struct EncepadoCimentacion {
    std::uint32_t id{0};
    std::string referencia{"E1"};
    std::uint32_t nudo_arranque_id{0};
    TipoEncepado tipo_encepado{TipoEncepado::Encepado2Pilotes};
    EntradaCatalogoPilote pilote_catalogo{};

    core::Vec2d centro_planta_m{0.0, 0.0};
    double ancho_x_m{2.25};
    double ancho_y_m{0.95};
    double canto_m{0.80};
    double vuelo_borde_m{0.25};
    double angulo_giro_deg{0.0};

    std::vector<PosicionPiloteEnEncepado> pilotes;

    // Armado por método de bielas y tirantes (encepado_dimensiona_completo)
    double diametro_tirante_ppal_mm{20.0};
    std::uint32_t num_barras_tirante_ppal{6};
    double area_tirante_dispuesta_cm2{18.85};
    double area_tirante_necesaria_cm2{0.0};

    double diametro_armadura_superior_mm{12.0};
    std::uint32_t num_barras_superiores{5};
    double diametro_cercos_verticales_mm{10.0};
    double separacion_cercos_m{0.15};
    std::uint32_t num_ramas_cercos{2};
};

struct ResultadoComprobacionEncepado {
    std::uint32_t encepado_id{0};
    std::string referencia;
    bool cumple_global{true};

    // Capacidad portante de pilotes / micropilotes (pilote_capacidad_portante_*, mplcomphund_*)
    double axil_max_pilote_pers_kn{0.0};
    double capacidad_pilote_pers_kn{650.0};
    double axil_max_pilote_acc_kn{0.0};
    double capacidad_pilote_acc_kn{850.0};
    double aprovechamiento_pilotes{0.0};
    bool cumple_capacidad_pilotes{true};

    // Método de Bielas y Tirantes (ángulos de biela, nudos comprimidos y tracción en tirante)
    double angulo_biela_comprimida_deg{45.0};
    bool cumple_rigidez_biela{true}; // Vuelo v <= 2 * d (encepado rígido)
    double traccion_tirante_ed_kn{0.0};
    double capacidad_tirante_rd_kn{0.0};
    double aprovechamiento_tirante{0.0};
    bool cumple_tirante_principal{true};

    double tension_biela_nodal_ed_mpa{0.0};
    double tension_biela_nodal_rd_mpa{0.0};
    double aprovechamiento_biela_comprimida{0.0};
    bool cumple_biela_comprimida{true};

    double cortante_o_suspension_ed_kn{0.0};
    double cortante_o_suspension_rd_kn{0.0};
    bool cumple_armadura_secundaria{true};

    // Esfuerzos de desequilibrio para viga centradora (encepado_esfuerzos_punto_equilibrio_para_viga_centradora)
    double mx_desequilibrio_viga_centradora_knm{0.0};
    double my_desequilibrio_viga_centradora_knm{0.0};

    double aprovechamiento_maximo{0.0};
    std::string listado_justificativo;
};

/**
 * @brief Vigas de Atado y Vigas Centradoras (`vigacim.dll`: `tbvigcim_*`, `vcvigcen_*`, `vigacim_*`).
 */
struct EntradaTablaVigaCimentacion {
    std::string codigo{"VC-40x50"};
    bool es_centradora{true};
    double ancho_b_m{0.40};
    double canto_h_m{0.50};

    std::uint32_t num_barras_sup{4};
    double diametro_sup_mm{16.0};
    std::uint32_t num_barras_inf{4};
    double diametro_inf_mm{16.0};
    std::uint32_t num_barras_piel{2};
    double diametro_piel_mm{12.0};
    double diametro_estribos_mm{8.0};
    double separacion_estribos_m{0.20};

    double momento_ult_negativo_knm{125.0};
    double momento_ult_positivo_knm{125.0};
    double cortante_ult_kn{160.0};
    double axil_atado_ult_kn{220.0};
};

struct VigaCimentacion {
    std::uint32_t id{0};
    std::string referencia{"VC1"};
    model::TipoElementoCimentacion tipo{model::TipoElementoCimentacion::VigaCentradora};
    std::uint32_t nudo_ini_id{0};
    std::uint32_t nudo_fin_id{0};
    bool centra_extremo_ini{true};
    bool centra_extremo_fin{false};

    double longitud_eje_a_eje_m{5.0};
    double longitud_libre_m{3.80};
    EntradaTablaVigaCimentacion seccion_armado{};
    double longitud_anclaje_extremo_ini_m{0.45};
    double longitud_anclaje_extremo_fin_m{0.45};
    double rigidez_rotacional_equivalente_knm_rad{0.0};
};

struct ResultadoComprobacionVigaCimentacion {
    std::uint32_t viga_id{0};
    std::string referencia;
    model::TipoElementoCimentacion tipo{model::TipoElementoCimentacion::VigaCentradora};
    bool cumple_global{true};

    double axil_diseno_atado_kn{0.0};
    double capacidad_axil_atado_kn{0.0};
    double momento_negativo_ed_knm{0.0};
    double momento_positivo_ed_knm{0.0};
    double cortante_max_ed_kn{0.0};

    double aprovechamiento_axil{0.0};
    double aprovechamiento_flexion{0.0};
    double aprovechamiento_cortante{0.0};
    double aprovechamiento_maximo{0.0};

    bool cumple_dimensiones_minimas{true};
    bool cumple_armado_longitudinal{true};
    bool cumple_armado_transversal{true};
    bool cumple_anclaje_extremos{true};
    std::string listado_justificativo;
};

/**
 * @brief Medición de volúmenes de hormigón y pesos de acero de toda la cimentación.
 */
struct MedicionCimentacion {
    std::size_t num_zapatas_ha{0};
    std::size_t num_zapatas_hm{0};
    std::size_t num_encepados{0};
    std::size_t num_pilotes_totales{0};
    std::size_t num_vigas_atado{0};
    std::size_t num_vigas_centradoras{0};

    double volumen_hormigon_limpieza_m3{0.0};
    double volumen_hormigon_zapatas_m3{0.0};
    double volumen_hormigon_encepados_m3{0.0};
    double volumen_hormigon_vigas_m3{0.0};
    double volumen_hormigon_total_m3{0.0};
    double longitud_total_pilotes_m{0.0};

    double peso_acero_zapatas_kg{0.0};
    double peso_acero_encepados_kg{0.0};
    double peso_acero_vigas_kg{0.0};
    double peso_acero_total_con_mermas_kg{0.0};
};

/**
 * @brief Primitivas 3D de sólidos y despiece de armaduras en cimentación (`vista_3D` en `planocim.dll`).
 */
struct BloqueHormigon3dCimentacion {
    std::string etiqueta;
    std::uint32_t color_rgba{0x9099A2FF};
    core::Vec3d centro_m{0.0, 0.0, 0.0};
    core::Vec3d dimensiones_m{1.0, 1.0, 0.5};
};

struct CilindroPilote3dCimentacion {
    std::string etiqueta;
    std::uint32_t color_rgba{0x6E7885FF};
    double radio_m{0.225};
    core::Vec3d p_ini_m{0.0, 0.0, 0.0};
    core::Vec3d p_fin_m{0.0, 0.0, -10.0};
};

struct BarraArmadura3dCimentacion {
    std::string etiqueta_elemento;
    std::string posicion; // "Parrilla Inf X", "Tirante Ppal", "Estribo VC", etc.
    double diametro_mm{12.0};
    std::vector<core::Vec3d> polilinea_m;
};

struct Escena3dCimentacion {
    std::vector<BloqueHormigon3dCimentacion> bloques_hormigon;
    std::vector<CilindroPilote3dCimentacion> cilindros_pilotes;
    std::vector<BarraArmadura3dCimentacion> barras_armadura_3d;
};

/**
 * @brief Modelo integral de la Solapa Cimentación (`plcdatos_t` en `planocim.dll`).
 */
struct ModeloCompletoCimentacion {
    OpcionesCalculoCimentacion opciones{};
    std::vector<ArranqueCimentacion> arranques;
    std::vector<ZapataCimentacion> zapatas;
    std::vector<EncepadoCimentacion> encepados;
    std::vector<VigaCimentacion> vigas;
    std::vector<model::LimiteCimentacion> limites;

    std::vector<ResultadoComprobacionZapata> resultados_zapatas;
    std::vector<ResultadoComprobacionEncepado> resultados_encepados;
    std::vector<ResultadoComprobacionVigaCimentacion> resultados_vigas;
    MedicionCimentacion medicion{};
};

/**
 * @brief Motor completo de la Solapa Cimentación (`planocim.dll`, `arranque.dll`, `zptipos.dll`, `encepado.dll`, `pilotes.dll`, `micropilotes.dll`, `vigacim.dll`).
 */
class FoundationEngine {
public:
    static std::size_t count_elements(const model::Obra& obra) noexcept;

    // 1. Catálogos de armado y pilotes (`tbzapata_aislada_*`, `ezbibplthorarm_*`, `ezbibmicroplt_*`, `tbvigcim_*`, `vcvigcen_*`)
    static std::vector<EntradaTablaArmadoZapata> build_footing_rebar_table();
    static std::vector<EntradaCatalogoPilote> build_pile_and_micropile_catalog();
    static std::vector<EntradaTablaVigaCimentacion> build_foundation_beam_catalog();

    // 2. Importación de arranques y reacciones desde la estructura 3D (`arranque.dll`: `arqdatos_*`, `cmbelcimci_*`)
    static std::vector<ArranqueCimentacion> extract_foundation_starters(
        const model::Obra& obra,
        const solver::SolverSummary* solver_summary = nullptr,
        const std::vector<connections::PlacaAnclaje3d>* baseplates = nullptr);

    // 3. Recorte poligonal con límites de cimentación (`planocim.dll`: `plclimites_recorta_contorno`)
    static std::vector<core::Vec2d> build_footing_rectangle_polygon(const ZapataCimentacion& zapata);
    static std::vector<core::Vec2d> clip_polygon_against_limit(
        const std::vector<core::Vec2d>& subject_poly,
        const model::LimiteCimentacion& limit_poly);
    static double compute_polygon_area_m2(const std::vector<core::Vec2d>& poly) noexcept;

    // 4. Comprobación y dimensionamiento de Zapatas (`zptipos.dll`: `opzapais_*`, `zmcuad_*`, `zmrec_*`)
    static ResultadoComprobacionZapata check_footing(
        const ZapataCimentacion& zapata,
        const std::vector<ArranqueCimentacion>& arranques,
        const model::DatosGeneralesObra& datos_generales,
        const OpcionesCalculoCimentacion& opciones = {},
        double momento_equilibrado_por_vigas_x_knm = 0.0,
        double momento_equilibrado_por_vigas_y_knm = 0.0);

    static ResultadoComprobacionZapata design_footing(
        ZapataCimentacion& zapata,
        const std::vector<ArranqueCimentacion>& arranques,
        const model::DatosGeneralesObra& datos_generales,
        const OpcionesCalculoCimentacion& opciones = {});

    // 5. Comprobación y dimensionamiento de Encepados sobre Pilotes/Micropilotes (`encepado.dll`, `pilotes.dll`, `micropilotes.dll`)
    static void populate_pile_positions(EncepadoCimentacion& encepado);

    static ResultadoComprobacionEncepado check_pile_cap(
        const EncepadoCimentacion& encepado,
        const ArranqueCimentacion& arranque,
        const model::DatosGeneralesObra& datos_generales,
        const OpcionesCalculoCimentacion& opciones = {});

    static ResultadoComprobacionEncepado design_pile_cap(
        EncepadoCimentacion& encepado,
        const ArranqueCimentacion& arranque,
        const model::DatosGeneralesObra& datos_generales,
        const OpcionesCalculoCimentacion& opciones = {});

    // 6. Comprobación y dimensionamiento de Vigas de Atado y Centradoras (`vigacim.dll`: `vcvigcen_*`, `vigacim_*`)
    static ResultadoComprobacionVigaCimentacion check_foundation_beam(
        const VigaCimentacion& viga,
        const ArranqueCimentacion& arranque_ini,
        const ArranqueCimentacion& arranque_fin,
        const model::DatosGeneralesObra& datos_generales,
        const OpcionesCalculoCimentacion& opciones = {},
        double excentricidad_zapata_ini_m = 0.0,
        double excentricidad_zapata_fin_m = 0.0);

    static ResultadoComprobacionVigaCimentacion design_foundation_beam(
        VigaCimentacion& viga,
        const ArranqueCimentacion& arranque_ini,
        const ArranqueCimentacion& arranque_fin,
        const model::DatosGeneralesObra& datos_generales,
        const OpcionesCalculoCimentacion& opciones = {},
        double excentricidad_zapata_ini_m = 0.0,
        double excentricidad_zapata_fin_m = 0.0);

    // 7. Generación automática de zapatas y vigas, eliminación de solapes, dimensionamiento integral y despiece 3D
    static ModeloCompletoCimentacion generate_automatic_foundation(
        model::Obra& obra,
        const solver::SolverSummary* solver_summary = nullptr,
        bool usar_encepados = false,
        bool generar_vigas_perimetrales = true);

    static std::size_t resolve_overlapping_footings(ModeloCompletoCimentacion& modelo);

    static void check_all_foundation(
        ModeloCompletoCimentacion& modelo,
        const model::DatosGeneralesObra& datos_generales);

    static void design_all_foundation(
        model::Obra& obra,
        ModeloCompletoCimentacion& modelo);

    static MedicionCimentacion compute_foundation_takeoff(const ModeloCompletoCimentacion& modelo);
    static Escena3dCimentacion build_3d_scene(const ModeloCompletoCimentacion& modelo);
};

} // namespace cypeclon::foundation
