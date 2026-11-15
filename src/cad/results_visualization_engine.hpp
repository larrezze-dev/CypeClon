#pragma once

#include "codes/design_check_engine.hpp"
#include "core/math.hpp"
#include "model/obra.hpp"
#include "solver/solver_engine.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::cad {

// ==============================================================================
// P-11.1 — Consulta Gráfica de Resultados en Estructura 3D
// (`visesf3d.dll`, `isolin.dll`, `modanres3d.dll`)
// ==============================================================================

/**
 * @brief Tipo de paleta cromática para mapas de isovalores y deformada (`isolin.dll`: `mapcolor_*`).
 */
enum class TipoEscalaMapaColor : std::uint8_t {
    AzulARojo_Jet = 0,                      ///< `mapcolor_crea_datos_dibujo_isovalores_escala_azul_a_rojo`
    BlancoValorMaximo = 1,                  ///< `mapcolor_crea_datos_dibujo_isovalores_color_blanco_valor_maximo`
    RojoValorMinimo = 2,                    ///< `mapcolor_crea_datos_dibujo_isovalores_rojo_valor_minimo`
    RojoValorMaximoAunqueSeaNegativo = 3,   ///< `mapcolor_crea_datos_dibujo_isovalores_rojo_valor_maximo_aunque_sea_negativo_udc`
    ValoresNegativosEnGris = 4              ///< `mapcolor_crea_datos_dibujo_isovalores_con_valores_negativos_en_gris`
};

/**
 * @brief Intervalo individual en la leyenda de isovalores (`mapcolor_nousar_panel_leyenda`).
 */
struct BandaLeyendaIsovalor {
    double valor_inferior{0.0};
    double valor_superior{0.0};
    double valor_medio{0.0};
    std::uint32_t color_rgb{0x0000FF};
    std::string etiqueta_rango;
};

/**
 * @brief Escala y leyenda completa de colores e isolíneas (`isolin.dll`: `dtisolin_*`, `isolins_*`, `mapcolor_*`).
 */
struct LeyendaIsovalores {
    TipoEscalaMapaColor tipo_escala{TipoEscalaMapaColor::AzulARojo_Jet};
    double valor_minimo{0.0};               ///< `dtisolin_valor_minimo_maximo`
    double valor_maximo{0.0};
    double incremento_isolineas{1.0};       ///< `isolins_crea_escala_lineal_imponiendo_incrementos`
    std::string magnitud_titulo;
    std::string unidad;
    bool valores_constantes{false};         ///< `mapcolor_son_valores_constantes_y_destruye`
    std::vector<BandaLeyendaIsovalor> bandas;
    std::vector<double> niveles_isolineas;

    std::uint32_t evaluar_color_rgb(double valor) const noexcept;
    std::string generar_texto_leyenda() const; ///< `mapcolor_nousar_anyade_listado_leyenda`
};

// ==============================================================================
// 1. Deformada 3D y Animación / Morphing (`visesf3d.dll`: `ve3animdef_*`, `isolin.dll`: `mapcolor_*_deformada`)
// ==============================================================================

enum class ModoOrigenDeformada : std::uint8_t {
    HipotesisSimple = 0,        ///< `txhipcmb_ids_hipotesis_con_modo_si_es_dinamica`
    Combinacion = 1,            ///< `txhipcmb_ids_combinaciones_sin_modos`
    ModoVibracionSismico = 2,   ///< `Calculo_ModosVibracion`
    ModoPandeoGlobal = 3,       ///< `Calculo_ModosPandeo` / `ve3solap_crea_solapa_pandeo_para_lanzar_como_dialogo`
    PasoPushover = 4            ///< `PO_Desplazamientos`
};

enum class ComponenteColorDeformada : std::uint8_t {
    DesplazamientoTotal_U = 0,
    Desplazamiento_Ux = 1,
    Desplazamiento_Uy = 2,
    Desplazamiento_Uz = 3,
    Giro_Rx = 4,
    Giro_Ry = 5,
    Giro_Rz = 6
};

struct PuntoDeformadaBarra3d {
    double x_local_m{0.0};
    core::Vec3d pos_indeformada_m{0.0, 0.0, 0.0};
    core::Vec3d desp_global_m{0.0, 0.0, 0.0};
    core::Vec3d pos_deformada_escalada_m{0.0, 0.0, 0.0};
    double valor_escalar_mm_o_mrad{0.0};
    std::uint32_t color_rgb{0x0080FF};
};

struct CurvaDeformadaBarra3d {
    std::uint32_t barra_id{0};
    std::string perfil_referencia;
    std::vector<PuntoDeformadaBarra3d> puntos;
    double desplazamiento_max_barra_mm{0.0};
};

