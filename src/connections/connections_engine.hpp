#pragma once

/**
 * @file connections_engine.hpp
 * @brief Fase 11 (P-12.1 / T-017 / A-030): Subsistema de Placas de Anclaje y Uniones Soldadas y Atornilladas.
 *
 * Evidencia de ingeniería inversa REA:
 * - `placancl.dll` (`ev_9896d28294203d8fd44ec7ef21d3bcd1c348b3cddedcd978757301d9402f9aac`, 81 exports):
 *   - `opplanc_*` (`opplanc_crea_por_defecto`, `opplanc_panel_edicion`)
 *   - `plplacan_*` (`plplacan_comprobacion`, `plplacan_dimensiones_placa`, `plplacan_espesor_placa`,
 *     `plplacan_inicializa_pernos`, `plplacan_longitud_anclaje_de_pernos_en_cimentacion`,
 *     `plplacan_se_calcula_traccion_max_perno`, `plplacan_con_rigidizadores`,
 *     `plplacan_peso_acero_placa_descompuesto`, `plplacan_peso_total_pernos_para_medicion`,
 *     `plplacan_append_elementos_3D_placa`, `plplacan_textos_cuadro_arranques`)
 *   - `supancl_*` (`supancl_crea_contorno_horizontal_superior`, `supancl_esta_contenido_contorno`,
 *     `supancl_punto_es_cercano_a_algun_borde`, `supancl_tiene_canto_limite`)
 * - `pneleme.dll` (`ev_83407287b8f0bbb0ae3a6d30f38d83b96673252333d945866613c8ad551d0cde`, 7 exports):
 *   - `pnplacan_panel_placa_anclaje`, `pnplacan_panel_placa_anclaje_u`, `pnbloqelemcim_panel_boton_bloqueo_desbloqueo`
 * - `uniones.dll` (`ev_f2bbaf0f7652ba2a0abc04943e70098897cd1633bc4a2d22637e908e8afed3d3`, 220 exports):
 *   - `unclunioned_detecta_todas_las_uniones_en_nudos`, `unclunioned_dimensiona_uniones_en_paralelo`,
 *     `unclunioned_calcula_estado_comprobacion_uniones_en_paralelo`,
 *     `unclunioned_longitudes_rigidas_extremos_barras`, `unclunioned_rigideces_rotacionales_en_extremos_piezas`,
 *     `undtrigunion_panel_diagrama_momento_curvatura`, `undtrigunion_rigideces_propuestas`,
 *     `untabla_*`, `ununioned_*`
 * - `unionbase.dll` (`ev_7dccd1f14bc989249de7b6008f4a0594f01b25ae1576163c4f28b76c48c8c97e`, 2,107 exports):
 *   - `ubacero_*`, `ubacperno_*`, `ubcalc_*`, `ubgsaws_*`, `ubgscomp_*`, `ubgscompnudo_*`, `ubgsnorm_*`, `ublrfd360_*`, `ubnbr8800_*`
 * - `unionbase2.dll` (`ev_e38911cb029a85d0023e53f9fc07e0134526350fe76b1d837829f7fef8d7d307`, 146 exports):
 *   - `ub2eltos_*` (catálogos de tornillería métrica/imperial, tuercas, arandelas, pares de apriete y angulares)
 * - `bimjsunion.dll` (`ev_19d27bf8683a86d75f03f65f9b40d358a0608bc915bccbc37afdaa950543198e`, 75 exports):
 *   - `bjsumat_*`, `bjsumodgeo_v1_*` (intercambio BIM JSON de nudos, perfiles y uniones)
 */

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/math.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

