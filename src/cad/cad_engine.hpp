#pragma once

#include "core/math.hpp"
#include "model/obra.hpp"
#include "units/units.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::cad {

enum class EntityKind : std::uint8_t {
    Nudo = 0,
    Barra = 1,
    Lamina = 2,
    Panyo = 3,
    Cimentacion = 4
};

struct SpatialItem {
    EntityKind kind{EntityKind::Nudo};
    std::uint32_t entity_id{0};
    core::BBox3d bounds{};
};

/**
 * @brief Octree espacial 3D para selección y picking rápido equivalente a ebaoctree (ed3dlib.dll / entobasi.dll).
 */
class Octree3d {
public:
    explicit Octree3d(const core::BBox3d& world_bounds, int max_depth = 6, std::size_t max_items_per_leaf = 16);

    void clear();
    void set_world_bounds(const core::BBox3d& world_bounds);
    void insert(const SpatialItem& item);

    std::vector<SpatialItem> query_box(const core::BBox3d& region) const;
    std::vector<SpatialItem> query_ray(const core::Vec3d& ray_origin, const core::Vec3d& ray_dir) const;

private:
    struct Node {
        core::BBox3d bounds;
        std::vector<SpatialItem> items;
        std::unique_ptr<Node> children[8]{};
        bool is_leaf{true};
    };

    void insert_impl(Node& node, const SpatialItem& item, int depth);
    void subdivide(Node& node);
    void query_box_impl(const Node& node, const core::BBox3d& region, std::vector<SpatialItem>& out) const;
    void query_ray_impl(const Node& node, const core::Vec3d& ray_origin, const core::Vec3d& ray_dir, std::vector<SpatialItem>& out) const;

    core::BBox3d world_bounds_;
    int max_depth_{6};
    std::size_t max_items_per_leaf_{16};
    Node root_;
};

// ============================================================================
// P-04.2 (1): Cámara 3D/2D Interactiva y Ray Casting (`pndibposecam` / `vistacpl`)
// ============================================================================

struct Ray3d {
    core::Vec3d origin{0.0, 0.0, 0.0};
    core::Vec3d direction{0.0, 0.0, -1.0};
};

enum class ProjectionMode : std::uint8_t {
    Orthographic = 0,  ///< Proyección cilíndrica ortogonal 2D/3D (`pndibposecam_es_camara_ortografica`)
    Isometric = 1,     ///< Proyección axonométrica/isométrica estándar
    Perspective = 2    ///< Proyección cónica en perspectiva (`pndibposecam_crea_pose_en_perspectiva_a_partir_de_dada`)
};

/**
 * @brief Cámara 3D/2D interactiva equivalente a `pndibposecam` (`panelarea.dll`) y `vistacpl` (`paneldib.dll`).
 *        Soporta órbita esférica (acimut/elevación), encuadre (pan), zoom rueda/ventana/extensión completa,
 *        proyección ortográfica/isométrica/perspectiva y lanzamiento de rayos 3D (`screen_to_ray` / `unproject`).
 */
class Camera3d {
public:
    Camera3d();

    ProjectionMode projection_mode() const noexcept { return proj_mode_; }
    void set_projection_mode(ProjectionMode mode) noexcept { proj_mode_ = mode; }
    bool is_orthographic() const noexcept { return proj_mode_ != ProjectionMode::Perspective; }

    const core::Vec3d& target() const noexcept { return target_; }
    void set_target(const core::Vec3d& t) noexcept { target_ = t; }

    double azimuth_rad() const noexcept { return azimuth_rad_; }
    double elevation_rad() const noexcept { return elevation_rad_; }
    double distance() const noexcept { return distance_; }
    double ortho_half_height() const noexcept { return ortho_half_height_; }
    double fov_y_rad() const noexcept { return fov_y_rad_; }

    void set_spherical_angles(double azimuth_rad, double elevation_rad) noexcept;
    void set_distance(double dist) noexcept;
    void set_ortho_half_height(double half_h) noexcept;
    void set_fov_y_rad(double fov_rad) noexcept;
    void set_clip_planes(double near_z, double far_z) noexcept;

