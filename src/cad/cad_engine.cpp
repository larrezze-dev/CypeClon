#include "cad/cad_engine.hpp"
#include "core/string_utils.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cypeclon::cad {

// ============================================================================
// Octree3d (`ebaoctree`)
// ============================================================================

Octree3d::Octree3d(const core::BBox3d& world_bounds, int max_depth, std::size_t max_items_per_leaf)
    : world_bounds_(world_bounds),
      max_depth_(max_depth),
      max_items_per_leaf_(max_items_per_leaf)
{
    root_.bounds = world_bounds_;
}

void Octree3d::clear() {
    root_ = Node{};
    root_.bounds = world_bounds_;
}

void Octree3d::set_world_bounds(const core::BBox3d& world_bounds) {
    world_bounds_ = world_bounds;
    clear();
}

void Octree3d::insert(const SpatialItem& item) {
    if (!root_.bounds.is_valid()) {
        root_.bounds = core::BBox3d{{-1000.0, -1000.0, -1000.0}, {1000.0, 1000.0, 1000.0}};
    }
    insert_impl(root_, item, 0);
}

void Octree3d::subdivide(Node& node) {
    const core::Vec3d mn = node.bounds.min_pt;
    const core::Vec3d mx = node.bounds.max_pt;
    const core::Vec3d c  = node.bounds.center();

    for (int i = 0; i < 8; ++i) {
        core::Vec3d cmin{
            (i & 1) ? c.x : mn.x,
            (i & 2) ? c.y : mn.y,
            (i & 4) ? c.z : mn.z
        };
        core::Vec3d cmax{
            (i & 1) ? mx.x : c.x,
            (i & 2) ? mx.y : c.y,
            (i & 4) ? mx.z : c.z
        };
        node.children[i] = std::make_unique<Node>();
        node.children[i]->bounds = core::BBox3d{cmin, cmax};
    }
    node.is_leaf = false;
}

void Octree3d::insert_impl(Node& node, const SpatialItem& item, int depth) {
    if (!node.bounds.intersects(item.bounds)) {
        return;
    }

    if (node.is_leaf) {
        if (depth >= max_depth_ || node.items.size() < max_items_per_leaf_) {
            node.items.push_back(item);
            return;
        }
        subdivide(node);
        std::vector<SpatialItem> existing = std::move(node.items);
        node.items.clear();
        for (const auto& ex : existing) {
            insert_impl(node, ex, depth);
        }
    }

    for (int i = 0; i < 8; ++i) {
        if (node.children[i] && node.children[i]->bounds.intersects(item.bounds)) {
            insert_impl(*node.children[i], item, depth + 1);
        }
    }
}

std::vector<SpatialItem> Octree3d::query_box(const core::BBox3d& region) const {
    std::vector<SpatialItem> out;
    query_box_impl(root_, region, out);
    return out;
}

void Octree3d::query_box_impl(const Node& node, const core::BBox3d& region, std::vector<SpatialItem>& out) const {
    if (!node.bounds.intersects(region)) return;
    for (const auto& it : node.items) {
        if (it.bounds.intersects(region)) {
            out.push_back(it);
        }
    }
    if (!node.is_leaf) {
        for (int i = 0; i < 8; ++i) {
            if (node.children[i]) {
                query_box_impl(*node.children[i], region, out);
            }
        }
    }
}

std::vector<SpatialItem> Octree3d::query_ray(const core::Vec3d& ray_origin, const core::Vec3d& ray_dir) const {
    std::vector<SpatialItem> out;
    query_ray_impl(root_, ray_origin, ray_dir, out);
    return out;
}

void Octree3d::query_ray_impl(const Node& node, const core::Vec3d& ray_origin, const core::Vec3d& ray_dir, std::vector<SpatialItem>& out) const {
    double tmin = 0.0;
    double tmax = 0.0;
    if (!node.bounds.intersect_ray(ray_origin, ray_dir, tmin, tmax)) {
        return;
    }
    for (const auto& it : node.items) {
        if (it.bounds.intersect_ray(ray_origin, ray_dir, tmin, tmax)) {
            out.push_back(it);
        }
    }
    if (!node.is_leaf) {
        for (int i = 0; i < 8; ++i) {
            if (node.children[i]) {
                query_ray_impl(*node.children[i], ray_origin, ray_dir, out);
            }
        }
    }
}

// ============================================================================
// P-04.2 (1): Camera3d (`pndibposecam` / `vistacpl`)
// ============================================================================

Camera3d::Camera3d() = default;

void Camera3d::set_spherical_angles(double azimuth_rad, double elevation_rad) noexcept {
    custom_plane_axes_.reset();
    azimuth_rad_ = azimuth_rad;
    constexpr double kMaxElev = std::numbers::pi * 0.5 - 1e-4;
    elevation_rad_ = std::clamp(elevation_rad, -kMaxElev, kMaxElev);
}

void Camera3d::set_distance(double dist) noexcept {
    distance_ = std::max(0.1, dist);
}

void Camera3d::set_ortho_half_height(double half_h) noexcept {
    ortho_half_height_ = std::max(0.05, half_h);
}

void Camera3d::set_fov_y_rad(double fov_rad) noexcept {
    fov_y_rad_ = std::clamp(fov_rad, 0.0872664626, 2.617993878); // [5 deg, 150 deg]
}

void Camera3d::set_clip_planes(double near_z, double far_z) noexcept {
    near_plane_ = std::max(1e-3, near_z);
    far_plane_ = std::max(near_plane_ + 1.0, far_z);
}

core::Vec3d Camera3d::forward_dir() const noexcept {
    if (custom_plane_axes_.has_value()) {
        return (-custom_plane_axes_->w).normalized();
    }
    const double ce = std::cos(elevation_rad_);
    const double se = std::sin(elevation_rad_);
    const double ca = std::cos(azimuth_rad_);
    const double sa = std::sin(azimuth_rad_);
    // Dirección desde target hacia el ojo es (ce*ca, ce*sa, se); forward es del ojo al target:
    return core::Vec3d{-ce * ca, -ce * sa, -se}.normalized();
}

core::Vec3d Camera3d::right_dir() const noexcept {
    if (custom_plane_axes_.has_value()) {
        return custom_plane_axes_->u.normalized();
    }
    // En coordenadas esféricas alrededor de Z global:
    const double sa = std::sin(azimuth_rad_);
    const double ca = std::cos(azimuth_rad_);
    return core::Vec3d{-sa, ca, 0.0}.normalized();
}

core::Vec3d Camera3d::up_dir() const noexcept {
    if (custom_plane_axes_.has_value()) {
        return custom_plane_axes_->v.normalized();
    }
    return right_dir().cross(forward_dir()).normalized();
}

core::Vec3d Camera3d::position() const noexcept {
    return target_ - forward_dir() * distance_;
}

void Camera3d::orbit(double delta_azimuth_rad, double delta_elevation_rad) noexcept {
    set_spherical_angles(azimuth_rad_ + delta_azimuth_rad, elevation_rad_ + delta_elevation_rad);
}

void Camera3d::pan(double delta_right_world, double delta_up_world) noexcept {
    target_ = target_ + right_dir() * delta_right_world + up_dir() * delta_up_world;
}

void Camera3d::pan_pixels(double dx_px, double dy_px, int viewport_w, int viewport_h) noexcept {
    const double scale = world_units_per_pixel(viewport_w, viewport_h);
    // Arrastrar ratón hacia la derecha mueve la cámara a la izquierda en escena (-dx_px), arriba en pantalla (+dy_px)
    pan(-dx_px * scale, dy_px * scale);
}

void Camera3d::zoom_wheel(double factor) noexcept {
    const double f = std::clamp(factor, 0.05, 20.0);
    ortho_half_height_ = std::max(0.05, ortho_half_height_ / f);
    distance_ = std::max(0.1, distance_ / f);
}

void Camera3d::zoom_window(double x0_px, double y0_px, double x1_px, double y1_px, int viewport_w, int viewport_h) noexcept {
    if (viewport_w <= 0 || viewport_h <= 0) return;
    const double cx_px = 0.5 * (x0_px + x1_px);
    const double cy_px = 0.5 * (y0_px + y1_px);
    const core::Vec3d new_center = unproject(cx_px, cy_px, 0.0, viewport_w, viewport_h);
    // Proyectar desplazamiento sobre el plano perpendicular a la vista pasando por target_
    const core::Vec3d delta = new_center - position();
    const core::Vec3d fwd = forward_dir();
    target_ = position() + delta - fwd * (delta.dot(fwd) - distance_);

    const double box_w_px = std::max(8.0, std::abs(x1_px - x0_px));
    const double box_h_px = std::max(8.0, std::abs(y1_px - y0_px));
    const double ratio_w = box_w_px / static_cast<double>(viewport_w);
    const double ratio_h = box_h_px / static_cast<double>(viewport_h);
    const double ratio = std::max(ratio_w, ratio_h);
    ortho_half_height_ = std::max(0.05, ortho_half_height_ * ratio);
    distance_ = std::max(0.1, distance_ * ratio);
}

