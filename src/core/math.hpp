#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace cypeclon::core {

// ==============================================================================
// Constantes y Tolerancias Geométricas (cypemath.dll)
// ==============================================================================
inline constexpr double kPi          = 3.1415926535897932384626433832795;
inline constexpr double kTwoPi       = 2.0 * kPi;
inline constexpr double kHalfPi      = 0.5 * kPi;
inline constexpr double kDegToRad    = kPi / 180.0;
inline constexpr double kRadToDeg    = 180.0 / kPi;
inline constexpr double kDefaultEps  = 1.0e-7;
inline constexpr double kGeomTol     = 1.0e-5; // Tolerancia geométrica estándar CAD (0.01 mm en metros)
inline constexpr double kAngleTolRad = 1.0e-6;

// Predicados equivalentes a ES_CERO, ES_CERO_PREC, SON_IGUALES, SON_IGUALES_PREC, etc.
constexpr double cuad(double x) noexcept { return x * x; }
constexpr double cubo(double x) noexcept { return x * x * x; }

inline bool es_cero(double x, double tol = kDefaultEps) noexcept {
    return std::abs(x) <= tol;
}

inline bool son_iguales(double a, double b, double tol = kDefaultEps) noexcept {
    return std::abs(a - b) <= tol;
}

inline bool es_mayor(double a, double b, double tol = kDefaultEps) noexcept {
    return (a - b) > tol;
}

inline bool es_menor(double a, double b, double tol = kDefaultEps) noexcept {
    return (b - a) > tol;
}

inline bool es_mayor_igual(double a, double b, double tol = kDefaultEps) noexcept {
    return (a - b) >= -tol;
}

inline bool es_menor_igual(double a, double b, double tol = kDefaultEps) noexcept {
    return (b - a) >= -tol;
}

// Redondeo con precisión (prec_redondea_decimales, mt_redondea_a_cifras_significativas)
double round_decimals(double value, int decimals) noexcept;
double round_decimals_floor(double value, int decimals) noexcept;
double round_decimals_ceil(double value, int decimals) noexcept;
double round_significant(double value, int sig_digits) noexcept;

// ==============================================================================
// Vec2d — Vector y Punto 2D (cypemath.dll / geom2d.dll)
// ==============================================================================
struct Vec2d {
    double x{0.0};
    double y{0.0};

    constexpr Vec2d() noexcept = default;
    constexpr Vec2d(double x_, double y_) noexcept : x(x_), y(y_) {}

    constexpr Vec2d operator+(const Vec2d& rhs) const noexcept { return {x + rhs.x, y + rhs.y}; }
    constexpr Vec2d operator-(const Vec2d& rhs) const noexcept { return {x - rhs.x, y - rhs.y}; }
    constexpr Vec2d operator-() const noexcept { return {-x, -y}; }
    constexpr Vec2d operator*(double s) const noexcept { return {x * s, y * s}; }
    constexpr Vec2d operator/(double s) const noexcept { return {x / s, y / s}; }

