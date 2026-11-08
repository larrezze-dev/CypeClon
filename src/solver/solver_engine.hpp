#pragma once

#include "codes/normativa_engine.hpp"
#include "core/math.hpp"
#include "model/obra.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cypeclon::solver {

// ============================================================================
// P-09.1: Álgebra Lineal Matricial y Autovalores (`cypematr.dll`, `frontlib.dll`)
// ============================================================================

/**
 * @brief Resultado del cálculo generalizado de autovalores y autovectores `[K - lambda * M] * phi = 0`
 *        equivalente a `cmatr_calcular_autovalores_y_autovectores` en `cypematr.dll`.
 */
struct GeneralizedEigenResult {
    bool converged{false};
    std::vector<double> eigenvalues;                  ///< lambda_1 <= lambda_2 <= ...
    std::vector<std::vector<double>> eigenvectors;    ///< Vectores propios phi_i M-ortonormales (dimension N)
    std::uint32_t iterations{0};
};

/**
 * @brief Motor de álgebra lineal densa/banda con soporte OpenMP (`cypematr.dll`, `frontlib.dll`).
 */
class LinearAlgebraEngine {
public:
    /**
     * @brief Resuelve `A * x = b` mediante factorización LDLT / eliminación gaussiana con pivoteo parcial
     *        equivalente a `cmatr_solucion_sistema_ecuaciones`.
     */
    static bool solucion_sistema_ecuaciones(
        const std::vector<double>& a_row_major,
        const std::vector<double>& b,
        std::size_t n,
        std::vector<double>& x_out
    ) noexcept;

    /**
     * @brief Resuelve en paralelo (OpenMP) múltiples vectores de carga `A * X = B` compartiendo matriz `A`.
     */
    static bool solucion_multiples_hipotesis(
        const std::vector<double>& a_row_major,
        const std::vector<std::vector<double>>& b_columns,
        std::size_t n,
        std::vector<std::vector<double>>& x_columns_out
    ) noexcept;

    /**
     * @brief Calcula el triple producto `T^T * K_local * T` (`cmatr_triple_producto_cambio_trasp_x_base_x_cambio`).
     */
    static std::vector<double> triple_producto_cambio_trasp_x_base_x_cambio(
        const std::vector<double>& t_row_major,
        const std::vector<double>& k_base_row_major,
        std::size_t n
    ) noexcept;

    /**
     * @brief Calcula `M^{1/2}` y `M^{-1/2}` diagonal/bloque (`fr_calcula_M_un_medio`, `fr_invierte_M_un_medio`)
     *        y resuelve el problema generalizado simétrico `[K - lambda * M] * phi = 0` mediante rotaciones de Jacobi.
     */
    static GeneralizedEigenResult calcular_autovalores_y_autovectores_generalizado(
        const std::vector<double>& k_row_major,
        const std::vector<double>& m_diag,
        std::size_t n,
        std::size_t num_modos_solicitados = 6,
        double tol = 1.0e-10,
        std::uint32_t max_sweeps = 80
    ) noexcept;
};

// ============================================================================
// P-09.1: Elemento Barra 3D Timoshenko de 12 GDL (`calcesf.dll`, `frontlib.dll`)
// ============================================================================

/**
 * @brief Propiedades mecánicas y geométricas de cálculo de una Barra 3D tras resolver perfil, material y offsets.
 */
struct PropiedadesCalculoBarra3d {
    std::uint32_t barra_id{0};
    std::uint32_t nudo_ini_id{0};
    std::uint32_t nudo_fin_id{0};
    core::Vec3d pos_ini_nudo{0.0, 0.0, 0.0};
    core::Vec3d pos_fin_nudo{0.0, 0.0, 0.0};
    double longitud_nudo_a_nudo_m{0.0};
    double longitud_flexible_m{0.0};       ///< `puobseccba_longitud_elastica_barra` (descontando dimensión finita de nudos)
    double offset_rigido_ini_m{0.0};
    double offset_rigido_fin_m{0.0};
    core::LocalAxes3d ejes_locales{};