void Camera3d::zoom_extents(const core::BBox3d& world_bbox, int viewport_w, int viewport_h) noexcept {
    if (!world_bbox.is_valid()) return;
    target_ = world_bbox.center();
    const core::Vec3d ext = world_bbox.size();
    const double radius = std::max(0.5, 0.5 * ext.norm());
    const double aspect = (viewport_h > 0 && viewport_w > 0)
                              ? static_cast<double>(viewport_w) / static_cast<double>(viewport_h)
                              : 1.5;

    // Proyectar las 8 esquinas del BBox sobre los ejes right y up de la cámara
    const core::Vec3d r = right_dir();
    const core::Vec3d u = up_dir();
    double max_r = 0.5;
    double max_u = 0.5;
    for (int i = 0; i < 8; ++i) {
        const core::Vec3d corner{
            (i & 1) ? world_bbox.max_pt.x : world_bbox.min_pt.x,
            (i & 2) ? world_bbox.max_pt.y : world_bbox.min_pt.y,
            (i & 4) ? world_bbox.max_pt.z : world_bbox.min_pt.z
        };
        const core::Vec3d rel = corner - target_;
        max_r = std::max(max_r, std::abs(rel.dot(r)));
        max_u = std::max(max_u, std::abs(rel.dot(u)));
    }
    const double margin = 1.15;
    ortho_half_height_ = std::max(max_u * margin, (max_r * margin) / std::max(0.1, aspect));
    const double half_fov = std::max(0.1, 0.5 * fov_y_rad_);
    distance_ = std::max(2.0, (ortho_half_height_ / std::tan(half_fov)) + radius);
}

void Camera3d::set_preset_view(std::string_view preset_name) noexcept {
    custom_plane_axes_.reset();
    if (preset_name == "XY" || preset_name == "Planta") {
        // Mirando desde +Z hacia -Z, con X a la derecha e Y hacia arriba
        core::LocalAxes3d axes{};
        axes.u = {1.0, 0.0, 0.0};
        axes.v = {0.0, 1.0, 0.0};
        axes.w = {0.0, 0.0, 1.0};
        custom_plane_axes_ = axes;
        proj_mode_ = ProjectionMode::Orthographic;
    } else if (preset_name == "XZ" || preset_name == "AlzadoFrontal") {
        // Mirando desde -Y hacia +Y, con X a la derecha y Z hacia arriba
        set_spherical_angles(-std::numbers::pi * 0.5, 0.0);
        proj_mode_ = ProjectionMode::Orthographic;
    } else if (preset_name == "YZ" || preset_name == "AlzadoLateral") {
        // Mirando desde +X hacia -X, con Y a la derecha y Z hacia arriba
        set_spherical_angles(0.0, 0.0);
        proj_mode_ = ProjectionMode::Orthographic;
    } else {
        // Vista 3D Isométrica por defecto
        set_spherical_angles(-std::numbers::pi * 0.25, 0.6154797086703873);
        proj_mode_ = ProjectionMode::Isometric;
    }
}

void Camera3d::align_to_work_plane(const core::LocalAxes3d& axes, const core::Vec3d& origin) noexcept {
    custom_plane_axes_ = axes;
    target_ = origin;
    proj_mode_ = ProjectionMode::Orthographic;
}

core::Mat4d Camera3d::view_matrix() const noexcept {
    return core::Mat4d::look_at(position(), target_, up_dir());
}

core::Mat4d Camera3d::projection_matrix(int viewport_w, int viewport_h) const noexcept {
    const double aspect = (viewport_w > 0 && viewport_h > 0)
                              ? static_cast<double>(viewport_w) / static_cast<double>(viewport_h)
                              : 1.5;
    if (proj_mode_ == ProjectionMode::Perspective) {
        return core::Mat4d::perspective(fov_y_rad_, aspect, near_plane_, far_plane_);
    }
    const double half_h = ortho_half_height_;
    const double half_w = half_h * aspect;
    return core::Mat4d::ortho(-half_w, half_w, -half_h, half_h, -far_plane_, far_plane_);
}

core::Mat4d Camera3d::view_projection_matrix(int viewport_w, int viewport_h) const noexcept {
    return projection_matrix(viewport_w, viewport_h) * view_matrix();
}

core::Vec3d Camera3d::world_to_screen(const core::Vec3d& world_pt, int viewport_w, int viewport_h) const noexcept {
    const core::Mat4d vp = view_projection_matrix(viewport_w, viewport_h);
    const core::Vec3d ndc = vp.transform_point(world_pt);
    const double sx = (ndc.x * 0.5 + 0.5) * static_cast<double>(viewport_w);
    const double sy = (1.0 - (ndc.y * 0.5 + 0.5)) * static_cast<double>(viewport_h);
    return {sx, sy, ndc.z};
}

core::Vec3d Camera3d::unproject(double screen_x, double screen_y, double ndc_z, int viewport_w, int viewport_h) const noexcept {
    if (viewport_w <= 0 || viewport_h <= 0) return target_;
    const double ndc_x = (2.0 * screen_x) / static_cast<double>(viewport_w) - 1.0;
    const double ndc_y = 1.0 - (2.0 * screen_y) / static_cast<double>(viewport_h);
    const core::Mat4d inv_vp = view_projection_matrix(viewport_w, viewport_h).inverted().value_or(core::Mat4d::identity());
    return inv_vp.transform_point({ndc_x, ndc_y, ndc_z});
}

Ray3d Camera3d::screen_to_ray(double screen_x, double screen_y, int viewport_w, int viewport_h) const noexcept {
    if (is_orthographic()) {
        const core::Vec3d p0 = unproject(screen_x, screen_y, 0.0, viewport_w, viewport_h);
        const core::Vec3d fwd = forward_dir();
        // Situar el origen del rayo suficientemente atrás a lo largo del eje de visión
        return Ray3d{p0 - fwd * (distance_ + 100.0), fwd};
    }
    const core::Vec3d eye = position();
    const core::Vec3d far_pt = unproject(screen_x, screen_y, 0.5, viewport_w, viewport_h);
    return Ray3d{eye, (far_pt - eye).normalized()};
}

std::optional<core::Vec3d> Camera3d::screen_to_plane(
    double screen_x,
    double screen_y,
    int viewport_w,
    int viewport_h,
    const core::Plane3d& plane
) const noexcept {
    const Ray3d ray = screen_to_ray(screen_x, screen_y, viewport_w, viewport_h);
    const double denom = plane.normal.dot(ray.direction);
    if (std::abs(denom) < 1e-9) {
        return std::nullopt;
    }
    const double t = -(plane.normal.dot(ray.origin) + plane.d) / denom;
    return ray.origin + ray.direction * t;
}

double Camera3d::world_units_per_pixel(int viewport_w, int viewport_h) const noexcept {
    (void)viewport_w;
    if (viewport_h <= 0) return 0.01;
    if (is_orthographic()) {
        return (2.0 * ortho_half_height_) / static_cast<double>(viewport_h);
    }
    const double visible_h = 2.0 * distance_ * std::tan(0.5 * fov_y_rad_);
    return visible_h / static_cast<double>(viewport_h);
}

// ============================================================================
// P-04.1: MdiSubWindow y MdiViewManager (`dispoMDI` + `ed3datvi_t`)
// ============================================================================

bool MdiSubWindow::contains_point_in_plane(const core::Vec3d& pt) const noexcept {
    if (is_3d_view()) {
        return true;
    }
    return std::abs(work_plane.signed_distance(pt)) <= depth_tolerance_m;
}

MdiViewManager::MdiViewManager() {
    create_view_3d("Vista 3D", false, {});
}

void MdiViewManager::set_mdi_frame_rect(const MdiWindowRect& frame_rect) noexcept {
    frame_rect_ = frame_rect;
    for (auto& v : views_) {
        if (v.maximized) {
            v.rect = frame_rect_;
        }
    }
}

const MdiSubWindow* MdiViewManager::active_view() const noexcept {
    return find_view(active_view_id_);
}

MdiSubWindow* MdiViewManager::active_view_mut() noexcept {
    return find_view_mut(active_view_id_);
}

const MdiSubWindow* MdiViewManager::find_view(std::uint32_t view_id) const noexcept {
    for (const auto& v : views_) {
        if (v.id == view_id) return &v;
    }
    return nullptr;
}

MdiSubWindow* MdiViewManager::find_view_mut(std::uint32_t view_id) noexcept {
    for (auto& v : views_) {
        if (v.id == view_id) return &v;
    }
    return nullptr;
}

std::uint32_t MdiViewManager::create_view_3d(std::string title, bool partial, std::vector<std::uint32_t> bar_ids) {
    MdiSubWindow win{};
    win.id = next_view_id_++;
    win.title = title.empty() ? ("Vista 3D (" + std::to_string(win.id) + ")") : std::move(title);
    win.type = partial ? MdiViewType::Vista3dParcial : MdiViewType::Vista3dGeneral;
    win.rect = frame_rect_;
    win.camera.set_preset_view("Isometrica");
    win.filter_bar_ids = std::move(bar_ids);
    active_view_id_ = win.id;
    views_.push_back(std::move(win));
    return active_view_id_;
}

std::uint32_t MdiViewManager::create_view_orthogonal_2d(MdiViewType ortho_type, double coord_offset_m, std::string title) {
    MdiSubWindow win{};
    win.id = next_view_id_++;
    win.type = ortho_type;
    win.rect = frame_rect_;

    if (ortho_type == MdiViewType::Plano2dXY) {
        win.cota_z_m = coord_offset_m;
        win.plane_origin = {0.0, 0.0, coord_offset_m};
        win.work_plane_axes = {win.plane_origin, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
        win.work_plane = core::Plane3d::from_point_and_normal(win.plane_origin, {0.0, 0.0, 1.0});
        win.title = title.empty() ? ("Planta XY Z=" + std::to_string(coord_offset_m)) : std::move(title);
    } else if (ortho_type == MdiViewType::Plano2dXZ) {
        win.plane_origin = {0.0, coord_offset_m, 0.0};
        win.work_plane_axes = {win.plane_origin, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, -1.0, 0.0}};
        win.work_plane = core::Plane3d::from_point_and_normal(win.plane_origin, {0.0, -1.0, 0.0});
        win.title = title.empty() ? ("Alzado XZ Y=" + std::to_string(coord_offset_m)) : std::move(title);
    } else {
        win.type = MdiViewType::Plano2dYZ;
        win.plane_origin = {coord_offset_m, 0.0, 0.0};
        win.work_plane_axes = {win.plane_origin, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}};
        win.work_plane = core::Plane3d::from_point_and_normal(win.plane_origin, {1.0, 0.0, 0.0});
        win.title = title.empty() ? ("Alzado YZ X=" + std::to_string(coord_offset_m)) : std::move(title);
    }
    win.camera.align_to_work_plane(win.work_plane_axes, win.plane_origin);
    active_view_id_ = win.id;
    views_.push_back(std::move(win));
    return active_view_id_;
}