namespace cypeclon::connections {

// ==============================================================================
// 1. Catálogo de Tornillería, Tuercas, Arandelas y Soldadura (`unionbase2.dll`: `ub2eltos_*`)
// ==============================================================================

enum class ClaseAceroTornillo : std::uint8_t {
    Clase_4_6 = 0,
    Clase_5_6 = 1,
    Clase_6_8 = 2,
    Clase_8_8 = 3,
    Clase_10_9 = 4,
    ASTM_A307 = 5,
    ASTM_A325 = 6,
    ASTM_A490 = 7
};

enum class SerieTornilleria : std::uint8_t {
    ISO_4014_4017_Ordinario = 0,
    EN_14399_HV_Pretensable = 1,
    ASTM_Imperial = 2
};

enum class ClaseSuperficieRozamiento : std::uint8_t {
    ClaseA_Granallado_Mu050 = 0,
    ClaseB_SilicatoAlcalinoZinc_Mu040 = 1,
    ClaseC_CepilladoAlambre_Mu030 = 2,
    ClaseD_SinTratar_Mu020 = 3
};

/**
 * @brief Propiedades geométricas y mecánicas de un tornillo según `unionbase2.dll` (`ub2eltos_*`).
 */
struct DatosTornilloCatalogo {
    std::string designacion{"M20"};
    double diametro_nominal_mm{20.0};
    double diametro_taladro_mm{22.0};              ///< `ub2eltos_diametro_maximo_taladro`
    double area_vastago_mm2{314.16};               ///< `ub2eltos_area_canya_tornillo`
    double area_resistente_traccion_mm2{245.0};    ///< `ub2eltos_area_resistente_tornillo` (As)
    double distancia_entre_caras_cabeza_mm{30.0};  ///< `ub2eltos_distancia_entre_caras_tornillo` (s)
    double distancia_entre_aristas_cabeza_mm{34.6};///< `ub2eltos_distancia_entre_aristas_tornillo` (e)
    double espesor_cabeza_mm{12.5};                ///< `ub2eltos_espesor_cabeza_tornillo`
    double espesor_tuerca_mm{18.0};                ///< `ub2eltos_espesor_tuerca`
    double diametro_exterior_arandela_mm{37.0};    ///< `ub2eltos_diametro_exterior_arandela`
    double espesor_arandela_mm{3.0};               ///< `ub2eltos_espesor_arandela`
    double par_apriete_8_8_nm{450.0};              ///< `ub2eltos_par_apriete_8_8_diametro_tornilleria_NM`
    double par_apriete_10_9_nm{650.0};             ///< `ub2eltos_par_apriete_10_9_diametro_tornilleria_NM`
};

// ==============================================================================
// 2. Subsistema de Placas de Anclaje (`placancl.dll`: `plplacan_*`, `supancl_*`, `pneleme.dll`)
// ==============================================================================

enum class TipoTerminacionPerno : std::uint8_t {
    ProlongacionRecta = 0,
    Patilla90Grados = 1,
    GanchoNormalizado180 = 2,
    PlacaAnclajeCircularExtremo = 3
};

enum class TipoMontajePernoPlaca : std::uint8_t {
    SoldadoAPlaca = 0,
    TuercaYContratuercaNivelacion = 1,
    ConPlacaAsientoIndependiente = 2
};

struct OpcionesCalculoPlacasAnclaje {
    std::string acero_placas{"S275"};
    std::string acero_pernos{"B 500 S"};
    double fck_hormigon_cimentacion_mpa{25.0};
    double espesor_mortero_nivelacion_mm{20.0};
    bool comprobar_arrancamiento_cono_hormigon{true};
    bool comprobar_aplastamiento_lateral_hormigon{true};
    bool transferir_cortante_por_rozamiento{false};
};

struct PernoAnclaje {
    std::uint32_t id{1};
    double x_local_mm{0.0};  ///< Coordenada respecto al centro de la placa (`plplacan_coordenadas_placa_desde_centro_perfil`)
    double y_local_mm{0.0};
    double diametro_mm{20.0};
    double longitud_anclaje_mm{450.0}; ///< `plplacan_longitud_anclaje_de_pernos_en_cimentacion`
    double longitud_patilla_mm{80.0};
    std::string acero_perno{"B 500 S"};
    double fyk_mpa{500.0};
    double fuk_mpa{550.0};
    TipoTerminacionPerno terminacion{TipoTerminacionPerno::Patilla90Grados};
    TipoMontajePernoPlaca montaje{TipoMontajePernoPlaca::TuercaYContratuercaNivelacion};
    double peso_unitario_kg{1.35};
};

struct RigidizadorPlacaAnclaje {
    std::uint32_t id{1};
    std::string codigo{"R1"};
    double espesor_mm{10.0};
    double altura_mm{150.0};
    double longitud_base_mm{100.0};
    double recorte_chaflan_mm{25.0};
    bool es_pasante{false}; ///< `plcompco_error_dimens_placa_rectangular_centrada_rigs_pasantes_*`
    double garganta_soldadura_placa_mm{6.0};
    double garganta_soldadura_perfil_mm{5.0};
    core::Vec2d pos_inicio_mm{0.0, 0.0};
    core::Vec2d pos_fin_mm{0.0, 0.0};
};

struct SuperficieAnclajeHormigon {
    double ancho_pedestal_x_mm{800.0};
    double largo_pedestal_y_mm{800.0};
    double canto_cimentacion_h_mm{600.0};
    double fck_mpa{25.0};
    bool tiene_canto_limite{true};        ///< `supancl_tiene_canto_limite`
    double factor_concentracion_kj{1.5};  ///< Factor de incremento homotecia $\sqrt{A_{c1}/A_{c0}} \le 3.0$
};

struct ComprobacionIndividualUnion {
    std::string codigo;
    std::string titulo;
    std::string articulo_norma;
    double valor_solicitante{0.0};
    double valor_resistente{0.0};
    std::string unidad;
    double aprovechamiento_pct{0.0};
    bool cumple{true};
    std::string expresion_justificativa;
};

struct ResultadoComprobacionPlacaAnclaje {
    bool cumple_global{true};
    double aprovechamiento_max_pct{0.0};
    std::string comprobacion_critica;

