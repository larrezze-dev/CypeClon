#pragma once

#include "codes/normativa_engine.hpp"
#include "core/math.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::codes {

// ==============================================================================
// P-10.1 — Motor de Comprobación ELU / ELS / Fuego y Dimensionamiento Óptimo
// (`compsec.dll`, `cspncomp.dll`, `normamet.dll`, `perfil2.dll`, `gestcomp.dll`, `rstfuego.dll`)
// ==============================================================================

/**
 * @brief Estado de una comprobación normativa individual (`gestcomp.dll`: `gcgscomp_es_comprobacion_*`).
 */
enum class EstadoArticuloComprobacion : std::uint8_t {
    Cumple = 0,          ///< eta <= 100% (`gcgscomp_cumple`)
    NoCumple = 1,        ///< eta > 100% o error geométrico/mecánico (`gcgscomp_es_comprobacion_con_error`)
    Aviso = 2,           ///< Cumple con aviso (`cspncomp_crea_comprobacion_con_aviso`)
    Informacion = 3,     ///< Nota informativa (`gcgscomp_es_comprobacion_info`)
    NoProcede = 4        ///< Comprobación no aplicable para estos esfuerzos/geometría (`gcgscomp_es_comprobacion_no_procede`)
};

/**
 * @brief Clase de sección transversal frente a abolladura local (CTE DB SE-A / Eurocódigo 3 / Eurocódigo 9).
 */
enum class ClaseSeccionTransversal : std::uint8_t {
    Clase1_PlasticaDuctil = 1,    ///< Capacidad de rótula plástica con rotación
    Clase2_PlasticaCompacta = 2,  ///< Capacidad de momento plástico sin rotación completa
    Clase3_Elastica = 3,          ///< Tensión elástica fy en fibra extrema sin abolladura previa
    Clase4_EsbeltaEfectiva = 4    ///< Abolladura local elástica antes de alcanzar fy (sección eficaz A_eff, W_eff)
};

/**
 * @brief Resultado detallado de la clasificación de sección y propiedades eficaces en Clase 4 (`gctextos_clase_seccion`).
 */
struct ResultadoClasificacionSeccion {
    ClaseSeccionTransversal clase_ala{ClaseSeccionTransversal::Clase1_PlasticaDuctil};
    ClaseSeccionTransversal clase_alma{ClaseSeccionTransversal::Clase1_PlasticaDuctil};
    ClaseSeccionTransversal clase_global{ClaseSeccionTransversal::Clase1_PlasticaDuctil};
    double epsilon_material{1.0};      ///< eps = sqrt(235 / fy) en acero o sqrt(250 / fo) en aluminio
    double esbeltez_ala_c_t{0.0};
    double esbeltez_alma_c_t{0.0};
    double factor_reduccion_area_rho{1.0}; ///< rho <= 1.0 para Clase 4 / conformados
    double area_eficaz_cm2{0.0};
    double wy_calculo_cm3{0.0};        ///< Wpl,y (Clases 1-2), Wel,y (Clase 3) o Weff,y (Clase 4)
    double wz_calculo_cm3{0.0};        ///< Wpl,z (Clases 1-2), Wel,z (Clase 3) o Weff,z (Clase 4)
};

/**
 * @brief Comprobación normativa individual artículo por artículo (`gestcomp.dll` / `cspncomp.dll`).
 */