std::uint32_t MdiViewManager::create_view_inclined_3pts(
    const core::Vec3d& p1,
    const core::Vec3d& p2,
    const core::Vec3d& p3,
    std::string title
) {
    MdiSubWindow win{};
    win.id = next_view_id_++;
    win.title = title.empty() ? ("Plano 3P (" + std::to_string(win.id) + ")") : std::move(title);
    win.type = MdiViewType::Plano2dInclinado3Puntos;
    win.plane_origin = p1;
    win.work_plane_axes = core::LocalAxes3d::from_three_points(p1, p2, p3).value_or(core::LocalAxes3d{});
    win.work_plane = core::Plane3d::from_three_points(p1, p2, p3).value_or(core::Plane3d{});
    win.rect = frame_rect_;
    win.camera.align_to_work_plane(win.work_plane_axes, p1);
    active_view_id_ = win.id;
    views_.push_back(std::move(win));
    return active_view_id_;
}

std::uint32_t MdiViewManager::create_view_portico(
    const core::Vec3d& p_start_xy,
    const core::Vec3d& p_end_xy,
    std::string title
) {
    const core::Vec3d p1{p_start_xy.x, p_start_xy.y, 0.0};
    const core::Vec3d p2{p_end_xy.x, p_end_xy.y, 0.0};
    const core::Vec3d p3{p_start_xy.x, p_start_xy.y, 10.0};
    MdiSubWindow win{};
    win.id = next_view_id_++;
    win.title = title.empty() ? ("Pórtico " + std::to_string(win.id)) : std::move(title);
    win.type = MdiViewType::VistaPortico;
    win.plane_origin = 0.5 * (p1 + p2);
    win.work_plane_axes = core::LocalAxes3d::from_three_points(p1, p2, p3).value_or(core::LocalAxes3d{});
    win.work_plane = core::Plane3d::from_three_points(p1, p2, p3).value_or(core::Plane3d{});
    win.rect = frame_rect_;
    win.camera.align_to_work_plane(win.work_plane_axes, win.plane_origin);
    active_view_id_ = win.id;
    views_.push_back(std::move(win));
    return active_view_id_;
}

std::uint32_t MdiViewManager::create_view_planta_nivel(const model::NivelPlanta& nivel) {
    MdiSubWindow win{};
    win.id = next_view_id_++;
    win.title = "Planta: " + nivel.nombre;
    win.type = MdiViewType::VistaPlantaNivel;
    win.nivel_planta_id = nivel.id;
    win.cota_z_m = nivel.cota_z_m;
    win.plane_origin = {0.0, 0.0, nivel.cota_z_m};
    win.work_plane_axes = {win.plane_origin, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    win.work_plane = core::Plane3d::from_point_and_normal(win.plane_origin, {0.0, 0.0, 1.0});
    win.rect = frame_rect_;
    win.camera.align_to_work_plane(win.work_plane_axes, win.plane_origin);
    active_view_id_ = win.id;
    views_.push_back(std::move(win));
    return active_view_id_;
}

bool MdiViewManager::rename_view(std::uint32_t view_id, std::string new_title) {
    if (new_title.empty()) return false;
    auto* v = find_view_mut(view_id);
    if (!v) return false;
    v->title = std::move(new_title);
    return true;
}

bool MdiViewManager::close_view(std::uint32_t view_id) {
    if (views_.size() <= 1) {
        // CYPE 3D mantiene al menos una subventana MDI abierta en el área de trabajo
        return false;
    }
    auto it = std::find_if(views_.begin(), views_.end(),
                           [view_id](const MdiSubWindow& w) { return w.id == view_id; });
    if (it == views_.end()) return false;
    views_.erase(it);
    if (active_view_id_ == view_id) {
        active_view_id_ = views_.back().id;
    }
    return true;
}

bool MdiViewManager::activate_view(std::uint32_t view_id) noexcept {
    for (const auto& v : views_) {
        if (v.id == view_id) {
            active_view_id_ = view_id;
            return true;
        }
    }
    return false;
}

bool MdiViewManager::activate_next() noexcept {
    if (views_.empty()) return false;
    for (std::size_t i = 0; i < views_.size(); ++i) {
        if (views_[i].id == active_view_id_) {
            const std::size_t next_idx = (i + 1) % views_.size();
            active_view_id_ = views_[next_idx].id;
            return true;
        }
    }
    active_view_id_ = views_.front().id;
    return true;
}

bool MdiViewManager::activate_previous() noexcept {
    if (views_.empty()) return false;
    for (std::size_t i = 0; i < views_.size(); ++i) {
        if (views_[i].id == active_view_id_) {
            const std::size_t prev_idx = (i == 0) ? (views_.size() - 1) : (i - 1);
            active_view_id_ = views_[prev_idx].id;
            return true;
        }
    }
    active_view_id_ = views_.front().id;
    return true;
}

void MdiViewManager::arrange_cascade() noexcept {
    const int n = static_cast<int>(views_.size());
    if (n == 0) return;
    constexpr int step_x = 28;
    constexpr int step_y = 28;
    const int base_w = std::max(320, (frame_rect_.width * 3) / 4);
    const int base_h = std::max(240, (frame_rect_.height * 3) / 4);

    for (int i = 0; i < n; ++i) {
        const int offset_idx = i % 8;
        views_[i].maximized = false;
        views_[i].minimized = false;
        views_[i].rect = MdiWindowRect{
            frame_rect_.x + offset_idx * step_x,
            frame_rect_.y + offset_idx * step_y,
            std::min(base_w, frame_rect_.width - offset_idx * step_x),
            std::min(base_h, frame_rect_.height - offset_idx * step_y)
        };
    }
}

void MdiViewManager::arrange_tile_horizontal() noexcept {
    const int n = static_cast<int>(views_.size());
    if (n == 0) return;
    const int band_h = frame_rect_.height / n;
    for (int i = 0; i < n; ++i) {
        views_[i].maximized = false;
        views_[i].minimized = false;
        const int y = frame_rect_.y + i * band_h;
        const int h = (i == n - 1) ? (frame_rect_.y + frame_rect_.height - y) : band_h;
        views_[i].rect = MdiWindowRect{frame_rect_.x, y, frame_rect_.width, h};
    }
}

void MdiViewManager::arrange_tile_vertical() noexcept {
    const int n = static_cast<int>(views_.size());
    if (n == 0) return;
    const int col_w = frame_rect_.width / n;
    for (int i = 0; i < n; ++i) {
        views_[i].maximized = false;
        views_[i].minimized = false;
        const int x = frame_rect_.x + i * col_w;
        const int w = (i == n - 1) ? (frame_rect_.x + frame_rect_.width - x) : col_w;
        views_[i].rect = MdiWindowRect{x, frame_rect_.y, w, frame_rect_.height};
    }
}

void MdiViewManager::arrange_order_grid() noexcept {
    const int n = static_cast<int>(views_.size());
    if (n == 0) return;
    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n)))));
    const int rows = std::max(1, (n + cols - 1) / cols);
    const int cell_w = frame_rect_.width / cols;
    const int cell_h = frame_rect_.height / rows;

    for (int i = 0; i < n; ++i) {
        const int r = i / cols;
        const int c = i % cols;
        views_[i].maximized = false;
        views_[i].minimized = false;
        views_[i].rect = MdiWindowRect{
            frame_rect_.x + c * cell_w,
            frame_rect_.y + r * cell_h,
            cell_w,
            cell_h
        };
    }
}

// ============================================================================
// P-04.2 (2) y P-04.3 (1): CadPicker y Filtros de Capas/Categorías
// ============================================================================

CadPicker::CadPicker()
    : octree_(core::BBox3d{{-500.0, -500.0, -500.0}, {500.0, 500.0, 500.0}})
{}

bool CadPicker::is_layer_visible(const model::Obra& obra, std::uint32_t capa_id) noexcept {
    const auto* capa = obra.find_capa(capa_id);
    return capa ? capa->visible : true;
}

bool CadPicker::is_layer_selectable(const model::Obra& obra, std::uint32_t capa_id) noexcept {
    const auto* capa = obra.find_capa(capa_id);
    if (!capa) return true;
    return capa->visible && !capa->bloqueada;
}

bool CadPicker::is_nudo_visible(const model::Obra& obra, const model::Nudo3d& n, const CadCategoryFilter& cat_filter) noexcept {
    return cat_filter.nudos_visible && is_layer_visible(obra, n.capa_id);
}

bool CadPicker::is_nudo_selectable(const model::Obra& obra, const model::Nudo3d& n, const CadCategoryFilter& cat_filter) noexcept {
    return cat_filter.nudos_visible && !cat_filter.nudos_bloqueada && is_layer_selectable(obra, n.capa_id);
}

