#pragma once

#include "core/math.hpp"
#include "io/cypesave.hpp"
#include "io/undoredo.hpp"
#include "model/shells_panels_loads.hpp"
#include "model/structure_elements.hpp"
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

enum class TipoBarraEstructural : std::uint8_t {
    Generica = 0,
    ColumnaPilar = 1,
    Viga = 2,
    Tirante = 3
};

struct Nudo3d {
    std::uint32_t id{0};
    core::Vec3d pos{0.0, 0.0, 0.0};
    std::string referencia;
    VinculacionExterior vinc_ext{VinculacionExterior::Libre};
    bool empotrado_interior{true};
    std::uint32_t capa_id{1};

    // Fase 5 (P-06.1): Vinculación exterior detallada, interior parcial y dimensión finita
    DatosVinculacionExteriorNudo vinc_ext_datos{};
    TipoVinculacionInterior tipo_vinc_int{TipoVinculacionInterior::Empotrado};
    double grado_empotramiento_interior{1.0};
    double radio_dimension_finita_m{0.0};
    std::uint32_t diafragma_id{0};
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
    TipoBarraEstructural tipo_estructural{TipoBarraEstructural::Generica};
    double desp_y_m{0.0};  ///< Desplazamiento/enrase local Y (m)
    double desp_z_m{0.0};  ///< Desplazamiento/enrase local Z (m)

    // Fase 5 (P-06.2 / P-06.3): Propiedades mecánicas, pandeo, flecha, empotramiento y rótulas
    bool solo_compresion{false};
    DisposicionSeccionBarra disposicion{};
    DatosPandeoBarra pandeo{};
    DatosPandeoLateralBarra pandeo_lateral{};
    DatosFlechaBarra flecha{};
    EmpotramientoExtremosBarra empotramiento{};
    AsignacionRotulaPlasticaBarra rotulas_plasticas{};
    std::uint32_t pieza_id{0};
    std::uint32_t agrupacion_id{0};
    bool incluir_peso_propio{true}; ///< `Carga_barra_activar_desactiva_pp`
};

struct Lamina3d {
    std::uint32_t id{0};
    std::vector<core::Vec3d> vertices;
    double espesor_m{0.20};
    std::string material{"C25/30"};
    std::uint32_t capa_id{1};

    // Fase 6 (P-07.1): Propiedades completas de Lámina MEF Shell (`calcmef`, `calcmallad3d`, `visesf3d`)
    FamiliaMaterialLamina familia_material{FamiliaMaterialLamina::Hormigon};
    double modulo_elasticidad_mpa{31000.0};
    double coef_poisson{0.20};
    double peso_especifico_kn_m3{25.0};
    bool incluir_peso_propio{true};
    EnraseLamina enrase{EnraseLamina::PlanoMedio};
    double desfase_z_m{0.0};
    std::uint32_t arista_eje_x_index{0};
    double angulo_giro_ejes_rad{0.0};
    bool invertir_eje_z{false};
    double tamano_discretizacion_m{1.00};
    FactoresRigidezLamina factores_rigidez{};
    std::vector<HuecoLamina3d> huecos;
    std::vector<VinculacionAristaLamina> vinculaciones_aristas;
    std::vector<BandaIntegracionLamina> bandas_integracion;
    MallaLamina3d malla{};
};

struct Panyo3d {
    std::uint32_t id{0};
    std::vector<core::Vec3d> vertices;
    core::Vec3d direccion_reparto{1.0, 0.0, 0.0};
    std::uint32_t capa_id{1};

    // Fase 6 (P-07.2): Referencia y lista opcional de barras portantes asociadas al paño (`panyos3d.dll`)
    std::string referencia{"Paño 1"};
    std::vector<std::uint32_t> barras_activas_reparto;
};

/**
 * @brief Hipótesis de carga almacenada en la Obra (`ghipocal.dll` / `combs3.dll`).
 */
