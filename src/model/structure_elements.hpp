#pragma once

#include "core/math.hpp"
#include "units/units.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::model {

class Obra;

// ============================================================================
// P-06.1: Operaciones y Propiedades de Nudos (`ed3dlib.dll`, `calcesf.dll`)
// ============================================================================

/**
 * @brief Tipos de vinculación exterior de un nudo 3D (`Nudo_Describir_Vinc_Ext` / `esport3d_*`).
 */
enum class VinculacionExterior : std::uint8_t {
    Libre = 0,                        ///< Nudo sin coacción exterior (`esport3d_crea_nudo_libre_6gdl`)
    ArticuladoFijo = 1,               ///< Coaccionados desplazamientos (Ux, Uy, Uz), giros libres
    Empotrado = 2,                    ///< Coaccionados los 6 GDL (Ux, Uy, Uz, Rx, Ry, Rz)
    ApoyoElastico = 3,                ///< Muelles elásticos lineales (Kx, Ky, Kz, Ktx, Kty, Ktz)
    DeslizaderaPlanoXY = 4,           ///< Desplazamiento libre en plano XY, coaccionado Uz (`esport3d_append_coaccion_plano`)
    DeslizaderaPlanoXZ = 5,           ///< Desplazamiento libre en plano XZ, coaccionado Uy
    DeslizaderaPlanoYZ = 6,           ///< Desplazamiento libre en plano YZ, coaccionado Ux
    DeslizaderaPlanoInclinado = 7,    ///< Desplazamiento libre en plano con vector normal dado (`esport3d_append_coaccion_plano`)
    DeslizaderaRectaLibre = 8,        ///< Desplazamiento libre únicamente según recta con vector director (`esport3d_append_coaccion_recta`)
    ApoyoNoLinealSoloCompresion = 9,  ///< Apoyo con levantamiento (solo activo a compresión)
    ApoyoNoLinealSoloTraccion = 10,   ///< Anclaje / colgante con despegue (solo activo a tracción)
    CoaccionGenericaPorGDL = 11       ///< Selección individual de cada uno de los 6 GDL globales
};

/**
 * @brief Tipo de vinculación interior entre las barras concurrentes en un nudo (`Nudo_Describir_Vinc_Int`).
 */
enum class TipoVinculacionInterior : std::uint8_t {
    Empotrado = 0,             ///< Nudo rígido (grado de empotramiento alpha = 1.0)
    Articulado = 1,            ///< Nudo articulado / rótula esférica (alpha = 0.0)
    ParcialmenteEmpotrado = 2  ///< Empotramiento parcial genérico (0.0 < alpha < 1.0)
};

/**
 * @brief Parámetros detallados de vinculación exterior, apoyos elásticos/no lineales y desplazamientos prescritos.
 */
struct DatosVinculacionExteriorNudo {
    std::array<bool, 6> gdl_coaccionados{false, false, false, false, false, false}; ///< [Ux, Uy, Uz, Rx, Ry, Rz]
    core::Vec3d vector_direccion_o_normal{0.0, 0.0, 1.0}; ///< Normal al plano o director de recta de deslizadera

    // Constantes elásticas traslacionales (kN/m) y rotacionales (kN·m/rad)
    double kx_kn_m{0.0};
    double ky_kn_m{0.0};
    double kz_kn_m{0.0};
    double ktx_knm_rad{0.0};
    double kty_knm_rad{0.0};
    double ktz_knm_rad{0.0};

    // Propiedades de apoyos no lineales con levantamiento
    bool solo_compresion{false};
    bool solo_traccion{false};
    bool giros_coaccionados{false};
    core::Vec3d sentido_apoyo_no_lineal{0.0, 0.0, 1.0};

    // Desplazamientos (m) y giros (rad) prescritos en apoyos (`Carga_nudo_desp_y_giros_prescritos`)
    core::Vec3d desp_prescrito_m{0.0, 0.0, 0.0};
    core::Vec3d giro_prescrito_rad{0.0, 0.0, 0.0};

    static DatosVinculacionExteriorNudo from_tipo(VinculacionExterior tipo, bool coaccionar_giros = false) noexcept;
};

/**
 * @brief Ligadura de desplazamientos entre un grupo de nudos (`Nudo_Ligaduras` / `esport3d_ligadura_crea`).
 */
struct LigaduraDesplazamientoNudos {
    std::uint32_t id{0};
    std::string nombre{"Ligadura 1"};
    std::vector<std::uint32_t> nudo_ids;
    bool ligar_dx{true};
    bool ligar_dy{true};
    bool ligar_dz{false};
};

/**
 * @brief Diafragma rígido en planta (`esport3d_diafragma_crea` / `opc_diafragmas` / `gen_diafrags`).
 */