bool CadPicker::is_barra_visible(const model::Obra& obra, const model::Barra3d& b, const CadCategoryFilter& cat_filter) noexcept {
    if (!is_layer_visible(obra, b.capa_id)) return false;
    if (b.es_tirante || b.tipo_estructural == model::TipoBarraEstructural::Tirante) {
        return cat_filter.tirantes_visible;
    }
    if (b.tipo_estructural == model::TipoBarraEstructural::ColumnaPilar) {
        return cat_filter.pilares_visible;
    }
    if (b.tipo_estructural == model::TipoBarraEstructural::Viga) {
        return cat_filter.vigas_visible;
    }
    return cat_filter.barras_visible;
}

bool CadPicker::is_barra_selectable(const model::Obra& obra, const model::Barra3d& b, const CadCategoryFilter& cat_filter) noexcept {
    if (!is_layer_selectable(obra, b.capa_id)) return false;
    if (b.es_tirante || b.tipo_estructural == model::TipoBarraEstructural::Tirante) {
        return cat_filter.tirantes_visible && !cat_filter.tirantes_bloqueada;
    }
    if (b.tipo_estructural == model::TipoBarraEstructural::ColumnaPilar) {
        return cat_filter.pilares_visible && !cat_filter.pilares_bloqueada;
    }
    if (b.tipo_estructural == model::TipoBarraEstructural::Viga) {
        return cat_filter.vigas_visible && !cat_filter.vigas_bloqueada;
    }
    return cat_filter.barras_visible && !cat_filter.barras_bloqueada;
}

bool CadPicker::is_lamina_visible(const model::Obra& obra, const model::Lamina3d& l, const CadCategoryFilter& cat_filter) noexcept {
    return cat_filter.laminas_visible && is_layer_visible(obra, l.capa_id);
}

bool CadPicker::is_panyo_visible(const model::Obra& obra, const model::Panyo3d& p, const CadCategoryFilter& cat_filter) noexcept {
    return cat_filter.panyos_visible && is_layer_visible(obra, p.capa_id);
}

void CadPicker::rebuild_index(const model::Obra& obra, const CadCategoryFilter& cat_filter) {
    core::BBox3d bounds = obra.bounding_box();
    if (!bounds.is_valid()) {
        bounds = core::BBox3d{{-100.0, -100.0, -100.0}, {100.0, 100.0, 100.0}};
    } else {
        bounds.expand(bounds.min_pt - core::Vec3d{10.0, 10.0, 10.0});
        bounds.expand(bounds.max_pt + core::Vec3d{10.0, 10.0, 10.0});
    }
    octree_.set_world_bounds(bounds);

    constexpr double kNodePad = 0.25;
    for (const auto& n : obra.estructura().nudos) {
        if (!is_nudo_selectable(obra, n, cat_filter)) continue;
        core::BBox3d nb{
            n.pos - core::Vec3d{kNodePad, kNodePad, kNodePad},
            n.pos + core::Vec3d{kNodePad, kNodePad, kNodePad}
        };
        octree_.insert(SpatialItem{EntityKind::Nudo, n.id, nb});
    }

    constexpr double kBarPad = 0.20;
    for (const auto& b : obra.estructura().barras) {
        if (!is_barra_selectable(obra, b, cat_filter)) continue;
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (!ni || !nf) continue;
        core::BBox3d bb{};
        bb.expand(ni->pos);
        bb.expand(nf->pos);
        bb.min_pt = bb.min_pt - core::Vec3d{kBarPad, kBarPad, kBarPad};
        bb.max_pt = bb.max_pt + core::Vec3d{kBarPad, kBarPad, kBarPad};
        octree_.insert(SpatialItem{EntityKind::Barra, b.id, bb});
    }
}

namespace {
double distance_point_to_segment_2d(
    const core::Vec2d& pt,
    const core::Vec2d& a,
    const core::Vec2d& b,
    double& out_t,
    core::Vec2d& out_closest
) noexcept {
    const core::Vec2d ab = b - a;
    const double len_sq = ab.norm_sq();
    if (len_sq < 1e-12) {
        out_t = 0.0;
        out_closest = a;
        return (pt - a).norm();
    }
    out_t = std::clamp((pt - a).dot(ab) / len_sq, 0.0, 1.0);
    out_closest = a + ab * out_t;
    return (pt - out_closest).norm();
}

bool segment_intersects_rect_2d(
    const core::Vec2d& p0,
    const core::Vec2d& p1,
    const core::BBox2d& rect
) noexcept {
    if (rect.contains(p0) || rect.contains(p1)) {
        return true;
    }
    const core::Vec2d c00{rect.min_pt.x, rect.min_pt.y};
    const core::Vec2d c10{rect.max_pt.x, rect.min_pt.y};
    const core::Vec2d c11{rect.max_pt.x, rect.max_pt.y};
    const core::Vec2d c01{rect.min_pt.x, rect.max_pt.y};
    return core::intersect_segments_2d(p0, p1, c00, c10).intersects ||
           core::intersect_segments_2d(p0, p1, c10, c11).intersects ||
           core::intersect_segments_2d(p0, p1, c11, c01).intersects ||
           core::intersect_segments_2d(p0, p1, c01, c00).intersects;
}
} // namespace

std::optional<PickHit> CadPicker::pick_entity(
    const model::Obra& obra,
    double screen_x,
    double screen_y,
    const Camera3d& camera,
    int viewport_w,
    int viewport_h,
    double tolerance_px,
    const CadCategoryFilter& cat_filter
) const {
    const Ray3d ray = camera.screen_to_ray(screen_x, screen_y, viewport_w, viewport_h);
    const double world_tol = std::max(0.25, tolerance_px * camera.world_units_per_pixel(viewport_w, viewport_h) * 2.0);
    const auto candidates = octree_.query_ray(ray.origin, ray.direction);

    const core::Vec2d cursor_px{screen_x, screen_y};
    std::optional<PickHit> best_nudo;
    std::optional<PickHit> best_barra;

    auto eval_nudo = [&](const model::Nudo3d& n) {
        if (!is_nudo_selectable(obra, n, cat_filter)) return;
        const core::Vec3d scr = camera.world_to_screen(n.pos, viewport_w, viewport_h);
        const double d_px = (core::Vec2d{scr.x, scr.y} - cursor_px).norm();
        if (d_px <= tolerance_px) {
            if (!best_nudo || d_px < best_nudo->screen_distance_px) {
                best_nudo = PickHit{EntityKind::Nudo, n.id, n.pos, d_px, scr.z};
            }
        }
    };

    auto eval_barra = [&](const model::Barra3d& b) {
        if (!is_barra_selectable(obra, b, cat_filter)) return;
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (!ni || !nf) return;
        const core::Vec3d s0 = camera.world_to_screen(ni->pos, viewport_w, viewport_h);
        const core::Vec3d s1 = camera.world_to_screen(nf->pos, viewport_w, viewport_h);
        double t_seg = 0.0;
        core::Vec2d closest_px{};
        const double d_px = distance_point_to_segment_2d(cursor_px, {s0.x, s0.y}, {s1.x, s1.y}, t_seg, closest_px);
        if (d_px <= tolerance_px) {
            const core::Vec3d hit_w = ni->pos + (nf->pos - ni->pos) * t_seg;
            const double depth_z = s0.z + (s1.z - s0.z) * t_seg;
            if (!best_barra || d_px < best_barra->screen_distance_px) {
                best_barra = PickHit{EntityKind::Barra, b.id, hit_w, d_px, depth_z};
            }
        }
    };

    for (const auto& item : candidates) {
        if (item.kind == EntityKind::Nudo) {
            if (const auto* n = obra.find_nudo(item.entity_id)) {
                eval_nudo(*n);
            }
        } else if (item.kind == EntityKind::Barra) {
            if (const auto* b = obra.find_barra(item.entity_id)) {
                eval_barra(*b);
            }
        }
    }

    // Respaldo exacto en pantalla si el radio en píxeles excede el pad del Octree en escalas de zoom alejadas
    if (!best_nudo && !best_barra) {
        (void)world_tol;
        for (const auto& n : obra.estructura().nudos) {
            eval_nudo(n);
        }
        for (const auto& b : obra.estructura().barras) {
            eval_barra(b);
        }
    }

    // Priorizar nudos sobre barras cuando ambos están dentro de la tolerancia de captura
    if (best_nudo) return best_nudo;
    return best_barra;
}

std::vector<PickHit> CadPicker::select_box(
    const model::Obra& obra,
    double x0_px,
    double y0_px,
    double x1_px,
    double y1_px,
    const Camera3d& camera,
    int viewport_w,
    int viewport_h,
    SelectionBoxMode mode,
    const CadCategoryFilter& cat_filter
) const {
    const bool crossing = (mode == SelectionBoxMode::CrossingCapture) ||
                          (mode == SelectionBoxMode::AutoByDragDirection && x1_px < x0_px);

    core::BBox2d screen_rect{};
    screen_rect.expand({x0_px, y0_px});
    screen_rect.expand({x1_px, y1_px});

    std::vector<PickHit> results;

    for (const auto& n : obra.estructura().nudos) {
        if (!is_nudo_selectable(obra, n, cat_filter)) continue;
        const core::Vec3d s = camera.world_to_screen(n.pos, viewport_w, viewport_h);
        if (screen_rect.contains({s.x, s.y})) {
            results.push_back(PickHit{EntityKind::Nudo, n.id, n.pos, 0.0, s.z});
        }
    }

    for (const auto& b : obra.estructura().barras) {
        if (!is_barra_selectable(obra, b, cat_filter)) continue;
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (!ni || !nf) continue;

        const core::Vec3d s0 = camera.world_to_screen(ni->pos, viewport_w, viewport_h);
        const core::Vec3d s1 = camera.world_to_screen(nf->pos, viewport_w, viewport_h);
        const core::Vec2d p0{s0.x, s0.y};
        const core::Vec2d p1{s1.x, s1.y};

        const bool inside_both = screen_rect.contains(p0) && screen_rect.contains(p1);
        const bool selected = crossing ? segment_intersects_rect_2d(p0, p1, screen_rect) : inside_both;
        if (selected) {
            results.push_back(PickHit{EntityKind::Barra, b.id, 0.5 * (ni->pos + nf->pos), 0.0, 0.5 * (s0.z + s1.z)});
        }
    }

    return results;
}