    // Constantes elásticas del material (SI: kN/m^2 = kPa)
    double e_kpa{2.10e8};                  ///< Módulo de Young E (kN/m^2, p.ej. 210 GPa = 2.1e8 kPa)
    double g_kpa{8.0769e7};                ///< Módulo de cortante G (kN/m^2)
    double alpha_termico_c_inv{1.2e-5};    ///< Coeficiente de dilatación térmica (1/°C)
    double peso_especifico_kn_m3{78.5};    ///< Peso específico gamma (kN/m^3)
    double fy_mpa{275.0};                  ///< Límite elástico fy (MPa)

    // Constantes geométricas de sección (SI: m^2, m^4, m^3)
    double area_m2{0.00285};
    double avy_m2{0.00140};
    double avz_m2{0.00140};
    double iy_m4{1.943e-5};                ///< Inercia alrededor del eje local Y (flexión en plano XZ)
    double iz_m4{1.420e-6};                ///< Inercia alrededor del eje local Z (flexión en plano XY)
    double it_m4{7.020e-8};                ///< Inercia torsional de Saint-Venant It
    double wply_m3{2.206e-4};
    double wplz_m3{4.460e-5};
    double h_m{0.200};
    double b_m{0.100};

    // Parámetros adimensionales de deformación por cortante de Timoshenko
    double phi_shear_xy{0.0};              ///< Phi_y = 12 * E * Iz / (G * Avy * L^2)
    double phi_shear_xz{0.0};              ///< Phi_z = 12 * E * Iy / (G * Avz * L^2)

    // Coeficientes efectivos de empotramiento en extremos [0..1] (combinando barra, nudo y rigidez rotacional)
    double fixity_xy_ini{1.0};
    double fixity_xy_fin{1.0};
    double fixity_xz_ini{1.0};
    double fixity_xz_fin{1.0};
    double fixity_torsion_ini{1.0};
    double fixity_torsion_fin{1.0};
};

/**
 * @brief Esfuerzos internos y desplazamientos locales en una abscisa `x` de una barra 3D
 *        equivalente a `cypefrnt_esfuerzos_en_punto_barra3D_con_giros` (`frontlib.dll`).
 */
struct PuntoEsfuerzosBarra {
    double x_m{0.0};           ///< Coordenada local x desde el nudo inicial [0..L] (m)
    double x_rel{0.0};         ///< Posición relativa x/L [0..1]
    double n_kn{0.0};          ///< Esfuerzo axil N (kN): > 0 tracción, < 0 compresión
    double vy_kn{0.0};         ///< Esfuerzo cortante Vy en eje local Y (kN)
    double vz_kn{0.0};         ///< Esfuerzo cortante Vz en eje local Z (kN)
    double mt_knm{0.0};        ///< Momento torsor Mt alrededor de eje local X (kN·m)
    double my_knm{0.0};        ///< Momento flector My alrededor de eje local Y / plano XZ (kN·m)
    double mz_knm{0.0};        ///< Momento flector Mz alrededor de eje local Z / plano XY (kN·m)
    double ux_local_m{0.0};    ///< Desplazamiento axial local ux(x) (m)
    double uy_local_m{0.0};    ///< Flecha / desplazamiento transversal local uy(x) (m)
    double uz_local_m{0.0};    ///< Flecha / desplazamiento transversal local uz(x) (m)
    double giro_x_rad{0.0};    ///< Giro torsional theta_x(x) (rad)
    double giro_y_rad{0.0};    ///< Giro de flexión theta_y(x) (rad)
    double giro_z_rad{0.0};    ///< Giro de flexión theta_z(x) (rad)
};

/**
 * @brief Formulación matricial completa del elemento Barra 3D de Timoshenko de 12 GDL (`calcesf.dll`).
 */
class TimoshenkoBarElement3d {
public:
    using Matrix12x12 = std::array<double, 144>;
    using Vector12 = std::array<double, 12>;