struct DiafragmaRigidoPlanta {
    std::uint32_t id{0};
    std::string nombre{"Diafragma 1"};
    double cota_z_m{0.0};
    std::vector<std::uint32_t> nudo_ids;
    bool activo{true};
};

// ============================================================================
// P-06.2: Bibliotecas de Perfiles y Secciones Compuestas (`bibperf3`, `seccion`, `gesserperf`, `piezamet`, `pzmadera`)
// ============================================================================

enum class FamiliaMaterialPerfil : std::uint8_t {
    AceroLaminado = 0,
    AceroConformado = 1,
    AceroArmado = 2,
    Madera = 3,
    Aluminio = 4,
    Hormigon = 5,
    Generico = 6
};

enum class GeometriaSeccionTipo : std::uint8_t {
    DobleT = 0,             ///< IPE, HEB, HEA, HEM, W
    CanalUPN = 1,           ///< UPN, U laminado
    AngularSimetrico = 2,   ///< L de lados iguales
    AngularAsimetrico = 3,  ///< LD de lados desiguales
    TeSimple = 4,           ///< T simple laminada o armada
    TuboCuadradoSHS = 5,    ///< Tubo hueco cuadrado SHS
    TuboRectangularRHS = 6, ///< Tubo hueco rectangular RHS
    TuboCircularCHS = 7,    ///< Tubo hueco circular CHS
    RedondoMacizo = 8,      ///< Barra circular maciza (tirantes R)
    ConformadoC = 9,        ///< Canal rigidizado conformado en frío (CF)
    ConformadoZ = 10,       ///< Zeta rigidizada conformada en frío (ZF)
    ConformadoU = 11,       ///< Canal simple conformado en frío (UF)
    ConformadoOmega = 12,   ///< Perfil Omega conformado en frío
    RectangularMacizo = 13, ///< Sección rectangular maciza (madera / hormigón / pletina)
    CircularMacizo = 14,    ///< Sección circular maciza (madera / hormigón / aluminio)
    Generico = 15           ///< Sección genérica definida por sus constantes mecánicas
};

/**
 * @brief Disposiciones simples y compuestas de perfiles (`calcomp_*`, `mtlbasdibu_*`).
 */
enum class DisposicionCompuestaPerfil : std::uint8_t {
    Simple = 0,                       ///< Perfil simple único
    DobleEnCajonSoldado = 1,          ///< 2 perfiles enfrentados formando cajón cerrado soldado
    DobleEnCajonPresillas = 2,        ///< 2 perfiles enfrentados unidos por presillas/empresillado
    DobleEnIConUnionContinua = 3,     ///< 2 perfiles adosados por el alma formando doble T
    DobleEnIConPresillas = 4,         ///< 2 perfiles de espaldas unidos con presillas
    DobleAngularEnT = 5,              ///< 2 angulares de espaldas en T
    DobleAngularEnCruz = 6,           ///< 2 angulares en estrella / cruz
    VigaAlveolarCastelada = 7,        ///< Viga Boyd / alveolar de alma aligerada (`calcomp_parametros_calculo_esfuerzos_viga_boyd`)
    ConPlatabandasLaterales = 8       ///< Perfil doble T reforzado con platabandas (`calcomp_parametros_doblete_con_platabandas_laterales_exteriores`)
};

/**
 * @brief Cartela simple o doble en extremo de barra (`calcomp_*_doblete_con_cartelas`).
 */
struct CartelaExtremoBarra {
    bool activa_ini{false};
    bool activa_fin{false};
    bool doble_cartela_sup_inf{false}; ///< false = cartela simple inferior, true = cartela doble simétrica
    double longitud_ini_m{1.20};
    double canto_adicional_ini_mm{200.0};
    double longitud_fin_m{1.20};
    double canto_adicional_fin_mm{200.0};
};

/**
 * @brief Propiedades geométricas y mecánicas completas de un perfil (`bp3perfil_parametros_*` / `calsec_*`).
 */
struct ProfileSectionProperties {
    std::string referencia{"IPE 200"};
    std::string serie{"IPE"};
    FamiliaMaterialPerfil familia{FamiliaMaterialPerfil::AceroLaminado};
    GeometriaSeccionTipo tipo_forma{GeometriaSeccionTipo::DobleT};

    // Dimensiones principales de sección transversal (mm)
    double h_mm{200.0};      ///< Canto total h (mm)
    double b_mm{100.0};      ///< Ancho total b (mm)
    double tw_mm{5.6};       ///< Espesor de alma tw (mm)
    double tf_mm{8.5};       ///< Espesor de ala tf (mm)
    double r_mm{12.0};       ///< Radio de acuerdo / plegado r (mm)
    double labio_c_mm{0.0};  ///< Longitud de labio rigidizador c en conformados (mm)