struct HipotesisCargaObra {
    std::uint32_t id{1};
    std::string codigo{"PP"};
    std::string descripcion{"Peso propio"};
    std::uint8_t naturaleza{0};        ///< 0=PesoPropio, 1=CargasMuertas, 2=SobrecargaUso, 3=Viento, 4=Sismo, 5=Nieve, 6=Temp, 7=Terreno, 8=Accidental, 9=Fuego
    std::uint8_t disposicion{0};       ///< 0=Simultanea, 1=Compatible, 2=Incompatible
    std::uint8_t categoria_uso{0};     ///< CodigoCategoriaUso (G1=0, A=1, B=2, C=3, D=4, E=5, F=6, G=7)
    std::uint32_t grupo_incompatibilidad{0};
    bool es_automatica{true};
    bool actua{true};
};

/**
 * @brief Carga superficial sobre un Paño 3D (`ed3genport_append_superficie_carga` / `panyos3d.dll`).
 */
struct CargaSuperficialPanyo {
    std::uint32_t id{0};
    std::uint32_t panyo_id{0};
    std::uint32_t hipotesis_id{1};
    double valor_kn_m2{0.0};           ///< Magnitud de la carga superficial (kN/m^2)
    core::Vec3d direccion{0.0, 0.0, -1.0}; ///< Vector director (p.ej. {0,0,-1} gravitatoria o normal al paño para viento)
    bool en_ejes_locales_panyo{false};
    std::string descripcion_zona;      ///< p.ej. "Cubierta Zona F", "Fachada Barlovento D"

    // Fase 6 / Fase 7 (P-07.2 / P-08.1): Ámbitos parciales y distribución variable (`p3dcarsup_*`)
    AmbitoCargaSuperficial ambito{AmbitoCargaSuperficial::TodoElPanyo};
    bool es_variable{false};
    double valor_q2_kn_m2{0.0};
    double valor_q3_kn_m2{0.0};
    std::vector<core::Vec3d> poligono_zona;
};

/**
 * @brief Carga sobre una Barra 3D (`ed3genport_append_carga_a_ultima_barra` / `ed3mnucar.c`).
 */
struct CargaLinealBarra {
    std::uint32_t id{0};
    std::uint32_t barra_id{0};
    std::uint32_t hipotesis_id{1};
    double q_ini_kn_m{0.0};            ///< Valor inicial de carga lineal (kN/m) o fuerza puntual P (kN) o momento M (kN·m)
    double q_fin_kn_m{0.0};            ///< Valor final de carga lineal (kN/m)
    double x_ini_rel{0.0};             ///< Posición relativa inicial [0..1]
    double x_fin_rel{1.0};             ///< Posición relativa final [0..1]
    core::Vec3d direccion{0.0, 0.0, -1.0};
    bool en_ejes_locales{false};

    // Fase 7 (P-08.1): Tipología completa de carga en barra (puntual, momento, térmica, pretensado y reparto de paños)
    TipoCargaBarra tipo{TipoCargaBarra::Uniforme};
    SistemaEjesCarga sistema_ejes{SistemaEjesCarga::Generales};
    double delta_temp_uniforme_c{0.0};
    double delta_temp_gradiente_c{0.0};
    double axil_pretensado_kn{0.0};
    double excentricidad_pretensado_m{0.0};
    bool generada_por_panyo{false};
    std::uint32_t panyo_origen_id{0};
};

/**
 * @brief Carga puntual y/o desplazamiento prescrito en Nudo 3D (`ed3obra_append_listado_cargas_nudos_a_documento`).
 */
struct CargaPuntualNudo {
    std::uint32_t id{0};
    std::uint32_t nudo_id{0};
    std::uint32_t hipotesis_id{1};
    core::Vec3d fuerzas_kn{0.0, 0.0, 0.0};    ///< Fx, Fy, Fz (kN)
    core::Vec3d momentos_knm{0.0, 0.0, 0.0};  ///< Mx, My, Mz (kN·m)