struct ArticuloComprobacionNormativa {
    std::string codigo_id;              ///< p.ej. "ESBELTEZ", "CLASE_SECCION", "AXIL_TRACCION", "PANDEO_COMP", "PANDEO_LATERAL", "INTERACCION_N_MY_MZ", "FLECHA_ELS", "INCENDIO_ELU"
    std::string titulo_comprobacion;    ///< p.ej. "Resistencia a flexión y axil combinados" (`gctextos_texto_Titulo*`)
    std::string referencia_normativa;   ///< p.ej. "CTE DB SE-A Art. 6.3.4 / EN 1993-1-1 Art. 6.3.3" (`gcform_texto_articulo_principal`)
    EstadoArticuloComprobacion estado{EstadoArticuloComprobacion::Cumple};
    double valor_solicitante{0.0};      ///< Esfuerzo o deformación de cálculo actuante Ed
    double valor_resistente_o_limite{0.0}; ///< Capacidad resistente Rd o límite admisible
    std::string unidad;                 ///< "kN", "kN·m", "mm", "adim", "%"
    double aprovechamiento_pct{0.0};    ///< Coeficiente de aprovechamiento eta (%) = 100 * Ed / Rd (`csccomps_texto_coeficiente_aprovechamiento`)
    std::string combinacion_pesima;     ///< Fórmula o nombre de la combinación pésima (`cscomps_datos_combinaciones_pesimas_comprobaciones`)
    double abscisa_pesima_m{0.0};       ///< Posición x (m) en la barra donde se produce el pésimo
    std::string expresion_formula;      ///< Expresión algebraica y sustitución numérica (`gccreaform_*`)
    std::string nota_justificativa;     ///< Explicación técnica detallada para el listado extendido
};

/**
 * @brief Resultado integral de comprobación ELU, ELS y Fuego de una Barra 3D (`compsec.dll`: `cscomps_*`, `cslistad_*`).
 */
struct ResultadoComprobacionBarra {
    std::uint32_t barra_id{0};
    std::string referencia_barra;
    std::string perfil_referencia;
    std::string serie_perfil;
    std::string material_referencia;
    std::string norma_aplicada;         ///< `cscomps_norma_comprobaciones`
    model::FamiliaMaterialPerfil familia_material{model::FamiliaMaterialPerfil::AceroLaminado};
    double longitud_barra_m{0.0};

    ResultadoClasificacionSeccion clasificacion_seccion{};
    std::vector<ArticuloComprobacionNormativa> articulos;

    bool cumple_elu{true};
    bool cumple_els_flecha{true};
    bool cumple_fuego{true};
    bool cumple_global{true};           ///< `cscomps_cumple_comprobaciones_barra`

    double aprovechamiento_elu_max_pct{0.0};
    double aprovechamiento_els_max_pct{0.0};
    double aprovechamiento_fuego_max_pct{0.0};
    double aprovechamiento_global_max_pct{0.0}; ///< `cscomps_resultado_pesimo_comprobaciones_barra`

    std::string articulo_pesimo_titulo; ///< `cscomps_comprobacion_pesima_barra`
    std::string combinacion_pesima_global;

    // Datos específicos de resistencia al fuego (`rstfuego.dll`)
    double temperatura_alcanzada_fuego_c{20.0};
    double temperatura_critica_fuego_c{550.0};
    double espesor_proteccion_necesario_mm{0.0};

    // Generadores de listados justificados (`cslistad_append_listado_resumido_barras` / `cslistad_append_listado_extendido`)
    std::string generar_listado_resumido() const;
    std::string generar_listado_extendido() const;
};

/**
 * @brief Evaluación de un perfil candidato dentro de una serie (`perfil2.dll`: `pncmpser_*`).
 */
struct EvaluacionPerfilEnSerie {
    std::string perfil_referencia;
    double peso_kg_m{0.0};
    double area_cm2{0.0};
    double h_mm{0.0};
    bool cumple_global{false};
    bool cumple_elu{false};
    bool cumple_els{false};
    bool cumple_fuego{false};
    double aprovechamiento_elu_pct{0.0};
    double aprovechamiento_els_pct{0.0};
    double aprovechamiento_fuego_pct{0.0};
    double aprovechamiento_max_pct{0.0};
    ClaseSeccionTransversal clase_seccion{ClaseSeccionTransversal::Clase1_PlasticaDuctil};
};

/**
 * @brief Modo de búsqueda en el dimensionamiento automático de perfiles (`ed3dlib.dll`: `Calculo_Dimensionamiento`).
 */
enum class ModoDimensionamientoSerie : std::uint8_t {
    DesdePrimerPerfilSerie = 0,  ///< `"dimensionamiento_desde_primer_perfil"`: busca el más ligero de toda la serie que cumpla
    DesdePerfilActual = 1        ///< `"dimensionamiento_desde_perfil_actual"`: solo considera perfiles de peso >= al actual
};