    /**
     * @brief Extrae y calcula todas las propiedades mecánicas y geométricas de una `Barra3d` dentro de la `Obra`.
     */
    static PropiedadesCalculoBarra3d extraer_propiedades_barra(
        const model::Obra& obra,
        const model::Barra3d& barra
    ) noexcept;

    /**
     * @brief Matriz de rigidez elástica local 12x12 con deformación por cortante de Timoshenko,
     *        condensación estática de articulaciones/empotramiento parcial y offsets rígidos.
     */
    static Matrix12x12 calcular_matriz_rigidez_local_12x12(
        const PropiedadesCalculoBarra3d& props
    ) noexcept;

    /**
     * @brief Matriz de transformación 12x12 de coordenadas globales a locales (`u_local = T * u_global`),
     *        incluyendo el efecto cinemático de excentricidades y dimensión finita de nudos.
     */
    static Matrix12x12 calcular_matriz_transformacion_12x12(
        const PropiedadesCalculoBarra3d& props
    ) noexcept;

    /**
     * @brief Matriz de rigidez elástica global 12x12 (`K_global = T^T * K_local * T`).
     */
    static Matrix12x12 calcular_matriz_rigidez_global_12x12(
        const PropiedadesCalculoBarra3d& props
    ) noexcept;

    /**
     * @brief Matriz de rigidez geométrica local y global 12x12 `K_G(N)` para análisis de 2.º orden (P-Delta)
     *        y estabilidad global por pandeo (`cyopcnlgeo_*`, `cyosgruanpan_*`).
     * @param axil_n_kn Esfuerzo axil de la barra (kN, > 0 tracción rigidiza, < 0 compresión ablanda).
     */
    static Matrix12x12 calcular_matriz_geometrica_local_12x12(
        const PropiedadesCalculoBarra3d& props,
        double axil_n_kn
    ) noexcept;

    static Matrix12x12 calcular_matriz_geometrica_global_12x12(
        const PropiedadesCalculoBarra3d& props,
        double axil_n_kn
    ) noexcept;

    /**
     * @brief Calcula el vector de fuerzas nodales equivalentes en ejes locales (12x1) para todas las
     *        cargas que actúan sobre la barra en una hipótesis dada (incluyendo peso propio si procede),
     *        corrigiendo por empotramiento parcial en los extremos.
     */
    static Vector12 calcular_vector_cargas_equivalentes_locales_12(
        const PropiedadesCalculoBarra3d& props,
        const model::Barra3d& barra,
        const std::vector<model::CargaLinealBarra>& cargas_barra_hipotesis,
        bool incluir_peso_propio_en_esta_hipotesis
    ) noexcept;

    /**
     * @brief Evalúa los esfuerzos internos `N(x), Vy(x), Vz(x), Mt(x), My(x), Mz(x)` y desplazamientos
     *        locales en una posición relativa `x_rel` [0..1] a partir de los desplazamientos globales de sus extremos.
     */
    static PuntoEsfuerzosBarra evaluar_esfuerzos_en_punto(
        const PropiedadesCalculoBarra3d& props,
        const model::Barra3d& barra,
        const Vector12& u_global_extremos,
        const std::vector<model::CargaLinealBarra>& cargas_barra_hipotesis,
        bool incluir_peso_propio_en_esta_hipotesis,
        double x_rel
    ) noexcept;
};

// ============================================================================
// P-09.1: Elemento Finito Triangular Shell de 18 GDL CST + DKT (`calcmef.dll`)
// ============================================================================

/**
 * @brief Elemento finito plano de lámina de 3 nodos y 6 GDL/nodo (18 GDL en total) combinando
 *        membrana CST (Constant Strain Triangle) + flexión de placa DKT (Discrete Kirchhoff Triangle)
 *        + rigidez rotacional de perforación (*drilling DOF*) y ortotropía (`FactoresRigidezLamina`).
 */