    constexpr Vec2d& operator+=(const Vec2d& rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
    constexpr Vec2d& operator-=(const Vec2d& rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
    constexpr Vec2d& operator*=(double s) noexcept { x *= s; y *= s; return *this; }
    constexpr Vec2d& operator/=(double s) noexcept { x /= s; y /= s; return *this; }

    constexpr double dot(const Vec2d& rhs) const noexcept { return x * rhs.x + y * rhs.y; }
    constexpr double cross(const Vec2d& rhs) const noexcept { return x * rhs.y - y * rhs.x; }
    constexpr double norm_sq() const noexcept { return x * x + y * y; }
    double norm() const noexcept { return std::hypot(x, y); }

    Vec2d normalized(double tol = kDefaultEps) const noexcept {
        const double len = norm();
        return (len > tol) ? (*this / len) : Vec2d{0.0, 0.0};
    }

    constexpr Vec2d perp_ccw() const noexcept { return {-y, x}; }
    constexpr Vec2d perp_cw() const noexcept { return {y, -x}; }

    double distance_to(const Vec2d& other) const noexcept { return (*this - other).norm(); }
    constexpr double distance_sq_to(const Vec2d& other) const noexcept { return (*this - other).norm_sq(); }

    Vec2d rotated(double angle_rad, const Vec2d& origin = {0.0, 0.0}) const noexcept {
        const double c = std::cos(angle_rad);
        const double s = std::sin(angle_rad);
        const double dx = x - origin.x;
        const double dy = y - origin.y;
        return {origin.x + dx * c - dy * s, origin.y + dx * s + dy * c};
    }

    bool equals(const Vec2d& other, double tol = kGeomTol) const noexcept {
        return son_iguales(x, other.x, tol) && son_iguales(y, other.y, tol);
    }
};

constexpr Vec2d operator*(double s, const Vec2d& v) noexcept { return v * s; }

// ==============================================================================
// Vec3d — Vector y Punto 3D (cypemath.dll / c3d.dll)
// ==============================================================================
struct Vec3d {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    constexpr Vec3d() noexcept = default;
    constexpr Vec3d(double x_, double y_, double z_) noexcept : x(x_), y(y_), z(z_) {}

    static constexpr Vec3d zero() noexcept { return {0.0, 0.0, 0.0}; }
    static constexpr Vec3d unit_x() noexcept { return {1.0, 0.0, 0.0}; }
    static constexpr Vec3d unit_y() noexcept { return {0.0, 1.0, 0.0}; }
    static constexpr Vec3d unit_z() noexcept { return {0.0, 0.0, 1.0}; }

    constexpr double operator[](std::size_t idx) const noexcept {
        return (idx == 0) ? x : ((idx == 1) ? y : z);
    }
    constexpr double& operator[](std::size_t idx) noexcept {
        return (idx == 0) ? x : ((idx == 1) ? y : z);
    }

    constexpr Vec3d operator+(const Vec3d& rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    constexpr Vec3d operator-(const Vec3d& rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    constexpr Vec3d operator-() const noexcept { return {-x, -y, -z}; }
    constexpr Vec3d operator*(double s) const noexcept { return {x * s, y * s, z * s}; }
    constexpr Vec3d operator/(double s) const noexcept { return {x / s, y / s, z / s}; }

    constexpr Vec3d& operator+=(const Vec3d& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    constexpr Vec3d& operator-=(const Vec3d& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    constexpr Vec3d& operator*=(double s) noexcept { x *= s; y *= s; z *= s; return *this; }
    constexpr Vec3d& operator/=(double s) noexcept { x /= s; y /= s; z /= s; return *this; }

    constexpr double dot(const Vec3d& rhs) const noexcept { return x * rhs.x + y * rhs.y + z * rhs.z; }
    constexpr Vec3d cross(const Vec3d& rhs) const noexcept {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        };
    }

    constexpr double norm_sq() const noexcept { return x * x + y * y + z * z; }
    double norm() const noexcept { return std::sqrt(norm_sq()); }

    Vec3d normalized(double tol = kDefaultEps) const noexcept {
        const double len = norm();
        return (len > tol) ? (*this / len) : Vec3d{0.0, 0.0, 0.0};
    }

    double distance_to(const Vec3d& other) const noexcept { return (*this - other).norm(); }
    constexpr double distance_sq_to(const Vec3d& other) const noexcept { return (*this - other).norm_sq(); }

    bool equals(const Vec3d& other, double tol = kGeomTol) const noexcept {
        return son_iguales(x, other.x, tol) &&
               son_iguales(y, other.y, tol) &&
               son_iguales(z, other.z, tol);
    }

    bool is_parallel_to(const Vec3d& other, double tol = kAngleTolRad) const noexcept {
        const Vec3d u1 = normalized();
        const Vec3d u2 = other.normalized();
        if (u1.norm_sq() == 0.0 || u2.norm_sq() == 0.0) {
            return false;
        }
        return u1.cross(u2).norm() <= tol;
    }

    bool is_same_direction(const Vec3d& other, double tol = kAngleTolRad) const noexcept {
        return is_parallel_to(other, tol) && (dot(other) > 0.0);
    }

    // Fórmula de Rodrigues (mt_gira_v1_sobre_v2 en cypemath.dll)
    Vec3d rotated_around_axis(const Vec3d& axis_unit, double angle_rad) const noexcept;
};

constexpr Vec3d operator*(double s, const Vec3d& v) noexcept { return v * s; }

// ==============================================================================
// Mat3d — Matriz 3x3 (Rotaciones, Tensores de Inercia y Ejes Locales de Barras)
// ==============================================================================
struct Mat3d {
    // Almacenamiento por filas: m[row][col]
    std::array<std::array<double, 3>, 3> m{{
        {{1.0, 0.0, 0.0}},
        {{0.0, 1.0, 0.0}},
        {{0.0, 0.0, 1.0}}
    }};

    static constexpr Mat3d identity() noexcept { return Mat3d{}; }
    static constexpr Mat3d zero() noexcept {
        Mat3d res;
        res.m = {{{{0.0, 0.0, 0.0}}, {{0.0, 0.0, 0.0}}, {{0.0, 0.0, 0.0}}}};
        return res;
    }

    static constexpr Mat3d from_rows(const Vec3d& r0, const Vec3d& r1, const Vec3d& r2) noexcept {
        Mat3d res;
        res.m[0] = {r0.x, r0.y, r0.z};
        res.m[1] = {r1.x, r1.y, r1.z};
        res.m[2] = {r2.x, r2.y, r2.z};
        return res;
    }

    static constexpr Mat3d from_cols(const Vec3d& c0, const Vec3d& c1, const Vec3d& c2) noexcept {
        Mat3d res;
        res.m[0] = {c0.x, c1.x, c2.x};
        res.m[1] = {c0.y, c1.y, c2.y};
        res.m[2] = {c0.z, c1.z, c2.z};
        return res;
    }

    static Mat3d rotation_x(double rad) noexcept;
    static Mat3d rotation_y(double rad) noexcept;
    static Mat3d rotation_z(double rad) noexcept;
    static Mat3d rotation_axis(const Vec3d& axis, double rad) noexcept;

    constexpr double operator()(std::size_t r, std::size_t c) const noexcept { return m[r][c]; }
    constexpr double& operator()(std::size_t r, std::size_t c) noexcept { return m[r][c]; }

    constexpr Vec3d row(std::size_t r) const noexcept { return {m[r][0], m[r][1], m[r][2]}; }
    constexpr Vec3d col(std::size_t c) const noexcept { return {m[0][c], m[1][c], m[2][c]}; }

    constexpr Mat3d transposed() const noexcept {
        Mat3d t;
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 3; ++c) {
                t.m[r][c] = m[c][r];
            }
        }
        return t;
    }

    constexpr double determinant() const noexcept {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
               m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
               m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    std::optional<Mat3d> inverted(double tol = 1.0e-12) const noexcept;

    constexpr Vec3d operator*(const Vec3d& v) const noexcept {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }

    constexpr Mat3d operator*(const Mat3d& rhs) const noexcept {
        Mat3d res = Mat3d::zero();
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 3; ++c) {
                res.m[r][c] = m[r][0] * rhs.m[0][c] +
                              m[r][1] * rhs.m[1][c] +
                              m[r][2] * rhs.m[2][c];
            }
        }
        return res;
    }
};

// ==============================================================================
// Mat4d — Matriz Homogénea 4x4 (Transformaciones Espaciales 3D y Cámaras OpenGL)
// ==============================================================================
struct Mat4d {
    std::array<std::array<double, 4>, 4> m{{
        {{1.0, 0.0, 0.0, 0.0}},
        {{0.0, 1.0, 0.0, 0.0}},
        {{0.0, 0.0, 1.0, 0.0}},
        {{0.0, 0.0, 0.0, 1.0}}
    }};

    static constexpr Mat4d identity() noexcept { return Mat4d{}; }
    static constexpr Mat4d zero() noexcept {
        Mat4d res;
        for (auto& r : res.m) r = {0.0, 0.0, 0.0, 0.0};
        return res;
    }

    static constexpr Mat4d translation(const Vec3d& t) noexcept {
        Mat4d res;
        res.m[0][3] = t.x;
        res.m[1][3] = t.y;
        res.m[2][3] = t.z;
        return res;
    }

    static constexpr Mat4d scaling(const Vec3d& s) noexcept {
        Mat4d res;
        res.m[0][0] = s.x;
        res.m[1][1] = s.y;
        res.m[2][2] = s.z;
        return res;
    }

    static constexpr Mat4d from_rotation_translation(const Mat3d& rot, const Vec3d& t) noexcept {
        Mat4d res;
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 3; ++c) {
                res.m[r][c] = rot.m[r][c];
            }
        }
        res.m[0][3] = t.x;
        res.m[1][3] = t.y;
        res.m[2][3] = t.z;
        return res;
    }