// ============================================================================
// P-04.2 (3): DxfTemplate y SnapEngine (`ebaobcap` / `dxflib` / `geom2d`)
// ============================================================================

void DxfTemplate::set_transformation(const core::Vec3d& offset, double scale, double rotation_z_rad) noexcept {
    offset_ = offset;
    scale_ = scale;
    rotation_z_rad_ = rotation_z_rad;
}

core::Vec3d DxfTemplate::transform_point(const core::Vec3d& local_pt) const noexcept {
    const double c = std::cos(rotation_z_rad_);
    const double s = std::sin(rotation_z_rad_);
    const double sx = local_pt.x * scale_;
    const double sy = local_pt.y * scale_;
    const double sz = local_pt.z * scale_;
    return {
        offset_.x + (sx * c - sy * s),
        offset_.y + (sx * s + sy * c),
        offset_.z + sz
    };
}

void DxfTemplate::add_layer(std::string name, bool visible, std::uint32_t color_rgb) {
    for (auto& l : layers_) {
        if (l.name == name) {
            l.visible = visible;
            l.color_rgb = color_rgb;
            return;
        }
    }
    layers_.push_back(DxfLayer{std::move(name), visible, color_rgb});
}

bool DxfTemplate::is_layer_visible(std::string_view layer_name) const noexcept {
    for (const auto& l : layers_) {
        if (l.name == layer_name) return l.visible;
    }
    return true;
}

void DxfTemplate::add_point(std::string layer, const core::Vec3d& pos) {
    points_.push_back(DxfPointEntity{std::move(layer), pos});
}

void DxfTemplate::add_line(std::string layer, const core::Vec3d& p0, const core::Vec3d& p1) {
    lines_.push_back(DxfLineEntity{std::move(layer), p0, p1});
}

void DxfTemplate::add_polyline(std::string layer, const std::vector<core::Vec3d>& vertices, bool closed) {
    if (vertices.size() < 2) return;
    for (std::size_t i = 0; i + 1 < vertices.size(); ++i) {
        lines_.push_back(DxfLineEntity{layer, vertices[i], vertices[i + 1]});
    }
    if (closed && vertices.size() >= 3) {
        lines_.push_back(DxfLineEntity{std::move(layer), vertices.back(), vertices.front()});
    }
}

namespace {
struct SegmentCandidate3d {
    core::Vec3d a;
    core::Vec3d b;
    bool from_dxf{false};
};

std::optional<core::Vec3d> intersect_segments_3d_coplanar(
    const core::Vec3d& a0,
    const core::Vec3d& a1,
    const core::Vec3d& b0,
    const core::Vec3d& b1
) noexcept {
    const core::Vec3d u = a1 - a0;
    const core::Vec3d v = b1 - b0;
    const core::Vec3d w = a0 - b0;
    const double a = u.dot(u);
    const double b = u.dot(v);
    const double c = v.dot(v);
    const double d = u.dot(w);
    const double e = v.dot(w);
    const double denom = a * c - b * b;
    if (denom < 1e-10) {
        return std::nullopt;
    }
    const double s = (b * e - c * d) / denom;
    const double t = (a * e - b * d) / denom;
    if (s < -1e-6 || s > 1.0 + 1e-6 || t < -1e-6 || t > 1.0 + 1e-6) {
        return std::nullopt;
    }
    const core::Vec3d pa = a0 + u * std::clamp(s, 0.0, 1.0);
    const core::Vec3d pb = b0 + v * std::clamp(t, 0.0, 1.0);
    if ((pa - pb).norm() > 1e-3) {
        return std::nullopt;
    }
    return 0.5 * (pa + pb);
}
} // namespace

SnapResult SnapEngine::find_snap(
    const model::Obra& obra,
    double screen_x,
    double screen_y,
    const Camera3d& camera,
    int viewport_w,
    int viewport_h,
    const core::Plane3d& work_plane,
    const std::optional<core::Vec3d>& reference_pt,
    const DxfTemplate* dxf_template,
    const CadCategoryFilter& cat_filter
) const {
    if (!config_.enabled) {
        return SnapResult{};
    }

    const core::Vec2d cursor_px{screen_x, screen_y};
    const double tol_px = config_.snap_tolerance_px;

    // Recopilar todos los segmentos 3D visibles (barras de la obra + plantilla DXF activa)
    std::vector<SegmentCandidate3d> segments;
    for (const auto& b : obra.estructura().barras) {
        if (!CadPicker::is_barra_visible(obra, b, cat_filter)) continue;
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (ni && nf) {
            segments.push_back(SegmentCandidate3d{ni->pos, nf->pos, false});
        }
    }

    if (config_.snap_plantilla_dxf && dxf_template && dxf_template->active()) {
        for (const auto& ln : dxf_template->lines()) {
            if (!dxf_template->is_layer_visible(ln.layer)) continue;
            segments.push_back(SegmentCandidate3d{
                dxf_template->transform_point(ln.p0),
                dxf_template->transform_point(ln.p1),
                true
            });
        }
    }

    auto screen_dist = [&](const core::Vec3d& wpt) -> double {
        const core::Vec3d s = camera.world_to_screen(wpt, viewport_w, viewport_h);
        return (core::Vec2d{s.x, s.y} - cursor_px).norm();
    };

    // 1. Extremo (Nudos visibles, extremos de barras y puntos/extremos DXF)
    if (config_.snap_extremo) {
        SnapResult best{};
        best.screen_distance_px = tol_px + 1.0;

        for (const auto& n : obra.estructura().nudos) {
            if (!CadPicker::is_nudo_visible(obra, n, cat_filter)) continue;
            const double d = screen_dist(n.pos);
            if (d <= tol_px && d < best.screen_distance_px) {
                best = SnapResult{true, SnapType::Extremo, n.pos, d, "Extremo (" + n.referencia + ")"};
            }
        }
        for (const auto& seg : segments) {
            for (const auto& end_pt : {seg.a, seg.b}) {
                const double d = screen_dist(end_pt);
                if (d <= tol_px && d < best.screen_distance_px) {
                    const SnapType st = seg.from_dxf ? SnapType::PlantillaDxf : SnapType::Extremo;
                    best = SnapResult{true, st, end_pt, d, seg.from_dxf ? "Extremo DXF" : "Extremo"};
                }
            }
        }
        if (config_.snap_plantilla_dxf && dxf_template && dxf_template->active()) {
            for (const auto& pt : dxf_template->points()) {
                if (!dxf_template->is_layer_visible(pt.layer)) continue;
                const core::Vec3d wpt = dxf_template->transform_point(pt.pos);
                const double d = screen_dist(wpt);
                if (d <= tol_px && d < best.screen_distance_px) {
                    best = SnapResult{true, SnapType::PlantillaDxf, wpt, d, "Punto DXF"};
                }
            }
        }
        if (best.hit) return best;
    }

    // 2. Intersección (corte 3D entre barras o líneas DXF)
    if (config_.snap_interseccion && segments.size() >= 2) {
        SnapResult best{};
        best.screen_distance_px = tol_px + 1.0;
        for (std::size_t i = 0; i < segments.size(); ++i) {
            for (std::size_t j = i + 1; j < segments.size(); ++j) {
                if (auto ip = intersect_segments_3d_coplanar(segments[i].a, segments[i].b, segments[j].a, segments[j].b)) {
                    const double d = screen_dist(*ip);
                    if (d <= tol_px && d < best.screen_distance_px) {
                        best = SnapResult{true, SnapType::Interseccion, *ip, d, "Intersección"};
                    }
                }
            }
        }
        if (best.hit) return best;
    }

    // 3. Punto medio
    if (config_.snap_punto_medio) {
        SnapResult best{};
        best.screen_distance_px = tol_px + 1.0;
        for (const auto& seg : segments) {
            const core::Vec3d mid = 0.5 * (seg.a + seg.b);
            const double d = screen_dist(mid);
            if (d <= tol_px && d < best.screen_distance_px) {
                best = SnapResult{true, SnapType::PuntoMedio, mid, d, "Punto medio"};
            }
        }
        if (best.hit) return best;
    }

    // 4. Perpendicular ( pie de la perpendicular desde `reference_pt` al segmento )
    if (config_.snap_perpendicular && reference_pt.has_value()) {
        SnapResult best{};
        best.screen_distance_px = tol_px + 1.0;
        for (const auto& seg : segments) {
            const core::Vec3d ab = seg.b - seg.a;
            const double len_sq = ab.norm_sq();
            if (len_sq < 1e-12) continue;
            const double t = (*reference_pt - seg.a).dot(ab) / len_sq;
            if (t >= -1e-4 && t <= 1.0 + 1e-4) {
                const core::Vec3d foot = seg.a + ab * std::clamp(t, 0.0, 1.0);
                const double d = screen_dist(foot);
                if (d <= tol_px && d < best.screen_distance_px) {
                    best = SnapResult{true, SnapType::Perpendicular, foot, d, "Perpendicular"};
                }
            }
        }
        if (best.hit) return best;
    }

    // 5. Más cercano (proyección exacta sobre el segmento 3D)
    if (config_.snap_mas_cercano) {
        SnapResult best{};
        best.screen_distance_px = tol_px + 1.0;
        for (const auto& seg : segments) {
            const core::Vec3d s0 = camera.world_to_screen(seg.a, viewport_w, viewport_h);
            const core::Vec3d s1 = camera.world_to_screen(seg.b, viewport_w, viewport_h);
            double t_seg = 0.0;
            core::Vec2d closest_px{};
            const double d = distance_point_to_segment_2d(cursor_px, {s0.x, s0.y}, {s1.x, s1.y}, t_seg, closest_px);
            if (d <= tol_px && d < best.screen_distance_px) {
                const core::Vec3d wpt = seg.a + (seg.b - seg.a) * t_seg;
                best = SnapResult{true, SnapType::MasCercano, wpt, d, "Más cercano"};
            }
        }
        if (best.hit) return best;
    }

    // Intersección del cursor con el plano de trabajo para Rastreo Polar y Rejilla
    const auto plane_pt = camera.screen_to_plane(screen_x, screen_y, viewport_w, viewport_h, work_plane);
    if (!plane_pt.has_value()) {
        return SnapResult{};
    }

    // 6. Rastreo polar (bloqueo angular cada `polar_angle_step_deg` desde `reference_pt`)
    if (config_.snap_rastreo_polar && reference_pt.has_value()) {
        const core::Vec3d delta = *plane_pt - *reference_pt;
        const double r = std::hypot(delta.x, delta.y);
        if (r > 1e-5) {
            const double angle = std::atan2(delta.y, delta.x);
            const double step_rad = config_.polar_angle_step_deg * (std::numbers::pi / 180.0);
            const double snapped_angle = std::round(angle / step_rad) * step_rad;
            const core::Vec3d polar_pt{
                reference_pt->x + r * std::cos(snapped_angle),
                reference_pt->y + r * std::sin(snapped_angle),
                plane_pt->z
            };
            const double d = screen_dist(polar_pt);
            if (d <= tol_px) {
                const int deg = static_cast<int>(std::round(snapped_angle * 180.0 / std::numbers::pi));
                return SnapResult{true, SnapType::RastreoPolar, polar_pt, d, "Polar " + std::to_string(deg) + "°"};
            }
        }
    }

    // 7. Rejilla (redondeo al múltiplo de `grid_step_m` en el plano de trabajo)
    if (config_.snap_rejilla && config_.grid_step_m > 1e-6) {
        const double step = config_.grid_step_m;
        const core::Vec3d grid_pt{
            std::round(plane_pt->x / step) * step,
            std::round(plane_pt->y / step) * step,
            plane_pt->z
        };
        const double d = screen_dist(grid_pt);
        if (d <= tol_px) {
            return SnapResult{true, SnapType::Rejilla, grid_pt, d, "Rejilla"};
        }
    }

    return SnapResult{};
}

