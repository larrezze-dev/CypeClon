#pragma once

#include "core/math.hpp"
#include "model/structure_elements.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::model {

class Obra;
struct Lamina3d;
struct Panyo3d;
struct Barra3d;
struct Nudo3d;
struct CargaLinealBarra;
struct CargaSuperficialPanyo;

// ============================================================================
// P-07.1: Subsistema de Láminas / Elementos Finitos Shell (`calcmef.dll`, `calcmallad3d.dll`, `visesf3d.dll`)
// ============================================================================

enum class FamiliaMaterialLamina : std::uint8_t {
    Hormigon = 0,
    AceroLaminado = 1,
    AceroConformado = 2,
    Aluminio = 3,
    Generico = 4
};

enum class EnraseLamina : std::uint8_t {
    CaraSuperior = 0,
    PlanoMedio = 1,
    CaraInferior = 2,
    ExcentricidadUsuario = 3
};

enum class TipoVinculacionInteriorArista : std::uint8_t {
    Empotrada = 0,   ///< `cm3dmalla_crea_vincint_arista_empotrada`
    Articulada = 1,  ///< `cm3dmalla_crea_vincint_arista_articulada`
    Libre = 2,       ///< `cm3dmalla_crea_vincint_arista_libre`
    Generica = 3     ///< `cm3dmalla_crea_vincint_arista_generica`
};

/**
 * @brief Hueco poligonal interior dentro de una lámina (`cm3dmalla_crea_hueco_lamina`).
 */
struct HuecoLamina3d {
    std::uint32_t id{0};
    std::vector<core::Vec3d> vertices;
};

/**
 * @brief Factores de rigidez ortótropos de una lámina (`lamef_edfrig`).
 */
struct FactoresRigidezLamina {
    double axil_x{1.0};
    double axil_y{1.0};
    double axil_xy{1.0};
    double flexion_x{1.0};
    double flexion_y{1.0};
    double flexion_xy{1.0};
    double cortante_xz{1.0};
    double cortante_yz{1.0};
};

/**
 * @brief Vinculación interior y exterior asignada a una arista de lámina (`lamef_vinc_int_aristas`, `lamef_vinc_ext_aristas`).
 */
struct VinculacionAristaLamina {
    std::uint32_t arista_index{0};
    TipoVinculacionInteriorArista vinc_interior{TipoVinculacionInteriorArista::Empotrada};
    double rigidez_rotacional_arista_knm_rad_m{0.0};
    VinculacionExterior vinc_exterior{VinculacionExterior::Libre};
    double rigidez_elastica_lineal_kz_kn_m2{0.0}; ///< `cm3dmalla_factor_conversion_coaccion_elastica_nudo_malla_original_puntual_a_lineal`
};

/**
 * @brief Esfuerzos unitarios de lámina en un punto (por metro de ancho) según `calcmef.dll` (`cefesf_*`).
 */
struct EsfuerzosUnitariosLamina {
    // Esfuerzos de membrana (kN/m)
    double nx_kn_m{0.0};
    double ny_kn_m{0.0};
    double nxy_kn_m{0.0};

    // Esfuerzos de flexión y torsión (kN·m/m)
    double mx_knm_m{0.0};
    double my_knm_m{0.0};
    double mxy_knm_m{0.0};

    // Cortantes transversales de placa (kN/m)
    double qx_kn_m{0.0};
    double qy_kn_m{0.0};
};

/**
 * @brief Banda de integración en una lámina (`lamef_bandas_integracion` / `cefbandint_*`).
 */
struct BandaIntegracionLamina {
    std::uint32_t id{0};
    std::string nombre{"Banda 1"};
    core::Vec3d punto_inicial{0.0, 0.0, 0.0};
    core::Vec3d punto_final{1.0, 0.0, 0.0};
    double ancho_izquierdo_m{0.50}; ///< `cefbandint_anchos_banda_integracion_en_punto`
    double ancho_derecho_m{0.50};

    double ancho_total_m() const noexcept { return ancho_izquierdo_m + ancho_derecho_m; }
    double longitud_m() const noexcept { return punto_inicial.distance_to(punto_final); }
};

/**
 * @brief Esfuerzos integrados de barra equivalente en una banda de integración (`cefbandint_integra_esfuerzos_*`).
 */
struct EsfuerzosIntegradosBanda {
    double axil_n_kn{0.0};
    double cortante_vy_kn{0.0};
    double cortante_vz_kn{0.0};
    double torsor_mt_knm{0.0};
    double flector_my_knm{0.0};
    double flector_mz_knm{0.0};
};

/**
 * @brief Cuantías de armado necesarias Wood-Armer en las 4 capas de una lámina de H.A. (`cefcal_calcula_cuantias_necesarias_combinacion`).
 */