    static Mat4d look_at(const Vec3d& eye, const Vec3d& target, const Vec3d& up) noexcept;
    static Mat4d ortho(double left, double right, double bottom, double top, double z_near, double z_far) noexcept;
    static Mat4d perspective(double fov_y_rad, double aspect, double z_near, double z_far) noexcept;

    constexpr double operator()(std::size_t r, std::size_t c) const noexcept { return m[r][c]; }
    constexpr double& operator()(std::size_t r, std::size_t c) noexcept { return m[r][c]; }

    constexpr Mat4d operator*(const Mat4d& rhs) const noexcept {
        Mat4d res = Mat4d::zero();
        for (std::size_t r = 0; r < 4; ++r) {
            for (std::size_t c = 0; c < 4; ++c) {
                res.m[r][c] = m[r][0] * rhs.m[0][c] +
                              m[r][1] * rhs.m[1][c] +
                              m[r][2] * rhs.m[2][c] +
                              m[r][3] * rhs.m[3][c];
            }
        }
        return res;
    }

    constexpr Vec3d transform_point(const Vec3d& p) const noexcept {
        const double x = m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3];
        const double y = m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3];
        const double z = m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3];
        const double w = m[3][0] * p.x + m[3][1] * p.y + m[3][2] * p.z + m[3][3];
        if (w != 0.0 && w != 1.0) {
            return {x / w, y / w, z / w};
        }
        return {x, y, z};
    }

    constexpr Vec3d transform_vector(const Vec3d& v) const noexcept {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }

    std::optional<Mat4d> inverted(double tol = 1.0e-12) const noexcept;
};