    // Fase 7 (P-08.1): Desplazamientos (m) y giros (rad) prescritos por hipótesis (`Carga_nudo_desp_y_giros_prescritos`)
    bool tiene_desplazamiento_prescrito{false};
    core::Vec3d desp_prescrito_m{0.0, 0.0, 0.0};
    core::Vec3d giro_prescrito_rad{0.0, 0.0, 0.0};
};

/**
 * @brief Nivel o planta horizontal de edificación equivalente a `ed3planta` / `pngrupla.dll`.
 */
struct NivelPlanta {
    std::uint32_t id{0};
    std::string nombre{"Planta 1"};
    double cota_z_m{0.0};
    double altura_m{3.0};
    std::uint32_t grupo_id{1};
    bool visible{true};
};

/**
 * @brief Rejilla de referencia espacial equivalente a `Planos_Rejilla` (`ed3mnugeo.c`).
 */
struct RejillaTrabajo {
    std::uint32_t id{1};
    std::string nombre{"Rejilla general"};
    core::Vec3d origen{0.0, 0.0, 0.0};
    std::vector<double> coordenadas_x{-10.0, -5.0, 0.0, 5.0, 10.0};
    std::vector<double> coordenadas_y{-10.0, -5.0, 0.0, 5.0, 10.0};
    bool visible{true};
};

struct EtiquetaCapa {
    std::uint32_t id{1};
    std::string nombre{"Por defecto"};
    bool visible{true};
    bool bloqueada{false};
    std::uint32_t color_rgb{0x0080FF};
};

struct ConfiguracionPilotesTerreno {
    // Pestañas Pilotes (Hormigón armado) y Micropilotes en Datos Generales (04_dialog_step4.png)
    double excentricidad_momento_minimo_m{0.10};
    bool considerar_esfuerzos_axil_reducido{false};
    double factor_pilotes_fuste{0.00};
    double factor_pilotes_punta{0.80};
    double coeficiente_pandeo{1.00};
    bool analisis_estabilidad_lateral{false};
};

struct ConfiguracionEstadosLimite {
    // Diálogo Estados límite (05_dialog_step5.png, 358x438)
    std::string situacion_hormigon{"E.L.U. de rotura. Código Estructural"};
    std::string situacion_cimentacion{"E.L.U. de rotura. CTE DB-SE-C"};
    std::string situacion_acero_laminado{"E.L.U. de rotura. CTE DB SE-A"};
    std::string situacion_acero_conformado{"E.L.U. de rotura. CTE DB SE-A"};
    std::string situacion_madera{"E.L.U. de rotura. CTE DB SE-M"};
    std::string situacion_aluminio{"E.L.U. de rotura. Eurocódigo 9"};
    std::string categoria_uso_nieve{"Altitud <= 1000 m"};
    bool comprobar_desplazamientos{true};
    bool comprobar_tensiones_terreno{true};
};

struct DatosGeneralesObra {
    std::string ruta_directorio{"D:\\CYPE Ingenieros\\Proyectos\\CYPE 3D\\"};
    std::string clave{"Obra1"};
    std::string descripcion{"Obra de CYPE 3D"};
    units::UnitSystem sistema_unidades{units::UnitSystem::SI};

    // Selección individual de normativas por familia (`cypenorma.dll`)
    std::string norma_hormigon{"Eurocódigo 2"};
    std::string norma_acero_laminado{"CTE DB SE-A"};
    std::string norma_acero_conformado{"Código Estructural"};
    std::string norma_madera{"CTE DB SE-M"};
    std::string norma_aluminio{"Eurocódigo 9"};
    std::string norma_cimentacion{"CTE DB SE-C"};

    // Normas y materiales por defecto (04_after_wizard_aceptar.png / 14_dialog_datos_generales.png)
    std::string normas_resumen{"Eurocódigo 2, CTE DB SE-A, Código Estructural, CTE DB SE-M y Eurocódigo 9"};
    std::string acero_laminado{"S275 (UNE-EN 10025-2)"};
    std::string acero_conformado{"S235"};
    std::string madera{"Aserrada, procedente de coníferas o chopo (C24)"};
    std::string aluminio{"EN AW-5083 - F"};
    std::string hormigon_estructural{"C25/30"};
    std::string calidad_pernos{"8.8"};