    // Constantes mecánicas en unidades de catálogo estructural (cm^2, cm^4, cm^3, cm^6)
    double area_cm2{28.5};   ///< Área bruta A (cm^2)
    double avy_cm2{14.0};    ///< Área de cortante según eje Y local Avy (cm^2)
    double avz_cm2{14.0};    ///< Área de cortante según eje Z local Avz (cm^2)
    double iy_cm4{1943.0};   ///< Momento de inercia eje fuerte Iy (cm^4)
    double iz_cm4{142.0};    ///< Momento de inercia eje débil Iz (cm^4)
    double it_cm4{7.02};     ///< Módulo de torsión uniforme de Saint-Venant It (cm^4)
    double iw_cm6{12990.0};  ///< Constante de alabeo Iw (cm^6)
    double wely_cm3{194.3};  ///< Módulo resistente elástico eje fuerte Wel,y (cm^3)
    double welz_cm3{28.5};   ///< Módulo resistente elástico eje débil Wel,z (cm^3)
    double wply_cm3{220.6};  ///< Módulo resistente plástico eje fuerte Wpl,y (cm^3)
    double wplz_cm3{44.6};   ///< Módulo resistente plástico eje débil Wpl,z (cm^3)
    double ry_cm{8.26};      ///< Radio de giro eje fuerte iy (cm)
    double rz_cm{2.24};      ///< Radio de giro eje débil iz (cm)
    double peso_kg_m{22.4};  ///< Masa por unidad de longitud (kg/m)
    double masividad_am_v_m_inv{279.0}; ///< Factor de forma / masividad Am/V (m^-1)

    // Conversiones directas al Sistema Internacional (m^2, m^4, m^3, m^6)
    double area_m2() const noexcept { return area_cm2 * 1.0e-4; }
    double avy_m2() const noexcept { return avy_cm2 * 1.0e-4; }
    double avz_m2() const noexcept { return avz_cm2 * 1.0e-4; }
    double iy_m4() const noexcept { return iy_cm4 * 1.0e-8; }
    double iz_m4() const noexcept { return iz_cm4 * 1.0e-8; }
    double it_m4() const noexcept { return it_cm4 * 1.0e-8; }
    double iw_m6() const noexcept { return iw_cm6 * 1.0e-12; }
    double wely_m3() const noexcept { return wely_cm3 * 1.0e-6; }
    double welz_m3() const noexcept { return welz_cm3 * 1.0e-6; }
    double wply_m3() const noexcept { return wply_cm3 * 1.0e-6; }
    double wplz_m3() const noexcept { return wplz_cm3 * 1.0e-6; }
};

/**
 * @brief Gestor de bibliotecas de perfiles equivalente a `bibperf3.dll`, `seccion.dll` y `gesserperf.dll`.
 */
class ProfileLibraryManager {
public:
    ProfileLibraryManager();

    const std::vector<ProfileSectionProperties>& catalog() const noexcept { return catalog_; }
    std::vector<std::string> list_series(std::optional<FamiliaMaterialPerfil> familia = std::nullopt) const;
    std::vector<ProfileSectionProperties> profiles_in_series(std::string_view serie_name) const;
    const ProfileSectionProperties* find_profile(std::string_view referencia) const noexcept;

    void register_custom_profile(const ProfileSectionProperties& prop);

    // Constructores paramétricos exactos (`calsec_*`, `calconf_*`, `calcomp_*`)
    static ProfileSectionProperties compute_double_t(
        std::string_view ref, std::string_view serie,
        double h_mm, double b_mm, double tw_mm, double tf_mm, double r_mm,
        FamiliaMaterialPerfil familia = FamiliaMaterialPerfil::AceroLaminado,
        double density_kg_m3 = 7850.0
    ) noexcept;

    static ProfileSectionProperties compute_channel_upn(
        std::string_view ref, std::string_view serie,
        double h_mm, double b_mm, double tw_mm, double tf_mm, double r_mm
    ) noexcept;

    static ProfileSectionProperties compute_angle_l(
        std::string_view ref, std::string_view serie,
        double h_mm, double b_mm, double t_mm, double r_mm = 0.0
    ) noexcept;

    static ProfileSectionProperties compute_hollow_rect(
        std::string_view ref, std::string_view serie,
        double h_mm, double b_mm, double t_mm,
        FamiliaMaterialPerfil familia = FamiliaMaterialPerfil::AceroLaminado,
        double density_kg_m3 = 7850.0
    ) noexcept;

    static ProfileSectionProperties compute_hollow_circ(
        std::string_view ref, std::string_view serie,
        double d_ext_mm, double t_mm,
        FamiliaMaterialPerfil familia = FamiliaMaterialPerfil::AceroLaminado,
        double density_kg_m3 = 7850.0
    ) noexcept;