// ==============================================================================
// BBox2d & BBox3d — Cajas Envolventes 2D y 3D (CAD / Octree ebaoctree)
// ==============================================================================
struct BBox2d {
    Vec2d min_pt{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    Vec2d max_pt{-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};

    constexpr BBox2d() noexcept = default;
    constexpr BBox2d(const Vec2d& pmin, const Vec2d& pmax) noexcept : min_pt(pmin), max_pt(pmax) {}

    constexpr bool is_valid() const noexcept { return min_pt.x <= max_pt.x && min_pt.y <= max_pt.y; }

    void expand(const Vec2d& p) noexcept {
        min_pt.x = std::min(min_pt.x, p.x);
        min_pt.y = std::min(min_pt.y, p.y);
        max_pt.x = std::max(max_pt.x, p.x);
        max_pt.y = std::max(max_pt.y, p.y);
    }

    void expand(const BBox2d& other) noexcept {
        if (!other.is_valid()) return;
        expand(other.min_pt);
        expand(other.max_pt);
    }

    constexpr Vec2d center() const noexcept { return (min_pt + max_pt) * 0.5; }
    constexpr Vec2d size() const noexcept { return max_pt - min_pt; }
    constexpr double area() const noexcept {
        if (!is_valid()) return 0.0;
        const Vec2d s = size();
        return s.x * s.y;
    }

    constexpr bool contains(const Vec2d& p, double tol = kGeomTol) const noexcept {
        return is_valid() &&
               p.x >= min_pt.x - tol && p.x <= max_pt.x + tol &&
               p.y >= min_pt.y - tol && p.y <= max_pt.y + tol;
    }

    constexpr bool intersects(const BBox2d& other, double tol = kGeomTol) const noexcept {
        if (!is_valid() || !other.is_valid()) return false;
        return !(max_pt.x + tol < other.min_pt.x || min_pt.x - tol > other.max_pt.x ||
                 max_pt.y + tol < other.min_pt.y || min_pt.y - tol > other.max_pt.y);
    }
};

struct BBox3d {
    Vec3d min_pt{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };
    Vec3d max_pt{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    constexpr BBox3d() noexcept = default;
    constexpr BBox3d(const Vec3d& pmin, const Vec3d& pmax) noexcept : min_pt(pmin), max_pt(pmax) {}

    constexpr bool is_valid() const noexcept {
        return min_pt.x <= max_pt.x && min_pt.y <= max_pt.y && min_pt.z <= max_pt.z;
    }

    void expand(const Vec3d& p) noexcept {
        min_pt.x = std::min(min_pt.x, p.x);
        min_pt.y = std::min(min_pt.y, p.y);
        min_pt.z = std::min(min_pt.z, p.z);
        max_pt.x = std::max(max_pt.x, p.x);
        max_pt.y = std::max(max_pt.y, p.y);
        max_pt.z = std::max(max_pt.z, p.z);
    }

    void expand(const BBox3d& other) noexcept {
        if (!other.is_valid()) return;
        expand(other.min_pt);
        expand(other.max_pt);
    }

    constexpr Vec3d center() const noexcept { return (min_pt + max_pt) * 0.5; }
    constexpr Vec3d size() const noexcept { return max_pt - min_pt; }
    constexpr double volume() const noexcept {
        if (!is_valid()) return 0.0;
        const Vec3d s = size();
        return s.x * s.y * s.z;
    }

    constexpr bool contains(const Vec3d& p, double tol = kGeomTol) const noexcept {
        return is_valid() &&
               p.x >= min_pt.x - tol && p.x <= max_pt.x + tol &&
               p.y >= min_pt.y - tol && p.y <= max_pt.y + tol &&
               p.z >= min_pt.z - tol && p.z <= max_pt.z + tol;
    }

    constexpr bool intersects(const BBox3d& other, double tol = kGeomTol) const noexcept {
        if (!is_valid() || !other.is_valid()) return false;
        return !(max_pt.x + tol < other.min_pt.x || min_pt.x - tol > other.max_pt.x ||
                 max_pt.y + tol < other.min_pt.y || min_pt.y - tol > other.max_pt.y ||
                 max_pt.z + tol < other.min_pt.z || min_pt.z - tol > other.max_pt.z);
    }

    // Intersección Rayo-AABB para selección 3D (picking en área de trabajo)
    bool intersect_ray(const Vec3d& ray_origin, const Vec3d& ray_dir, double& t_min_out, double& t_max_out) const noexcept;
};

// ==============================================================================
// Plane3d & LocalAxes3d — Planos 3D y Ejes Locales (basicos.dll!ejes2d / cypemath.dll)
// ==============================================================================
struct Plane3d {
    // Ecuación implícita A*x + B*y + C*z + D = 0 con (A, B, C) unitario
    Vec3d normal{0.0, 0.0, 1.0};
    double d{0.0};