class ShellTriangleElement3d {
public:
    using Matrix18x18 = std::array<double, 324>;
    using Vector18 = std::array<double, 18>;

    /**
     * @brief Ensambla la matriz de rigidez global 18x18 de un elemento triangular de lámina.
     */
    static Matrix18x18 calcular_matriz_rigidez_global_18x18(
        const core::Vec3d& p1,
        const core::Vec3d& p2,
        const core::Vec3d& p3,
        double espesor_m,
        double e_kpa,
        double nu,
        const model::FactoresRigidezLamina& factores = {}
    ) noexcept;

    /**
     * @brief Calcula el vector de fuerzas nodales equivalentes globales (18x1) por peso propio y cargas en lámina.
     */
    static Vector18 calcular_vector_cargas_globales_18(
        const core::Vec3d& p1,
        const core::Vec3d& p2,
        const core::Vec3d& p3,
        double espesor_m,
        double peso_especifico_kn_m3,
        bool incluir_peso_propio,
        const std::vector<model::CargaLamina3d>& cargas_lamina_hipotesis,
        const core::LocalAxes3d& ejes_lamina
    ) noexcept;

    /**
     * @brief Recupera los esfuerzos unitarios `Nx, Ny, Nxy, Mx, My, Mxy, Qx, Qy` en el baricentro del triángulo
     *        a partir del vector de desplazamientos globales de sus 3 nodos (`cypefrnt_esfuerzos_en_punto_triangulo_6_nodos`).
     */
    static model::EsfuerzosUnitariosLamina recuperar_esfuerzos_baricentro(
        const core::Vec3d& p1,
        const core::Vec3d& p2,
        const core::Vec3d& p3,
        double espesor_m,
        double e_kpa,
        double nu,
        const model::FactoresRigidezLamina& factores,
        const Vector18& u_global_18
    ) noexcept;
};

// ============================================================================
// P-09.1: Estructuras de Resultados Estáticos por Hipótesis, Combinación y Envolvente
// ============================================================================

struct DesplazamientoNudoResultado {
    std::uint32_t nudo_id{0};
    double ux_m{0.0};
    double uy_m{0.0};
    double uz_m{0.0};
    double rx_rad{0.0};
    double ry_rad{0.0};
    double rz_rad{0.0};

    double desplazamiento_total_m() const noexcept;
};

struct ReaccionApoyoNudoResultado {
    std::uint32_t nudo_id{0};
    double fx_kn{0.0};
    double fy_kn{0.0};
    double fz_kn{0.0};
    double mx_knm{0.0};
    double my_knm{0.0};
    double mz_knm{0.0};
};

struct EsfuerzosBarraResultado {
    std::uint32_t barra_id{0};
    double longitud_m{0.0};
    std::vector<PuntoEsfuerzosBarra> puntos; ///< Evaluados en N estaciones a lo largo de la barra

    // Valores extremos en la barra para esta hipótesis/combinación
    double n_max_kn{0.0};
    double n_min_kn{0.0};
    double vy_max_abs_kn{0.0};
    double vz_max_abs_kn{0.0};
    double mt_max_abs_knm{0.0};
    double my_max_knm{0.0};
    double my_min_knm{0.0};
    double mz_max_knm{0.0};
    double mz_min_knm{0.0};
    double flecha_max_xy_mm{0.0};
    double flecha_max_xz_mm{0.0};
    double flecha_relativa_l_n{0.0};
};

struct EsfuerzosElementoLaminaResultado {
    std::uint32_t lamina_id{0};
    std::uint32_t triangulo_id{0};
    core::Vec3d baricentro{0.0, 0.0, 0.0};
    model::EsfuerzosUnitariosLamina esfuerzos{};
    double von_mises_cara_sup_mpa{0.0};
    double von_mises_cara_inf_mpa{0.0};
    model::CuantiasArmadoLamina cuantias_wood_armer{};
};

