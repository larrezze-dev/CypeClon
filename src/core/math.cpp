#include "core/math.hpp"

namespace cypeclon::core {

double round_decimals(double value, int decimals) noexcept {
    if (decimals < 0) return value;
    const double factor = std::pow(10.0, decimals);
    return std::round(value * factor) / factor;
}

double round_decimals_floor(double value, int decimals) noexcept {
    if (decimals < 0) return value;
    const double factor = std::pow(10.0, decimals);
    return std::floor(value * factor + 1.0e-12) / factor;
}

double round_decimals_ceil(double value, int decimals) noexcept {
    if (decimals < 0) return value;
    const double factor = std::pow(10.0, decimals);
    return std::ceil(value * factor - 1.0e-12) / factor;
}

double round_significant(double value, int sig_digits) noexcept {
    if (value == 0.0 || sig_digits <= 0) return 0.0;
    const double mag = std::floor(std::log10(std::abs(value)));
    const double factor = std::pow(10.0, sig_digits - 1 - static_cast<int>(mag));
    return std::round(value * factor) / factor;
}

Vec3d Vec3d::rotated_around_axis(const Vec3d& axis_unit, double angle_rad) const noexcept {
    const Vec3d k = axis_unit.normalized();
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    return (*this) * c + k.cross(*this) * s + k * (k.dot(*this) * (1.0 - c));
}

Mat3d Mat3d::rotation_x(double rad) noexcept {
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    Mat3d r;
    r.m[0] = {1.0, 0.0, 0.0};
    r.m[1] = {0.0,   c,  -s};
    r.m[2] = {0.0,   s,   c};
    return r;
}

Mat3d Mat3d::rotation_y(double rad) noexcept {
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    Mat3d r;
    r.m[0] = {  c, 0.0,   s};
    r.m[1] = {0.0, 1.0, 0.0};
    r.m[2] = { -s, 0.0,   c};
    return r;
}

Mat3d Mat3d::rotation_z(double rad) noexcept {
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    Mat3d r;
    r.m[0] = {  c,  -s, 0.0};
    r.m[1] = {  s,   c, 0.0};
    r.m[2] = {0.0, 0.0, 1.0};
    return r;
}

Mat3d Mat3d::rotation_axis(const Vec3d& axis, double rad) noexcept {
    const Vec3d u = axis.normalized();
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    const double t = 1.0 - c;
    Mat3d r;
    r.m[0] = {t * u.x * u.x + c,       t * u.x * u.y - s * u.z, t * u.x * u.z + s * u.y};
    r.m[1] = {t * u.x * u.y + s * u.z, t * u.y * u.y + c,       t * u.y * u.z - s * u.x};
    r.m[2] = {t * u.x * u.z - s * u.y, t * u.y * u.z + s * u.x, t * u.z * u.z + c};
    return r;
}

std::optional<Mat3d> Mat3d::inverted(double tol) const noexcept {
    const double det = determinant();
    if (std::abs(det) <= tol) {
        return std::nullopt;
    }
    const double inv_det = 1.0 / det;
    Mat3d inv;
    inv.m[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inv_det;
    inv.m[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * inv_det;
    inv.m[0][2] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv_det;

    inv.m[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * inv_det;
    inv.m[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv_det;
    inv.m[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * inv_det;

    inv.m[2][0] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inv_det;
    inv.m[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * inv_det;
    inv.m[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv_det;
    return inv;
}

Mat4d Mat4d::look_at(const Vec3d& eye, const Vec3d& target, const Vec3d& up) noexcept {
    const Vec3d f = (target - eye).normalized();
    const Vec3d s = f.cross(up).normalized();
    const Vec3d u = s.cross(f);

    Mat4d res = Mat4d::identity();
    res.m[0][0] =  s.x; res.m[0][1] =  s.y; res.m[0][2] =  s.z; res.m[0][3] = -s.dot(eye);
    res.m[1][0] =  u.x; res.m[1][1] =  u.y; res.m[1][2] =  u.z; res.m[1][3] = -u.dot(eye);
    res.m[2][0] = -f.x; res.m[2][1] = -f.y; res.m[2][2] = -f.z; res.m[2][3] =  f.dot(eye);
    return res;
}

Mat4d Mat4d::ortho(double left, double right, double bottom, double top, double z_near, double z_far) noexcept {
    Mat4d res = Mat4d::identity();
    res.m[0][0] =  2.0 / (right - left);
    res.m[1][1] =  2.0 / (top - bottom);
    res.m[2][2] = -2.0 / (z_far - z_near);
    res.m[0][3] = -(right + left) / (right - left);
    res.m[1][3] = -(top + bottom) / (top - bottom);
    res.m[2][3] = -(z_far + z_near) / (z_far - z_near);
    return res;
}

Mat4d Mat4d::perspective(double fov_y_rad, double aspect, double z_near, double z_far) noexcept {
    const double tan_half = std::tan(0.5 * fov_y_rad);
    Mat4d res = Mat4d::zero();
    res.m[0][0] =  1.0 / (aspect * tan_half);
    res.m[1][1] =  1.0 / tan_half;
    res.m[2][2] = -(z_far + z_near) / (z_far - z_near);
    res.m[2][3] = -(2.0 * z_far * z_near) / (z_far - z_near);
    res.m[3][2] = -1.0;
    return res;
}

std::optional<Mat4d> Mat4d::inverted(double tol) const noexcept {
    // Eliminación de Gauss-Jordan 4x4 con pivoteo parcial
    std::array<std::array<double, 8>, 4> aug{};
    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            aug[r][c] = m[r][c];
            aug[r][c + 4] = (r == c) ? 1.0 : 0.0;
        }
    }

    for (std::size_t col = 0; col < 4; ++col) {
        std::size_t pivot = col;
        double max_abs = std::abs(aug[col][col]);
        for (std::size_t r = col + 1; r < 4; ++r) {
            if (std::abs(aug[r][col]) > max_abs) {
                max_abs = std::abs(aug[r][col]);
                pivot = r;
            }
        }
        if (max_abs <= tol) {
            return std::nullopt;
        }
        if (pivot != col) {
            std::swap(aug[col], aug[pivot]);
        }
        const double inv_piv = 1.0 / aug[col][col];
        for (std::size_t c = 0; c < 8; ++c) {
            aug[col][c] *= inv_piv;
        }
        for (std::size_t r = 0; r < 4; ++r) {
            if (r == col) continue;
            const double factor = aug[r][col];
            for (std::size_t c = 0; c < 8; ++c) {
                aug[r][c] -= factor * aug[col][c];
            }
        }
    }

    Mat4d inv;
    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            inv.m[r][c] = aug[r][c + 4];
        }
    }
    return inv;
}

bool BBox3d::intersect_ray(const Vec3d& ray_origin, const Vec3d& ray_dir, double& t_min_out, double& t_max_out) const noexcept {
    if (!is_valid()) return false;
    double tmin = -std::numeric_limits<double>::infinity();
    double tmax =  std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < 3; ++i) {
        if (std::abs(ray_dir[i]) < 1.0e-12) {
            if (ray_origin[i] < min_pt[i] || ray_origin[i] > max_pt[i]) {
                return false;
            }
        } else {
            const double inv_d = 1.0 / ray_dir[i];
            double t1 = (min_pt[i] - ray_origin[i]) * inv_d;
            double t2 = (max_pt[i] - ray_origin[i]) * inv_d;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    t_min_out = tmin;
    t_max_out = tmax;
    return tmax >= 0.0;
}

Plane3d Plane3d::from_point_and_normal(const Vec3d& point, const Vec3d& normal_vec) noexcept {
    const Vec3d n = normal_vec.normalized();
    return Plane3d{n, -n.dot(point)};
}

std::optional<Plane3d> Plane3d::from_three_points(const Vec3d& p0, const Vec3d& p1, const Vec3d& p2, double tol) noexcept {
    const Vec3d cross = (p1 - p0).cross(p2 - p0);
    if (cross.norm() <= tol) {
        return std::nullopt;
    }
    return from_point_and_normal(p0, cross);
}

std::optional<Vec3d> Plane3d::intersect_line(const Vec3d& line_point, const Vec3d& line_dir, double tol) const noexcept {
    const double denom = normal.dot(line_dir);
    if (std::abs(denom) <= tol) {
        return std::nullopt;
    }
    const double t = -(normal.dot(line_point) + d) / denom;
    return line_point + line_dir * t;
}

std::optional<LocalAxes3d> LocalAxes3d::from_bar_endpoints(
    const Vec3d& nudo_inicial,
    const Vec3d& nudo_final,
    double roll_angle_rad,
    double tol) noexcept
{
    const Vec3d diff = nudo_final - nudo_inicial;
    const double len = diff.norm();
    if (len <= tol) {
        return std::nullopt;
    }

    const Vec3d u = diff / len;
    Vec3d v0;
    Vec3d w0;

    // Comprobar si la barra es vertical (paralela al eje Z global)
    const double horiz_len = std::hypot(u.x, u.y);
    if (horiz_len <= 1.0e-6) {
        // Barra vertical: u = (0, 0, +-1)
        // En CYPE 3D, para barras verticales hacia arriba (+Z), Y local = +Y global y Z local = -X global
        v0 = {0.0, 1.0, 0.0};
        w0 = u.cross(v0).normalized();
    } else {
        // Barra general: Y local horizontal perpendicular a u (v0 = Z_global x u normalizado, o tal que w0.z > 0)
        v0 = Vec3d{-u.y / horiz_len, u.x / horiz_len, 0.0};
        w0 = u.cross(v0).normalized();
    }

    LocalAxes3d axes;
    axes.origin = nudo_inicial;
    axes.u = u;
    if (std::abs(roll_angle_rad) > 1.0e-12) {
        const double c = std::cos(roll_angle_rad);
        const double s = std::sin(roll_angle_rad);
        axes.v = v0 * c + w0 * s;
        axes.w = -v0 * s + w0 * c;
    } else {
        axes.v = v0;
        axes.w = w0;
    }
    return axes;
}

std::optional<LocalAxes3d> LocalAxes3d::from_three_points(
    const Vec3d& p0,
    const Vec3d& p1,
    const Vec3d& p2,
    double tol) noexcept
{
    const Vec3d d1 = p1 - p0;
    const double len1 = d1.norm();
    if (len1 <= tol) {
        return std::nullopt;
    }
    const Vec3d u = d1 / len1;
    const Vec3d normal = d1.cross(p2 - p0);
    const double n_len = normal.norm();
    if (n_len <= tol * len1) {
        return std::nullopt;
    }
    const Vec3d w = normal / n_len;
    const Vec3d v = w.cross(u).normalized();
    return LocalAxes3d{p0, u, v, w};
}

SegmentIntersection2d intersect_segments_2d(
    const Vec2d& a0, const Vec2d& a1,
    const Vec2d& b0, const Vec2d& b1,
    double tol) noexcept
{
    const Vec2d r = a1 - a0;
    const Vec2d s = b1 - b0;
    const double rxs = r.cross(s);
    const Vec2d qp = b0 - a0;

    if (std::abs(rxs) <= 1.0e-12) {
        return {};
    }

    const double t = qp.cross(s) / rxs;
    const double u = qp.cross(r) / rxs;

    const double r_len = r.norm();
    const double s_len = s.norm();
    const double t_tol = (r_len > 0.0) ? (tol / r_len) : tol;
    const double u_tol = (s_len > 0.0) ? (tol / s_len) : tol;

    if (t >= -t_tol && t <= 1.0 + t_tol && u >= -u_tol && u <= 1.0 + u_tol) {
        const double tc = std::clamp(t, 0.0, 1.0);
        return SegmentIntersection2d{true, a0 + r * tc, tc, std::clamp(u, 0.0, 1.0)};
    }
    return {};
}

double distance_point_to_segment_3d(const Vec3d& p, const Vec3d& s0, const Vec3d& s1, Vec3d* closest_pt) noexcept {
    const Vec3d ab = s1 - s0;
    const double len_sq = ab.norm_sq();
    double t = 0.0;
    if (len_sq > 1.0e-16) {
        t = std::clamp((p - s0).dot(ab) / len_sq, 0.0, 1.0);
    }
    const Vec3d proj = s0 + ab * t;
    if (closest_pt) {
        *closest_pt = proj;
    }
    return p.distance_to(proj);
}

bool are_collinear_3d(const Vec3d& p0, const Vec3d& p1, const Vec3d& p2, double tol) noexcept {
    Vec3d closest;
    const Vec3d d = p2 - p0;
    const double len = d.norm();
    if (len <= tol) {
        return p1.distance_to(p0) <= tol;
    }
    const double area2 = (p1 - p0).cross(p2 - p0).norm();
    return (area2 / len) <= tol;
}

std::vector<double> solve_quadratic(double a, double b, double c, double tol) {
    if (std::abs(a) <= tol) {
        if (std::abs(b) <= tol) return {};
        return {-c / b};
    }
    const double disc = b * b - 4.0 * a * c;
    if (disc < -tol) {
        return {};
    }
    if (std::abs(disc) <= tol) {
        return {-b / (2.0 * a)};
    }
    const double s = std::sqrt(std::max(0.0, disc));
    double r1 = (-b - s) / (2.0 * a);
    double r2 = (-b + s) / (2.0 * a);
    if (r1 > r2) std::swap(r1, r2);
    return {r1, r2};
}

} // namespace cypeclon::core