    // Tensiones bajo la placa y flexión de vuelos
    double tension_max_hormigon_mpa{0.0};
    double resistencia_calculo_hormigon_fjd_mpa{0.0};
    double tension_von_mises_placa_mpa{0.0};
    double limite_elastico_placa_fyd_mpa{0.0};

    // Esfuerzos máximos en el perno más solicitado (`plplacan_se_calcula_traccion_max_perno`)
    double traccion_max_perno_kn{0.0};
    double cortante_max_perno_kn{0.0};
    double resistencia_traccion_acero_perno_kn{0.0};
    double resistencia_adherencia_anclaje_perno_kn{0.0};
    double resistencia_cono_hormigon_perno_kn{0.0};
    double resistencia_cortante_perno_kn{0.0};
    double longitud_anclaje_necesaria_mm{0.0};

    // Medición de acero (`plplacan_peso_acero_placa_descompuesto`, `plplacan_peso_total_pernos_para_medicion`)
    double peso_placa_base_kg{0.0};
    double peso_rigidizadores_kg{0.0};
    double peso_total_pernos_kg{0.0};
    double peso_total_conjunto_kg{0.0};

    std::vector<ComprobacionIndividualUnion> comprobaciones;
    std::string texto_cuadro_arranques; ///< `plplacan_textos_cuadro_arranques`
    std::string memoria_calculo_detallada;
};

struct Prisma3dElementoUnion {
    std::string nombre;
    core::Vec3d centro_global_m{0.0, 0.0, 0.0};
    core::Vec3d semidimensiones_locales_m{0.05, 0.05, 0.01};
    core::LocalAxes3d ejes{};
    std::uint32_t color_rgb{0x94A3B8};
};

struct PlacaAnclaje3d {
    std::uint32_t id{1};
    std::uint32_t nudo_id{0};
    std::uint32_t barra_pilar_id{0};
    std::string perfil_pilar{"HEB 200"};
    std::string acero_placa{"S275"};
    double fy_placa_mpa{275.0};
    double fu_placa_mpa{430.0};

