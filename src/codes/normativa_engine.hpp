#pragma once

#include "core/math.hpp"
#include "units/units.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::model {
class Obra;
}

namespace cypeclon::codes {

// ==============================================================================
// 1. P-05.1 — Normativas Estructurales y Catálogos de Materiales
//    (cypenorma.dll, datmater.dll, dmatext.dll, eurocodigo9.dll)
// ==============================================================================

enum class FamiliaNormativa : std::uint8_t {
    Hormigon = 0,        ///< cypenorm_caja_seleccion_norma_hormigon
    AceroLaminado = 1,   ///< cypenorm_caja_seleccion_norma_acero_laminado
    AceroConformado = 2, ///< cypenorm_caja_seleccion_norma_acero_conformado
    Madera = 3,          ///< cypenorm_caja_seleccion_norma_madera
    Aluminio = 4,        ///< cypenorm_caja_seleccion_norma_aluminio
    Cimentacion = 5      ///< Normativa geotécnica y estructural de cimentación
};

struct NormaEstructuralSpec {
    std::string id;
    std::string nombre_visible;
    std::string pais;
    FamiliaNormativa familia{FamiliaNormativa::Hormigon};
    double gamma_principal{1.50};   ///< gamma_c (hormigón) o gamma_M0 (acero/aluminio) o gamma_M (madera)
    double gamma_secundario{1.15};  ///< gamma_s (armaduras) o gamma_M1 (pandeo)
    double gamma_rotura{1.25};      ///< gamma_M2 (rotura sección neta / pernos)
    bool soporta_comprobacion_fuego{true};
    std::string material_defecto;
};

/**
 * @brief Propiedades mecánicas completas de Hormigón estructural (`datmater.dll` — `dathorm_*`).
 */
struct PropiedadesHormigon {
    std::string referencia;             ///< p.ej. "C25/30", "HA-25", "C30/37"
    std::string normativa_asociada;     ///< "Código Estructural / Eurocódigo 2", "EHE-08", "ACI 318"
    double fck_mpa{25.0};               ///< Resistencia característica a compresión cilíndrica f_ck (MPa)
    double fcm_mpa{33.0};               ///< Resistencia media a compresión f_cm = f_ck + 8 (MPa)
    double fctm_mpa{2.56};              ///< Resistencia media a tracción f_ctm (MPa)
    double ecm_mpa{31476.0};            ///< Módulo de elasticidad longitudinal secante E_cm (MPa)
    double gc_mpa{13115.0};             ///< Módulo de cortadura G_c = E_cm / (2*(1+nu)) (MPa)
    double poisson_nu{0.20};            ///< Coeficiente de Poisson nu (`datmater_modulo_poisson_hormigon`)
    double peso_especifico_kn_m3{25.0}; ///< Peso específico de hormigón armado gamma (kN/m^3)
    double alpha_termico_k_inv{1.0e-5}; ///< Coeficiente de dilatación térmica alpha_T (1/K)
    double eps_c2{0.0020};              ///< Deformación en rotura parábola-rectángulo eps_c2
    double eps_cu2{0.0035};             ///< Deformación última eps_cu2

    double fcd_mpa(double gamma_c = 1.50, double alpha_cc = 1.0) const noexcept {
        return (gamma_c > 0.0) ? (alpha_cc * fck_mpa / gamma_c) : fck_mpa;
    }
};

/**
 * @brief Propiedades mecánicas de Acero laminado y conformado (`datmater.dll` — `acerolam_*`, `acerocon_*`).
 */
struct PropiedadesAceroEstructural {
    std::string referencia;             ///< p.ej. "S275 (UNE-EN 10025-2)", "S235", "S355", "A36"
    std::string referencia_corta;       ///< p.ej. "S275", "S235", "S355", "A36", "A572 Gr.50"
    std::string norma_producto;         ///< "UNE-EN 10025-2", "EN 10346", "ASTM A36", "ASTM A572"
    bool es_conformado{false};          ///< false = laminado/armado, true = conformado en frío
    double fy_nominal_mpa{275.0};       ///< Límite elástico para t <= 16 mm (MPa)
    double fy_t16_40_mpa{265.0};        ///< Límite elástico para 16 < t <= 40 mm (MPa)
    double fy_t40_63_mpa{255.0};        ///< Límite elástico para 40 < t <= 63 mm (MPa)
    double fu_nominal_mpa{430.0};       ///< Tensión de rotura última f_u (MPa)
    double e_mpa{210000.0};             ///< Módulo de elasticidad E (MPa)
    double g_mpa{80769.23};             ///< Módulo de cortadura G (MPa)
    double poisson_nu{0.30};            ///< Coeficiente de Poisson nu
    double peso_especifico_kn_m3{78.5}; ///< Peso específico gamma (kN/m^3)
    double alpha_termico_k_inv{1.2e-5}; ///< Coeficiente de dilatación térmica alpha_T (1/K)