    static ProfileSectionProperties compute_cold_formed(
        std::string_view ref, std::string_view serie,
        GeometriaSeccionTipo shape,
        double h_mm, double b_mm, double lip_c_mm, double t_mm
    ) noexcept;

    static ProfileSectionProperties compute_solid_rect(
        std::string_view ref, std::string_view serie,
        double b_mm, double h_mm,
        FamiliaMaterialPerfil familia = FamiliaMaterialPerfil::Madera,
        double density_kg_m3 = 420.0
    ) noexcept;

    static ProfileSectionProperties compute_solid_circ(
        std::string_view ref, std::string_view serie,
        double d_mm,
        FamiliaMaterialPerfil familia = FamiliaMaterialPerfil::AceroLaminado,
        double density_kg_m3 = 7850.0
    ) noexcept;

    // Cálculo de propiedades de perfiles compuestos y acartelados (`calcomp_*`)
    static ProfileSectionProperties compute_composite_section(
        const ProfileSectionProperties& base,
        DisposicionCompuestaPerfil disposicion,
        double separacion_mm = 0.0,
        double espesor_platabanda_mm = 0.0,
        double ancho_platabanda_mm = 0.0
    ) noexcept;

    static ProfileSectionProperties compute_haunched_section_at_end(
        const ProfileSectionProperties& base,
        double canto_adicional_cartela_mm,
        bool doble_cartela = false
    ) noexcept;

private:
    std::vector<ProfileSectionProperties> catalog_;
};

/**
 * @brief Pieza continua formada por una cadena colineal de barras (`Barra_CrearPieza`).
 */
struct PiezaEstructural {
    std::uint32_t id{0};
    std::string nombre{"Pieza 1"};
    std::vector<std::uint32_t> barra_ids;
    std::uint32_t nudo_extremo_inicial_id{0};
    std::uint32_t nudo_extremo_final_id{0};
    double longitud_total_m{0.0};
};

/**
 * @brief Agrupación de barras que comparten sección, material y dimensionamiento (`Barra_Agrupar`).
 */
struct AgrupacionBarras {
    std::uint32_t id{0};
    std::string nombre{"Agrupación 1"};
    std::vector<std::uint32_t> barra_ids;
    std::string perfil_comun{"IPE 200"};
    std::string material_comun{"S275"};
};

// ============================================================================
// P-06.3: Propiedades Mecánicas de Barras (`elements`, `compflecha`, `dathipfle`, `mtlbas`, `pushover`)
// ============================================================================

/**
 * @brief Puntos cardinales de enrase/ajuste de la sección respecto al eje de introducción (`Barra_DescribirEnrase`).
 */
enum class PuntoAjusteSeccion : std::uint8_t {
    CentroGravedad = 0,
    SuperiorIzquierda = 1,
    SuperiorCentro = 2,
    SuperiorDerecha = 3,
    MedioIzquierda = 4,
    MedioCentro = 5,
    MedioDerecha = 6,
    InferiorIzquierda = 7,
    InferiorCentro = 8,
    InferiorDerecha = 9
};

/**
 * @brief Disposición espacial de la sección de una barra (ángulo, enrase y excentricidades).
 */
struct DisposicionSeccionBarra {
    double angulo_giro_rad{0.0};
    PuntoAjusteSeccion punto_ajuste{PuntoAjusteSeccion::CentroGravedad};
    double desp_y_ini_m{0.0};
    double desp_z_ini_m{0.0};
    double desp_y_fin_m{0.0};
    double desp_z_fin_m{0.0};
    DisposicionCompuestaPerfil disposicion_compuesta{DisposicionCompuestaPerfil::Simple};
    double separacion_perfiles_mm{0.0};
    double separacion_presillas_m{0.80};
    CartelaExtremoBarra cartelas{};

    // Calcula el desplazamiento total del CDG en (y, z) locales sumando el punto de ajuste y la excentricidad
    core::Vec2d offset_cdg_extremo_inicial_m(double b_m, double h_m) const noexcept;
    core::Vec2d offset_cdg_extremo_final_m(double b_m, double h_m) const noexcept;
};

enum class ModoCalculoPandeo : std::uint8_t {
    SinPandeo = 0,                      ///< beta = 0
    CoeficienteBetaUsuario = 1,         ///< Usuario define beta_xy / beta_xz
    LongitudEfectivaLkUsuario = 2,      ///< Usuario define Lk_xy / Lk_xz (m)
    AutoIntraslacional = 3,             ///< Cálculo aproximado estructura intraslacional (`vpandeo_beta_intraslacional`)
    AutoTraslacional = 4                ///< Cálculo aproximado estructura traslacional (`vpandeo_beta_traslacional`)
};