/**
 * @brief Resultado del dimensionamiento óptimo en serie para una barra o agrupación (`perfil2.dll`: `pncmpser_*`).
 */
struct ResultadoDimensionamientoBarra {
    std::uint32_t barra_id{0};
    std::string perfil_inicial;
    std::string perfil_optimo_seleccionado;
    std::string serie_analizada;
    bool encontrado_perfil_que_cumple{false};
    bool perfil_modificado{false};
    double peso_inicial_kg_m{0.0};
    double peso_optimo_kg_m{0.0};
    double aprovechamiento_optimo_pct{0.0};
    std::vector<EvaluacionPerfilEnSerie> tabla_comparativa_serie; ///< `pncmpser_nousar_panel_comprobar_serie`
};

/**
 * @brief Resumen global de comprobación y dimensionamiento de toda la estructura.
 */
struct ResumenComprobacionObra {
    std::vector<ResultadoComprobacionBarra> comprobaciones_barras;
    std::vector<ResultadoDimensionamientoBarra> dimensionamientos_barras;
    std::size_t barras_que_cumplen{0};
    std::size_t barras_que_no_cumplen{0};
    double aprovechamiento_maximo_obra_pct{0.0};
    std::uint32_t barra_mas_solicitada_id{0};
    std::string listado_resumido_estructura;

    const ResultadoComprobacionBarra* find_comprobacion_barra(std::uint32_t barra_id) const noexcept;
};

/**
 * @brief Motor de Comprobación ELU/ELS/Fuego y Dimensionamiento Óptimo de Perfiles (`compsec.dll`, `normamet.dll`, `perfil2.dll`, `gestcomp.dll`, `rstfuego.dll`).
 */
class MemberDesignEngine {
public:
    /**
     * @brief Clasifica una sección transversal metálica (Clase 1, 2, 3 o 4) y calcula sus módulos eficaces.
     */
    static ResultadoClasificacionSeccion clasificar_seccion_metalica(
        const model::ProfileSectionProperties& profile,
        double fy_mpa,
        double axil_n_kn,
        double my_knm,
        double mz_knm,
        bool es_aluminio = false
    ) noexcept;

    /**
     * @brief Calcula el factor de reducción por pandeo por flexión `chi` según las curvas de pandeo europeas (`a0, a, b, c, d`) o aluminio (`A, B`).
     */
    static double factor_reduccion_pandeo_chi(
        double esbeltez_relativa_lambda_bar,
        double factor_imperfeccion_alpha,
        double lambda_0 = 0.20
    ) noexcept;

    /**
     * @brief Selecciona el factor de imperfección `alpha` de la curva de pandeo según la forma del perfil y el eje (`y` o `z`).
     */
    static double factor_imperfeccion_pandeo_acero(
        const model::ProfileSectionProperties& profile,
        bool eje_fuerte_y,
        double fy_mpa = 275.0
    ) noexcept;

    /**
     * @brief Ejecuta todas las comprobaciones artículo por artículo (ELU, ELS flecha y Fuego) sobre una barra 3D.
     */
    static ResultadoComprobacionBarra comprobar_barra(
        const model::Obra& obra,
        const model::Barra3d& barra,
        const solver::SolverSummary& solver_results
    );

    /**
     * @brief Comprueba todas las barras de la estructura y construye el resumen general.
     */
    static ResumenComprobacionObra comprobar_todas_las_barras(
        const model::Obra& obra,
        const solver::SolverSummary& solver_results
    );

    /**
     * @brief Evalúa todos los perfiles de la serie de una barra y selecciona el perfil óptimo de mínimo peso que cumple ELU + ELS + Fuego (`pncmpser_*`).
     */
    static ResultadoDimensionamientoBarra dimensionar_barra_en_serie(
        model::Obra& obra,
        std::uint32_t barra_id,
        const solver::SolverSummary& solver_results,
        ModoDimensionamientoSerie modo = ModoDimensionamientoSerie::DesdePrimerPerfilSerie,
        bool aplicar_cambio_en_obra = true
    );