struct TrianguloDeformadoLamina3d {
    std::uint32_t lamina_id{0};
    std::uint32_t triangulo_id{0};
    std::array<core::Vec3d, 3> vertices_indeformados_m{};
    std::array<core::Vec3d, 3> desplazamientos_globales_m{};
    std::array<core::Vec3d, 3> vertices_deformados_escalados_m{};
    std::array<double, 3> valores_escalares_mm{};
    std::array<std::uint32_t, 3> colores_vertices_rgb{};
};

/**
 * @brief Modelo completo de Deformada 3D con soporte de animación armónica (`ve3animdef_genera_animaciones_deformada`).
 */
struct EscenaDeformada3d {
    ModoOrigenDeformada origen{ModoOrigenDeformada::HipotesisSimple};
    std::uint32_t caso_o_modo_id{1};
    std::string titulo_caso;
    ComponenteColorDeformada componente_color{ComponenteColorDeformada::DesplazamientoTotal_U};
    double factor_escala_desplazamientos{50.0};
    double fase_animacion_rad{1.5707963267948966}; ///< pi/2 = deformación máxima (+1.0)

    std::vector<CurvaDeformadaBarra3d> barras_deformadas;
    std::vector<TrianguloDeformadoLamina3d> triangulos_laminas_deformados;
    LeyendaIsovalores leyenda{};

    double desplazamiento_maximo_mm{0.0};
    std::uint32_t nudo_desplazamiento_maximo_id{0};

    /**
     * @brief Evalúa un fotograma instantáneo de la animación de deformada (`mapcolor_dibuja_triangulo_isovalor_3d_para_animacion_morphing`).
     * @param fase_rad Ángulo de fase en radianes $[0, 2\pi]$ (factor $\sin(\text{fase\_rad})$).
     */
    EscenaDeformada3d generar_fotograma_animacion(double fase_rad) const;
};

// ==============================================================================
// 2. Diagramas 3D de Esfuerzos en Barras, Envolventes y Cursor Puntual (`modanres3d.dll`: `mar3dbarra_*`, `mar3desfbar_*`)
// ==============================================================================

enum class ComponenteEsfuerzoBarra : std::uint8_t {
    Axil_N = 0,         ///< Esfuerzo axil N (kN)
    Cortante_Ty = 1,    ///< Cortante según eje local Y (kN)
    Cortante_Tz = 2,    ///< Cortante según eje local Z (kN)
    Torsor_Mt = 3,      ///< Momento torsor Mt (kN·m)
    Flector_My = 4,     ///< Momento flector en plano fuerte XZ alrededor de Y (kN·m)
    Flector_Mz = 5,     ///< Momento flector en plano débil XY alrededor de Z (kN·m)
    Flecha_XY = 6,      ///< Flecha local en plano XY (mm)
    Flecha_XZ = 7       ///< Flecha local en plano XZ (mm)
};

struct PuntoDiagramaEsfuerzo3d {
    double x_m{0.0};
    double x_rel{0.0};
    core::Vec3d punto_directriz_3d{0.0, 0.0, 0.0};  ///< Coordenada 3D sobre el eje de la barra
    core::Vec3d punto_diagrama_3d{0.0, 0.0, 0.0};   ///< `mar3dbarra_coords_3D_punto_esfuerzo`
    double valor{0.0};
    std::uint32_t color_rgb{0x0080FF};
};

/**
 * @brief Lectura puntual con cursor sobre una barra (`mar3dbarra_esfuerzos_en_posicion`).
 */
struct LecturaCursorBarra3d {
    std::uint32_t barra_id{0};
    double abscisa_x_m{0.0};
    double x_rel{0.0};
    core::Vec3d coordenada_3d_m{0.0, 0.0, 0.0};
    core::Plane3d plano_seccion_transversal{}; ///< `mar3dbarra_plano_seccion`

    // Esfuerzos internos interpolados en x
    double n_kn{0.0};
    double ty_kn{0.0};
    double tz_kn{0.0};
    double mt_knm{0.0};
    double my_knm{0.0};
    double mz_knm{0.0};

    // Desplazamientos locales y globales interpolados en x (`mar3desfbar_get_desplazamientos_*`)
    core::Vec3d desp_local_mm{0.0, 0.0, 0.0};
    core::Vec3d desp_global_mm{0.0, 0.0, 0.0};
};

/**
 * @brief Ley o envolvente 3D de un esfuerzo sobre una barra (`mar3dbarra_*`, `ve3grubarra_*`).
 */