    // Bloque Hormigón armado (columna derecha)
    std::string hormigon_pilares{"C25/30"};
    std::string hormigon_vigas{"C25/30"};
    std::string hormigon_cimentacion{"C25/30"};
    std::string acero_barras{"S-400"};
    std::string caracteristicas_arido{"Cuarcita (15 mm), 30 mm"};
    std::string ambiente_vigas{"X0"};

    // Acciones (Sismo, Viento, Fuego) y Terreno de cimentación
    bool con_sismo_dinamico{false};
    std::string norma_sismo{"NCSE-02"};
    double aceleracion_sismica_basica_g{0.12};
    double factor_ductilidad_q{2.0};
    std::uint32_t modos_vibracion_sismo{6};
    bool comprobar_cortante_basal{true};

    bool resistencia_al_fuego_activa{false};
    std::string clase_resistencia_fuego{"R 60"};
    std::string revestimiento_fuego_acero{"Pintura intumescente"};

    bool proceso_constructivo_activo{false};
    bool verificar_deslizamiento_zapatas{true};
    double tension_terreno_persistente_mpa{0.200};
    double tension_terreno_accidental_mpa{0.300};

    ConfiguracionPilotesTerreno pilotes_hormigon{};
    ConfiguracionPilotesTerreno micropilotes{};
    ConfiguracionEstadosLimite estados_limite{};
};

struct EstructuraDatos {
    // Equivalente a ed3datos_t en ed3dlib.dll
    std::vector<Nudo3d> nudos;
    std::vector<Barra3d> barras;
    std::vector<Lamina3d> laminas;
    std::vector<Panyo3d> panyos;
    std::vector<NivelPlanta> niveles;
    std::vector<RejillaTrabajo> rejillas;
    std::vector<EtiquetaCapa> capas;
    std::vector<HipotesisCargaObra> hipotesis;
    std::vector<CargaSuperficialPanyo> cargas_superficiales;
    std::vector<CargaLinealBarra> cargas_lineales;
    std::vector<CargaPuntualNudo> cargas_nudos;

    // Fase 5 (P-06.1 a P-06.4): Ligaduras, diafragmas, piezas, agrupaciones, flecha, tirantes, rótulas, pilares y vigas
    std::vector<LigaduraDesplazamientoNudos> ligaduras;
    std::vector<DiafragmaRigidoPlanta> diafragmas;
    std::vector<PiezaEstructural> piezas;
    std::vector<AgrupacionBarras> agrupaciones;
    std::vector<GrupoFlecha> grupos_flecha;
    std::vector<CuadroArriostramientoTirantes> cuadros_tirantes;
    std::vector<DefinicionRotulaPlastica> catalogo_rotulas_plasticas;
    std::vector<PilarEstructural> pilares;
    std::vector<PorticoVigasEstructural> porticos_vigas;
    ProfileLibraryManager biblioteca_perfiles{};

    // Fase 6 y Fase 7 (P-07.1, P-07.2, P-08.1): Cargas en láminas y configuración de visualización de cargas
    std::vector<CargaLamina3d> cargas_laminas;
    ConfiguracionVisualizacionCargas config_vis_cargas{};

    std::uint32_t next_nudo_id{1};
    std::uint32_t next_barra_id{1};
    std::uint32_t next_lamina_id{1};
    std::uint32_t next_panyo_id{1};
    std::uint32_t next_nivel_id{1};
    std::uint32_t next_capa_id{2};
    std::uint32_t next_hipotesis_id{1};
    std::uint32_t next_carga_sup_id{1};
    std::uint32_t next_carga_lin_id{1};
    std::uint32_t next_carga_nudo_id{1};
    std::uint32_t next_ligadura_id{1};
    std::uint32_t next_diafragma_id{1};
    std::uint32_t next_pieza_id{1};
    std::uint32_t next_agrupacion_id{1};
    std::uint32_t next_grupo_flecha_id{1};
    std::uint32_t next_cuadro_tirantes_id{1};
    std::uint32_t next_rotula_id{1};
    std::uint32_t next_pilar_id{1};
    std::uint32_t next_portico_id{1};
    std::uint32_t next_carga_lamina_id{1};
};