    // Vectores de orientación de cámara en WCS (`pndibposecam_matriz_orientacion_camara`)
    core::Vec3d position() const noexcept;
    core::Vec3d forward_dir() const noexcept;
    core::Vec3d right_dir() const noexcept;
    core::Vec3d up_dir() const noexcept;

    // Operaciones interactivas de navegación (`areazoom_*` / `vistacpl_*`)
    void orbit(double delta_azimuth_rad, double delta_elevation_rad) noexcept;
    void pan(double delta_right_world, double delta_up_world) noexcept;
    void pan_pixels(double dx_px, double dy_px, int viewport_w, int viewport_h) noexcept;
    void zoom_wheel(double factor) noexcept;
    void zoom_window(double x0_px, double y0_px, double x1_px, double y1_px, int viewport_w, int viewport_h) noexcept;
    void zoom_extents(const core::BBox3d& world_bbox, int viewport_w, int viewport_h) noexcept;

    // Configuración de vistas predefinidas y alineación a plano de trabajo
    void set_preset_view(std::string_view preset_name) noexcept;
    void align_to_work_plane(const core::LocalAxes3d& axes, const core::Vec3d& origin) noexcept;

    // Matrices de transformación 4x4 (`Mat4d`)
    core::Mat4d view_matrix() const noexcept;
    core::Mat4d projection_matrix(int viewport_w, int viewport_h) const noexcept;
    core::Mat4d view_projection_matrix(int viewport_w, int viewport_h) const noexcept;

    // Transformaciones WCS <-> Viewport y lanzamiento de rayos (`ebobtransfcap_*` / `vistacpl_*`)
    core::Vec3d world_to_screen(const core::Vec3d& world_pt, int viewport_w, int viewport_h) const noexcept;
    core::Vec3d unproject(double screen_x, double screen_y, double ndc_z, int viewport_w, int viewport_h) const noexcept;
    Ray3d screen_to_ray(double screen_x, double screen_y, int viewport_w, int viewport_h) const noexcept;
    std::optional<core::Vec3d> screen_to_plane(double screen_x, double screen_y, int viewport_w, int viewport_h, const core::Plane3d& plane) const noexcept;
    double world_units_per_pixel(int viewport_w, int viewport_h) const noexcept;

private:
    ProjectionMode proj_mode_{ProjectionMode::Isometric};
    core::Vec3d target_{0.0, 0.0, 0.0};
    double azimuth_rad_{-0.7853981633974483};   ///< -45 grados
    double elevation_rad_{0.6154797086703873};  ///< ~35.264 grados (isométrica verdadera)
    double distance_{35.0};
    double ortho_half_height_{10.0};
    double fov_y_rad_{0.7853981633974483};      ///< 45 grados
    double near_plane_{0.1};
    double far_plane_{1000.0};
    std::optional<core::LocalAxes3d> custom_plane_axes_{};
};

// ============================================================================
// P-04.1: Gestor de Ventanas 2D/3D MDI (`dispoMDI` + `ed3datvi_t`)
// ============================================================================

enum class MdiViewType : std::uint8_t {
    Vista3dGeneral = 0,          ///< Vista 3D de estructura completa (`Obra_Vista3D`)
    Vista3dParcial = 1,          ///< Vista 3D de ventana activa / selección (`Obra_Vista3DSeleccionPiezas`)
    Plano2dXY = 2,               ///< Planta horizontal XY a cota Z constante
    Plano2dXZ = 3,               ///< Alzado frontal XZ (pórtico en Y constante)
    Plano2dYZ = 4,               ///< Alzado lateral YZ (pórtico en X constante)
    Plano2dInclinado3Puntos = 5, ///< Plano inclinado definido por 3 puntos en el espacio
    VistaPortico = 6,            ///< Vista de pórtico vertical definido por 2 puntos en planta
    VistaPlantaNivel = 7         ///< Vista asociada a nivel/planta de edificación (`ed3planta` / `pngrupla`)
};

struct MdiWindowRect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

/**
 * @brief Subventana de dibujo MDI equivalente a `ed3datvi_t` / `pntvista` (`entornop.dll`).
 */