    /**
     * @brief Dimensiona todas las barras metálicas/madera/aluminio de la obra respetando las agrupaciones (`AgrupacionBarras`).
     */
    static ResumenComprobacionObra dimensionar_estructura_completa(
        model::Obra& obra,
        solver::SolverSummary& solver_results,
        ModoDimensionamientoSerie modo = ModoDimensionamientoSerie::DesdePrimerPerfilSerie,
        bool recalcular_rigidez_tras_dimensionar = true
    );
};

// ==============================================================================
// P-10.2 — Editores Avanzados de Pilares y Vigas de Hormigón Armado
// (`pilares.dll`, `pilararm.dll`, `visesfpil.dll`, `porvigas.dll`, `seccviga.dll`, `comphorm.dll`)
// ==============================================================================

/**
 * @brief Punto del diagrama de interacción `N - My - Mz` de agotamiento de una sección de H.A. (`comphorm.dll`: `chcalc_volumenes_capacidad`).
 */
struct PuntoDiagramaInteraccionNM {
    double profundidad_fibra_neutra_x_m{0.0};
    double deformacion_hormigon_max_permil{3.5};
    double deformacion_acero_max_permil{10.0};
    double axil_resistente_n_rd_kn{0.0};       ///< > 0 compresión, < 0 tracción en convenio de pilares H.A.
    double momento_resistente_my_rd_knm{0.0};
    double momento_resistente_mz_rd_knm{0.0};
    double momento_resultante_m_rd_knm{0.0};
};

/**
 * @brief Resultado de equilibrio y comprobación biaxial `N - My - Mz` de una combinación en un tramo de pilar
 *        (`compsec.dll`: `csresult_panel_resultados_equilibrio_combinacion_seccion_hormigon`, `visesfpil.dll`: `vepesfycombpil_*`).
 */
struct ComprobacionCombinacionTramoPilar {
    std::uint32_t combinacion_id{0};
    std::string nombre_combinacion;
    bool en_cabeza_tramo{false};               ///< false = pie de tramo, true = cabeza de tramo (`vepesfycombpil_punto_crea`)

    // Esfuerzos solicitantes de primer orden y amplificados por esbeltez/pandeo (`chresist_momentos_amplificados_por_pandeo_comb_seccion_*`)
    double ned_compresion_kn{0.0};             ///< > 0 compresión, < 0 tracción
    double my_ed_1er_orden_knm{0.0};
    double mz_ed_1er_orden_knm{0.0};
    double my_ed_amplificado_knm{0.0};         ///< Incluye excentricidad mínima accidental e_0 = max(h/30, 20 mm) y 2.º orden e_2
    double mz_ed_amplificado_knm{0.0};
    double vy_ed_kn{0.0};
    double vz_ed_kn{0.0};

    // Capacidad última en la semirrecta de solicitación (`chcalc_volumenes_capacidad`)
    double n_rd_agotamiento_kn{0.0};
    double my_rd_agotamiento_knm{0.0};
    double mz_rd_agotamiento_knm{0.0};
    double v_rd_cortante_kn{0.0};

    // Equilibrio del plano de deformación en agotamiento (`chcalc_barras_equilibrio`)
    double profundidad_eje_neutro_x_m{0.0};
    double angulo_eje_neutro_deg{0.0};
    double eps_c_max_permil{0.0};
    double eps_s_traccion_permil{0.0};

    double aprovechamiento_flexocompresion_pct{0.0}; ///< `chresist_coef_aprovechamiento_solicitaciones_normales_pilar_*`
    double aprovechamiento_cortante_pct{0.0};
    bool cumple{true};
};

/**
 * @brief Posición de una barra de armadura en el despiece de un pilar o viga (`chcalc_longitudes_anclaje_pata_barra_*`, `chcalc_longitudes_solape_*`).
 */