/**
 * @brief Parámetros de pandeo por compresión/flexión en los planos XY y XZ (`elements.dll`: `vpandeo_*`).
 */
struct DatosPandeoBarra {
    ModoCalculoPandeo modo_xy{ModoCalculoPandeo::CoeficienteBetaUsuario};
    ModoCalculoPandeo modo_xz{ModoCalculoPandeo::CoeficienteBetaUsuario};
    double beta_xy{1.0};
    double beta_xz{1.0};
    double lk_xy_m{0.0};
    double lk_xz_m{0.0};
    double beta_torsion{1.0};

    double longitud_efectiva_xy_m(double longitud_barra_m) const noexcept;
    double longitud_efectiva_xz_m(double longitud_barra_m) const noexcept;

    // Fórmulas normativas de Julian-Lawrence / Wood (`vpandeo_beta_intraslacional` y `vpandeo_beta_traslacional`)
    static double vpandeo_beta_intraslacional(double ga, double gb) noexcept;
    static double vpandeo_beta_traslacional(double ga, double gb) noexcept;
};

/**
 * @brief Parámetros de pandeo lateral por flexión (`Barra_Pandeo_Lateral`).
 */
struct DatosPandeoLateralBarra {
    bool comprobar_ala_superior{true};
    bool comprobar_ala_inferior{true};
    double beta_lt_ala_sup{1.0};
    double beta_lt_ala_inf{1.0};
    double lc_ala_sup_m{0.0};  ///< 0.0 = usa longitud total de barra * beta_lt_ala_sup
    double lc_ala_inf_m{0.0};  ///< 0.0 = usa longitud total de barra * beta_lt_ala_inf
    double coeficiente_c1{1.0};
    bool calcular_c1_automatico{true};

    // Calcula el momento crítico elástico de pandeo lateral Mcr (kN·m) según EC3 / CTE DB SE-A
    double compute_mcr_knm(
        const ProfileSectionProperties& profile,
        double longitud_barra_m,
        bool ala_superior = true,
        double e_mpa = 210000.0,
        double g_mpa = 80769.23
    ) const noexcept;
};

enum class TipoMedicionFlecha : std::uint8_t {
    SecanteEntreExtremos = 0,
    TangenteExtremoInicial = 1,
    TangenteExtremoFinal = 2
};

enum class CriterioLimiteFlecha : std::uint8_t {
    RelativoLN = 0,
    AbsolutoMm = 1,
    MenorDeAmbos = 2
};

struct LimiteFlechaEspecificacion {
    bool activa{true};
    CriterioLimiteFlecha criterio{CriterioLimiteFlecha::RelativoLN};
    double divisor_relativo_l_n{300.0}; ///< L / N (p.ej. L/300, L/400, L/500)
    double limite_absoluto_mm{10.0};    ///< Límite absoluto en mm

    double flecha_admisible_mm(double longitud_referencia_m) const noexcept;
};

/**
 * @brief Límites de flecha de una barra (`compflecha.dll`, `dathipfle.dll`, `svgrulimfle_*`).
 */
struct DatosFlechaBarra {
    TipoMedicionFlecha tipo_medicion{TipoMedicionFlecha::SecanteEntreExtremos};
    LimiteFlechaEspecificacion flecha_instantanea_sobrecarga{true, CriterioLimiteFlecha::RelativoLN, 350.0, 0.0};
    LimiteFlechaEspecificacion flecha_activa{true, CriterioLimiteFlecha::MenorDeAmbos, 400.0, 10.0};
    LimiteFlechaEspecificacion flecha_total_plazo_infinito{true, CriterioLimiteFlecha::RelativoLN, 300.0, 0.0};
    std::uint32_t grupo_flecha_id{0};
};

/**
 * @brief Grupo de flecha formado por una o varias barras colineales (`Barra_Crear_grupo_flecha`).
 */
struct GrupoFlecha {
    std::uint32_t id{0};
    std::string nombre{"Grupo de flecha 1"};
    std::vector<std::uint32_t> barra_ids;
    std::uint32_t nudo_extremo_a_id{0};
    std::uint32_t nudo_extremo_b_id{0};
    double longitud_referencia_m{0.0};
    TipoMedicionFlecha tipo_medicion{TipoMedicionFlecha::SecanteEntreExtremos};
};

/**
 * @brief Coeficientes de empotramiento y rigideces rotacionales en extremos de barra (`mtlbasexprigrot_*`).
 */
struct EmpotramientoExtremosBarra {
    // Coeficientes de empotramiento [0.0 = articulado, 1.0 = perfectamente empotrado]
    double emp_xy_ini{1.0};
    double emp_xy_fin{1.0};
    double emp_xz_ini{1.0};
    double emp_xz_fin{1.0};
    double emp_torsion_ini{1.0};
    double emp_torsion_fin{1.0};