struct MdiSubWindow {
    std::uint32_t id{0};
    std::string title{"Vista 3D"};
    MdiViewType type{MdiViewType::Vista3dGeneral};
    core::Vec3d plane_origin{0.0, 0.0, 0.0};
    core::LocalAxes3d work_plane_axes{};
    core::Plane3d work_plane{{0.0, 0.0, 1.0}, 0.0};
    std::uint32_t nivel_planta_id{0};
    double cota_z_m{0.0};
    double depth_tolerance_m{0.15};
    Camera3d camera{};
    MdiWindowRect rect{390, 180, 1530, 814};
    bool visible{true};
    bool minimized{false};
    bool maximized{true};
    bool show_uniones_3d{false};
    std::vector<std::uint32_t> filter_bar_ids{};

    bool is_3d_view() const noexcept {
        return type == MdiViewType::Vista3dGeneral || type == MdiViewType::Vista3dParcial;
    }

    bool contains_point_in_plane(const core::Vec3d& pt) const noexcept;
};

/**
 * @brief Gestor de múltiples subventanas MDI equivalente a `dispoMDI` (`entornop.dll`).
 *        Gestiona creación, renombrado, cierre, navegación (`Anterior`, `Siguiente`) y algoritmos
 *        de disposición (`Cascada`, `Mosaico horizontal`, `Mosaico vertical`, `Ordenar ventanas`).
 */
class MdiViewManager {
public:
    MdiViewManager();

    void set_mdi_frame_rect(const MdiWindowRect& frame_rect) noexcept;
    const MdiWindowRect& mdi_frame_rect() const noexcept { return frame_rect_; }

    const std::vector<MdiSubWindow>& views() const noexcept { return views_; }
    std::vector<MdiSubWindow>& views_mut() noexcept { return views_; }

    std::uint32_t active_view_id() const noexcept { return active_view_id_; }
    const MdiSubWindow* active_view() const noexcept;
    MdiSubWindow* active_view_mut() noexcept;
    const MdiSubWindow* find_view(std::uint32_t view_id) const noexcept;
    MdiSubWindow* find_view_mut(std::uint32_t view_id) noexcept;

    // Creación de vistas (`entorno_crea_vista_MDI_para_entorno` / `pantmapa_abre_vista_nueva_si_se_puede`)
    std::uint32_t create_view_3d(std::string title = "Vista 3D", bool partial = false, std::vector<std::uint32_t> bar_ids = {});
    std::uint32_t create_view_orthogonal_2d(MdiViewType ortho_type, double coord_offset_m = 0.0, std::string title = "");
    std::uint32_t create_view_inclined_3pts(const core::Vec3d& p1, const core::Vec3d& p2, const core::Vec3d& p3, std::string title = "Plano 3 puntos");
    std::uint32_t create_view_portico(const core::Vec3d& p_start_xy, const core::Vec3d& p_end_xy, std::string title = "Pórtico");
    std::uint32_t create_view_planta_nivel(const model::NivelPlanta& nivel);

    // Gestión de ciclo de vida y navegación (`Ventana_Renombrar`, `Cerrar`, `Anterior`, `Siguiente`, `Seleccionar`)
    bool rename_view(std::uint32_t view_id, std::string new_title);
    bool close_view(std::uint32_t view_id);
    bool activate_view(std::uint32_t view_id) noexcept;
    bool activate_next() noexcept;
    bool activate_previous() noexcept;

    // Algoritmos de disposición MDI (`Ventana_Cascada`, `MosaicoHorizontal`, `MosaicoVertical`, `OrdenarVentanas`)
    void arrange_cascade() noexcept;
    void arrange_tile_horizontal() noexcept;
    void arrange_tile_vertical() noexcept;
    void arrange_order_grid() noexcept;

private:
    MdiWindowRect frame_rect_{390, 180, 1530, 814};
    std::vector<MdiSubWindow> views_;
    std::uint32_t active_view_id_{0};
    std::uint32_t next_view_id_{1};
};

// ============================================================================
// P-04.2 (2) y P-04.3 (1): Filtros de Categorías/Etiquetas y Picking Espacial
// ============================================================================