enum class TipoElementoCimentacion : std::uint8_t {
    ZapataHormigonArmado = 0,
    ZapataHormigonMasa,
    Encepado,
    ElementoLineal,
    VigaAtado,
    VigaCentradora
};

struct LimiteCimentacion {
    std::uint32_t id{0};
    std::vector<core::Vec2d> vertices;
    bool cerrado{true};
};

struct ElementoCimentacion {
    std::uint32_t id{0};
    TipoElementoCimentacion tipo{TipoElementoCimentacion::ZapataHormigonArmado};
    std::uint32_t nudo_arranque_id{0};
    double ancho_x_m{1.20};
    double ancho_y_m{1.20};
    double canto_m{0.50};
    double angulo_giro_deg{0.0};
};

struct CimentacionDatos {
    // Equivalente a plcdatos_t en planocim.dll
    std::vector<ElementoCimentacion> elementos;
    std::vector<LimiteCimentacion> limites;
    std::uint32_t next_elemento_id{1};
    std::uint32_t next_limite_id{1};
};

/**
 * @brief Estructura maestra de proyecto `Obra` equivalente a la estructura de obra de e3dcomun.dll.
 *        Integra `ed3datos_t` (Estructura), `plcdatos_t` (Cimentación), `cypesave` y `undoredo`.
 */
class Obra {
public:
    Obra();

    bool abierta() const noexcept { return abierta_; }
    void set_abierta(bool estado) noexcept { abierta_ = estado; }

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
    std::uint32_t add_lamina(const std::vector<core::Vec3d>& vertices, double espesor_m = 0.20, std::string_view material = "C25/30", std::uint32_t capa_id = 1);
    std::uint32_t add_panyo(const std::vector<core::Vec3d>& vertices, const core::Vec3d& dir_reparto = {1.0, 0.0, 0.0}, std::uint32_t capa_id = 1);
    std::uint32_t add_nivel_planta(std::string_view nombre, double cota_z_m, double altura_m = 3.0, std::uint32_t grupo_id = 1);
    std::uint32_t add_etiqueta_capa(std::string_view nombre, std::uint32_t color_rgb = 0x0080FF, bool visible = true, bool bloqueada = false);
    std::uint32_t add_hipotesis_carga(
        std::string_view codigo,
        std::string_view descripcion,
        std::uint8_t naturaleza = 0,
        std::uint8_t disposicion = 0,
        std::uint8_t categoria_uso = 0,
        std::uint32_t grupo_incompatibilidad = 0,
        bool es_automatica = false);
    std::uint32_t add_carga_superficial_panyo(
        std::uint32_t panyo_id,
        std::uint32_t hipotesis_id,
        double valor_kn_m2,
        const core::Vec3d& direccion = {0.0, 0.0, -1.0},
        bool en_ejes_locales_panyo = false,
        std::string_view descripcion_zona = "");
    std::uint32_t add_carga_lineal_barra(
        std::uint32_t barra_id,
        std::uint32_t hipotesis_id,
        double q_ini_kn_m,
        double q_fin_kn_m,
        const core::Vec3d& direccion = {0.0, 0.0, -1.0},
        bool en_ejes_locales = false);
    std::uint32_t add_carga_puntual_nudo(
        std::uint32_t nudo_id,
        std::uint32_t hipotesis_id,
        const core::Vec3d& fuerzas_kn,
        const core::Vec3d& momentos_knm = {0.0, 0.0, 0.0});
    std::uint32_t add_elemento_cimentacion(TipoElementoCimentacion tipo, std::uint32_t nudo_arranque_id, double ancho_x = 1.20, double ancho_y = 1.20, double canto = 0.50);