    // Rigideces rotacionales explícitas de uniones semirrígidas (kN·m/rad)
    bool usar_rigidez_rotacional_ini{false};
    bool usar_rigidez_rotacional_fin{false};
    double k_rot_xy_ini_knm_rad{0.0};
    double k_rot_xz_ini_knm_rad{0.0};
    double k_rot_xy_fin_knm_rad{0.0};
    double k_rot_xz_fin_knm_rad{0.0};

    static EmpotramientoExtremosBarra biarticulado() noexcept {
        return EmpotramientoExtremosBarra{0.0, 0.0, 0.0, 0.0, 1.0, 1.0};
    }
};

/**
 * @brief Recuadro arriostrado mediante pareja de tirantes en X (`esp3dcmb_append_cuadro_arriostramiento`).
 */
struct CuadroArriostramientoTirantes {
    std::uint32_t id{0};
    std::uint32_t barra_diagonal_1_id{0};
    std::uint32_t barra_diagonal_2_id{0};
    std::array<std::uint32_t, 4> nudos_recuadro{0, 0, 0, 0};
    bool geometricamente_valido{false};
    double angulo_diagonal_deg{45.0};
    std::string mensaje_validacion;
};

enum class TipoRotulaPlastica : std::uint8_t {
    FlexionEjeFuerteM3 = 0,
    FlexionEjeDebilM2 = 1,
    CortanteV2 = 2,
    AxilP = 3,
    InteraccionPM2M3 = 4
};

/**
 * @brief Definición de rótula plástica en la biblioteca de la obra (`pushover.dll`: `puobibrotplas`).
 */
struct DefinicionRotulaPlastica {
    std::uint32_t id{0};
    std::string nombre{"FEMA356-Acero-Flexion-M3"};
    TipoRotulaPlastica tipo{TipoRotulaPlastica::FlexionEjeFuerteM3};
    double factor_endurecimiento_post_fluencia{0.03};
    double rotacion_plastica_io_rad{0.010};  ///< Immediate Occupancy (IO)
    double rotacion_plastica_ls_rad{0.025};  ///< Life Safety (LS)
    double rotacion_plastica_cp_rad{0.040};  ///< Collapse Prevention (CP)
    double rotacion_plastica_ult_rad{0.050}; ///< Punto C -> D caída de resistencia
    double capacidad_residual_c{0.20};       ///< Resistencia residual tras fallo (fracción de Mp)
};

/**
 * @brief Asignación de rótulas plásticas en los extremos de una barra (`puorotplasbar`).
 */
struct AsignacionRotulaPlasticaBarra {
    bool tiene_rotula_ini{false};
    std::uint32_t rotula_ini_id{0};
    double pos_relativa_ini{0.05}; ///< Distancia relativa d/L desde nudo inicial

    bool tiene_rotula_fin{false};
    std::uint32_t rotula_fin_id{0};
    double pos_relativa_fin{0.95}; ///< Distancia relativa 1 - d/L desde nudo inicial
};

// ============================================================================
// P-06.4: Pilares y Vigas de Hormigón Armado / Mixtos (`pilargeo`, `pilares`, `pilararm`, `seccviga`, `porvigas`, `comppilhorm`, `discap`)
// ============================================================================

enum class TipoSeccionPilar : std::uint8_t {
    HormigonRectangular = 0,
    HormigonCircular = 1,
    MetalicoPerfil = 2,
    Madera = 3,
    MixtoPerfilEmbebido = 4,
    MixtoTubularRellenoHormigon = 5
};

enum class PuntoFijoPilar : std::uint8_t {
    Centro = 0,
    CaraNorte = 1,
    CaraSur = 2,
    CaraEste = 3,
    CaraOeste = 4,
    EsquinaNE = 5,
    EsquinaNO = 6,
    EsquinaSE = 7,
    EsquinaSO = 8
};

/**
 * @brief Despiece de armado longitudinal y transversal de un tramo de pilar (`pilconfarm_*` / `apsecarm_*`).
 */
struct ArmadoTramoPilar {
    std::uint32_t num_barras_esquina{4};
    std::uint32_t diametro_esquina_mm{16};
    std::uint32_t num_barras_cara_x{0};   ///< Nº de barras intermedias en cada una de las 2 caras X
    std::uint32_t diametro_cara_x_mm{12};
    std::uint32_t num_barras_cara_y{0};   ///< Nº de barras intermedias en cada una de las 2 caras Y
    std::uint32_t diametro_cara_y_mm{12};

    // Para sección circular (`pilconfarm_entradas_armado_longitudinal_seccion_circular`)
    std::uint32_t num_barras_circular{6};
    std::uint32_t diametro_circular_mm{16};