struct CuantiasArmadoLamina {
    double asx_inf_cm2_m{0.0}; ///< `cefpunto_append_cuantia_x_inferior_en_punto`
    double asx_sup_cm2_m{0.0}; ///< `cefpunto_append_cuantia_x_superior_en_punto`
    double asy_inf_cm2_m{0.0}; ///< `cefpunto_append_cuantia_y_inferior_en_punto`
    double asy_sup_cm2_m{0.0}; ///< `cefpunto_append_cuantia_y_superior_en_punto`
    bool cumple_cuantia_minima{true};
};

/**
 * @brief Nodo y elemento finito triangular de 3 nodos (18 GDL: DKT flexión + CST membrana) de la malla de lámina (`calcmallad3d.dll`).
 */
struct NodoMallaLamina {
    std::uint32_t id{0};
    core::Vec3d pos{0.0, 0.0, 0.0};
    bool en_borde_exterior{false};
    std::int32_t arista_borde_index{-1}; ///< `cm3dmalla_es_nudo_malla_generado_pertenece_a_arista_con_vinc_exterior`
};

struct TrianguloMefLamina {
    std::uint32_t id{0};
    std::array<std::uint32_t, 3> nodo_ids{0, 0, 0};
    double area_m2{0.0};
    core::Vec3d baricentro{0.0, 0.0, 0.0};
};

struct MallaLamina3d {
    double tamano_max_elemento_m{1.00};
    std::vector<NodoMallaLamina> nodos;
    std::vector<TrianguloMefLamina> triangulos;
    double area_total_mallada_m2{0.0};
};

/**
 * @brief Motor de cálculo geométrico, mallado MEF, transformación tensorial, Von Mises y Wood-Armer de Láminas (`calcmef.dll`, `calcmallad3d.dll`).
 */
class ShellElementEngine {
public:
    static core::LocalAxes3d calcular_ejes_locales_lamina(
        const std::vector<core::Vec3d>& vertices,
        std::uint32_t arista_eje_x_index = 0,
        double angulo_giro_ejes_rad = 0.0,
        bool invertir_eje_z = false
    ) noexcept;

    static double calcular_area_poligono_3d(const std::vector<core::Vec3d>& vertices) noexcept;

    static bool es_punto_interior_lamina(
        const Lamina3d& lamina,
        const core::Vec3d& punto_3d,
        double tolerancia_m = 1.0e-3
    ) noexcept;

    // Discretización automática en elementos finitos triangulares (`cm3dmalla_genera_mallado_de_elementos_modelo3d`)
    static MallaLamina3d generar_mallado_lamina(
        const Lamina3d& lamina,
        double tamano_max_elemento_m = 1.00
    );

    // Transformación de esfuerzos de lámina al girar los ejes un ángulo theta (`ceftransesf_transforma_fuerzas_lamina`, `ceftransesf_transforma_momentos_lamina`)
    static EsfuerzosUnitariosLamina transformar_esfuerzos_lamina(
        const EsfuerzosUnitariosLamina& esf,
        double angulo_rotacion_rad
    ) noexcept;

    // Cálculo de tensión equivalente de Von Mises en cara superior, media o inferior (`cefbandint_calcula_tensiones_en_caras`, `cefbandint_tension_von_mises`)
    static double calcular_tension_von_mises_mpa(
        const EsfuerzosUnitariosLamina& esf,
        double espesor_m,
        int cara_z = 1 ///< +1 = cara superior, 0 = plano medio, -1 = cara inferior
    ) noexcept;

    // Integración de esfuerzos de lámina en una banda de integración (`cefbandint_integra_esfuerzos_todas_hipot_en_punto_banda_laminas`)
    static EsfuerzosIntegradosBanda integrar_esfuerzos_en_banda(
        const EsfuerzosUnitariosLamina& esf_en_punto,
        const BandaIntegracionLamina& banda
    ) noexcept;

    // Dimensionamiento de cuantías necesarias de armadura en losas/láminas de H.A. por Wood-Armer (`cefcal_calcula_cuantias_necesarias_combinacion`)
    static CuantiasArmadoLamina calcular_cuantias_wood_armer(
        const EsfuerzosUnitariosLamina& esf,
        double espesor_m,
        double recubrimiento_m = 0.030,
        double fcd_mpa = 16.67,
        double fyd_mpa = 434.78
    ) noexcept;
};

// ============================================================================
// P-07.2: Subsistema de Paños / Superficies de Carga (`panyos3d.dll`)
// ============================================================================

enum class AmbitoCargaSuperficial : std::uint8_t {
    TodoElPanyo = 0,      ///< `p3dcarsup_append_uniforme_en_todo_el_panyo`
    FranjaTodoX = 1,      ///< `p3dcarsup_append_uniforme_en_todo_x_de_panyo`
    FranjaTodoY = 2,      ///< `p3dcarsup_append_uniforme_en_todo_y_de_panyo`
    PoligonalEnZona = 3   ///< `p3dcarsup_append_poligonal_uniforme` / `p3dcarsup_append_poligonal_variable`
};