struct PosicionDespieceArmadura {
    std::uint32_t posicion_id{1};
    std::string marca;                         ///< p.ej. "Pos. 1 (Esquinas)", "Pos. 2 (Caras X)", "Pos. 3 (Estribos)"
    std::uint32_t numero_barras{4};
    std::uint32_t diametro_mm{16};
    double longitud_recta_m{3.00};
    double longitud_anclaje_o_solape_m{0.50};  ///< `chcalc_longitudes_solape_barra_pilar` / `chcalc_longitudes_anclaje_pata_barra_viga`
    double longitud_patilla_m{0.0};            ///< `chcalc_longitud_patilla_estribo`
    double longitud_total_unitaria_m{3.50};
    double peso_total_kg{0.0};
};

/**
 * @brief Estado completo de un tramo de pilar en el Editor Avanzado de Pilares (`pilares.dll`, `pilararm.dll`, `visesfpil.dll`, `comphorm.dll`).
 */
struct ResumenEditorTramoPilar {
    std::uint32_t pilar_id{0};
    std::string referencia_pilar;
    std::size_t indice_tramo{0};
    double cota_inf_m{0.0};
    double cota_sup_m{3.0};
    double esbeltez_mecanica_lambda_x{0.0};
    double esbeltez_mecanica_lambda_y{0.0};
    double esbeltez_limite_inferior_lambda_lim{25.0};

    model::ArmadoTramoPilar armado_dispuesto{};
    double area_hormigon_cm2{0.0};
    double area_acero_dispuesta_cm2{0.0};
    double cuantia_geometrica_pct{0.0};

    std::vector<PuntoDiagramaInteraccionNM> curva_interaccion_n_my; ///< Plano My (Mz = 0)
    std::vector<PuntoDiagramaInteraccionNM> curva_interaccion_n_mz; ///< Plano Mz (My = 0)
    std::vector<ComprobacionCombinacionTramoPilar> comprobaciones_combinaciones;
    ComprobacionCombinacionTramoPilar combinacion_pesima{};

    model::ResultadoComprobacionPilar verificacion_normativa_y_capacidad{};
    std::vector<PosicionDespieceArmadura> despiece_armaduras;
    double peso_total_acero_tramo_kg{0.0};

    bool cumple_tramo{true};
    double aprovechamiento_maximo_pct{0.0};
    std::string listado_comprobaciones_tramo;
};

/**
 * @brief Punto de abscisa en un vano de viga con envolventes de esfuerzos y gráficas de área de armadura necesaria vs. dispuesta
 *        (`porvigas.dll`, `seccviga.dll`, `comphorm.dll`: `chresist_hay_areas_necesarias_capacidad_resistente_viga`).
 */
struct PuntoEnvolventeYArmadoViga {
    double x_m{0.0};                           ///< Abscisa desde el extremo izquierdo del vano (m)
    double x_rel{0.0};                         ///< Posición relativa x/L [0..1]

    // Envolventes de esfuerzos de cálculo ELU
    double m_ed_negativo_knm{0.0};             ///< Momento negativo envolvente (tracción superior, <= 0)
    double m_ed_positivo_knm{0.0};             ///< Momento positivo envolvente (tracción inferior, >= 0)
    double v_ed_max_kn{0.0};                   ///< Cortante envolvente de cálculo V_Ed (kN)
    double t_ed_max_knm{0.0};                  ///< Torsor envolvente de cálculo T_Ed (kN·m)

    // Momentos y cortantes últimos resistentes con el armado dispuesto (`chmomagovig_crea`)
    double m_rd_negativo_knm{0.0};
    double m_rd_positivo_knm{0.0};
    double v_rd_total_kn{0.0};                 ///< min(VRd,max biela comprimida, VRd,c + VRd,s)
    double v_rd_max_biela_kn{0.0};

    // Gráficas de Áreas de Armadura Necesaria vs. Efectivamente Dispuesta (con decalaje y longitudes de anclaje)
    double as_sup_necesaria_cm2{0.0};          ///< Área superior necesaria por flexión + cuantía mínima + torsión (cm^2)
    double as_sup_dispuesta_efectiva_cm2{0.0}; ///< Área superior dispuesta y anclada en esta abscisa (cm^2)
    double as_inf_necesaria_cm2{0.0};          ///< Área inferior necesaria por flexión + cuantía mínima + torsión (cm^2)
    double as_inf_dispuesta_efectiva_cm2{0.0}; ///< Área inferior dispuesta y anclada en esta abscisa (cm^2)
    double asw_s_necesaria_cm2_m{0.0};         ///< Área transversal de estribos necesaria por cortante + torsión + mínimo (cm^2/m)
    double asw_s_dispuesta_cm2_m{0.0};         ///< Área transversal de estribos dispuesta en esta zona (cm^2/m)