    static Plane3d from_point_and_normal(const Vec3d& point, const Vec3d& normal_vec) noexcept;
    static std::optional<Plane3d> from_three_points(const Vec3d& p0, const Vec3d& p1, const Vec3d& p2, double tol = kGeomTol) noexcept;

    // mt_distancia_con_signo_punto_a_plano
    constexpr double signed_distance(const Vec3d& p) const noexcept {
        return normal.dot(p) + d;
    }

    constexpr Vec3d project_point(const Vec3d& p) const noexcept {
        return p - normal * signed_distance(p);
    }

    // mt_existe_interseccion_recta_con_plano
    std::optional<Vec3d> intersect_line(const Vec3d& line_point, const Vec3d& line_dir, double tol = kDefaultEps) const noexcept;
};

/**
 * @brief Sistema de coordenadas local en 3D equivalente a ejes2d (basicos.dll) y ejes de barra 3D (barra3d.dll).
 * Convención de CYPE 3D para barras:
 * - Eje X local (u): de nudo inicial a nudo final.
 * - Para barras no verticales, el eje Z local (w) está en el plano vertical que contiene a la barra (con componente Z global > 0),
 *   y el eje Y local (v = w x u) es horizontal; luego se aplica el ángulo de giro de sección (roll_angle_rad).
 * - Para barras verticales (paralelas a Z global), por defecto el eje Y local apunta según +Y global (o -Y según sentido) antes del giro.
 */
struct LocalAxes3d {
    Vec3d origin{0.0, 0.0, 0.0};
    Vec3d u{1.0, 0.0, 0.0}; // Eje X local
    Vec3d v{0.0, 1.0, 0.0}; // Eje Y local
    Vec3d w{0.0, 0.0, 1.0}; // Eje Z local