struct ResultadoHipotesisOCombinacion {
    std::uint32_t id{0};
    std::string codigo;
    std::string descripcion;
    bool es_combinacion{false};
    std::vector<DesplazamientoNudoResultado> desplazamientos_nudos;
    std::vector<ReaccionApoyoNudoResultado> reacciones_apoyos;
    std::vector<EsfuerzosBarraResultado> esfuerzos_barras;
    std::vector<EsfuerzosElementoLaminaResultado> esfuerzos_laminas;

    const DesplazamientoNudoResultado* find_desplazamiento_nudo(std::uint32_t nudo_id) const noexcept;
    const ReaccionApoyoNudoResultado* find_reaccion_nudo(std::uint32_t nudo_id) const noexcept;
    const EsfuerzosBarraResultado* find_esfuerzos_barra(std::uint32_t barra_id) const noexcept;
};

struct EnvolventeEsfuerzosBarra {
    std::uint32_t barra_id{0};
    double n_max_traccion_kn{0.0};
    double n_max_compresion_kn{0.0};
    double vy_max_abs_kn{0.0};
    double vz_max_abs_kn{0.0};
    double mt_max_abs_knm{0.0};
    double my_max_pos_knm{0.0};
    double my_min_neg_knm{0.0};
    double mz_max_pos_knm{0.0};
    double mz_min_neg_knm{0.0};
    double flecha_max_mm{0.0};
    std::string combinacion_pesima_my;
    std::string combinacion_pesima_n;
};

// ============================================================================
// P-09.2: Análisis Modal Espectral, P-Delta, Tirantes/Apoyos No Lineales, Pandeo y Pushover
// ============================================================================

/**
 * @brief Método de combinación modal para análisis sísmico dinámico (`sismocal.dll`).
 */
enum class MetodoCombinacionModal : std::uint8_t {
    CQC = 0,  ///< Combinación Cuadrática Completa de Der Kiureghian (considera acoplamiento de modos cercanos)
    SRSS = 1  ///< Raíz Cuadrada de la Suma de los Cuadrados
};

struct ModoVibracionResultado {
    std::uint32_t modo_index{1};
    double autovalor_omega2{0.0};
    double omega_rad_s{0.0};
    double frecuencia_hz{0.0};
    double periodo_s{0.0};

    // Factores de participación modal y masas efectivas traslacionales
    double factor_participacion_x{0.0};
    double factor_participacion_y{0.0};
    double factor_participacion_z{0.0};
    double masa_efectiva_x_ton{0.0};
    double masa_efectiva_y_ton{0.0};
    double masa_efectiva_z_ton{0.0};
    double porcentaje_masa_x{0.0};
    double porcentaje_masa_y{0.0};
    double porcentaje_masa_z{0.0};

    // Ordenadas espectrales de diseño y cortantes basales modales
    double aceleracion_espectral_x_g{0.0};
    double aceleracion_espectral_y_g{0.0};
    double cortante_basal_x_kn{0.0};
    double cortante_basal_y_kn{0.0};

    // Forma modal normalizada en los nudos de la estructura
    std::vector<DesplazamientoNudoResultado> forma_modal_nudos;
};

/**
 * @brief Resultado completo del Análisis Modal Espectral (`cyopcanmodal_*`, `sismocal.dll`).
 */
struct ModalSpectralAnalysisResult {
    bool converged{false};
    MetodoCombinacionModal metodo_combinacion{MetodoCombinacionModal::CQC};
    double amortiguamiento_modal_xi{0.05};
    double masa_total_estructura_ton{0.0};
    std::vector<ModoVibracionResultado> modos;

    double suma_porcentaje_masa_x{0.0};
    double suma_porcentaje_masa_y{0.0};
    double suma_porcentaje_masa_z{0.0};
    bool cumple_masa_participativa_90pct{false};