    /// `acerolam_limite_elastico_segun_espesor`
    double fy_segun_espesor(double espesor_mm) const noexcept {
        if (espesor_mm <= 16.0) return fy_nominal_mpa;
        if (espesor_mm <= 40.0) return fy_t16_40_mpa;
        return fy_t40_63_mpa;
    }
};

/**
 * @brief Propiedades mecánicas de Acero en barras para armaduras (`datmater.dll`, `dmatext.dll`).
 */
struct PropiedadesAceroArmaduras {
    std::string referencia;             ///< p.ej. "B 500 S", "B 500 SD", "B 400 S", "S-400", "S-500"
    double fyk_mpa{500.0};              ///< Límite elástico característico f_yk (MPa)
    double fuk_mpa{575.0};              ///< Carga unitaria de rotura f_uk (MPa)
    double es_mpa{200000.0};            ///< Módulo de elasticidad del acero de armar E_s (MPa)
    double peso_especifico_kn_m3{78.5}; ///< Peso específico gamma (kN/m^3)
    bool alta_ductilidad{false};        ///< Clase SD / C para diseño sismorresistente

    double fyd_mpa(double gamma_s = 1.15) const noexcept {
        return (gamma_s > 0.0) ? (fyk_mpa / gamma_s) : fyk_mpa;
    }