    // Abertura característica de fisura ELS w_k (`chcompfis_comprobaciones_posiciones_seccion_generica_viga`, `gcdocufis_*`)
    double abertura_fisura_wk_mm{0.0};
    double abertura_fisura_max_adm_mm{0.30};

    bool cumple_flexion_superior{true};
    bool cumple_flexion_inferior{true};
    bool cumple_cortante{true};
    bool cumple_fisuracion{true};
};

/**
 * @brief Estado completo de un vano de viga en el Editor Avanzado de Vigas / Pórticos (`porvigas.dll`, `seccviga.dll`, `comphorm.dll`).
 */
struct ResumenEditorVanoViga {
    std::uint32_t portico_id{0};
    std::uint32_t vano_id{0};
    std::string referencia_vano;
    double luz_vano_m{0.0};
    model::ArmadoVanoViga armado_dispuesto{};

    std::vector<PuntoEnvolventeYArmadoViga> puntos_grafica; ///< Estaciones a lo largo del vano
    model::ResultadoComprobacionVanoViga verificacion_capacidad{};
    std::vector<PosicionDespieceArmadura> despiece_armaduras;

    double as_sup_nec_max_izq_cm2{0.0};
    double as_sup_disp_izq_cm2{0.0};
    double as_sup_nec_max_der_cm2{0.0};
    double as_sup_disp_der_cm2{0.0};
    double as_inf_nec_max_vano_cm2{0.0};
    double as_inf_disp_vano_cm2{0.0};
    double asw_nec_max_cm2_m{0.0};
    double asw_disp_extremos_cm2_m{0.0};

    double flecha_activa_mm{0.0};              ///< Calculada con inercia fisurada equivalente de Branson `I_ef` (`chcalc_momento_inercia_*_seccion_homogeneizada_tiempos_calculo`)
    double flecha_activa_admisible_mm{0.0};
    double abertura_fisura_max_wk_mm{0.0};
    double peso_total_acero_vano_kg{0.0};

    bool cumple_vano{true};
    double aprovechamiento_maximo_pct{0.0};
    std::string listado_comprobaciones_vano;
};

/**
 * @brief Motor del Editor Avanzado de Pilares y del Editor Avanzado de Vigas/Pórticos de Hormigón Armado
 *        (`pilares.dll`, `pilararm.dll`, `visesfpil.dll`, `porvigas.dll`, `seccviga.dll`, `comphorm.dll`).
 */
class ReinforcedConcreteEditorEngine {
public:
    // --- Catálogo de tablas de armado estándar de pilares y vigas ---
    static const std::vector<std::uint32_t>& diametros_comerciales_mm();

    // --- Diagrama de interacción y comprobación de Pilares (`visesfpil.dll`, `comphorm.dll`) ---

    /**
     * @brief Genera la curva de interacción última `N - M` de un tramo de pilar integrando el diagrama parábola-rectángulo
     *        del hormigón ($\varepsilon_{c2}=2.0\text{\textperthousand}, \varepsilon_{cu2}=3.5\text{\textperthousand}$) y las capas de armadura (`chcalc_volumenes_capacidad`).
     */
    static std::vector<PuntoDiagramaInteraccionNM> generar_diagrama_interaccion_pilar(
        const model::TramoPilarPlanta& tramo,
        double fcd_mpa = 16.67,
        double fyd_mpa = 434.78,
        double es_mpa = 200000.0,
        bool alrededor_eje_y = true,
        std::size_t num_puntos = 25
    );