/**
 * @brief Motor de descomposición de cargas superficiales de Paños en cargas lineales sobre barras (`p3dcalsup_nousar_append_cargas_lineales_a_barras_panyo`).
 */
class PanelLoadDistributionEngine {
public:
    static std::vector<CargaLinealBarra> descomponer_cargas_panyo_en_barras(
        const Panyo3d& panyo,
        const std::vector<CargaSuperficialPanyo>& cargas_panyo,
        const std::vector<Barra3d>& barras_obra,
        const std::vector<Nudo3d>& nudos_obra,
        double tolerancia_coplanar_m = 0.15
    );
};

// ============================================================================
// P-08.1: Subsistema de Cargas en Nudos, Barras, Láminas y Paños (`ed3dlib.dll`, `calcesf.dll`)
// ============================================================================

enum class TipoCargaBarra : std::uint8_t {
    Uniforme = 0,               ///< Carga distribuida constante q en toda la barra
    Puntual = 1,                ///< Fuerza concentrada P en posición x_ini_rel
    Trapezoidal = 2,            ///< Carga variable linealmente q_ini -> q_fin entre [x_ini_rel, x_fin_rel]
    Triangular = 3,             ///< Carga triangular (q_ini = 0 o q_fin = 0)
    MomentoPuntual = 4,         ///< Momento concentrado M (kN·m) en posición x_ini_rel
    IncrementoTemperatura = 5,  ///< Incremento térmico uniforme dT_unif (°C) + gradiente dT_grad (°C)
    Pretensado = 6              ///< Axil de pretensado N0 (kN) con excentricidad ez (m)
};

enum class SistemaEjesCarga : std::uint8_t {
    Generales = 0,
    LocalesElemento = 1,
    ProyeccionPlanta = 2
};

enum class TipoCargaLamina : std::uint8_t {
    SuperficialUniforme = 0,    ///< `cm3dmalla_append_carga_superficial_a_lamina`
    SuperficialSubpoligono = 1, ///< Carga superficial acotada a un sub-polígono interior
    PuntualEnLamina = 2,        ///< `cm3dmalla_append_carga_puntual_a_lamina`
    LinealEnArista = 3,         ///< Carga lineal sobre una arista de la lámina
    TermicaLamina = 4           ///< Incremento uniforme y gradiente térmico en el espesor de la lámina
};

/**
 * @brief Carga aplicada sobre una Lámina 3D (`nueva_carga_lamina` / `cm3dmalla_append_carga_*_a_lamina`).
 */
struct CargaLamina3d {
    std::uint32_t id{0};
    std::uint32_t lamina_id{0};
    std::uint32_t hipotesis_id{1};
    TipoCargaLamina tipo{TipoCargaLamina::SuperficialUniforme};
    SistemaEjesCarga sistema_ejes{SistemaEjesCarga::Generales};
    double valor_kn_m2_o_kn{0.0};
    core::Vec3d direccion{0.0, 0.0, -1.0};
    core::Vec3d punto_aplicacion{0.0, 0.0, 0.0};
    std::vector<core::Vec3d> subpoligono;
    double delta_temp_uniforme_c{0.0};
    double delta_temp_gradiente_c{0.0};
};

/**
 * @brief Reacciones de empotramiento perfecto de una carga sobre barra (para ensamblaje matricial en `calcesf.dll`).
 */
struct ReaccionesEmpotramientoCargaBarra {
    double f_local_ini_kn{0.0};
    double f_local_fin_kn{0.0};
    double m_empotramiento_ini_knm{0.0};
    double m_empotramiento_fin_knm{0.0};
    double axil_termico_o_pretensado_kn{0.0};
};

/**
 * @brief Configuración de visualización de cargas y escalas de dibujo (`Carga_hipotesis_vista`, `Carga_escalas`).
 */
struct ConfiguracionVisualizacionCargas {
    std::uint32_t hipotesis_activa_vista_id{1};
    bool mostrar_cargas_visibles{true};
    bool mostrar_valores_numericos{true};
    double escala_cargas_puntuales{1.0};
    double escala_cargas_lineales{1.0};
    double escala_cargas_superficiales{1.0};
    double escala_cargas_termicas{1.0};
};

/**
 * @brief Utilidades de cálculo de cargas equivalentes y empotramiento perfecto en barras (`calcesf.dll`).
 */
class MemberLoadCalculator {
public:
    static ReaccionesEmpotramientoCargaBarra calcular_empotramiento_perfecto(
        const CargaLinealBarra& carga,
        double longitud_barra_m,
        double area_seccion_m2 = 0.005,
        double inercia_m4 = 1.0e-4,
        double canto_h_m = 0.20,
        double e_mpa = 210000.0,
        double alpha_termico = 1.2e-5
    ) noexcept;
};

} // namespace cypeclon::model