    /// Longitud básica de anclaje en Posición I (buena adherencia) y Posición II (`dxancla_long_anclaje_pos_I`)
    double longitud_anclaje_basica_mm(double diametro_mm, double fctd_mpa, bool posicion_ii = false) const noexcept;
};

/**
 * @brief Propiedades mecánicas de Madera estructural aserrada y laminada (`pzmadera.dll`, `CTE DB SE-M`, `EC5`).
 */
struct PropiedadesMadera {
    std::string referencia;             ///< p.ej. "C24", "C18", "C30", "GL24h", "GL28h", "GL32h"
    std::string descripcion_completa;   ///< p.ej. "Aserrada, procedente de coníferas o chopo (C24)"
    bool es_laminada_encolada{false};   ///< false = madera maciza aserrada, true = madera laminada encolada
    double fm_k_mpa{24.0};              ///< Resistencia característica a flexión f_m,k (MPa)
    double ft_0_k_mpa{14.5};            ///< Tracción paralela a la fibra f_t,0,k (MPa)
    double fc_0_k_mpa{21.0};            ///< Compresión paralela a la fibra f_c,0,k (MPa)
    double fv_k_mpa{4.0};               ///< Cortante f_v,k (MPa)
    double e0_mean_mpa{11000.0};        ///< Módulo de elasticidad medio paralelo E_0,mean (MPa)
    double e0_05_mpa{7400.0};           ///< Módulo de elasticidad al 5% percentil E_0,05 (MPa)
    double g_mean_mpa{690.0};           ///< Módulo de cortadura medio G_mean (MPa)
    double densidad_k_kg_m3{350.0};     ///< Densidad característica rho_k (kg/m^3)
    double peso_especifico_kn_m3{4.2};  ///< Peso específico medio para peso propio (kN/m^3)
    double alpha_termico_k_inv{5.0e-6}; ///< Dilatación térmica paralela a las fibras (1/K)
};

/**
 * @brief Propiedades mecánicas de Aleaciones de Aluminio extruido (`eurocodigo9.dll` — `ec9mat_*`).
 */
struct PropiedadesAluminio {
    std::string referencia;             ///< p.ej. "EN AW-5083 - F", "EN AW-6061 - T6", "EN AW-6082 - T6"
    std::string aleacion;               ///< "EN AW-5083", "EN AW-6060", "EN AW-6061", "EN AW-6082"
    std::string estado_entrega;         ///< "F", "O/H111", "T5", "T6"
    double fo_mpa{110.0};               ///< Límite elástico convencional 0.2% f_o (MPa) (`ec9mat_limite_elastico_Tn_m2`)
    double fu_mpa{270.0};               ///< Resistencia última a tracción f_u (MPa)
    double rho_o_haz{1.0};              ///< Factor reductor HAZ límite elástico (`ec9mat_factor_reduccion_HAZ`)
    double rho_u_haz{1.0};              ///< Factor reductor HAZ rotura
    double e_mpa{70000.0};              ///< Módulo de elasticidad E (`ec9mat_modulo_elasticidad_aluminio_Nw_por_mm2`)
    double g_mpa{27000.0};              ///< Módulo transversal G (`ec9mat_modulo_elasticidad_transversal_aluminio_Nw_por_mm2`)
    double poisson_nu{0.30};            ///< Coeficiente de Poisson (`ec9mat_modulo_poisson_aluminio`)
    double densidad_kg_m3{2700.0};      ///< Densidad (`ec9mat_densidad_aluminio_kg_por_m3`)
    double peso_especifico_kn_m3{26.5}; ///< Peso específico (kN/m^3)
    double alpha_termico_k_inv{2.3e-5}; ///< Coeficiente de dilatación térmica (`ec9mat_coeficiente_dilatacion_aluminio`)
    std::string clase_pandeo{"B"};      ///< Curva de pandeo BC según EN 1999-1-1 ("A" o "B")
};

/**
 * @brief Propiedades mecánicas de Pernos / Tornillos de uniones y anclajes (`acerolam_panel_acero_pernos`).
 */
struct PropiedadesPernoTornillo {
    std::string calidad;                ///< "4.6", "5.6", "6.8", "8.8", "10.9", "A325", "A490"
    std::string norma_serie;            ///< "ISO 898-1" o "ASTM"
    double fyb_mpa{640.0};              ///< Límite elástico del tornillo f_yb (MPa)
    double fub_mpa{800.0};              ///< Tensión de rotura del tornillo f_ub (MPa)
    double e_mpa{210000.0};
    double peso_especifico_kn_m3{78.5};
};

struct DiametroPernoMetrico {
    std::string nombre;           ///< "M12", "M16", "M20", "M22", "M24", "M27", "M30", "M36"
    double diametro_mm{20.0};
    double area_nominal_mm2{314.16};    ///< `acerolam_area_nominal_perno`
    double area_resistente_mm2{245.0};  ///< `acerolam_area_resistente_perno`
};

// ==============================================================================
// 2. P-05.2 — Categorías de Uso, Hipótesis de Carga y Combinaciones ELU / ELS
//    (catuso.dll, ghipocal.dll, combs.dll, combs3.dll, combsanl.dll, sestlim.dll)
// ==============================================================================

enum class CodigoCategoriaUso : std::uint8_t {
    G1_CubiertaConservacion = 0, ///< G1 / H: Cubiertas accesibles solo para conservación
    A_Residencial = 1,           ///< A / G2: Zonas residenciales
    B_Administrativo = 2,        ///< B: Oficinas y zonas administrativas
    C_PublicaConcurrencia = 3,   ///< C / G3: Zonas de reunión y pública concurrencia
    D_Comercial = 4,             ///< D / G4: Zonas comerciales
    E_Almacenamiento = 5,        ///< E / G5: Zonas de almacenamiento y uso industrial
    F_TraficoLigero = 6,         ///< F: Tráfico y aparcamiento de vehículos ligeros (<= 30 kN)
    G_TraficoPesado = 7          ///< G: Tráfico de vehículos pesados (30 kN a 160 kN)
};

/**
 * @brief Coeficientes de simultaneidad psi_0, psi_1 y psi_2 (`catuso.dll` — `catusocoef_*`, `stlcatusoob_*`).
 */
struct CategoriaUsoSpec {
    CodigoCategoriaUso codigo{CodigoCategoriaUso::G1_CubiertaConservacion};
    std::string clave_cte;          ///< "G1", "A", "B", "C", "D", "E", "F", "G"
    std::string clave_eurocodigo;   ///< "H", "A", "B", "C", "D", "E", "F", "G"
    std::string descripcion;
    double psi_0{0.0};              ///< Coeficiente de valor de combinación
    double psi_1{0.0};              ///< Coeficiente de valor frecuente
    double psi_2{0.0};              ///< Coeficiente de valor cuasipermanente
};

/**
 * @brief Naturalezas de hipótesis de carga (`ghipocal.dll` — `hipocal_naturaleza`, `combs3.dll`).
 */
enum class NaturalezaHipotesis : std::uint8_t {
    PesoPropio = 0,       ///< G1: Peso propio (`hipocal_es_peso_propio`)
    CargasMuertas = 1,    ///< G2: Cargas permanentes / solados (`hipocal_es_cargas_muertas`)
    SobrecargaUso = 2,    ///< Q: Sobrecarga de uso (`hipocal_es_sobrecarga_de_uso`)
    Viento = 3,           ///< Qv: Viento (`hipocal_es_viento`)
    Sismo = 4,            ///< E: Acción sísmica (`hipocal_es_sismica`)
    Nieve = 5,            ///< Qn: Nieve (`hipocal_es_nieve`)
    Temperatura = 6,      ///< Qt: Temperatura (`hipocal_es_temperatura`)
    EmpujesTerreno = 7,   ///< Gt: Empujes del terreno (`cb3hipusr_ids_hipotesis_empujes_terreno`)
    Accidental = 8,       ///< A: Acción accidental (`hipocal_es_accidental`)
    Fuego = 9             ///< Af: Situación accidental de incendio
};

/**
 * @brief Regla de combinabilidad entre hipótesis adicionales de una misma naturaleza (`cb3hipusr`).
 */
enum class DisposicionHipotesis : std::uint8_t {
    Simultanea = 0,   ///< Actúa siempre conjuntamente con las de su subgrupo
    Compatible = 1,   ///< Independiente: puede actuar sola o combinada con otras compatibles
    Incompatible = 2  ///< Mutuamente excluyente dentro de su `grupo_incompatibilidad` (p.ej. Viento V1..V8)
};

struct HipotesisCargaSpec {
    std::uint32_t id{1};
    std::string codigo;         ///< "PP", "CM", "Q1", "V1", "V2", "N1", "SX", "SY"
    std::string descripcion;    ///< "Peso propio", "Cargas muertas", "Sobrecarga de uso", "Viento +X (Presión interior)"
    NaturalezaHipotesis naturaleza{NaturalezaHipotesis::PesoPropio};
    DisposicionHipotesis disposicion{DisposicionHipotesis::Simultanea};
    CodigoCategoriaUso categoria_uso{CodigoCategoriaUso::G1_CubiertaConservacion};
    std::uint32_t grupo_incompatibilidad{0}; ///< Hipótesis con el mismo grupo_incompatibilidad > 0 e Incompatible no se suman entre sí
    bool es_automatica{true};
    bool actua{true};
    double psi_0{0.70};
    double psi_1{0.50};
    double psi_2{0.30};
};

enum class SituacionProyecto : std::uint8_t {
    ELU_PersistenteTransitoria = 0,       ///< E.L.U. Rotura — Situaciones persistentes o transitorias
    ELU_Accidental = 1,                   ///< E.L.U. Rotura — Situaciones accidentales
    ELU_Sismica = 2,                      ///< E.L.U. Rotura — Situaciones sísmicas
    ELU_Incendio = 3,                     ///< E.L.U. Rotura — Situación accidental de incendio (`sestlimge_*_incendio`)
    ELS_Caracteristica = 4,               ///< E.L.S. Combinación característica (poco probable)
    ELS_Frecuente = 5,                    ///< E.L.S. Combinación frecuente
    ELS_Cuasipermanente = 6,              ///< E.L.S. Combinación cuasipermanente
    TensionTerreno_Persistente = 7,       ///< Tensiones sobre el terreno — Persistente/Transitoria (`sestlimci_coeficientes_tension_del_terreno`)
    TensionTerreno_AccidentalSismica = 8, ///< Tensiones sobre el terreno — Sísmica/Accidental
    AnalisisNoLineal = 9                  ///< Combinación no lineal (`combsanl.dll` / `cnlcombs`)
};

struct TerminoCombinacion {
    std::uint32_t hipotesis_id{0};
    std::string codigo_hipotesis;
    double coeficiente{1.0};
};

struct CombinacionGenerada {
    std::uint32_t id{0};
    SituacionProyecto situacion{SituacionProyecto::ELU_PersistenteTransitoria};
    std::string nombre_formula; ///< p.ej. "1.35·PP + 1.35·CM + 1.50·Q1 + 0.90·V1"
    std::vector<TerminoCombinacion> terminos;
    bool considerar_segundo_orden_pdelta{false};
    std::uint32_t escalones_carga_anl{1};