    // Geometría de la placa base (`plplacan_dimensiones_placa`, `plplacan_espesor_placa`)
    double ancho_lx_mm{350.0};
    double largo_ly_mm{350.0};
    double espesor_e_mm{20.0};
    double desp_excentrico_x_mm{0.0};
    double desp_excentrico_y_mm{0.0};
    double espesor_mortero_nivelacion_mm{20.0};

    // Soldaduras perfil -> placa base
    double garganta_soldadura_alas_mm{8.0};
    double garganta_soldadura_alma_mm{6.0};

    bool bloqueada{false}; ///< `pnbloqelemcim_panel_boton_bloqueo_desbloqueo`
    std::vector<PernoAnclaje> pernos;
    std::vector<RigidizadorPlacaAnclaje> rigidizadores;
    SuperficieAnclajeHormigon superficie_hormigon{};
    ResultadoComprobacionPlacaAnclaje ultima_comprobacion{};
    std::vector<Prisma3dElementoUnion> elementos_3d; ///< `plplacan_append_elementos_3D_placa`
};

// ==============================================================================
// 3. Subsistema de Uniones Soldadas y Atornilladas (`uniones.dll`, `unionbase.dll`, `bimjsunion.dll`)
// ==============================================================================

enum class TipologiaUnionEstructural : std::uint8_t {
    PlacaAnclajeBasePilar = 0,
    PilarVigaDintelSoldada = 1,
    PilarVigaChapaFrontalAtornillada = 2,
    CumbreraNaveChapaFrontalAtornillada = 3,
    CumbreraNaveSoldada = 4,
    EmpalmePiezasChapaFrontalAtornillada = 5,
    EmpalmePiezasCubrejuntasAtornillada = 6,
    VigaSecundariaVigaPrincipalDobleAngular = 7,
    NudoCelosiaTubular_CHS_RHS = 8,
    AnclajeTiranteConCartela = 9
};

enum class ClasificacionRigidezUnion : std::uint8_t {
    NominalmenteArticulada = 0,
    Semirrigida = 1,
    Rigida = 2
};

struct CordonSoldaduraUnion {
    std::uint32_t id{1};
    std::string descripcion{"Soldadura en ángulo ala-chapa frontal"};
    bool penetracion_completa{false};
    double garganta_efectiva_a_mm{6.0};
    double cateto_s_mm{8.5};
    double longitud_efectiva_mm{180.0};
    double coef_correlacion_beta_w{0.85};
    double fu_metal_base_mpa{430.0};
    double tension_equivalente_ed_mpa{0.0};
    double tension_resistente_rd_mpa{0.0};
    double aprovechamiento_pct{0.0};
    bool cumple{true};
};

struct GrupoTornillosUnion {
    std::uint32_t id{1};
    std::string descripcion{"Tornillería en chapa frontal"};
    std::string diametro_designacion{"M20"};
    ClaseAceroTornillo clase{ClaseAceroTornillo::Clase_8_8};
    SerieTornilleria serie{SerieTornilleria::ISO_4014_4017_Ordinario};
    bool pretensados{false};
    ClaseSuperficieRozamiento superficie_rozamiento{ClaseSuperficieRozamiento::ClaseA_Granallado_Mu050};
    std::uint32_t num_filas{3};
    std::uint32_t num_columnas{2};
    double paso_vertical_p1_mm{90.0};
    double paso_horizontal_p2_mm{100.0};
    double distancia_borde_vertical_e1_mm{45.0};
    double distancia_borde_horizontal_e2_mm{40.0};