    // Cortantes basales combinados (CQC/SRSS) y corrección por cortante basal estático (`cypefrnt_hay_sismo_dinamico_con_comprobacion_cortante_basal`)
    double cortante_basal_dinamico_x_kn{0.0};
    double cortante_basal_dinamico_y_kn{0.0};
    double cortante_basal_estatico_minimo_x_kn{0.0};
    double cortante_basal_estatico_minimo_y_kn{0.0};
    double factor_escala_cortante_basal_x{1.0};
    double factor_escala_cortante_basal_y{1.0};

    // Respuestas sísmicas combinadas (CQC / SRSS escaladas) en Sismo X y Sismo Y
    ResultadoHipotesisOCombinacion respuesta_sismo_x{};
    ResultadoHipotesisOCombinacion respuesta_sismo_y{};
};

/**
 * @brief Resultado del análisis de efectos de 2.º orden P-Delta (`cyopcnlgeo_*`).
 */
struct SecondOrderPDeltaResult {
    bool converged{false};
    std::uint32_t iteraciones{0};
    double indice_estabilidad_gamma_z{1.0};  ///< Coeficiente de amplificación global gamma_z = 1 / (1 - dM_II / dM_I)
    double factor_amplificacion_aplicado{1.0};
    bool requiere_segundo_orden{false};      ///< true si gamma_z > 1.10
    bool estructura_inestable{false};        ///< true si gamma_z > 1.33 o no converge
    ResultadoHipotesisOCombinacion resultado_amplificado{};
};

/**
 * @brief Método de resolución no lineal de tirantes (barras que solo trabajan a tracción).
 */
enum class MetodoCalculoTirantes : std::uint8_t {
    MetodoCypeCuadrosRigidos = 0,         ///< Método clásico de CYPE 3D (mitad de área o desactivación directa de diagonal comprimida en recuadros)
    MetodoGeneralIterativoOpenSees = 1    ///< Método general no lineal iterativo (`cyosbarra_crea_solo_traccion` / `cyosbarra_crea_solo_compresion`)
};

struct NonlinearIterativeResult {
    bool converged{false};
    MetodoCalculoTirantes metodo{MetodoCalculoTirantes::MetodoGeneralIterativoOpenSees};
    std::uint32_t iteraciones{0};
    std::vector<std::uint32_t> tirantes_desactivados_por_compresion;
    std::vector<std::uint32_t> puntales_desactivados_por_traccion;
    std::vector<std::uint32_t> apoyos_levantados_sin_contacto;
    ResultadoHipotesisOCombinacion resultado_equilibrio{};
};

/**
 * @brief Modo de pandeo global linealizado (`cyosgruanpan_*`, `cyosestabglob_*`).
 */
struct ModoPandeoGlobal {
    std::uint32_t modo_index{1};
    double factor_carga_critica_lambda_cr{0.0}; ///< Multiplicador crítico de carga lambda_cr
    bool pandeo_relevante{false};               ///< Según EC3: lambda_cr < 10.0 exige análisis de 2.º orden
    std::vector<DesplazamientoNudoResultado> forma_pandeo_nudos;
};

struct GlobalBucklingAnalysisResult {
    bool converged{false};
    std::uint32_t hipotesis_o_combinacion_id{1};
    std::vector<ModoPandeoGlobal> modos_pandeo;
    double lambda_cr_minimo{0.0};
};

/**
 * @brief Estados de daño de una rótula plástica según FEMA 356 / ASCE 41 / EC8-3 (`pushover.dll`).
 */
enum class EstadoRotulaPlastica : std::uint8_t {
    Elastico = 0,              ///< M < Mp (Tramo A-B)
    FluenciaInicial_B = 1,     ///< M >= Mp, theta_p < IO
    OcupacionInmediata_IO = 2, ///< IO <= theta_p < LS
    SeguridadVida_LS = 3,      ///< LS <= theta_p < CP
    PrevencionColapso_CP = 4,  ///< CP <= theta_p < Ult
    AgotamientoColapso_C_E = 5 ///< theta_p >= Ult (caída de resistencia C-D-E)
};