    double coeficiente_de(std::uint32_t hipotesis_id) const noexcept;
};

struct ParametrosGeneracionCombinaciones {
    double gamma_g_desfavorable{1.35};
    double gamma_g_favorable{1.00};
    double gamma_q_desfavorable{1.50};
    double gamma_q_favorable{0.00};
    bool altitud_nieve_mayor_1000m{false};
    bool incluir_combinaciones_sismicas{false};
    bool incluir_combinaciones_incendio{false};
    bool incluir_els_desplazamientos{true};
    bool incluir_tensiones_terreno{true};
};

// ==============================================================================
// 3. P-05.3 — Acción Sísmica, Viento y Resistencia al Fuego
//    (sismo.dll, sismocal.dll, aisc341comp.dll, veviento.dll, rstfuego.dll, catfabpin.dll, catfabmor.dll)
// ==============================================================================

enum class NormaSismica : std::uint8_t {
    SinSismo = 0,
    NCSE_02_Espana = 1,         ///< Norma de Construcción Sismorresistente NCSE-02
    Eurocodigo8_EN1998_1 = 2,   ///< Eurocódigo 8 (EN 1998-1)
    ASCE_7_22_USA = 3,          ///< ASCE/SEI 7-22
    CFE_Sismo_Mexico = 4,       ///< Manual de Diseño de Obras Civiles CFE (México)
    EspectroUsuario = 5         ///< Espectro definido por el usuario
};

enum class TipoTerrenoSismico : std::uint8_t {
    TipoI_RocaCompacta_A = 0,  ///< Terreno Tipo I (NCSE-02) / Clase A (EC8)
    TipoII_RocaFracturada_B = 1,///< Terreno Tipo II (NCSE-02) / Clase B (EC8)
    TipoIII_SueloMedio_C = 2,  ///< Terreno Tipo III (NCSE-02) / Clase C (EC8)
    TipoIV_SueloBlando_D = 3   ///< Terreno Tipo IV (NCSE-02) / Clase D (EC8)
};

enum class MetodoSuperposicionModal : std::uint8_t {
    CQC = 0,  ///< Combinación Cuadrática Completa (Complete Quadratic Combination)
    SRSS = 1  ///< Raíz Cuadrada de la Suma de los Cuadrados
};

enum class SistemaSismorresistenteAisc341 : std::uint8_t {
    Ninguno = 0,
    OMF_PorticoOrdinarioMomento = 1,       ///< Ordinary Moment Frame
    IMF_PorticoIntermedioMomento = 2,      ///< Intermediate Moment Frame
    SMF_PorticoEspecialMomento = 3,        ///< Special Moment Frame
    OCBF_PorticoArriostradoConcentricoOrdinario = 4,
    SCBF_PorticoArriostradoConcentricoEspecial = 5,
    EBF_PorticoArriostradoExcentrico = 6   ///< Eccentrically Braced Frame (`as3compport_append_comprobacion_analisis_portico_EBF`)
};

struct PuntoEspectroSismico {
    double periodo_s{0.0};
    double aceleracion_g{0.0};
    double aceleracion_m_s2{0.0};
};

struct ConfiguracionSismo {
    bool activo{false};
    NormaSismica norma{NormaSismica::NCSE_02_Espana};
    TipoTerrenoSismico tipo_terreno{TipoTerrenoSismico::TipoII_RocaFracturada_B};
    MetodoSuperposicionModal metodo_superposicion{MetodoSuperposicionModal::CQC};
    SistemaSismorresistenteAisc341 sistema_aisc341{SistemaSismorresistenteAisc341::Ninguno};