    double traccion_max_tornillo_kn{0.0};
    double cortante_max_tornillo_kn{0.0};
    double resistencia_traccion_ft_rd_kn{0.0};
    double resistencia_cortante_fv_rd_kn{0.0};
    double resistencia_aplastamiento_fb_rd_kn{0.0};
    double resistencia_punzonamiento_bp_rd_kn{0.0};
    double aprovechamiento_max_pct{0.0};
    bool cumple{true};
};

struct ChapaRigidizadorUnion {
    std::uint32_t id{1};
    std::string nombre{"Chapa frontal"};
    double ancho_mm{200.0};
    double alto_mm{420.0};
    double espesor_mm{16.0};
    std::string acero{"S275"};
    double fy_mpa{275.0};
    double peso_kg{10.5};
};

/**
 * @brief Análisis de rigidez rotacional por el método de las componentes (EN 1993-1-8 §6 / `undtrigunion_*`).
 */
struct AnalisisRigidezRotacionalUnion {
    std::uint32_t barra_conectada_id{0};
    bool en_extremo_inicial{true};

    // Coeficientes de rigidez de las componentes básicas k_i (mm) (`ubgsnorm_Ki_componentes_union_soldada`)
    double k1_alma_pilar_cortante_mm{0.0};
    double k2_alma_pilar_compresion_mm{0.0};
    double k3_alma_pilar_traccion_mm{0.0};
    double k4_ala_pilar_flexion_mm{0.0};
    double k5_chapa_frontal_flexion_mm{0.0};
    double k10_tornillos_traccion_mm{0.0};
    double brazo_palanca_equivalente_z_mm{0.0};

    // Rigidez inicial Sj,ini y secante Sj = Sj,ini / eta (kN·m/rad)
    double rigidez_inicial_sj_ini_knm_rad{0.0};
    double coef_modificacion_rigidez_eta{2.0};
    double rigidez_secante_propuesta_sj_knm_rad{0.0};
    double rigidez_asignada_en_modelo_knm_rad{0.0};
    double momento_resistente_mj_rd_knm{0.0};
    double capacidad_rotacion_phi_cd_mrad{0.0};

    ClasificacionRigidezUnion clasificacion{ClasificacionRigidezUnion::Semirrigida};
    double diferencia_relativa_rigidez_pct{0.0};
    bool diferencia_rigidez_tolerable{true}; ///< `ubcalc_es_diferencia_rigidez_tolerable` (<= 20%)

    // Curva Momento-Curvatura / Momento-Rotación (`undtrigunion_panel_diagrama_momento_curvatura`)
    std::vector<std::pair<double, double>> curva_rotacion_mrad_momento_knm;
};

struct PlanoDetalleUnion2d {
    std::string titulo_lamina;
    std::string escala{"1:10"};
    std::vector<std::string> vistas_incluidas;
    std::vector<std::string> cotas_y_simbolos_soldadura;
    std::vector<std::string> cuadro_medicion_despiece;
};

struct UnionEstructural3d {
    std::uint32_t id{1};
    std::string referencia{"U1"};
    std::uint32_t nudo_id{0};
    std::uint32_t grupo_union_id{0}; ///< Nudos con misma tipología y geometría (`ununioned_se_pueden_agrupar`)
    TipologiaUnionEstructural tipologia{TipologiaUnionEstructural::PilarVigaChapaFrontalAtornillada};
    bool es_atornillada{true};
    bool bloqueada{false};
    bool lleva_cartela_inferior{false};
    double longitud_cartela_mm{0.0};
    double canto_cartela_mm{0.0};

    std::vector<std::uint32_t> barras_concurrentes_ids;
    std::vector<ChapaRigidizadorUnion> chapas_y_rigidizadores;
    std::vector<CordonSoldaduraUnion> soldaduras;
    std::vector<GrupoTornillosUnion> grupos_tornillos;
    std::vector<AnalisisRigidezRotacionalUnion> rigideces_rotacionales;
    std::vector<ComprobacionIndividualUnion> comprobaciones;
    std::vector<Prisma3dElementoUnion> prismas_3d; ///< `untabla_append_elementos3D_union`
    PlanoDetalleUnion2d plano_detalle{};

