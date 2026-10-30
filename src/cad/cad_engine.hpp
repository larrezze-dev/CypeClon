#pragma once

#include "core/math.hpp"

#include <cstdint>
#include <memory>
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
 * @brief Octree espacial 3D para selección y picking rápido equivalente a ebaoctree (ed3dlib.dll).
 */
class Octree3d {
public:
    explicit Octree3d(const core::BBox3d& world_bounds, int max_depth = 6, std::size_t max_items_per_leaf = 16);

    void clear();
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

/**
 * @brief Configuración de barra de capturas CAD equivalente a ebarcbca_crea_configuracion_barra_capturas.
 */
struct SnapConfig {
    bool enabled{true};
    bool snap_extremo{true};
    bool snap_punto_medio{true};
    bool snap_perpendicular{true};
    bool snap_interseccion{true};
    bool snap_mas_cercano{false};
    bool snap_rejilla{true};
    double grid_step_m{1.0};
};

} // namespace cypeclon::cad