    double aceleracion_basica_ag_g{0.12};   ///< a_b / g o a_gR / g (en fracciones de g)
    double coeficiente_contribucion_k{1.0}; ///< K en NCSE-02 (1.0 a 1.3)
    double factor_importancia_gamma_i{1.0}; ///< Importancia normal = 1.0, especial = 1.3
    double factor_comportamiento_qx{2.0};   ///< Ductilidad / comportamiento q en dirección X (`datsismo_factores_comportamiento_sismico_EUROCODIGOS`)
    double factor_comportamiento_qy{2.0};   ///< Ductilidad / comportamiento q en dirección Y
    double amortiguamiento_porcentaje{5.0}; ///< Amortiguamiento relativo xi (%)
    double fraccion_sobrecarga_masa{0.30};  ///< `datsismo_coef_permanencia_sobrecarga`
    double fraccion_nieve_masa{0.00};       ///< `datsismo_coef_permanencia_nieve_sin_accion_sismica`

    bool numero_modos_automatico{true};     ///< `siscnummodos_crea_porcentaje_masa_movilizada` (90% masa)
    std::uint32_t numero_modos_manual{6};   ///< `siscnummodos_crea_num_modos_usuario`
    double porcentaje_masa_minimo{90.0};

    bool comprobar_cortante_basal{true};    ///< `sisopccorbas_esta_activada_comprobacion_cortante_basal`
    double factor_reductor_cortante_basal{0.85}; ///< `sisopccorbas_factor_reductor_cortante_basal`

    std::vector<PuntoEspectroSismico> curva_usuario;

    /// Calcula la ordenada espectral de diseño S_d(T) en fracciones de g
    double calcular_aceleracion_diseno_g(double periodo_s, bool direccion_y = false) const noexcept;

    /// Genera la curva completa del espectro de diseño para visualización y solver modal (`sisadrsut`)
    std::vector<PuntoEspectroSismico> generar_curva_espectro(std::size_t num_puntos = 60, double t_max_s = 4.0) const;
};

enum class NormaViento : std::uint8_t {
    CTE_DB_SE_AE = 0,
    Eurocodigo1_EN1991_1_4 = 1,
    ASCE_7_22 = 2,
    CIRSOC_102 = 3
};

enum class ZonaEolicaCte : std::uint8_t {
    ZonaA_26ms = 0, ///< v_b = 26 m/s (q_b = 0.42 kN/m^2)
    ZonaB_27ms = 1, ///< v_b = 27 m/s (q_b = 0.45 kN/m^2)
    ZonaC_29ms = 2  ///< v_b = 29 m/s (q_b = 0.52 kN/m^2)
};

enum class GradoAsperezaEntorno : std::uint8_t {
    GradoI_BordeMar = 0,     ///< I: Borde del mar o lagos
    GradoII_RuralLlano = 1,  ///< II: Terreno rural llano sin obstáculos
    GradoIII_RuralAccidentado = 2, ///< III: Zona rural accidentada o con vegetación/edificios dispersos
    GradoIV_UrbanaIndustrial = 3,  ///< IV: Zona urbana, industrial o forestal
    GradoV_CentroGrandesCiudades = 4 ///< V: Centros de negocio de grandes ciudades
};

/**
 * @brief Configuración de Acción de Viento (`veviento.dll`).
 */
struct ConfiguracionViento {
    bool activo{true};
    NormaViento norma{NormaViento::CTE_DB_SE_AE};
    ZonaEolicaCte zona_eolica{ZonaEolicaCte::ZonaB_27ms};
    GradoAsperezaEntorno grado_aspereza{GradoAsperezaEntorno::GradoIII_RuralAccidentado};
    double velocidad_basica_m_s{27.0};
    double periodo_retorno_anos{50.0};