struct DiagramaEsfuerzoBarra3d {
    std::uint32_t barra_id{0};
    ComponenteEsfuerzoBarra componente{ComponenteEsfuerzoBarra::Flector_My};
    bool es_envolvente{false};
    double longitud_barra_m{0.0};
    double escala_dibujo_m_por_unidad{0.02};
    std::string unidad{"kN·m"};

    // Curva principal (ley de hipótesis/combinación o envolvente máxima positiva)
    std::vector<PuntoDiagramaEsfuerzo3d> curva_principal_o_max;
    // Curva secundaria (envolvente mínima negativa cuando es_envolvente == true)
    std::vector<PuntoDiagramaEsfuerzo3d> curva_envolvente_min;

    // Extremos registrados en la barra
    double valor_maximo{0.0};
    double abscisa_maximo_m{0.0};
    double valor_minimo{0.0};
    double abscisa_minimo_m{0.0};
};

// ==============================================================================
// 3. Isovalores e Isolíneas en Láminas MEF (`visesf3d.dll`: `ve3lamef_*`, `isolin.dll`: `isolins_*`, `mapcolor_*`)
// ==============================================================================

enum class MagnitudIsovalorLamina : std::uint8_t {
    Desplazamiento_Ux = 0,
    Desplazamiento_Uy = 1,
    Desplazamiento_Uz = 2,
    Desplazamiento_Total = 3,
    Esfuerzo_Nx = 4,
    Esfuerzo_Ny = 5,
    Esfuerzo_Nxy = 6,
    Momento_Mx = 7,
    Momento_My = 8,
    Momento_Mxy = 9,
    Cortante_Qx = 10,
    Cortante_Qy = 11,
    Tension_VonMises_CaraSup = 12,
    Tension_VonMises_CaraInf = 13,
    Tension_VonMises_Max = 14,
    Cuantia_WoodArmer_SupX = 15,
    Cuantia_WoodArmer_SupY = 16,
    Cuantia_WoodArmer_InfX = 17,
    Cuantia_WoodArmer_InfY = 18
};

struct TrianguloIsovalorLamina3d {
    std::uint32_t lamina_id{0};
    std::uint32_t triangulo_id{0};
    std::array<std::uint32_t, 3> nodo_ids{0, 0, 0};
    std::array<core::Vec3d, 3> vertices_3d{};
    std::array<double, 3> valores_vertices{0.0, 0.0, 0.0};
    std::array<std::uint32_t, 3> colores_vertices_rgb{0, 0, 0};
    double valor_baricentro{0.0};
};

/**
 * @brief Polilínea continua de nivel (isolínea) sobre una lámina (`isolins_append_isolineas_triangulo` / `isolins_unir_isolineas_con_extremos_muy_proximos`).
 */
struct CurvaIsolinea3d {
    std::uint32_t lamina_id{0};
    double valor_nivel{0.0};
    std::uint32_t color_rgb{0x000000};
    std::vector<core::Vec3d> puntos_3d;
};

struct MapaIsovaloresLamina3d {
    MagnitudIsovalorLamina magnitud{MagnitudIsovalorLamina::Tension_VonMises_Max};
    bool suavizado_en_nudos{true}; ///< false = `ve3datos_set_con_opcion_consulta_esfuerzos_sin_alisar`
    bool mostrar_isolineas{true};  ///< `dtisolin_hay_isolineas`
    bool mostrar_bordes_malla{true}; ///< `mapcolor_DibujarBordesTriang`

    std::vector<TrianguloIsovalorLamina3d> triangulos;
    std::vector<CurvaIsolinea3d> isolineas;
    LeyendaIsovalores leyenda{};

    double valor_minimo_global{0.0};
    double valor_maximo_global{0.0};
    core::Vec3d punto_maximo_3d{0.0, 0.0, 0.0};
};

// ==============================================================================
// 4. Reacciones en Apoyos y Coloreado por Aprovechamiento (`modanres3d.dll`: `mar3dnudreacc_*`, `visesf3d.dll`)
// ==============================================================================

struct VectorReaccionApoyo3d {
    std::uint32_t nudo_id{0};
    core::Vec3d posicion_nudo_3d{0.0, 0.0, 0.0};
    core::Vec3d fuerza_reaccion_kn{0.0, 0.0, 0.0};   ///< Fx, Fy, Fz (kN)
    core::Vec3d momento_reaccion_knm{0.0, 0.0, 0.0}; ///< Mx, My, Mz (kN·m)
    double resultante_fuerza_kn{0.0};
    double resultante_momento_knm{0.0};
    core::Vec3d extremo_flecha_fuerza_3d{0.0, 0.0, 0.0};
    std::string etiqueta_texto;
};