struct EstadoRotulaExtremoBarra {
    std::uint32_t barra_id{0};
    bool extremo_inicial{true};
    double momento_actuante_knm{0.0};
    double momento_plastificacion_mp_knm{0.0}; ///< `puobseccba_momento_plastificacion_gdl_Gy`
    double rotacion_plastica_rad{0.0};
    EstadoRotulaPlastica estado{EstadoRotulaPlastica::Elastico};
};

struct PasoCurvaCapacidadPushover {
    std::uint32_t paso_index{0};
    double factor_carga_lateral{0.0};
    double desplazamiento_control_m{0.0};
    double cortante_basal_kn{0.0};
    double rigidez_tangente_kn_m{0.0};
    std::uint32_t num_rotulas_fluencia_b{0};
    std::uint32_t num_rotulas_io{0};
    std::uint32_t num_rotulas_ls{0};
    std::uint32_t num_rotulas_cp{0};
    std::uint32_t num_rotulas_colapso{0};
    std::vector<EstadoRotulaExtremoBarra> estado_rotulas;
};

/**
 * @brief Resultado del Análisis Sísmico Estático No Lineal Pushover (`pushover.dll`, `pushoverbas.dll`, `cyopcanpushover_*`).
 */
struct PushoverAnalysisResult {
    bool converged{false};
    std::uint32_t nudo_control_id{0};
    core::Vec3d direccion_empuje{1.0, 0.0, 0.0};
    std::vector<PasoCurvaCapacidadPushover> curva_capacidad;

    // Idealización bilineal de la curva de capacidad (FEMA 356 / EC8 Anexo B / N2 Method)
    double desplazamiento_fluencia_dy_m{0.0};
    double cortante_fluencia_vy_kn{0.0};
    double desplazamiento_ultimo_du_m{0.0};
    double cortante_maximo_vmax_kn{0.0};
    double ductilidad_disponible_mu{1.0};
    double periodo_efectivo_teff_s{0.0};

    // Punto de Desempeño (Performance Point) frente al espectro de demanda sísmica
    bool tiene_punto_desempeno{false};
    double desplazamiento_desempeno_m{0.0};
    double cortante_basal_desempeno_kn{0.0};
    double ductilidad_demanda{1.0};
    EstadoRotulaPlastica estado_global_desempeno{EstadoRotulaPlastica::Elastico};
};

// ============================================================================
// Resumen Maestro de Cálculo de la Obra (`SolverEngine`)
// ============================================================================

struct SolverSummary {
    bool converged{false};
    std::size_t dof_count{0};
    std::size_t active_bar_count{0};
    std::size_t active_shell_triangle_count{0};
    double max_displacement_m{0.0};
    double max_axial_compression_kn{0.0};
    double max_bending_moment_knm{0.0};
    double max_reaction_fz_kn{0.0};

    std::vector<ResultadoHipotesisOCombinacion> resultados_hipotesis;
    std::vector<ResultadoHipotesisOCombinacion> resultados_combinaciones;
    std::vector<EnvolventeEsfuerzosBarra> envolventes_barras;

    std::optional<ModalSpectralAnalysisResult> analisis_modal;
    std::optional<SecondOrderPDeltaResult> analisis_pdelta;
    std::optional<GlobalBucklingAnalysisResult> analisis_pandeo_global;
    std::optional<PushoverAnalysisResult> analisis_pushover;
};

/**
 * @brief Motor principal de Cálculo Matricial 3D, MEF de Láminas, Dinámico Modal, P-Delta,
 *        No Lineal de Tirantes/Apoyos, Estabilidad Global y Pushover (Fase 8: P-09.1 y P-09.2).
 */
class SolverEngine {
public:
    /**
     * @brief Ejecuta el análisis elástico lineal 3D para todas las hipótesis y combinaciones de la obra,
     *        incluyendo barras Timoshenko (12 GDL), láminas MEF (18 GDL), diafragmas rígidos, ligaduras y apoyos.
     */
    static SolverSummary analyze_linear_static(
        const model::Obra& obra,
        std::size_t num_estaciones_barra = 11
    );