    /**
     * @brief Evalúa un estado de solicitación biaxial `(N_Ed, My_Ed, Mz_Ed, Vy_Ed, Vz_Ed)` sobre el tramo de pilar,
     *        incluyendo excentricidades de 1.º y 2.º orden por pandeo (`chresist_momentos_amplificados_por_pandeo_comb_seccion_*`)
     *        y contorno de Bresler ($(\mu_y)^{\alpha} + (\mu_z)^{\alpha} \le 1.0$).
     */
    static ComprobacionCombinacionTramoPilar comprobar_combinacion_tramo_pilar(
        const model::TramoPilarPlanta& tramo,
        std::uint32_t combinacion_id,
        std::string_view nombre_combinacion,
        bool en_cabeza_tramo,
        double ned_compresion_kn,
        double my_ed_knm,
        double mz_ed_knm,
        double vy_ed_kn = 0.0,
        double vz_ed_kn = 0.0,
        double fcd_mpa = 16.67,
        double fyd_mpa = 434.78
    );

    /**
     * @brief Analiza un tramo de pilar en el Editor Avanzado de Pilares extrayendo esfuerzos de `SolverSummary`
     *        (o combinaciones sintéticas si la barra no tiene esfuerzos) y generando despiece y listados.
     */
    static ResumenEditorTramoPilar analizar_tramo_pilar_editor(
        const model::Obra& obra,
        const model::PilarEstructural& pilar,
        std::size_t indice_tramo,
        const solver::SolverSummary& solver_results
    );

    /**
     * @brief Dimensiona automáticamente la sección y el armado simétrico (esquinas + caras + estribos) de un pilar
     *        respetando si tiene el armado bloqueado (`pilar.bloqueado_armado`) (`pilares_calc` / `pilares_armaduras`).
     */
    static std::vector<ResumenEditorTramoPilar> dimensionar_pilar_completo(
        model::Obra& obra,
        model::PilarEstructural& pilar,
        const solver::SolverSummary& solver_results,
        bool redimensionar_geometria_si_no_cumple = false
    );

    /**
     * @brief Edición manual interactiva del armado de un tramo de pilar y recálculo inmediato de cumplimiento (`pilares_armaduras`).
     */
    static ResumenEditorTramoPilar actualizar_armado_manual_tramo_pilar(
        model::Obra& obra,
        std::uint32_t pilar_id,
        std::size_t indice_tramo,
        const model::ArmadoTramoPilar& nuevo_armado,
        const solver::SolverSummary& solver_results
    );

    // --- Editor Avanzado de Vigas / Pórticos (`porvigas.dll`, `seccviga.dll`, `comphorm.dll`) ---

    /**
     * @brief Analiza un vano de viga en el Editor Avanzado de Vigas, calculando envolventes de esfuerzos,
     *        curvas de área necesaria vs. dispuesta ($A_{s,\text{nec}}(x)$ vs. $A_{s,\text{disp}}(x)$), fisuración $w_k$,
     *        flecha diferida de Branson y despiece completo de armaduras.
     */
    static ResumenEditorVanoViga analizar_vano_viga_editor(
        const model::Obra& obra,
        const model::PorticoVigasEstructural& portico,
        const model::VanoVigaHormigon& vano,
        const solver::SolverSummary& solver_results,
        std::size_t num_estaciones = 21
    );

    /**
     * @brief Dimensiona automáticamente el armado longitudinal (montaje, refuerzos negativos/positivos, piel) y transversal (estribos)
     *        de todos los vanos de un pórtico de vigas (`pv_calc` / `pv_ed_editar_todos_portico`).
     */
    static std::vector<ResumenEditorVanoViga> dimensionar_portico_vigas(
        model::Obra& obra,
        model::PorticoVigasEstructural& portico,
        const solver::SolverSummary& solver_results
    );

    /**
     * @brief Edición manual interactiva del armado de un vano de viga y recálculo inmediato de la gráfica $A_{s,\text{nec}}$ vs $A_{s,\text{disp}}$.
     */
    static ResumenEditorVanoViga actualizar_armado_manual_vano_viga(
        model::Obra& obra,
        std::uint32_t portico_id,
        std::uint32_t vano_id,
        const model::ArmadoVanoViga& nuevo_armado,
        const solver::SolverSummary& solver_results
    );
};

} // namespace cypeclon::codes