enum class RangoColorAprovechamiento : std::uint8_t {
    Holgado_Menor50Pct = 0,       ///< eta <= 50% (Verde / Azul)
    Moderado_50a90Pct = 1,        ///< 50% < eta <= 90% (Amarillo / Verde claro)
    Ajustado_90a100Pct = 2,       ///< 90% < eta <= 100% (Ámbar / Naranja)
    Superado_Mayor100Pct = 3,     ///< eta > 100% (Rojo intenso — No cumple)
    SinComprobar = 4              ///< Gris
};

struct ColorAprovechamientoBarra3d {
    std::uint32_t barra_id{0};
    std::string perfil_referencia;
    core::Vec3d p_ini_3d{0.0, 0.0, 0.0};
    core::Vec3d p_fin_3d{0.0, 0.0, 0.0};
    double aprovechamiento_pct{0.0};
    bool cumple{true};
    RangoColorAprovechamiento rango{RangoColorAprovechamiento::Holgado_Menor50Pct};
    std::uint32_t color_rgb{0x22C55E};
    std::string etiqueta_barra;
};

/**
 * @brief Motor Maestro de Consulta Gráfica de Resultados 3D (`visesf3d.dll`, `isolin.dll`, `modanres3d.dll`).
 */
class ResultsVisualizationEngine {
public:
    // --- 1. Escalas de color y leyendas (`isolin.dll`: `mapcolor_*`, `isolins_*`) ---
    static LeyendaIsovalores construir_leyenda_isovalores(
        double valor_min,
        double valor_max,
        std::string_view titulo,
        std::string_view unidad,
        TipoEscalaMapaColor tipo_escala = TipoEscalaMapaColor::AzulARojo_Jet,
        std::size_t num_bandas = 10
    );

    // --- 2. Deformada 3D y Animación (`visesf3d.dll`: `ve3animdef_*`) ---
    static EscenaDeformada3d generar_deformada_3d(
        const model::Obra& obra,
        const solver::SolverSummary& solver_results,
        ModoOrigenDeformada origen = ModoOrigenDeformada::HipotesisSimple,
        std::uint32_t caso_o_modo_id = 1,
        ComponenteColorDeformada componente_color = ComponenteColorDeformada::DesplazamientoTotal_U,
        double factor_escala = 0.0, ///< 0.0 = auto-escala para que u_max aparente sea ~10% de la dimensión de la obra
        std::size_t subdivisiones_por_barra = 12
    );

    // --- 3. Diagramas de Esfuerzos en Barras, Envolventes y Cursores (`modanres3d.dll`: `mar3dbarra_*`) ---
    static std::vector<DiagramaEsfuerzoBarra3d> generar_diagramas_esfuerzos_barras(
        const model::Obra& obra,
        const solver::SolverSummary& solver_results,
        ComponenteEsfuerzoBarra componente,
        bool usar_envolvente = true,
        std::uint32_t hipotesis_o_combinacion_id = 1,
        bool es_combinacion = false,
        double escala_dibujo = 0.0
    );

    static LecturaCursorBarra3d consultar_cursor_barra_en_abscisa(
        const model::Obra& obra,
        const solver::SolverSummary& solver_results,
        std::uint32_t barra_id,
        double abscisa_x_m,
        std::uint32_t hipotesis_o_combinacion_id = 1,
        bool es_combinacion = false
    );

    // --- 4. Mapas de Isovalores e Isolíneas en Láminas (`visesf3d.dll`: `ve3lamef_*`, `isolin.dll`: `isolins_*`) ---
    static MapaIsovaloresLamina3d generar_mapa_isovalores_laminas(
        const model::Obra& obra,
        const solver::SolverSummary& solver_results,
        MagnitudIsovalorLamina magnitud,
        std::uint32_t hipotesis_o_combinacion_id = 1,
        bool es_combinacion = false,
        bool suavizar_en_nudos = true,
        TipoEscalaMapaColor tipo_escala = TipoEscalaMapaColor::AzulARojo_Jet,
        std::size_t num_niveles_isolineas = 8
    );

    // --- 5. Reacciones en Apoyos y Coloreado por Aprovechamiento (`mar3dnudreacc_*`, `ve3datos_*`) ---
    static std::vector<VectorReaccionApoyo3d> generar_vectores_reacciones_apoyos(
        const model::Obra& obra,
        const solver::SolverSummary& solver_results,
        std::uint32_t hipotesis_o_combinacion_id = 1,
        bool es_combinacion = false,
        double longitud_flecha_max_m = 1.50
    );

    static std::vector<ColorAprovechamientoBarra3d> generar_mapa_colores_aprovechamiento_barras(
        const model::Obra& obra,
        const codes::ResumenComprobacionObra& design_summary
    );
};

} // namespace cypeclon::cad