    /// Presión dinámica básica del viento q_b (kN/m^2)
    double presion_dinamica_basica_kn_m2() const noexcept;

    /// Coeficiente de exposición c_e(z) según altura z (m) y grado de aspereza (CTE DB SE-AE / EC1)
    double coeficiente_exposicion_ce(double z_m) const noexcept;

    /// Presión estática de cálculo a la altura z: q_e(z) = q_b * c_e(z) (kN/m^2)
    double presion_estatica_z_kn_m2(double z_m) const noexcept {
        return presion_dinamica_basica_kn_m2() * coeficiente_exposicion_ce(z_m);
    }
};

enum class ClaseResistenciaFuego : std::uint8_t {
    R15 = 0,  ///< 15 minutos (`rfclase_panel_nota_informativa_de_la_resistencia_15`)
    R30 = 1,  ///< 30 minutos
    R45 = 2,  ///< 45 minutos
    R60 = 3,  ///< 60 minutos
    R90 = 4,  ///< 90 minutos
    R120 = 5, ///< 120 minutos
    R180 = 6, ///< 180 minutos
    R240 = 7  ///< 240 minutos
};

enum class TipoRevestimientoIgnifugo : std::uint8_t {
    SinProteccion = 0,
    PinturaIntumescente = 1, ///< `catfabpin.dll` (`rfbibcat_get_biblioteca_pinturas_intumescentes`)
    MorteroProyeccion = 2,   ///< `catfabmor.dll` (`rfbibcat_get_biblioteca_morteros_intumescentes`)
    PlacaSilicatoYeso = 3    ///< Placas de fibrosilicato / cartón-yeso
};

struct RevestimientoIgnifugoSpec {
    std::string id;
    std::string fabricante;
    std::string nombre_comercial;
    TipoRevestimientoIgnifugo tipo{TipoRevestimientoIgnifugo::PinturaIntumescente};
    double conductividad_w_mk{0.10};    ///< Conductividad térmica efectiva lambda_p (W/(m·K))
    double calor_especifico_j_kgk{1100.0}; ///< Calor específico c_p (J/(kg·K))
    double densidad_kg_m3{300.0};       ///< Densidad seca rho_p (kg/m^3)
    double espesor_min_mm{0.25};        ///< Espesor mínimo comercial aplicable (mm)
    double espesor_max_mm{35.0};        ///< Espesor máximo comercial aplicable (mm)
};

/**
 * @brief Configuración de Resistencia al Fuego (`rstfuego.dll`, `ed3incob`, `catfabpin.dll`, `catfabmor.dll`).
 */
struct ConfiguracionResistenciaFuego {
    bool comprobar_resistencia_fuego{false};
    ClaseResistenciaFuego resistencia_requerida{ClaseResistenciaFuego::R60};
    TipoRevestimientoIgnifugo tipo_proteccion_acero{TipoRevestimientoIgnifugo::PinturaIntumescente};
    std::string producto_revestimiento_id{"PROMAPAINT_SC4"};
    double temperatura_critica_acero_c{550.0}; ///< Temperatura crítica por defecto theta_cr (°C)
    double velocidad_carbonizacion_madera_mm_min{0.65}; ///< beta_0 según CTE DB SI / EN 1995-1-2

    std::uint32_t tiempo_requerido_minutos() const noexcept;
    std::string etiqueta_clase() const;