    bool cumple_global{true};
    double aprovechamiento_max_pct{0.0};
    double peso_total_acero_chapas_kg{0.0};
    double peso_total_tornilleria_kg{0.0};
    double longitud_total_soldadura_m{0.0};
    std::string memoria_calculo_union;
};

// ==============================================================================
// 4. Motor Principal de Uniones y Placas de Anclaje (`ConnectionsEngine`)
// ==============================================================================

class ConnectionsEngine {
public:
    // --- Catálogo de Tornillería (`unionbase2.dll`: `ub2eltos_*`) ---
    static const std::vector<DatosTornilloCatalogo>& catalogo_tornillos_metricos();
    static DatosTornilloCatalogo obtener_datos_tornillo(double diametro_mm);
    static void propiedades_mecanicas_clase_tornillo(ClaseAceroTornillo clase, double& fyb_mpa, double& fub_mpa);
    static double coeficiente_rozamiento_superficie(ClaseSuperficieRozamiento clase);

    // --- Subsistema de Placas de Anclaje (`placancl.dll`: `plplacan_*`) ---
    static PlacaAnclaje3d generar_placa_anclaje_por_defecto(
        const model::Obra& obra,
        std::uint32_t nudo_id,
        const OpcionesCalculoPlacasAnclaje& opciones = {}
    );

    static std::vector<PlacaAnclaje3d> generar_todas_las_placas_anclaje(
        const model::Obra& obra,
        const OpcionesCalculoPlacasAnclaje& opciones = {}
    );

    static ResultadoComprobacionPlacaAnclaje comprobar_placa_anclaje(
        const model::Obra& obra,
        PlacaAnclaje3d& placa,
        const solver::SolverSummary& solver_results
    );

    static ResultadoComprobacionPlacaAnclaje dimensionar_placa_anclaje(
        const model::Obra& obra,
        PlacaAnclaje3d& placa,
        const solver::SolverSummary& solver_results
    );

    static void igualar_placas_anclaje(
        const PlacaAnclaje3d& origen,
        std::vector<PlacaAnclaje3d>& destino_placas,
        const std::vector<std::uint32_t>& nudos_destino
    );

    // --- Subsistema de Uniones Soldadas y Atornilladas (`uniones.dll`, `unionbase.dll`) ---
    static std::vector<UnionEstructural3d> detectar_y_generar_uniones_obra(
        const model::Obra& obra,
        const solver::SolverSummary& solver_results,
        bool preferir_atornilladas = true,
        ClaseAceroTornillo clase_tornillos = ClaseAceroTornillo::Clase_8_8,
        bool tornillos_pretensados = false
    );

    static void comprobar_union_estructural(
        const model::Obra& obra,
        UnionEstructural3d& union_est,
        const solver::SolverSummary& solver_results
    );

    static void dimensionar_union_estructural(
        const model::Obra& obra,
        UnionEstructural3d& union_est,
        const solver::SolverSummary& solver_results
    );

    static std::size_t agrupar_uniones_equivalentes(std::vector<UnionEstructural3d>& uniones);

    static std::size_t aplicar_rigideces_rotacionales_propuestas_a_obra(
        model::Obra& obra,
        const std::vector<UnionEstructural3d>& uniones
    );

    // --- Interoperabilidad BIM / Fabricación (`bimjsunion.dll`, `unclunioned_tekla_*`, `unclunioned_cis2_*`) ---
    static std::string exportar_bimjsunion_json(
        const model::Obra& obra,
        const std::vector<UnionEstructural3d>& uniones,
        const std::vector<PlacaAnclaje3d>& placas
    );
};

} // namespace cypeclon::connections