    // Armadura transversal (estribos y ramas)
    std::uint32_t diametro_estribo_mm{8};
    double separacion_zona_central_cm{15.0};
    double separacion_zona_critica_cm{10.0};
    double longitud_zona_critica_m{0.60};
    std::uint32_t num_ramas_x{2};
    std::uint32_t num_ramas_y{2};

    double area_total_longitudinal_cm2(bool es_circular = false) const noexcept;
    double cuantia_geometrica(double area_hormigon_cm2, bool es_circular = false) const noexcept;
};

/**
 * @brief Tramo de pilar entre dos plantas consecutivas (`eppnarsec_panel_secciones_por_planta`).
 */
struct TramoPilarPlanta {
    std::uint32_t nivel_inferior_id{0};
    std::uint32_t nivel_superior_id{0};
    double cota_inf_m{0.0};
    double cota_sup_m{3.0};
    std::uint32_t barra_3d_id{0};

    TipoSeccionPilar tipo_seccion{TipoSeccionPilar::HormigonRectangular};
    double ancho_x_m{0.30};
    double ancho_y_m{0.30};
    double diametro_m{0.35};
    std::string perfil_metalico{"HEB 160"};
    std::string hormigon{"C25/30"};
    std::string acero_barras{"B 500 S"};

    double recubrimiento_geometrico_m{0.030};
    double recubrimiento_mecanico_m{0.045};

    // Coeficientes de rigidez y empotramiento por tramo
    double coef_rigidez_axil{1.0};
    double coef_rigidez_flexion_x{1.0};
    double coef_rigidez_flexion_y{1.0};
    double coef_rigidez_torsion{0.20};
    double coef_empotramiento_pie{1.0};
    double coef_empotramiento_cabeza{1.0};
    double beta_pandeo_x{1.0};
    double beta_pandeo_y{1.0};

    ArmadoTramoPilar armado{};

    double area_bruta_hormigon_cm2() const noexcept;
    double capacidad_axil_plastica_kn(double fcd_mpa = 16.67, double fyd_mpa = 434.78, double fy_perfil_mpa = 275.0, double area_perfil_cm2 = 0.0) const noexcept;
};

/**
 * @brief Elemento estructural Pilar de edificación multi-planta (`pilargeo.dll` / `pilararm.dll`).
 */
struct PilarEstructural {
    std::uint32_t id{0};
    std::string referencia{"P1"};
    core::Vec2d pos_planta_xy{0.0, 0.0};
    double angulo_giro_deg{0.0};
    PuntoFijoPilar punto_fijo{PuntoFijoPilar::Centro};
    bool con_vinculacion_exterior{true};
    bool bloqueado_armado{false};
    std::vector<TramoPilarPlanta> tramos;
};

/**
 * @brief Resultado de comprobación normativa y diseño por capacidad sísmica de un tramo de pilar (`comppilhorm`, `discap`).
 */
struct ResultadoComprobacionPilar {
    bool cumple_global{true};
    bool cumple_dimension_minima{true};
    bool cumple_cuantia_min_max{true};
    bool cumple_separacion_estribos{true};
    bool cumple_capacidad_flexion_nudo{true};
    double cuantia_dispuesta{0.0};
    double cuantia_minima_norma{0.004};
    double cuantia_maxima_norma{0.040};
    double separacion_max_central_adm_cm{20.0};
    double separacion_max_critica_adm_cm{10.0};
    double cortante_capacidad_sismica_kn{0.0};
    double aprovechamiento_maximo{0.0};
    std::vector<std::string> mensajes_comprobacion;
};

enum class TipoSeccionViga : std::uint8_t {
    Rectangular = 0,
    SeccionEnT = 1,
    SeccionEnLDerecha = 2,
    SeccionEnLIzquierda = 3,
    CelosiaHormigon = 4,
    Pretensada = 5,
    VigaCimentacion = 6,
    ZunchoBorde = 7
};

/**
 * @brief Armado longitudinal (montaje, refuerzos y piel) y transversal de un vano de viga (`pvarmado_*`).
 */
struct ArmadoVanoViga {
    // Armadura de montaje corrida
    std::uint32_t num_montaje_sup{2};
    std::uint32_t diametro_montaje_sup_mm{12};
    std::uint32_t num_montaje_inf{2};
    std::uint32_t diametro_montaje_inf_mm{12};

    // Refuerzos de negativos (apoyo izquierdo y derecho) y positivos (centro de vano)
    std::uint32_t num_ref_sup_izq{2};
    std::uint32_t diametro_ref_sup_izq_mm{16};
    double longitud_ref_sup_izq_m{1.50};

    std::uint32_t num_ref_sup_der{2};
    std::uint32_t diametro_ref_sup_der_mm{16};
    double longitud_ref_sup_der_m{1.50};