struct CadCategoryFilter {
    bool nudos_visible{true};
    bool nudos_bloqueada{false};
    bool barras_visible{true};
    bool barras_bloqueada{false};
    bool tirantes_visible{true};
    bool tirantes_bloqueada{false};
    bool pilares_visible{true};
    bool pilares_bloqueada{false};
    bool vigas_visible{true};
    bool vigas_bloqueada{false};
    bool panyos_visible{true};
    bool panyos_bloqueada{false};
    bool laminas_visible{true};
    bool laminas_bloqueada{false};
};

struct PickHit {
    EntityKind kind{EntityKind::Nudo};
    std::uint32_t entity_id{0};
    core::Vec3d hit_point_world{0.0, 0.0, 0.0};
    double screen_distance_px{0.0};
    double depth_t{0.0};
};

enum class SelectionBoxMode : std::uint8_t {
    AutoByDragDirection = 0,  ///< `x1 >= x0`: Ventana (interior completo); `x1 < x0`: Captura (cruce o interior)
    WindowInside = 1,         ///< Sólo entidades íntegramente contenidas en el rectángulo
    CrossingCapture = 2       ///< Entidades contenidas o que intersecan las aristas del rectángulo
};

/**
 * @brief Motor de picking 3D por rayo y caja de selección acelerado con `Octree3d` (`ebaobcap` / `ebaoctree`).
 */
class CadPicker {
public:
    CadPicker();

    void rebuild_index(const model::Obra& obra, const CadCategoryFilter& cat_filter = {});
    const Octree3d& octree() const noexcept { return octree_; }

    static bool is_layer_visible(const model::Obra& obra, std::uint32_t capa_id) noexcept;
    static bool is_layer_selectable(const model::Obra& obra, std::uint32_t capa_id) noexcept;
    static bool is_nudo_visible(const model::Obra& obra, const model::Nudo3d& n, const CadCategoryFilter& cat_filter) noexcept;
    static bool is_nudo_selectable(const model::Obra& obra, const model::Nudo3d& n, const CadCategoryFilter& cat_filter) noexcept;
    static bool is_barra_visible(const model::Obra& obra, const model::Barra3d& b, const CadCategoryFilter& cat_filter) noexcept;
    static bool is_barra_selectable(const model::Obra& obra, const model::Barra3d& b, const CadCategoryFilter& cat_filter) noexcept;
    static bool is_lamina_visible(const model::Obra& obra, const model::Lamina3d& l, const CadCategoryFilter& cat_filter) noexcept;
    static bool is_panyo_visible(const model::Obra& obra, const model::Panyo3d& p, const CadCategoryFilter& cat_filter) noexcept;

    std::optional<PickHit> pick_entity(
        const model::Obra& obra,
        double screen_x,
        double screen_y,
        const Camera3d& camera,
        int viewport_w,
        int viewport_h,
        double tolerance_px = 8.0,
        const CadCategoryFilter& cat_filter = {}
    ) const;

    std::vector<PickHit> select_box(
        const model::Obra& obra,
        double x0_px,
        double y0_px,
        double x1_px,
        double y1_px,
        const Camera3d& camera,
        int viewport_w,
        int viewport_h,
        SelectionBoxMode mode = SelectionBoxMode::AutoByDragDirection,
        const CadCategoryFilter& cat_filter = {}
    ) const;

private:
    Octree3d octree_;
};

// ============================================================================
// P-04.2 (3): Motor de Capturas a Objetos 3D y Plantillas DXF (`SnapEngine` / `dxflib`)
// ============================================================================

/**
 * @brief Configuración de barra de capturas CAD equivalente a `ebarcbca_crea_configuracion_barra_capturas`.
 */
struct SnapConfig {
    bool enabled{true};
    bool snap_extremo{true};
    bool snap_punto_medio{true};
    bool snap_perpendicular{true};
    bool snap_interseccion{true};
    bool snap_mas_cercano{false};
    bool snap_rastreo_polar{false};
    bool snap_rejilla{true};
    bool snap_plantilla_dxf{true};
    double grid_step_m{1.0};
    double polar_angle_step_deg{45.0};
    double snap_tolerance_px{12.0};
};