// ============================================================================
// P-04.2 (4): DimensionSystem (`cyplanobj.dll` / `acotaseg` / `cypecota`)
// ============================================================================

std::uint32_t DimensionSystem::add_dimension(
    const core::Vec3d& p_start,
    const core::Vec3d& p_end,
    double offset_m,
    const core::Vec3d& plane_normal,
    units::UnitSystem unit_system
) {
    LinearDimension3d dim{};
    dim.id = next_dim_id_++;
    dim.p_start = p_start;
    dim.p_end = p_end;
    dim.plane_normal = plane_normal.normalized();
    dim.offset_m = offset_m;
    dim.measured_length_m = (p_end - p_start).norm();

    const core::Vec3d dir = (p_end - p_start).normalized();
    core::Vec3d perp = dim.plane_normal.cross(dir);
    if (perp.norm_sq() < 1e-10) {
        perp = {0.0, 1.0, 0.0};
    } else {
        perp = perp.normalized();
    }

    const double ext_overhang = (offset_m >= 0.0) ? 0.12 : -0.12;
    dim.witness1_start = p_start;
    dim.witness1_end = p_start + perp * (offset_m + ext_overhang);
    dim.witness2_start = p_end;
    dim.witness2_end = p_end + perp * (offset_m + ext_overhang);
    dim.dim_line_start = p_start + perp * offset_m;
    dim.dim_line_end = p_end + perp * offset_m;
    dim.text_position = 0.5 * (dim.dim_line_start + dim.dim_line_end) + perp * (0.08 * (offset_m >= 0.0 ? 1.0 : -1.0));
    dim.formatted_text = units::UnitManager::format(units::Magnitude::Length, dim.measured_length_m, unit_system);

    dimensions_.push_back(std::move(dim));
    return dimensions_.back().id;
}

bool DimensionSystem::remove_dimension(std::uint32_t dim_id) {
    auto it = std::find_if(dimensions_.begin(), dimensions_.end(),
                           [dim_id](const LinearDimension3d& d) { return d.id == dim_id; });
    if (it == dimensions_.end()) return false;
    dimensions_.erase(it);
    return true;
}

void DimensionSystem::show_all() noexcept {
    global_visible_ = true;
    for (auto& d : dimensions_) {
        d.visible = true;
    }
}

void DimensionSystem::hide_all() noexcept {
    global_visible_ = false;
    for (auto& d : dimensions_) {
        d.visible = false;
    }
}

const LinearDimension3d* DimensionSystem::find_dimension(std::uint32_t dim_id) const noexcept {
    for (const auto& d : dimensions_) {
        if (d.id == dim_id) return &d;
    }
    return nullptr;
}

// ============================================================================
// P-04.3: Perfiles 2D/3D Extruidos, `cyhlr` y `SceneTessellator`
// ============================================================================

namespace {
std::vector<double> extract_numbers(std::string_view text) {
    std::vector<double> nums;
    std::string cur;
    for (char ch : text) {
        if ((ch >= '0' && ch <= '9') || ch == '.') {
            cur.push_back(ch);
        } else if (!cur.empty()) {
            try {
                nums.push_back(std::stod(cur));
            } catch (...) {}
            cur.clear();
        }
    }
    if (!cur.empty()) {
        try {
            nums.push_back(std::stod(cur));
        } catch (...) {}
    }
    return nums;
}
} // namespace

CrossSectionProfile2d CrossSectionProfile2d::from_profile_name(std::string_view profile_spec) {
    CrossSectionProfile2d prof{};
    prof.name = std::string(core::StringUtils::trim(profile_spec));
    const std::string upper = core::StringUtils::to_upper(prof.name);
    const auto nums = extract_numbers(upper);

    if (upper.starts_with("UPN") || upper.starts_with("U ")) {
        prof.kind = ProfileShapeKind::ChannelUPN;
        const double h_mm = nums.empty() ? 160.0 : nums[0];
        prof.height_z_m = h_mm * 0.001;
        prof.width_y_m = (0.35 * h_mm + 10.0) * 0.001;
        prof.web_thickness_m = std::max(0.005, prof.height_z_m * 0.045);
        prof.flange_thickness_m = std::max(0.007, prof.height_z_m * 0.065);

        const double hy = 0.5 * prof.width_y_m;
        const double hz = 0.5 * prof.height_z_m;
        const double tw = prof.web_thickness_m;
        const double tf = prof.flange_thickness_m;
        // Polígono en C (8 vértices en sentido antihorario)
        prof.outer_contour = {
            {-hy, -hz}, {hy, -hz}, {hy, -hz + tf}, {-hy + tw, -hz + tf},
            {-hy + tw, hz - tf}, {hy, hz - tf}, {hy, hz}, {-hy, hz}
        };
    } else if (upper.starts_with("L ") || upper.starts_with("L-") ||
               upper.starts_with("LD")) {
        prof.kind = ProfileShapeKind::AngleL;
        const double h_mm = nums.size() >= 1 ? nums[0] : 80.0;
        const double b_mm = nums.size() >= 2 ? nums[1] : h_mm;
        const double t_mm = nums.size() >= 3 ? nums[2] : std::max(5.0, 0.1 * h_mm);
        prof.height_z_m = h_mm * 0.001;
        prof.width_y_m = b_mm * 0.001;
        prof.web_thickness_m = t_mm * 0.001;
        prof.flange_thickness_m = t_mm * 0.001;

        const double hy = 0.5 * prof.width_y_m;
        const double hz = 0.5 * prof.height_z_m;
        const double t = prof.web_thickness_m;
        // Polígono en L (6 vértices)
        prof.outer_contour = {
            {-hy, -hz}, {hy, -hz}, {hy, -hz + t},
            {-hy + t, -hz + t}, {-hy + t, hz}, {-hy, hz}
        };
    } else if (upper.starts_with("SHS") || upper.starts_with("RHS") ||
               upper.starts_with("TUB")) {
        prof.kind = ProfileShapeKind::HollowRect;
        const double h_mm = nums.size() >= 1 ? nums[0] : 100.0;
        const double b_mm = nums.size() >= 2 ? nums[1] : h_mm;
        const double t_mm = nums.size() >= 3 ? nums[2] : 5.0;
        prof.height_z_m = h_mm * 0.001;
        prof.width_y_m = b_mm * 0.001;
        prof.web_thickness_m = t_mm * 0.001;
        prof.flange_thickness_m = t_mm * 0.001;

        const double hy = 0.5 * prof.width_y_m;
        const double hz = 0.5 * prof.height_z_m;
        const double t = std::min({prof.web_thickness_m, hy * 0.45, hz * 0.45});
        prof.outer_contour = {{-hy, -hz}, {hy, -hz}, {hy, hz}, {-hy, hz}};
        prof.inner_hole_contour = {{-hy + t, -hz + t}, {hy - t, -hz + t}, {hy - t, hz - t}, {-hy + t, hz - t}};
    } else if (upper.starts_with("CHS") || upper.starts_with("CIRC")) {
        prof.kind = ProfileShapeKind::HollowCirc;
        const double d_mm = nums.size() >= 1 ? nums[0] : 114.3;
        const double t_mm = nums.size() >= 2 ? nums[1] : 4.0;
        prof.height_z_m = d_mm * 0.001;
        prof.width_y_m = d_mm * 0.001;
        prof.web_thickness_m = t_mm * 0.001;
        prof.flange_thickness_m = t_mm * 0.001;

        const double r_out = 0.5 * prof.height_z_m;
        const double r_in = std::max(0.001, r_out - prof.web_thickness_m);
        constexpr int kSegs = 12;
        for (int i = 0; i < kSegs; ++i) {
            const double ang = (2.0 * std::numbers::pi * i) / kSegs;
            prof.outer_contour.push_back({r_out * std::cos(ang), r_out * std::sin(ang)});
            prof.inner_hole_contour.push_back({r_in * std::cos(ang), r_in * std::sin(ang)});
        }
    } else if (upper.starts_with("RECT") || upper.starts_with("HA ") ||
               upper.starts_with("MAD")) {
        prof.kind = ProfileShapeKind::SolidRect;
        const double b_cm_or_mm = nums.size() >= 1 ? nums[0] : 30.0;
        const double h_cm_or_mm = nums.size() >= 2 ? nums[1] : 50.0;
        const double unit_scale = (b_cm_or_mm <= 100.0 && h_cm_or_mm <= 150.0) ? 0.01 : 0.001;
        prof.width_y_m = b_cm_or_mm * unit_scale;
        prof.height_z_m = h_cm_or_mm * unit_scale;
        const double hy = 0.5 * prof.width_y_m;
        const double hz = 0.5 * prof.height_z_m;
        prof.outer_contour = {{-hy, -hz}, {hy, -hz}, {hy, hz}, {-hy, hz}};
    } else {
        // Por defecto: Doble T (IPE, HEB, HEA, HEM, IPN)
        prof.kind = ProfileShapeKind::DoubleT;
        const double h_mm = nums.empty() ? 200.0 : nums[0];
        const bool is_he = upper.starts_with("HE");
        prof.height_z_m = h_mm * 0.001;
        prof.width_y_m = is_he ? std::min(0.300, h_mm * 0.001) : (0.5 * h_mm * 0.001);
        prof.web_thickness_m = std::max(0.004, prof.height_z_m * (is_he ? 0.045 : 0.028));
        prof.flange_thickness_m = std::max(0.006, prof.height_z_m * (is_he ? 0.075 : 0.0425));

        const double hy = 0.5 * prof.width_y_m;
        const double hz = 0.5 * prof.height_z_m;
        const double htw = 0.5 * prof.web_thickness_m;
        const double tf = prof.flange_thickness_m;
        // Polígono en doble T (12 vértices en sentido antihorario)
        prof.outer_contour = {
            {-hy, -hz},
            { hy, -hz},
            { hy, -hz + tf},
            { htw, -hz + tf},
            { htw,  hz - tf},
            { hy,  hz - tf},
            { hy,  hz},
            {-hy,  hz},
            {-hy,  hz - tf},
            {-htw,  hz - tf},
            {-htw, -hz + tf},
            {-hy, -hz + tf}
        };
    }

    return prof;
}

