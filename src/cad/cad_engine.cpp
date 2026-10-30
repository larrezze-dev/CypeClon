#include "cad/cad_engine.hpp"

namespace cypeclon::cad {

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

} // namespace cypeclon::cad