enum class SnapType : std::uint8_t {
    None = 0,
    Extremo = 1,
    PuntoMedio = 2,
    Perpendicular = 3,
    Interseccion = 4,
    MasCercano = 5,
    RastreoPolar = 6,
    Rejilla = 7,
    PlantillaDxf = 8
};

struct SnapResult {
    bool hit{false};
    SnapType type{SnapType::None};
    core::Vec3d world_pos{0.0, 0.0, 0.0};
    double screen_distance_px{0.0};
    std::string tooltip;
};

struct DxfLayer {
    std::string name{"0"};
    bool visible{true};
    std::uint32_t color_rgb{0x808080};
};

struct DxfPointEntity {
    std::string layer{"0"};
    core::Vec3d pos{0.0, 0.0, 0.0};
};

struct DxfLineEntity {
    std::string layer{"0"};
    core::Vec3d p0{0.0, 0.0, 0.0};
    core::Vec3d p1{0.0, 0.0, 0.0};
};

/**
 * @brief Plantilla DXF/DWG de referencia equivalente a `dxfdatos` / `cplbufdb` (`dxflib.dll`).
 */
class DxfTemplate {
public:
    DxfTemplate() = default;

    bool active() const noexcept { return active_; }
    void set_active(bool a) noexcept { active_ = a; }

    void set_transformation(const core::Vec3d& offset, double scale = 1.0, double rotation_z_rad = 0.0) noexcept;
    core::Vec3d transform_point(const core::Vec3d& local_pt) const noexcept;

    void add_layer(std::string name, bool visible = true, std::uint32_t color_rgb = 0x808080);
    bool is_layer_visible(std::string_view layer_name) const noexcept;

    void add_point(std::string layer, const core::Vec3d& pos);
    void add_line(std::string layer, const core::Vec3d& p0, const core::Vec3d& p1);
    void add_polyline(std::string layer, const std::vector<core::Vec3d>& vertices, bool closed = false);

    const std::vector<DxfLayer>& layers() const noexcept { return layers_; }
    const std::vector<DxfPointEntity>& points() const noexcept { return points_; }
    const std::vector<DxfLineEntity>& lines() const noexcept { return lines_; }

private:
    bool active_{true};
    core::Vec3d offset_{0.0, 0.0, 0.0};
    double scale_{1.0};
    double rotation_z_rad_{0.0};
    std::vector<DxfLayer> layers_{{"0", true, 0x808080}};
    std::vector<DxfPointEntity> points_;
    std::vector<DxfLineEntity> lines_;
};

/**
 * @brief Motor de capturas a objetos 3D, rastreo polar, rejilla y plantillas DXF (`ebaobcap` / `dxfcapt`).
 */
class SnapEngine {
public:
    SnapEngine() = default;

    SnapConfig& config() noexcept { return config_; }
    const SnapConfig& config() const noexcept { return config_; }

    SnapResult find_snap(
        const model::Obra& obra,
        double screen_x,
        double screen_y,
        const Camera3d& camera,
        int viewport_w,
        int viewport_h,
        const core::Plane3d& work_plane = {{0.0, 0.0, 1.0}, 0.0},
        const std::optional<core::Vec3d>& reference_pt = std::nullopt,
        const DxfTemplate* dxf_template = nullptr,
        const CadCategoryFilter& cat_filter = {}
    ) const;

private:
    SnapConfig config_{};
};

// ============================================================================
// P-04.2 (4): Sistema de Acotación 3D (`cyplanobj.dll` / `acotaseg` / `cypecota`)
// ============================================================================

struct LinearDimension3d {
    std::uint32_t id{0};
    core::Vec3d p_start{0.0, 0.0, 0.0};
    core::Vec3d p_end{0.0, 0.0, 0.0};
    core::Vec3d plane_normal{0.0, 0.0, 1.0};
    double offset_m{0.50};
    bool visible{true};
    std::uint32_t layer_id{1};