void HiddenLineRemover::add_clipping_plane(const core::Plane3d& plane) {
    clipping_planes_.push_back(plane);
}

bool HiddenLineRemover::is_point_visible_by_clipping(const core::Vec3d& pt) const noexcept {
    for (const auto& pl : clipping_planes_) {
        if (pl.signed_distance(pt) < -1e-6) {
            return false;
        }
    }
    return true;
}

std::optional<Segment3d> HiddenLineRemover::clip_segment(const Segment3d& seg) const noexcept {
    Segment3d out = seg;
    for (const auto& pl : clipping_planes_) {
        const double d0 = pl.signed_distance(out.p0);
        const double d1 = pl.signed_distance(out.p1);
        if (d0 < 0.0 && d1 < 0.0) {
            return std::nullopt;
        }
        if (d0 < 0.0 || d1 < 0.0) {
            const double t = d0 / (d0 - d1);
            const core::Vec3d ip = out.p0 + (out.p1 - out.p0) * t;
            if (d0 < 0.0) {
                out.p0 = ip;
            } else {
                out.p1 = ip;
            }
        }
    }
    return out;
}

namespace {
bool point_in_triangle_2d(
    const core::Vec2d& pt,
    const core::Vec2d& a,
    const core::Vec2d& b,
    const core::Vec2d& c,
    double& out_u,
    double& out_v,
    double& out_w
) noexcept {
    const core::Vec2d v0 = b - a;
    const core::Vec2d v1 = c - a;
    const core::Vec2d v2 = pt - a;
    const double d00 = v0.dot(v0);
    const double d01 = v0.dot(v1);
    const double d11 = v1.dot(v1);
    const double d20 = v2.dot(v0);
    const double d21 = v2.dot(v1);
    const double denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < 1e-10) {
        return false;
    }
    out_v = (d11 * d20 - d01 * d21) / denom;
    out_w = (d00 * d21 - d01 * d20) / denom;
    out_u = 1.0 - out_v - out_w;
    constexpr double kEps = -1e-4;
    return (out_u >= kEps && out_v >= kEps && out_w >= kEps);
}
} // namespace

HiddenLineResult HiddenLineRemover::compute_hlr(
    const std::vector<BarExtrudedMesh3d>& meshes,
    const Camera3d& camera,
    int viewport_w,
    int viewport_h
) const {
    HiddenLineResult result{};

    struct ProjectedTriangle {
        std::uint32_t barra_id;
        core::Vec3d s0;
        core::Vec3d s1;
        core::Vec3d s2;
        core::BBox2d bbox;
    };

    std::vector<ProjectedTriangle> proj_tris;
    for (const auto& mesh : meshes) {
        for (const auto& tri : mesh.triangle_indices) {
            const core::Vec3d& v0 = mesh.vertices[tri[0]];
            const core::Vec3d& v1 = mesh.vertices[tri[1]];
            const core::Vec3d& v2 = mesh.vertices[tri[2]];
            if (!is_point_visible_by_clipping(v0) &&
                !is_point_visible_by_clipping(v1) &&
                !is_point_visible_by_clipping(v2)) {
                continue;
            }
            ProjectedTriangle pt{};
            pt.barra_id = mesh.barra_id;
            pt.s0 = camera.world_to_screen(v0, viewport_w, viewport_h);
            pt.s1 = camera.world_to_screen(v1, viewport_w, viewport_h);
            pt.s2 = camera.world_to_screen(v2, viewport_w, viewport_h);
            pt.bbox.expand({pt.s0.x, pt.s0.y});
            pt.bbox.expand({pt.s1.x, pt.s1.y});
            pt.bbox.expand({pt.s2.x, pt.s2.y});
            proj_tris.push_back(pt);
        }
    }

    for (const auto& mesh : meshes) {
        for (const auto& edge : mesh.contour_edges) {
            auto clipped = clip_segment(edge);
            if (!clipped.has_value()) continue;

            const core::Vec3d mid_w = 0.5 * (clipped->p0 + clipped->p1);
            const core::Vec3d mid_s = camera.world_to_screen(mid_w, viewport_w, viewport_h);
            const core::Vec2d mid_2d{mid_s.x, mid_s.y};

            bool occluded = false;
            for (const auto& pt : proj_tris) {
                if (pt.barra_id == mesh.barra_id) continue;
                if (!pt.bbox.contains(mid_2d)) continue;
                double u = 0.0, v = 0.0, w = 0.0;
                if (point_in_triangle_2d(mid_2d, {pt.s0.x, pt.s0.y}, {pt.s1.x, pt.s1.y}, {pt.s2.x, pt.s2.y}, u, v, w)) {
                    const double tri_z = u * pt.s0.z + v * pt.s1.z + w * pt.s2.z;
                    // En NDC [-1, +1], valores menores de Z están más cerca del observador
                    if (tri_z + 1e-4 < mid_s.z) {
                        occluded = true;
                        break;
                    }
                }
            }

            if (occluded) {
                Segment3d occ = *clipped;
                occ.dashed = true;
                occ.color_rgb = 0x9098A0;
                result.occluded_segments.push_back(occ);
            } else {
                result.visible_segments.push_back(*clipped);
            }
        }
    }

    return result;
}

