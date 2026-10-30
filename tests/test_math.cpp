#include "core/math.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void assert_math(bool cond, const char* expr, int line) {
    if (!cond) {
        std::cerr << "[FAIL test_math.cpp:" << line << "] " << expr << "\n";
        std::exit(1);
    }
}
#define MATH_ASSERT(expr) assert_math((expr), #expr, __LINE__)
} // namespace

void run_math_tests() {
    using namespace cypeclon::core;

    // 1. Tolerancias y redondeo (cypemath)
    MATH_ASSERT(es_cero(1.0e-9));
    MATH_ASSERT(son_iguales(10.00000001, 10.0));
    MATH_ASSERT(son_iguales(round_decimals(3.14159265, 3), 3.142));
    MATH_ASSERT(son_iguales(round_significant(0.0123456, 3), 0.0123));

    // 2. Vec2d y Vec3d
    const Vec2d v2a{3.0, 4.0};
    MATH_ASSERT(son_iguales(v2a.norm(), 5.0));
    const Vec2d v2rot = Vec2d{1.0, 0.0}.rotated(kHalfPi);
    MATH_ASSERT(v2rot.equals({0.0, 1.0}));

    const Vec3d vx = Vec3d::unit_x();
    const Vec3d vy = Vec3d::unit_y();
    const Vec3d vz = vx.cross(vy);
    MATH_ASSERT(vz.equals(Vec3d::unit_z()));
    MATH_ASSERT(vx.rotated_around_axis(Vec3d::unit_z(), kHalfPi).equals(vy));

    // 3. Mat3d y Mat4d (determinante, inversa, transformaciones)
    const Mat3d rot = Mat3d::rotation_z(kPi / 4.0) * Mat3d::rotation_x(kPi / 6.0);
    MATH_ASSERT(son_iguales(rot.determinant(), 1.0));
    const auto rot_inv = rot.inverted();
    MATH_ASSERT(rot_inv.has_value());
    const Mat3d prod3 = rot * (*rot_inv);
    MATH_ASSERT(prod3.row(0).equals({1.0, 0.0, 0.0}) &&
                prod3.row(1).equals({0.0, 1.0, 0.0}) &&
                prod3.row(2).equals({0.0, 0.0, 1.0}));

    const Mat4d tr = Mat4d::from_rotation_translation(rot, {10.0, -5.0, 3.5});
    const auto tr_inv = tr.inverted();
    MATH_ASSERT(tr_inv.has_value());
    const Vec3d pt{2.5, -1.2, 8.0};
    const Vec3d pt_back = tr_inv->transform_point(tr.transform_point(pt));
    MATH_ASSERT(pt_back.equals(pt));

    // 4. BBox2d y BBox3d + intersección Rayo-AABB
    BBox3d box;
    box.expand({0.0, 0.0, 0.0});
    box.expand({4.0, 6.0, 3.0});
    MATH_ASSERT(box.is_valid());
    MATH_ASSERT(son_iguales(box.volume(), 72.0));
    MATH_ASSERT(box.contains({2.0, 3.0, 1.5}));
    double tmin = 0.0, tmax = 0.0;
    MATH_ASSERT(box.intersect_ray({-5.0, 3.0, 1.5}, {1.0, 0.0, 0.0}, tmin, tmax));
    MATH_ASSERT(son_iguales(tmin, 5.0) && son_iguales(tmax, 9.0));

    // 5. Ejes locales de barras 3D (barra horizontal y barra vertical) y planos
    const auto bar_horiz = LocalAxes3d::from_bar_endpoints({0.0, 0.0, 0.0}, {6.0, 0.0, 0.0}, 0.0);
    MATH_ASSERT(bar_horiz.has_value());
    MATH_ASSERT(bar_horiz->u.equals({1.0, 0.0, 0.0}));
    MATH_ASSERT(bar_horiz->v.equals({0.0, 1.0, 0.0}));
    MATH_ASSERT(bar_horiz->w.equals({0.0, 0.0, 1.0}));

    const auto bar_vert = LocalAxes3d::from_bar_endpoints({2.0, 3.0, 0.0}, {2.0, 3.0, 4.0}, 0.0);
    MATH_ASSERT(bar_vert.has_value());
    MATH_ASSERT(bar_vert->u.equals({0.0, 0.0, 1.0}));
    MATH_ASSERT(son_iguales(bar_vert->local_to_global_rotation().determinant(), 1.0));

    // 6. Intersección de segmentos 2D y recta-plano 3D
    const auto seg_hit = intersect_segments_2d({0.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}, {4.0, 0.0});
    MATH_ASSERT(seg_hit.intersects && seg_hit.point.equals({2.0, 2.0}));

    const auto plane = Plane3d::from_three_points({0.0, 0.0, 5.0}, {10.0, 0.0, 5.0}, {0.0, 10.0, 5.0});
    MATH_ASSERT(plane.has_value());
    const auto line_hit = plane->intersect_line({2.0, 3.0, 0.0}, {0.0, 0.0, 1.0});
    MATH_ASSERT(line_hit.has_value() && line_hit->equals({2.0, 3.0, 5.0}));

    const auto roots = solve_quadratic(1.0, -5.0, 6.0);
    MATH_ASSERT(roots.size() == 2 && son_iguales(roots[0], 2.0) && son_iguales(roots[1], 3.0));
}