    // Geometría de líneas de referencia (witness lines), línea de cota y texto (`acotaseg_*`)
    core::Vec3d witness1_start{0.0, 0.0, 0.0};
    core::Vec3d witness1_end{0.0, 0.0, 0.0};
    core::Vec3d witness2_start{0.0, 0.0, 0.0};
    core::Vec3d witness2_end{0.0, 0.0, 0.0};
    core::Vec3d dim_line_start{0.0, 0.0, 0.0};
    core::Vec3d dim_line_end{0.0, 0.0, 0.0};
    core::Vec3d text_position{0.0, 0.0, 0.0};
    double measured_length_m{0.0};
    std::string formatted_text;
};

/**
 * @brief Gestor de cotas 3D en planos de trabajo equivalente a `acotaseg` / `cypecota` (`cyplanobj.dll`).
 *        Gobierna los comandos `Planos_Cotas_Anadir`, `Planos_Cotas_Borrar`, `Planos_Cotas_Mostrar`, `Planos_Cotas_Ocultar`.
 */
class DimensionSystem {
public:
    DimensionSystem() = default;

    std::uint32_t add_dimension(
        const core::Vec3d& p_start,
        const core::Vec3d& p_end,
        double offset_m = 0.50,
        const core::Vec3d& plane_normal = {0.0, 0.0, 1.0},
        units::UnitSystem unit_system = units::UnitSystem::SI
    );

    bool remove_dimension(std::uint32_t dim_id);
    void show_all() noexcept;
    void hide_all() noexcept;
    void toggle_global_visibility() noexcept { global_visible_ = !global_visible_; }
    bool global_visibility() const noexcept { return global_visible_; }
    void set_global_visibility(bool v) noexcept { global_visible_ = v; }

    const std::vector<LinearDimension3d>& dimensions() const noexcept { return dimensions_; }
    const LinearDimension3d* find_dimension(std::uint32_t dim_id) const noexcept;

private:
    std::vector<LinearDimension3d> dimensions_;
    std::uint32_t next_dim_id_{1};
    bool global_visible_{true};
};

// ============================================================================
// P-04.3: Perfiles 3D Extruidos, Línea Oculta (`cyhlr`) y Pipeline de Escena (`vistas3d` / `pnobvis3d`)
// ============================================================================

enum class ProfileShapeKind : std::uint8_t {
    DoubleT = 0,     ///< IPE, HEB, HEA, HEM, IPN, W
    ChannelUPN = 1,  ///< UPN, U, C
    AngleL = 2,      ///< L (angular de lados iguales o desiguales)
    HollowRect = 3,  ///< SHS, RHS, Tubo rectangular/cuadrado hueco
    HollowCirc = 4,  ///< CHS, Tubo circular hueco
    SolidRect = 5    ///< Sección rectangular maciza (hormigón / madera)
};

/**
 * @brief Sección transversal paramétrica 2D en ejes locales $(y, z)$ de la barra.
 */
struct CrossSectionProfile2d {
    std::string name{"IPE 200"};
    ProfileShapeKind kind{ProfileShapeKind::DoubleT};
    double height_z_m{0.200};
    double width_y_m{0.100};
    double web_thickness_m{0.0056};
    double flange_thickness_m{0.0085};
    std::vector<core::Vec2d> outer_contour;
    std::vector<core::Vec2d> inner_hole_contour;

    static CrossSectionProfile2d from_profile_name(std::string_view profile_spec);
};

struct Segment3d {
    core::Vec3d p0{0.0, 0.0, 0.0};
    core::Vec3d p1{0.0, 0.0, 0.0};
    std::uint32_t color_rgb{0x0080FF};
    bool dashed{false};
    std::uint32_t entity_id{0};
};

struct Triangle3d {
    core::Vec3d v0{0.0, 0.0, 0.0};
    core::Vec3d v1{0.0, 0.0, 0.0};
    core::Vec3d v2{0.0, 0.0, 0.0};
    core::Vec3d normal{0.0, 0.0, 1.0};
    std::uint32_t color_rgb{0x4A78B8};
    std::uint32_t entity_id{0};
};