    /**
     * @brief Resuelve una única hipótesis o vector de cargas ponderado (combinación) sobre la estructura.
     */
    static ResultadoHipotesisOCombinacion resolver_caso_carga(
        const model::Obra& obra,
        const std::vector< std::pair<std::uint32_t, double> >& factores_hipotesis,
        std::uint32_t resultado_id = 1,
        std::string_view codigo = "HIP",
        std::string_view descripcion = "",
        bool es_combinacion = false,
        std::size_t num_estaciones_barra = 11,
        const std::vector<std::uint32_t>& barras_desactivadas = {},
        const std::vector<std::uint32_t>& apoyos_nudos_desactivados = {},
        const std::vector< std::pair<std::uint32_t, double> >& axiles_segundo_orden_barras = {}
    );

    // ========================================================================
    // P-09.2: Motores de Análisis Avanzados
    // ========================================================================

    /**
     * @brief Fórmula de correlación modal CQC (Der Kiureghian) y SRSS (`sismocal.dll`).
     */
    static double correlacion_modal_cqc(double omega_i, double omega_j, double xi = 0.05) noexcept;
    static double combinar_respuestas_modales(
        const std::vector<double>& respuestas_modales,
        const std::vector<double>& omegas_rad_s,
        MetodoCombinacionModal metodo = MetodoCombinacionModal::CQC,
        double xi = 0.05
    ) noexcept;

    /**
     * @brief Ejecuta el Análisis Modal Espectral 3D (`cyopcanmodal_*`, `sismocal.dll`).
     */
    static ModalSpectralAnalysisResult analyze_modal_spectral(
        const model::Obra& obra,
        std::uint32_t num_modos = 6,
        MetodoCombinacionModal metodo = MetodoCombinacionModal::CQC,
        double amortiguamiento_xi = 0.05
    );

    /**
     * @brief Ejecuta el Análisis de 2.º Orden P-Delta (`cyopcnlgeo_*`).
     */
    static SecondOrderPDeltaResult analyze_second_order_pdelta(
        const model::Obra& obra,
        const std::vector< std::pair<std::uint32_t, double> >& factores_hipotesis,
        std::uint32_t max_iteraciones = 10,
        double tolerancia_rel = 1.0e-4
    );

    /**
     * @brief Ejecuta el cálculo no lineal iterativo de tirantes (solo tracción), puntales (solo compresión)
     *        y apoyos no lineales con levantamiento (`cyosbarra_crea_solo_traccion`).
     */
    static NonlinearIterativeResult analyze_nonlinear_tension_only_and_uplift(
        const model::Obra& obra,
        const std::vector< std::pair<std::uint32_t, double> >& factores_hipotesis,
        MetodoCalculoTirantes metodo = MetodoCalculoTirantes::MetodoGeneralIterativoOpenSees,
        std::uint32_t max_iteraciones = 15
    );

    /**
     * @brief Ejecuta el Análisis de Estabilidad Global / Pandeo Lineal por autovalores (`cyosgruanpan_*`).
     */
    static GlobalBucklingAnalysisResult analyze_global_buckling(
        const model::Obra& obra,
        const std::vector< std::pair<std::uint32_t, double> >& factores_hipotesis,
        std::uint32_t num_modos_pandeo = 4
    );

    /**
     * @brief Ejecuta el Análisis Sísmico Estático No Lineal Pushover (`pushover.dll`, `pushoverbas.dll`, `cyopcanpushover_*`).
     */
    static PushoverAnalysisResult analyze_pushover(
        const model::Obra& obra,
        const core::Vec3d& direccion_empuje = {1.0, 0.0, 0.0},
        std::uint32_t nudo_control_id = 0,
        std::uint32_t num_pasos = 25,
        double desplazamiento_objetivo_max_m = 0.25
    );
};

} // namespace cypeclon::solver