    const Nudo3d* find_nudo(std::uint32_t id) const noexcept;
    Nudo3d* find_nudo_mut(std::uint32_t id) noexcept;
    const Barra3d* find_barra(std::uint32_t id) const noexcept;
    Barra3d* find_barra_mut(std::uint32_t id) noexcept;
    const EtiquetaCapa* find_capa(std::uint32_t id) const noexcept;
    EtiquetaCapa* find_capa_mut(std::uint32_t id) noexcept;
    core::BBox3d bounding_box() const noexcept;

    // Fase 5 (P-06.1): Operaciones avanzadas sobre Nudos
    bool delete_nudo(std::uint32_t nudo_id, bool eliminar_barras_incidentes = true);
    bool vincular_empatar_nudos(std::uint32_t nudo_destino_id, std::uint32_t nudo_absorbido_id);
    std::uint32_t desempatar_nudo(std::uint32_t nudo_id, std::uint32_t barra_id);
    bool set_vinculacion_exterior_nudo(
        std::uint32_t nudo_id,
        VinculacionExterior tipo,
        const DatosVinculacionExteriorNudo& datos);
    bool set_vinculacion_interior_nudo(
        std::uint32_t nudo_id,
        TipoVinculacionInterior tipo,
        double grado_empotramiento = 1.0);
    std::uint32_t add_ligadura_nudos(
        std::string_view nombre,
        const std::vector<std::uint32_t>& nudo_ids,
        bool ligar_dx = true,
        bool ligar_dy = true,
        bool ligar_dz = false);
    std::size_t generar_diafragmas_por_niveles(double tolerancia_z_m = 0.05);
    void reiniciar_numeracion_nudos_y_barras();

    // Fase 5 (P-06.2): Operaciones topológicas sobre Barras, Piezas y Agrupaciones
    bool delete_barra(std::uint32_t barra_id);
    bool move_extremo_barra(std::uint32_t barra_id, bool extremo_inicial, std::uint32_t nuevo_nudo_id);
    bool invertir_sentido_eje_x_barra(std::uint32_t barra_id);
    double longitud_barra_m(std::uint32_t barra_id) const noexcept;
    std::size_t generar_nudos_y_dividir_barras_en_cruces(double tolerancia_m = 1.0e-3);
    std::uint32_t crear_pieza(const std::vector<std::uint32_t>& barra_ids, std::string_view nombre = "");
    bool dividir_pieza(std::uint32_t pieza_id);
    std::uint32_t agrupar_barras(const std::vector<std::uint32_t>& barra_ids, std::string_view nombre = "");
    bool desagrupar_barras(std::uint32_t agrupacion_id);

    // Fase 5 (P-06.3): Grupos de flecha, Tirantes en X, Pandeo automático y Rótulas Plásticas
    std::uint32_t crear_grupo_flecha(
        const std::vector<std::uint32_t>& barra_ids,
        TipoMedicionFlecha tipo = TipoMedicionFlecha::SecanteEntreExtremos);
    CuadroArriostramientoTirantes emparejar_tirantes_recuadro(
        std::uint32_t barra_diag1_id,
        std::uint32_t barra_diag2_id,
        double tol_coplanar_m = 0.02);
    std::size_t calcular_pandeo_automatico_estructura(
        bool estructura_traslacional_xy = false,
        bool estructura_traslacional_xz = false);
    std::uint32_t add_definicion_rotula_plastica(const DefinicionRotulaPlastica& def);

    // Fase 5 (P-06.4): Pilares y Vigas de Hormigón Armado / Mixtos
    std::uint32_t add_pilar_estructural(const PilarEstructural& pilar, bool generar_barras_3d = true);
    std::uint32_t add_portico_vigas(const PorticoVigasEstructural& portico);