    /// Calcula el espesor mínimo necesario de protección (mm) para un perfil metálico según su masividad A_m/V (m^-1) (`rfcomprevinc_calcula`)
    double calcular_espesor_proteccion_necesario_mm(
        double masividad_am_v_m_inv,
        double temp_critica_c,
        const RevestimientoIgnifugoSpec& revestimiento) const noexcept;
};

// ==============================================================================
// 4. P-05.4 — Clonación Completa del Generador de Pórticos
//    (`ed3dlib.dll` — `ed3genport_*`, `ed3obra_append_datos_importacion_generador_porticos`)
// ==============================================================================

enum class TipologiaVanoPortico : std::uint8_t {
    DosAguas = 0, ///< Pórtico simétrico o asimétrico a dos aguas
    UnAgua = 1,   ///< Pórtico a un agua (monopitch)
    Celosia = 2   ///< Cercha / celosía a dos aguas con cordón inferior horizontal
};

enum class TipoVanoCorrea : std::uint8_t {
    Simple_1Vano = 1,
    Continuo_2Vanos = 2,
    Continuo_3Vanos = 3
};

enum class TipoFijacionCorrea : std::uint8_t {
    FijacionRigida = 0, ///< Chapa colaborante / panel sándwich que coacciona pandeo lateral del ala superior
    FijacionGancho = 1  ///< Fijación por gancho sin coacción lateral
};

struct VanoPorticoSpec {
    TipologiaVanoPortico tipologia{TipologiaVanoPortico::DosAguas};
    double luz_m{20.0};                  ///< Luz del vano transversal L (m)
    double altura_pilar_izq_m{7.0};      ///< Altura del pilar izquierdo h_1 (m)
    double altura_pilar_der_m{7.0};      ///< Altura del pilar derecho h_2 (m)
    double altura_cumbrera_m{9.0};       ///< Cota Z de la cumbrera h_c (m) (para DosAguas / Celosia)
    std::string perfil_pilares{"HEB 240"};
    std::string perfil_dinteles{"IPE 300"};
    std::string perfil_cordon_inferior{"IPE 180"}; ///< Usado cuando `tipologia == TipologiaVanoPortico::Celosia`
    std::string perfil_montantes_diagonales{"SHS 80x4"};
    std::uint32_t divisiones_celosia_por_faldón{4};
};

struct ConfiguracionCorreasPortico {
    bool dimensionar_correas_cubierta{true};
    bool dimensionar_correas_fachada{false};
    std::string familia_perfil{"ZF"};           ///< "ZF", "CF", "IPE", "UPN"
    std::string perfil_seleccionado{"ZF-200x2.5"};
    std::string acero_correa{"S275"};
    double separacion_cubierta_m{1.50};         ///< Separación entre correas de cubierta d_c (m)
    double separacion_fachada_m{1.50};          ///< Separación entre correas de fachada (m)
    TipoVanoCorrea tipo_vano{TipoVanoCorrea::Continuo_2Vanos};
    TipoFijacionCorrea tipo_fijacion{TipoFijacionCorrea::FijacionRigida};
    double limite_flecha_relativa{200.0};       ///< L / 200
};

struct ResultadoDimensionamientoCorreas {
    std::string perfil_optimo;
    double separacion_m{1.50};
    std::uint32_t num_correas_por_faldon{0};
    double aprovechamiento_tension_pct{0.0};
    double aprovechamiento_flecha_pct{0.0};
    double peso_total_correas_kg{0.0};
    bool cumple{true};
};

struct ConfiguracionGeneradorPorticos {
    std::vector<VanoPorticoSpec> vanos{VanoPorticoSpec{}}; ///< 1 o varios vanos adosados
    std::uint32_t num_modulos_longitudinales{6};           ///< Número de vanos entre pórticos a lo largo del eje Y
    double separacion_entre_porticos_m{6.0};               ///< Distancia entre pórticos s (m)
    bool apoyos_empotrados{true};                          ///< true = empotrados en base, false = articulados
    bool incluir_pilares_hastiales{true};                  ///< Pilares intermedios en fachadas frontal/trasera
    std::uint32_t num_pilares_hastiales_por_vano{2};

    // Cerramientos y sobrecargas gravitatorias
    bool cerramiento_cubierta{true};
    double peso_cerramiento_cubierta_kn_m2{0.35};          ///< Chapa/panel + correas (kN/m^2)
    double sobrecarga_cubierta_kn_m2{0.40};                ///< Categoría G1 conservación (0.40 kN/m^2)
    bool cerramiento_lateral{true};
    double peso_cerramiento_lateral_kn_m2{0.30};           ///< Cerramiento de fachada (kN/m^2)

    // Nieve (CTE DB SE-AE / EC1)
    bool con_sobrecarga_nieve{true};
    std::uint8_t zona_climatica_nieve{2};                  ///< Zona 1..7 de invierno (CTE Anejo E)
    double altitud_emplazamiento_m{500.0};
    double carga_nieve_horizontal_sk_kn_m2{0.60};          ///< Valor característico de nieve en plano horizontal s_k (kN/m^2)

    // Viento (CTE DB SE-AE / EC1)
    ConfiguracionViento viento{};
    bool edificio_con_huecos_abiertos{true};               ///< Genera sub-hipótesis con presión interior (+cpi) y succión interior (-cpi)