    std::uint32_t num_ref_inf_vano{2};
    std::uint32_t diametro_ref_inf_vano_mm{16};

    // Armadura de piel (por cara lateral)
    std::uint32_t num_barras_piel_por_cara{0};
    std::uint32_t diametro_piel_mm{10};

    // Armadura transversal (estribos en 3 zonas: extremo izq, centro, extremo der)
    std::uint32_t diametro_estribo_mm{8};
    std::uint32_t num_ramas_estribo{2};
    double sep_extremo_izq_cm{10.0};
    double sep_central_cm{20.0};
    double sep_extremo_der_cm{10.0};
    double longitud_zona_confinada_extremos_m{1.00};

    double area_superior_apoyo_izq_cm2() const noexcept;
    double area_superior_apoyo_der_cm2() const noexcept;
    double area_inferior_centro_vano_cm2() const noexcept;
    double area_estribos_cm2_por_m(double separacion_cm) const noexcept;
};

/**
 * @brief Vano de viga de hormigón armado, pretensada o de cimentación (`seccviga.dll` / `porvigas.dll`).
 */
struct VanoVigaHormigon {
    std::uint32_t id{0};
    std::string referencia{"V-101"};
    std::uint32_t barra_3d_id{0};
    TipoSeccionViga tipo_seccion{TipoSeccionViga::Rectangular};

    double ancho_alma_b_m{0.30};
    double canto_total_h_m{0.50};
    double ancho_ala_izquierda_m{0.0};
    double ancho_ala_derecha_m{0.0};
    double espesor_ala_forjado_m{0.25};
    double recubrimiento_geom_m{0.030};
    std::string hormigon{"C25/30"};
    std::string acero_barras{"B 500 S"};

    // Parámetros de viga pretensada (`pvfampret_*`)
    double area_armadura_activa_cm2{0.0};
    double fuerza_pretensado_inicial_kn{0.0};
    double excentricidad_tendon_m{0.10};

    // Parámetros de viga de cimentación (`svseccion_es_viga_cimentacion`)
    double modulo_balasto_k_kn_m3{10000.0};
    double tension_admisible_terreno_mpa{0.20};

    ArmadoVanoViga armado{};

    double momento_resistente_negativo_izq_knm(double fcd_mpa = 16.67, double fyd_mpa = 434.78) const noexcept;
    double momento_resistente_negativo_der_knm(double fcd_mpa = 16.67, double fyd_mpa = 434.78) const noexcept;
    double momento_resistente_positivo_vano_knm(double fcd_mpa = 16.67, double fyd_mpa = 434.78) const noexcept;
};

/**
 * @brief Alineación / Pórtico continuo de vigas (`porvigas.dll` / `ed3pvacarm`).
 */
struct PorticoVigasEstructural {
    std::uint32_t id{0};
    std::string referencia{"Pórtico 1"};
    std::uint32_t nivel_planta_id{1};
    bool bloqueado_armado{false};
    double coeficiente_redistribucion{0.15};
    std::vector<VanoVigaHormigon> vanos;
};

/**
 * @brief Resultado de comprobación y diseño por capacidad sísmica de un vano de viga (`discap.dll` / `ed3pvacarm`).
 */
struct ResultadoComprobacionVanoViga {
    bool cumple_global{true};
    bool cumple_armadura_minima_flexion{true};
    bool cumple_armadura_piel{true};
    bool cumple_cortante_capacidad{true};
    double mrd_neg_izq_knm{0.0};
    double mrd_neg_der_knm{0.0};
    double mrd_pos_vano_knm{0.0};
    double cortante_disenyo_capacidad_kn{0.0};
    double cortante_resistente_vrd_kn{0.0};
    std::vector<std::string> errores_valoracion;
};

/**
 * @brief Motor de verificación normativa y diseño por capacidad de Pilares y Vigas de H.A. (`comppilhorm.dll`, `discap.dll`).
 */
class ConcreteStructureEngine {
public:
    static ResultadoComprobacionPilar verificar_tramo_pilar(
        const TramoPilarPlanta& tramo,
        double sum_mrb_vigas_concurrentes_knm = 0.0,
        double gamma_rd_capacidad = 1.30,
        bool con_criterio_sismico = true,
        double fcd_mpa = 16.67,
        double fyd_mpa = 434.78
    );

    static ResultadoComprobacionVanoViga verificar_vano_viga(
        const VanoVigaHormigon& vano,
        double luz_clara_m,
        double carga_isostatica_q_kn_m = 20.0,
        double gamma_rd_capacidad = 1.20,
        bool con_criterio_sismico = true,
        double fcd_mpa = 16.67,
        double fyd_mpa = 434.78
    );
};

} // namespace cypeclon::model