    // Fase 6 (P-07.1): Operaciones sobre Láminas (Elementos Finitos Shell)
    const Lamina3d* find_lamina(std::uint32_t id) const noexcept;
    Lamina3d* find_lamina_mut(std::uint32_t id) noexcept;
    bool delete_lamina(std::uint32_t lamina_id);
    bool move_lamina(std::uint32_t lamina_id, const core::Vec3d& delta);
    std::uint32_t add_hueco_lamina(std::uint32_t lamina_id, const std::vector<core::Vec3d>& vertices_hueco);
    std::uint32_t add_banda_integracion_lamina(std::uint32_t lamina_id, const BandaIntegracionLamina& banda);
    bool set_vinculacion_arista_lamina(std::uint32_t lamina_id, const VinculacionAristaLamina& vinc);
    std::uint32_t unir_laminas(std::uint32_t lamina_a_id, std::uint32_t lamina_b_id);
    std::pair<std::uint32_t, std::uint32_t> dividir_lamina(
        std::uint32_t lamina_id,
        std::size_t idx_vertice_a,
        std::size_t idx_vertice_b);
    std::size_t discretizar_todas_las_laminas(double tamano_defecto_m = 1.00);

    // Fase 6 (P-07.2): Operaciones sobre Paños / Superficies de Carga y Reparto a Barras
    const Panyo3d* find_panyo(std::uint32_t id) const noexcept;
    Panyo3d* find_panyo_mut(std::uint32_t id) noexcept;
    bool delete_panyo(std::uint32_t panyo_id);
    bool move_panyo(std::uint32_t panyo_id, const core::Vec3d& delta);
    bool set_direccion_reparto_panyo(std::uint32_t panyo_id, const core::Vec3d& direccion);
    std::pair<std::uint32_t, std::uint32_t> dividir_panyo(
        std::uint32_t panyo_id,
        std::size_t idx_vertice_a,
        std::size_t idx_vertice_b);
    std::size_t actualizar_reparto_cargas_panyos_a_barras();

    // Fase 7 (P-08.1): Gestión completa de Cargas en Nudos, Barras, Láminas y Paños
    bool editar_carga_nudo(std::uint32_t carga_id, const core::Vec3d& fuerzas_kn, const core::Vec3d& momentos_knm);
    bool delete_carga_nudo(std::uint32_t carga_id);
    bool cambiar_hipotesis_carga_nudo(std::uint32_t carga_id, std::uint32_t nueva_hipotesis_id);

    std::uint32_t add_carga_especial_barra(const CargaLinealBarra& carga);
    bool delete_carga_barra(std::uint32_t carga_id);
    bool cambiar_hipotesis_carga_barra(std::uint32_t carga_id, std::uint32_t nueva_hipotesis_id);

    std::uint32_t add_carga_lamina(const CargaLamina3d& carga);
    bool delete_carga_lamina(std::uint32_t carga_id);
    bool cambiar_hipotesis_carga_lamina(std::uint32_t carga_id, std::uint32_t nueva_hipotesis_id);

    bool delete_carga_superficial_panyo(std::uint32_t carga_id);
    bool cambiar_hipotesis_carga_superficial_panyo(std::uint32_t carga_id, std::uint32_t nueva_hipotesis_id);

    // Serialización binaria (.ed3 via cypesave) y JSON
    void write_binary(io::BinaryWriter& writer) const;
    bool read_binary(io::BinaryReader& reader);

    bool save_ed3(const std::filesystem::path& path);
    bool load_ed3(const std::filesystem::path& path);

    io::JsonValue to_json() const;
    bool from_json(const io::JsonValue& root);

private:
    bool abierta_{true};
    DatosGeneralesObra datos_generales_{};
    EstructuraDatos estructura_{};
    CimentacionDatos cimentacion_{};
    SolapaActiva solapa_activa_{SolapaActiva::Estructura};
    io::UndoRedoManager undo_redo_{64};
};

} // namespace cypeclon::model