    // Construye los ejes locales de una barra 3D dados sus dos extremos y el ángulo de giro (gamma en radianes)
    static std::optional<LocalAxes3d> from_bar_endpoints(
        const Vec3d& nudo_inicial,
        const Vec3d& nudo_final,
        double roll_angle_rad = 0.0,
        double tol = kGeomTol) noexcept;

    // Construye un plano de trabajo 2D en 3D a partir de 3 puntos no colineales (ejes2d_ejes_con_tres_puntos)
    static std::optional<LocalAxes3d> from_three_points(
        const Vec3d& p0,
        const Vec3d& p1,
        const Vec3d& p2,
        double tol = kGeomTol) noexcept;

    // Matriz de rotación 3x3 cuyas filas son (u, v, w): v_local = R * v_global
    constexpr Mat3d global_to_local_rotation() const noexcept {
        return Mat3d::from_rows(u, v, w);
    }

    // Matriz de rotación 3x3 cuyas columnas son (u, v, w): v_global = R^T * v_local
    constexpr Mat3d local_to_global_rotation() const noexcept {
        return Mat3d::from_cols(u, v, w);
    }

    // Transformaciones de puntos y vectores (ejes2d_local_a_global, ejes2d_coordenada_2D_a_3D, etc.)
    constexpr Vec3d point_local_to_global(const Vec3d& local_pt) const noexcept {
        return origin + u * local_pt.x + v * local_pt.y + w * local_pt.z;
    }

    constexpr Vec3d point_global_to_local(const Vec3d& global_pt) const noexcept {
        const Vec3d d = global_pt - origin;
        return {d.dot(u), d.dot(v), d.dot(w)};
    }

    constexpr Vec3d point_2d_to_3d(const Vec2d& pt2d) const noexcept {
        return origin + u * pt2d.x + v * pt2d.y;
    }

    constexpr Vec2d point_3d_to_2d(const Vec3d& pt3d) const noexcept {
        const Vec3d d = pt3d - origin;
        return {d.dot(u), d.dot(v)};
    }
};

// ==============================================================================
// Algoritmos Geométricos 2D/3D Adicionales (cypemath.dll)
// ==============================================================================
struct SegmentIntersection2d {
    bool intersects{false};
    Vec2d point{0.0, 0.0};
    double t_first{0.0};
    double t_second{0.0};
};

// mt_inter_segmentos_con_extremos_con_precision_dada
SegmentIntersection2d intersect_segments_2d(
    const Vec2d& a0, const Vec2d& a1,
    const Vec2d& b0, const Vec2d& b1,
    double tol = kGeomTol) noexcept;

// mt_distancia_a_segmento / mt_proyecta_punto_sobre_recta_3D
double distance_point_to_segment_3d(const Vec3d& p, const Vec3d& s0, const Vec3d& s1, Vec3d* closest_pt = nullptr) noexcept;

// mt_son_tres_puntos3D_en_linea
bool are_collinear_3d(const Vec3d& p0, const Vec3d& p1, const Vec3d& p2, double tol = kGeomTol) noexcept;

// mt_raices_ecuacion_segundo_grado (a*x^2 + b*x + c = 0)
std::vector<double> solve_quadratic(double a, double b, double c, double tol = kDefaultEps);

} // namespace cypeclon::core