    // Correas
    ConfiguracionCorreasPortico correas{};
};

struct ResumenExportacionGeneradorPorticos {
    std::size_t nudos_creados{0};
    std::size_t barras_creadas{0};
    std::size_t panyos_creados{0};
    std::size_t hipotesis_creadas{0};
    std::size_t cargas_superficiales_creadas{0};
    std::size_t cargas_lineales_creadas{0};
    std::size_t combinaciones_generadas{0};
    ResultadoDimensionamientoCorreas dimensionamiento_correas{};
};

/**
 * @brief Motor completo del Generador de Pórticos (`ed3genport` en `ed3dlib.dll`).
 */
class PorticoGeneratorEngine {
public:
    /// Calcula el coeficiente de forma de nieve mu_1 según el ángulo de inclinación del faldón alpha (grados)
    static double coeficiente_forma_nieve_mu1(double angulo_faldon_deg) noexcept;

    /// Calcula la carga de nieve superficial proyectada sobre el faldón q_n = mu_1 * s_k (kN/m^2)
    static double calcular_carga_nieve_faldon_kn_m2(
        const ConfiguracionGeneradorPorticos& cfg,
        double angulo_faldon_deg) noexcept;

    /// Dimensiona las correas de cubierta comprobando flexión y flecha bajo peso propio, sobrecarga, nieve y viento
    static ResultadoDimensionamientoCorreas dimensionar_correas_cubierta(
        const ConfiguracionGeneradorPorticos& cfg);

    /// Exporta la nave completa (nudos, pilares, dinteles/celosías, vigas de atado, hastiales, paños, hipótesis y cargas) a `model::Obra`
    static ResumenExportacionGeneradorPorticos exportar_a_obra(
        const ConfiguracionGeneradorPorticos& cfg,
        model::Obra& obra);
};

// ==============================================================================
// 5. Clase Maestra `NormativaEngine` (Fachada unificada de Fase 4)
// ==============================================================================

class NormativaEngine {
public:
    static std::string_view active_steel_code() noexcept { return "CTE DB SE-A / Eurocódigo 3"; }
    static std::string_view active_concrete_code() noexcept { return "Código Estructural / Eurocódigo 2"; }

    // --- Catálogos de Normativas (P-05.1) ---
    static const std::vector<NormaEstructuralSpec>& catalogo_normas(FamiliaNormativa familia);
    static const NormaEstructuralSpec* buscar_norma(FamiliaNormativa familia, std::string_view id_o_nombre) noexcept;
    static std::string construir_resumen_normas(
        std::string_view norma_hormigon,
        std::string_view norma_acero_laminado,
        std::string_view norma_acero_conformado,
        std::string_view norma_madera,
        std::string_view norma_aluminio);

    // --- Catálogos de Materiales (P-05.1) ---
    static const std::vector<PropiedadesHormigon>& catalogo_hormigones();
    static const PropiedadesHormigon* buscar_hormigon(std::string_view ref) noexcept;

    static const std::vector<PropiedadesAceroEstructural>& catalogo_aceros_laminados();
    static const std::vector<PropiedadesAceroEstructural>& catalogo_aceros_conformados();
    static const PropiedadesAceroEstructural* buscar_acero_estructural(std::string_view ref) noexcept;

    static const std::vector<PropiedadesAceroArmaduras>& catalogo_aceros_armaduras();
    static const PropiedadesAceroArmaduras* buscar_acero_armaduras(std::string_view ref) noexcept;

    static const std::vector<PropiedadesMadera>& catalogo_maderas();
    static const PropiedadesMadera* buscar_madera(std::string_view ref) noexcept;

    static const std::vector<PropiedadesAluminio>& catalogo_aluminios();
    static const PropiedadesAluminio* buscar_aluminio(std::string_view ref) noexcept;

    static const std::vector<PropiedadesPernoTornillo>& catalogo_pernos_tornillos();
    static const PropiedadesPernoTornillo* buscar_perno_tornillo(std::string_view calidad) noexcept;
    static const std::vector<DiametroPernoMetrico>& catalogo_diametros_pernos();

    // --- Categorías de Uso, Hipótesis y Combinaciones (P-05.2) ---
    static const std::vector<CategoriaUsoSpec>& catalogo_categorias_uso();
    static const CategoriaUsoSpec& obtener_categoria_uso(CodigoCategoriaUso codigo) noexcept;
    static std::vector<HipotesisCargaSpec> crear_hipotesis_por_defecto(bool con_viento = false, bool con_sismo = false, bool con_nieve = false);
    static std::vector<CombinacionGenerada> generar_combinaciones(
        const std::vector<HipotesisCargaSpec>& hipotesis,
        const ParametrosGeneracionCombinaciones& params = {});

    // --- Catálogos de Protección al Fuego (P-05.3) ---
    static const std::vector<RevestimientoIgnifugoSpec>& catalogo_pinturas_intumescentes();
    static const std::vector<RevestimientoIgnifugoSpec>& catalogo_morteros_intumescentes();
    static const RevestimientoIgnifugoSpec* buscar_revestimiento_ignifugo(std::string_view id) noexcept;
};

} // namespace cypeclon::codes