BarExtrudedMesh3d SceneTessellator::extrude_bar_mesh_3d(
    const model::Barra3d& barra,
    const core::Vec3d& p_ini,
    const core::Vec3d& p_fin,
    std::uint32_t color_rgb
) {
    BarExtrudedMesh3d mesh{};
    mesh.barra_id = barra.id;

    const CrossSectionProfile2d prof = CrossSectionProfile2d::from_profile_name(barra.perfil);
    mesh.shape_kind = prof.kind;

    const core::LocalAxes3d axes = core::LocalAxes3d::from_bar_endpoints(p_ini, p_fin, barra.angulo_giro_rad).value_or(core::LocalAxes3d{});
    const std::size_t n_pts = prof.outer_contour.size();
    if (n_pts < 3) return mesh;

    auto to_world = [&](const core::Vec2d& yz, const core::Vec3d& base_pt) -> core::Vec3d {
        return base_pt +
               axes.v * (yz.x + barra.desp_y_m) +
               axes.w * (yz.y + barra.desp_z_m);
    };

    // Generar caras laterales extruidas a lo largo del eje local X (`ebaprisma3d_crea_prisma_extruido`)
    for (std::size_t i = 0; i < n_pts; ++i) {
        const std::size_t next_i = (i + 1) % n_pts;
        const core::Vec2d c0 = prof.outer_contour[i];
        const core::Vec2d c1 = prof.outer_contour[next_i];

        const core::Vec3d v0_ini = to_world(c0, p_ini);
        const core::Vec3d v1_ini = to_world(c1, p_ini);
        const core::Vec3d v1_fin = to_world(c1, p_fin);
        const core::Vec3d v0_fin = to_world(c0, p_fin);

        const core::Vec3d face_normal = (v1_ini - v0_ini).cross(v0_fin - v0_ini).normalized();
        const std::uint32_t base_idx = static_cast<std::uint32_t>(mesh.vertices.size());

        mesh.vertices.push_back(v0_ini);
        mesh.vertices.push_back(v1_ini);
        mesh.vertices.push_back(v1_fin);
        mesh.vertices.push_back(v0_fin);

        for (int k = 0; k < 4; ++k) {
            mesh.normals.push_back(face_normal);
        }

        mesh.triangle_indices.push_back({base_idx + 0, base_idx + 1, base_idx + 2});
        mesh.triangle_indices.push_back({base_idx + 0, base_idx + 2, base_idx + 3});

        // Aristas longitudinales y de tapas para `genele3d_lineas_contorno_figura_extruida_dado_eje`
        mesh.contour_edges.push_back(Segment3d{v0_ini, v0_fin, color_rgb, false, barra.id});
        mesh.contour_edges.push_back(Segment3d{v0_ini, v1_ini, color_rgb, false, barra.id});
        mesh.contour_edges.push_back(Segment3d{v0_fin, v1_fin, color_rgb, false, barra.id});
    }

    // Tapas frontal e inicial (abanico respecto al centroide de la sección para tapas planas)
    const core::Vec3d cap_center_ini = to_world({0.0, 0.0}, p_ini);
    const core::Vec3d cap_center_fin = to_world({0.0, 0.0}, p_fin);
    const std::uint32_t idx_c_ini = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(cap_center_ini);
    mesh.normals.push_back(-axes.u);

    const std::uint32_t idx_c_fin = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(cap_center_fin);
    mesh.normals.push_back(axes.u);

    for (std::size_t i = 0; i < n_pts; ++i) {
        const std::uint32_t face_base = static_cast<std::uint32_t>(i * 4);
        mesh.triangle_indices.push_back({idx_c_ini, face_base + 1, face_base + 0});
        mesh.triangle_indices.push_back({idx_c_fin, face_base + 3, face_base + 2});
    }

    return mesh;
}

TessellatedScene SceneTessellator::build_scene(
    const model::Obra& obra,
    const Camera3d& camera,
    int viewport_w,
    int viewport_h,
    RenderRepresentationMode mode,
    const CadCategoryFilter& cat_filter,
    bool draw_local_axes,
    bool draw_supports,
    const DimensionSystem* dimensions
) const {
    TessellatedScene scene{};
    scene.mode = mode;

    // 1. Nudos visibles y símbolos de vinculación exterior (`VinculacionExterior`)
    for (const auto& n : obra.estructura().nudos) {
        if (!CadPicker::is_nudo_visible(obra, n, cat_filter)) continue;
        ++scene.visible_nudo_count;

        if (draw_supports && n.vinc_ext != model::VinculacionExterior::Libre) {
            constexpr double s = 0.25;
            if (n.vinc_ext == model::VinculacionExterior::Empotrado) {
                // Prisma/cubo de empotramiento en la base del nudo
                const core::Vec3d b0 = n.pos + core::Vec3d{-s, -s, -s};
                const core::Vec3d b1 = n.pos + core::Vec3d{ s, -s, -s};
                const core::Vec3d b2 = n.pos + core::Vec3d{ s,  s, -s};
                const core::Vec3d b3 = n.pos + core::Vec3d{-s,  s, -s};
                scene.support_symbol_segments.push_back({b0, b1, 0xC83232, false, n.id});
                scene.support_symbol_segments.push_back({b1, b2, 0xC83232, false, n.id});
                scene.support_symbol_segments.push_back({b2, b3, 0xC83232, false, n.id});
                scene.support_symbol_segments.push_back({b3, b0, 0xC83232, false, n.id});
            } else {
                // Pirámide triangular de apoyo articulado / elástico
                const core::Vec3d b0 = n.pos + core::Vec3d{-s, -s, -s};
                const core::Vec3d b1 = n.pos + core::Vec3d{ s, -s, -s};
                const core::Vec3d b2 = n.pos + core::Vec3d{ 0.0, s, -s};
                scene.support_symbol_segments.push_back({n.pos, b0, 0xD86418, false, n.id});
                scene.support_symbol_segments.push_back({n.pos, b1, 0xD86418, false, n.id});
                scene.support_symbol_segments.push_back({n.pos, b2, 0xD86418, false, n.id});
                scene.support_symbol_segments.push_back({b0, b1, 0xD86418, false, n.id});
                scene.support_symbol_segments.push_back({b1, b2, 0xD86418, false, n.id});
                scene.support_symbol_segments.push_back({b2, b0, 0xD86418, false, n.id});
            }
        }
    }

    // 2. Barras visibles: modo alámbrico, ejes locales y extrusión 3D
    for (const auto& b : obra.estructura().barras) {
        if (!CadPicker::is_barra_visible(obra, b, cat_filter)) continue;
        const auto* ni = obra.find_nudo(b.nudo_inicial_id);
        const auto* nf = obra.find_nudo(b.nudo_final_id);
        if (!ni || !nf) continue;

        ++scene.visible_barra_count;
        const auto* capa = obra.find_capa(b.capa_id);
        const std::uint32_t color = capa ? capa->color_rgb : 0x0080FF;

        scene.wireframe_segments.push_back(Segment3d{ni->pos, nf->pos, color, false, b.id});

        if (draw_local_axes) {
            const core::Vec3d mid = 0.5 * (ni->pos + nf->pos);
            const core::LocalAxes3d ax = core::LocalAxes3d::from_bar_endpoints(ni->pos, nf->pos, b.angulo_giro_rad).value_or(core::LocalAxes3d{});
            constexpr double kAxisLen = 0.35;
            scene.local_axes_segments.push_back(Segment3d{mid, mid + ax.u * kAxisLen, 0xE02020, false, b.id}); // X local: Rojo
            scene.local_axes_segments.push_back(Segment3d{mid, mid + ax.v * kAxisLen, 0x20B030, false, b.id}); // Y local: Verde
            scene.local_axes_segments.push_back(Segment3d{mid, mid + ax.w * kAxisLen, 0x2060E0, false, b.id}); // Z local: Azul
        }

        if (mode == RenderRepresentationMode::LineaOcultaHlr ||
            mode == RenderRepresentationMode::PerfilRealExtruido3d) {
            scene.extruded_bar_meshes.push_back(extrude_bar_mesh_3d(b, ni->pos, nf->pos, color));
        }
    }

    // 3. Láminas 3D (`Lamina3d`)
    for (const auto& lam : obra.estructura().laminas) {
        if (!CadPicker::is_lamina_visible(obra, lam, cat_filter)) continue;
        if (lam.vertices.size() < 3) continue;
        ++scene.visible_lamina_count;
        for (std::size_t i = 0; i < lam.vertices.size(); ++i) {
            const auto& p0 = lam.vertices[i];
            const auto& p1 = lam.vertices[(i + 1) % lam.vertices.size()];
            scene.wireframe_segments.push_back({p0, p1, 0x5090D0, false, lam.id});
        }
        const core::Vec3d nrm = (lam.vertices[1] - lam.vertices[0]).cross(lam.vertices[2] - lam.vertices[0]).normalized();
        for (std::size_t i = 1; i + 1 < lam.vertices.size(); ++i) {
            scene.surface_triangles.push_back(Triangle3d{
                lam.vertices[0], lam.vertices[i], lam.vertices[i + 1], nrm, 0x6CA0DC, lam.id
            });
        }
    }

    // 4. Paños de reparto de cargas (`Panyo3d`)
    for (const auto& pan : obra.estructura().panyos) {
        if (!CadPicker::is_panyo_visible(obra, pan, cat_filter)) continue;
        if (pan.vertices.size() < 3) continue;
        ++scene.visible_panyo_count;
        for (std::size_t i = 0; i < pan.vertices.size(); ++i) {
            const auto& p0 = pan.vertices[i];
            const auto& p1 = pan.vertices[(i + 1) % pan.vertices.size()];
            scene.wireframe_segments.push_back({p0, p1, 0x80A040, true, pan.id});
        }
    }

    // 5. Cálculo de líneas ocultas (`cyhlr`) cuando se solicita `LineaOcultaHlr`
    if (mode == RenderRepresentationMode::LineaOcultaHlr && !scene.extruded_bar_meshes.empty()) {
        scene.hlr_output = hlr_.compute_hlr(scene.extruded_bar_meshes, camera, viewport_w, viewport_h);
    }

    // 6. Cotas lineales 3D (`DimensionSystem` / `cyplanobj`)
    if (dimensions && dimensions->global_visibility()) {
        for (const auto& d : dimensions->dimensions()) {
            if (!d.visible) continue;
            scene.dimension_segments.push_back({d.witness1_start, d.witness1_end, 0x404040, false, d.id});
            scene.dimension_segments.push_back({d.witness2_start, d.witness2_end, 0x404040, false, d.id});
            scene.dimension_segments.push_back({d.dim_line_start, d.dim_line_end, 0x202020, false, d.id});
        }
    }

    return scene;
}

} // namespace cypeclon::cad