/**
 * @brief Malla 3D extruida de barra equivalente a `ebaprisma3d_crea_prisma_extruido` (`pnobvis3d.dll`)
 *        y `elemvi3d_append_prisma_generico_con_tapas` (`vistas3d.dll`).
 */
struct BarExtrudedMesh3d {
    std::uint32_t barra_id{0};
    ProfileShapeKind shape_kind{ProfileShapeKind::DoubleT};
    std::vector<core::Vec3d> vertices;
    std::vector<core::Vec3d> normals;
    std::vector<std::array<std::uint32_t, 3>> triangle_indices;
    std::vector<Segment3d> contour_edges;  ///< `genele3d_lineas_contorno_figura_extruida_dado_eje`
};

/**
 * @brief Resultado del cálculo de líneas ocultas (`cyhlr.dll`: `cyhlrsalida_calcula_ocultacion`).
 */
struct HiddenLineResult {
    std::vector<Segment3d> visible_segments;
    std::vector<Segment3d> occluded_segments;
};

/**
 * @brief Motor de eliminación de líneas ocultas y planos de recorte (`cyhlr.dll`).
 */
class HiddenLineRemover {
public:
    HiddenLineRemover() = default;

    void clear_clipping_planes() noexcept { clipping_planes_.clear(); }
    void add_clipping_plane(const core::Plane3d& plane);
    const std::vector<core::Plane3d>& clipping_planes() const noexcept { return clipping_planes_; }

    std::optional<Segment3d> clip_segment(const Segment3d& seg) const noexcept;
    bool is_point_visible_by_clipping(const core::Vec3d& pt) const noexcept;

    HiddenLineResult compute_hlr(
        const std::vector<BarExtrudedMesh3d>& meshes,
        const Camera3d& camera,
        int viewport_w,
        int viewport_h
    ) const;

private:
    std::vector<core::Plane3d> clipping_planes_;
};

enum class RenderRepresentationMode : std::uint8_t {
    AlambricoUnifilar = 0,   ///< Ejes unifilares + símbolos de apoyo + triedros de ejes locales
    LineaOcultaHlr = 1,      ///< Contornos de perfiles con cálculo de ocultación `cyhlr`
    PerfilRealExtruido3d = 2 ///< Mallas sólidas 3D extruidas con tapas y normales (`vistas3d` / `pnobvis3d`)
};

struct TessellatedScene {
    RenderRepresentationMode mode{RenderRepresentationMode::AlambricoUnifilar};
    std::vector<Segment3d> wireframe_segments;
    std::vector<Segment3d> support_symbol_segments;
    std::vector<Segment3d> local_axes_segments;
    std::vector<Segment3d> dimension_segments;
    std::vector<BarExtrudedMesh3d> extruded_bar_meshes;
    std::vector<Triangle3d> surface_triangles;
    HiddenLineResult hlr_output{};
    std::size_t visible_nudo_count{0};
    std::size_t visible_barra_count{0};
    std::size_t visible_lamina_count{0};
    std::size_t visible_panyo_count{0};
};

/**
 * @brief Pipeline de generación de geometría 3D de escena (`pnobvis3d.dll` / `vistas3d.dll` / `visual3d.dll`).
 */
class SceneTessellator {
public:
    SceneTessellator() = default;

    HiddenLineRemover& hlr() noexcept { return hlr_; }
    const HiddenLineRemover& hlr() const noexcept { return hlr_; }

    static BarExtrudedMesh3d extrude_bar_mesh_3d(
        const model::Barra3d& barra,
        const core::Vec3d& p_ini,
        const core::Vec3d& p_fin,
        std::uint32_t color_rgb = 0x3B74C4
    );

    TessellatedScene build_scene(
        const model::Obra& obra,
        const Camera3d& camera,
        int viewport_w,
        int viewport_h,
        RenderRepresentationMode mode = RenderRepresentationMode::AlambricoUnifilar,
        const CadCategoryFilter& cat_filter = {},
        bool draw_local_axes = true,
        bool draw_supports = true,
        const DimensionSystem* dimensions = nullptr
    ) const;

private:
    HiddenLineRemover hlr_{};
};

} // namespace cypeclon::cad
