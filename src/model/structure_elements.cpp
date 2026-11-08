#include "model/structure_elements.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cypeclon::model {

// ============================================================================
// P-06.1: DatosVinculacionExteriorNudo (`esport3d_*`)
// ============================================================================

DatosVinculacionExteriorNudo DatosVinculacionExteriorNudo::from_tipo(
    VinculacionExterior tipo,
    bool coaccionar_giros
) noexcept {
    DatosVinculacionExteriorNudo d{};
    d.giros_coaccionados = coaccionar_giros;

    switch (tipo) {
        case VinculacionExterior::Libre:
            d.gdl_coaccionados = {false, false, false, false, false, false};
            break;
        case VinculacionExterior::ArticuladoFijo:
            d.gdl_coaccionados = {true, true, true, false, false, false};
            break;
        case VinculacionExterior::Empotrado:
            d.gdl_coaccionados = {true, true, true, true, true, true};
            d.giros_coaccionados = true;
            break;
        case VinculacionExterior::ApoyoElastico:
            d.gdl_coaccionados = {false, false, false, false, false, false};
            d.kx_kn_m = 10000.0;
            d.ky_kn_m = 10000.0;
            d.kz_kn_m = 50000.0;
            break;
        case VinculacionExterior::DeslizaderaPlanoXY:
            d.gdl_coaccionados = {false, false, true, coaccionar_giros, coaccionar_giros, coaccionar_giros};
            d.vector_direccion_o_normal = {0.0, 0.0, 1.0};
            break;
        case VinculacionExterior::DeslizaderaPlanoXZ:
            d.gdl_coaccionados = {false, true, false, coaccionar_giros, coaccionar_giros, coaccionar_giros};
            d.vector_direccion_o_normal = {0.0, 1.0, 0.0};
            break;
        case VinculacionExterior::DeslizaderaPlanoYZ:
            d.gdl_coaccionados = {true, false, false, coaccionar_giros, coaccionar_giros, coaccionar_giros};
            d.vector_direccion_o_normal = {1.0, 0.0, 0.0};
            break;
        case VinculacionExterior::DeslizaderaPlanoInclinado:
            d.gdl_coaccionados = {false, false, true, coaccionar_giros, coaccionar_giros, coaccionar_giros};
            d.vector_direccion_o_normal = core::Vec3d{0.0, 0.70710678, 0.70710678}.normalized();
            break;
        case VinculacionExterior::DeslizaderaRectaLibre:
            d.gdl_coaccionados = {false, true, true, coaccionar_giros, coaccionar_giros, coaccionar_giros};
            d.vector_direccion_o_normal = {1.0, 0.0, 0.0};
            break;
        case VinculacionExterior::ApoyoNoLinealSoloCompresion:
            d.gdl_coaccionados = {true, true, true, coaccionar_giros, coaccionar_giros, coaccionar_giros};
            d.solo_compresion = true;
            d.solo_traccion = false;
            d.sentido_apoyo_no_lineal = {0.0, 0.0, 1.0};
            break;
        case VinculacionExterior::ApoyoNoLinealSoloTraccion:
            d.gdl_coaccionados = {true, true, true, coaccionar_giros, coaccionar_giros, coaccionar_giros};
            d.solo_compresion = false;
            d.solo_traccion = true;
            d.sentido_apoyo_no_lineal = {0.0, 0.0, -1.0};
            break;
        case VinculacionExterior::CoaccionGenericaPorGDL:
            d.gdl_coaccionados = {true, true, true, coaccionar_giros, coaccionar_giros, coaccionar_giros};
            break;
    }
    return d;
}

// ============================================================================
// P-06.2: ProfileLibraryManager (`bibperf3.dll`, `seccion.dll`, `gesserperf.dll`)
// ============================================================================

ProfileSectionProperties ProfileLibraryManager::compute_double_t(
    std::string_view ref,
    std::string_view serie,
    double h_mm,
    double b_mm,
    double tw_mm,
    double tf_mm,
    double r_mm,
    FamiliaMaterialPerfil familia,
    double density_kg_m3
) noexcept {
    ProfileSectionProperties p{};
    p.referencia = std::string(ref);
    p.serie = std::string(serie);
    p.familia = familia;
    p.tipo_forma = GeometriaSeccionTipo::DobleT;
    p.h_mm = h_mm;
    p.b_mm = b_mm;
    p.tw_mm = tw_mm;
    p.tf_mm = tf_mm;
    p.r_mm = r_mm;

    // Conversión a cm para las constantes de catálogo
    const double h = h_mm * 0.1;
    const double b = b_mm * 0.1;
    const double tw = tw_mm * 0.1;
    const double tf = tf_mm * 0.1;
    const double r = r_mm * 0.1;
    const double hw = std::max(0.0, h - 2.0 * tf);

    // Área de las 4 esquinas de acuerdo r^2*(4 - pi)
    const double a_fillets = (4.0 - std::numbers::pi) * r * r;
    p.area_cm2 = 2.0 * b * tf + hw * tw + a_fillets;

    // Áreas de cortante según EN 1993-1-1 §6.2.6(3)
    p.avz_cm2 = std::max(hw * tw, p.area_cm2 - 2.0 * b * tf + (tw + 2.0 * r) * tf);
    p.avy_cm2 = 2.0 * b * tf;

    // Inercias respecto a ejes principales Y (fuerte) y Z (débil)
    const double iy_rect = (b * h * h * h - (b - tw) * hw * hw * hw) / 12.0;
    const double d_fillet_y = std::max(0.0, 0.5 * hw - 0.2234 * r);
    const double iy_fillets = a_fillets * d_fillet_y * d_fillet_y;
    p.iy_cm4 = iy_rect + iy_fillets;

    const double iz_rect = (2.0 * tf * b * b * b + hw * tw * tw * tw) / 12.0;
    const double d_fillet_z = 0.5 * tw + 0.2234 * r;
    p.iz_cm4 = iz_rect + a_fillets * d_fillet_z * d_fillet_z;

    // Constante de torsión de Saint-Venant It (cm^4, incluyendo nudos de acuerdo alma-ala) y alabeo Iw (cm^6)
    const double d_j = tf + 0.35 * r;
    p.it_cm4 = (2.0 * b * tf * tf * tf + hw * tw * tw * tw) / 3.0 + 2.0 * 0.21 * (d_j * d_j * d_j * d_j);
    const double hs = h - tf;
    p.iw_cm6 = (p.iz_cm4 * hs * hs) * 0.25;

    // Módulos resistentes elásticos y plásticos
    p.wely_cm3 = (h > 1e-6) ? (2.0 * p.iy_cm4 / h) : 0.0;
    p.welz_cm3 = (b > 1e-6) ? (2.0 * p.iz_cm4 / b) : 0.0;
    p.wply_cm3 = b * tf * (h - tf) + 0.25 * tw * hw * hw + a_fillets * d_fillet_y;
    p.wplz_cm3 = 0.5 * tf * b * b + 0.25 * hw * tw * tw;

    p.ry_cm = (p.area_cm2 > 1e-6) ? std::sqrt(p.iy_cm4 / p.area_cm2) : 0.0;
    p.rz_cm = (p.area_cm2 > 1e-6) ? std::sqrt(p.iz_cm4 / p.area_cm2) : 0.0;
    p.peso_kg_m = p.area_m2() * density_kg_m3;

    const double perim_m = (2.0 * h_mm + 4.0 * b_mm - 2.0 * tw_mm) * 1.0e-3;
    p.masividad_am_v_m_inv = (p.area_m2() > 1e-9) ? (perim_m / p.area_m2()) : 200.0;
    return p;
}

ProfileSectionProperties ProfileLibraryManager::compute_channel_upn(
    std::string_view ref,
    std::string_view serie,
    double h_mm,
    double b_mm,
    double tw_mm,
    double tf_mm,
    double r_mm
) noexcept {
    ProfileSectionProperties p = compute_double_t(ref, serie, h_mm, b_mm, tw_mm, tf_mm, r_mm);
    p.tipo_forma = GeometriaSeccionTipo::CanalUPN;
    return p;
}

ProfileSectionProperties ProfileLibraryManager::compute_angle_l(
    std::string_view ref,
    std::string_view serie,
    double h_mm,
    double b_mm,
    double t_mm,
    double r_mm
) noexcept {
    ProfileSectionProperties p{};
    p.referencia = std::string(ref);
    p.serie = std::string(serie);
    p.familia = FamiliaMaterialPerfil::AceroLaminado;
    p.tipo_forma = ( std::abs(h_mm - b_mm) < 1e-3 )
        ? GeometriaSeccionTipo::AngularSimetrico
        : GeometriaSeccionTipo::AngularAsimetrico;
    p.h_mm = h_mm;
    p.b_mm = b_mm;
    p.tw_mm = t_mm;
    p.tf_mm = t_mm;
    p.r_mm = r_mm;

    const double h = h_mm * 0.1;
    const double b = b_mm * 0.1;
    const double t = t_mm * 0.1;

    const double a1 = h * t;
    const double a2 = (b - t) * t;
    p.area_cm2 = a1 + a2;
    p.avy_cm2 = b * t;
    p.avz_cm2 = h * t;

    const double zc = (a1 * (0.5 * h) + a2 * (0.5 * t)) / std::max(1e-6, p.area_cm2);
    const double yc = (a1 * (0.5 * t) + a2 * (t + 0.5 * (b - t))) / std::max(1e-6, p.area_cm2);

    const double iy1 = (t * h * h * h) / 12.0 + a1 * (0.5 * h - zc) * (0.5 * h - zc);
    const double iy2 = ((b - t) * t * t * t) / 12.0 + a2 * (0.5 * t - zc) * (0.5 * t - zc);
    p.iy_cm4 = iy1 + iy2;

    const double iz1 = (h * t * t * t) / 12.0 + a1 * (0.5 * t - yc) * (0.5 * t - yc);
    const double iz2 = (t * (b - t) * (b - t) * (b - t)) / 12.0 + a2 * (t + 0.5 * (b - t) - yc) * (t + 0.5 * (b - t) - yc);
    p.iz_cm4 = iz1 + iz2;

    p.it_cm4 = ((h + b - t) * t * t * t) / 3.0;
    p.iw_cm6 = 0.0;
    p.wely_cm3 = p.iy_cm4 / std::max(zc, h - zc);
    p.welz_cm3 = p.iz_cm4 / std::max(yc, b - yc);
    p.wply_cm3 = 1.5 * p.wely_cm3;
    p.wplz_cm3 = 1.5 * p.welz_cm3;
    p.ry_cm = std::sqrt(p.iy_cm4 / std::max(1e-6, p.area_cm2));
    p.rz_cm = std::sqrt(p.iz_cm4 / std::max(1e-6, p.area_cm2));
    p.peso_kg_m = p.area_m2() * 7850.0;
    const double perim_m = 2.0 * (h_mm + b_mm) * 1.0e-3;
    p.masividad_am_v_m_inv = perim_m / std::max(1e-9, p.area_m2());
    return p;
}

ProfileSectionProperties ProfileLibraryManager::compute_hollow_rect(
    std::string_view ref,
    std::string_view serie,
    double h_mm,
    double b_mm,
    double t_mm,
    FamiliaMaterialPerfil familia,
    double density_kg_m3
) noexcept {
    ProfileSectionProperties p{};
    p.referencia = std::string(ref);
    p.serie = std::string(serie);
    p.familia = familia;
    p.tipo_forma = (std::abs(h_mm - b_mm) < 1e-3)
        ? GeometriaSeccionTipo::TuboCuadradoSHS
        : GeometriaSeccionTipo::TuboRectangularRHS;
    p.h_mm = h_mm;
    p.b_mm = b_mm;
    p.tw_mm = t_mm;
    p.tf_mm = t_mm;
    p.r_mm = 1.5 * t_mm;

    const double h = h_mm * 0.1;
    const double b = b_mm * 0.1;
    const double t = t_mm * 0.1;
    const double hi = std::max(0.0, h - 2.0 * t);
    const double bi = std::max(0.0, b - 2.0 * t);

    p.area_cm2 = b * h - bi * hi;
    p.avz_cm2 = p.area_cm2 * h / std::max(1e-6, b + h);
    p.avy_cm2 = p.area_cm2 * b / std::max(1e-6, b + h);

    p.iy_cm4 = (b * h * h * h - bi * hi * hi * hi) / 12.0;
    p.iz_cm4 = (h * b * b * b - hi * bi * bi * bi) / 12.0;

    // Fórmula de Bredt para sección tubular cerrada
    const double am = (b - t) * (h - t);
    const double s_perim = 2.0 * ((b - t) + (h - t));
    p.it_cm4 = (s_perim > 1e-6) ? (4.0 * am * am * t / s_perim) : 0.0;
    p.iw_cm6 = 0.0;

    p.wely_cm3 = 2.0 * p.iy_cm4 / std::max(1e-6, h);
    p.welz_cm3 = 2.0 * p.iz_cm4 / std::max(1e-6, b);
    p.wply_cm3 = (b * h * h - bi * hi * hi) * 0.25;
    p.wplz_cm3 = (h * b * b - hi * bi * bi) * 0.25;

    p.ry_cm = std::sqrt(p.iy_cm4 / std::max(1e-6, p.area_cm2));
    p.rz_cm = std::sqrt(p.iz_cm4 / std::max(1e-6, p.area_cm2));
    p.peso_kg_m = p.area_m2() * density_kg_m3;
    const double perim_ext_m = 2.0 * (b_mm + h_mm) * 1.0e-3;
    p.masividad_am_v_m_inv = perim_ext_m / std::max(1e-9, p.area_m2());
    return p;
}

ProfileSectionProperties ProfileLibraryManager::compute_hollow_circ(
    std::string_view ref,
    std::string_view serie,
    double d_ext_mm,
    double t_mm,
    FamiliaMaterialPerfil familia,
    double density_kg_m3
) noexcept {
    ProfileSectionProperties p{};
    p.referencia = std::string(ref);
    p.serie = std::string(serie);
    p.familia = familia;
    p.tipo_forma = GeometriaSeccionTipo::TuboCircularCHS;
    p.h_mm = d_ext_mm;
    p.b_mm = d_ext_mm;
    p.tw_mm = t_mm;
    p.tf_mm = t_mm;

    const double d = d_ext_mm * 0.1;
    const double di = std::max(0.0, (d_ext_mm - 2.0 * t_mm) * 0.1);

    p.area_cm2 = 0.25 * std::numbers::pi * (d * d - di * di);
    p.avy_cm2 = (2.0 / std::numbers::pi) * p.area_cm2;
    p.avz_cm2 = p.avy_cm2;

    p.iy_cm4 = (std::numbers::pi / 64.0) * (d * d * d * d - di * di * di * di);
    p.iz_cm4 = p.iy_cm4;
    p.it_cm4 = 2.0 * p.iy_cm4;
    p.iw_cm6 = 0.0;

    p.wely_cm3 = 2.0 * p.iy_cm4 / std::max(1e-6, d);
    p.welz_cm3 = p.wely_cm3;
    p.wply_cm3 = (d * d * d - di * di * di) / 6.0;
    p.wplz_cm3 = p.wply_cm3;

    p.ry_cm = std::sqrt(p.iy_cm4 / std::max(1e-6, p.area_cm2));
    p.rz_cm = p.ry_cm;
    p.peso_kg_m = p.area_m2() * density_kg_m3;
    const double perim_ext_m = std::numbers::pi * d_ext_mm * 1.0e-3;
    p.masividad_am_v_m_inv = perim_ext_m / std::max(1e-9, p.area_m2());
    return p;
}

ProfileSectionProperties ProfileLibraryManager::compute_cold_formed(
    std::string_view ref,
    std::string_view serie,
    GeometriaSeccionTipo shape,
    double h_mm,
    double b_mm,
    double lip_c_mm,
    double t_mm
) noexcept {
    ProfileSectionProperties p{};
    p.referencia = std::string(ref);
    p.serie = std::string(serie);
    p.familia = FamiliaMaterialPerfil::AceroConformado;
    p.tipo_forma = shape;
    p.h_mm = h_mm;
    p.b_mm = b_mm;
    p.tw_mm = t_mm;
    p.tf_mm = t_mm;
    p.r_mm = 1.5 * t_mm;
    p.labio_c_mm = lip_c_mm;

    const double h = h_mm * 0.1;
    const double b = b_mm * 0.1;
    const double c = lip_c_mm * 0.1;
    const double t = t_mm * 0.1;

    const double dev_len = (h - 2.0 * t) + 2.0 * (b - t) + 2.0 * c;
    p.area_cm2 = dev_len * t;
    p.avz_cm2 = h * t;
    p.avy_cm2 = 2.0 * b * t;

    const double hw = std::max(0.0, h - 2.0 * t);
    const double iy_web = (t * hw * hw * hw) / 12.0;
    const double iy_flanges = 2.0 * (b * t * (0.5 * (h - t)) * (0.5 * (h - t)));
    const double d_lip = std::max(0.0, 0.5 * h - t - 0.5 * c);
    const double iy_lips = (c > 0.0) ? 2.0 * ((t * c * c * c) / 12.0 + (c * t) * d_lip * d_lip) : 0.0;
    p.iy_cm4 = iy_web + iy_flanges + iy_lips;

    p.iz_cm4 = 2.0 * ((t * b * b * b) / 12.0) + (c > 0.0 ? 2.0 * (c * t) * (0.5 * b) * (0.5 * b) : 0.0);
    p.it_cm4 = (dev_len * t * t * t) / 3.0;
    p.iw_cm6 = (p.iz_cm4 * h * h) * 0.25;

    p.wely_cm3 = 2.0 * p.iy_cm4 / std::max(1e-6, h);
    p.welz_cm3 = 2.0 * p.iz_cm4 / std::max(1e-6, b);
    p.wply_cm3 = 1.16 * p.wely_cm3;
    p.wplz_cm3 = 1.35 * p.welz_cm3;

    p.ry_cm = std::sqrt(p.iy_cm4 / std::max(1e-6, p.area_cm2));
    p.rz_cm = std::sqrt(p.iz_cm4 / std::max(1e-6, p.area_cm2));
    p.peso_kg_m = p.area_m2() * 7850.0;
    p.masividad_am_v_m_inv = 2.0 / std::max(1.0e-4, t_mm * 1.0e-3);
    return p;
}

ProfileSectionProperties ProfileLibraryManager::compute_solid_rect(
    std::string_view ref,
    std::string_view serie,
    double b_mm,
    double h_mm,
    FamiliaMaterialPerfil familia,
    double density_kg_m3
) noexcept {
    ProfileSectionProperties p{};
    p.referencia = std::string(ref);
    p.serie = std::string(serie);
    p.familia = familia;
    p.tipo_forma = GeometriaSeccionTipo::RectangularMacizo;
    p.h_mm = h_mm;
    p.b_mm = b_mm;
    p.tw_mm = b_mm;
    p.tf_mm = h_mm;

    const double b = b_mm * 0.1;
    const double h = h_mm * 0.1;
    p.area_cm2 = b * h;
    p.avy_cm2 = (5.0 / 6.0) * p.area_cm2;
    p.avz_cm2 = (5.0 / 6.0) * p.area_cm2;

    p.iy_cm4 = (b * h * h * h) / 12.0;
    p.iz_cm4 = (h * b * b * b) / 12.0;

    const double a_long = std::max(b, h);
    const double b_short = std::min(b, h);
    const double beta_t = (1.0 / 3.0) * (1.0 - 0.63 * (b_short / std::max(1e-6, a_long)));
    p.it_cm4 = beta_t * a_long * b_short * b_short * b_short;
    p.iw_cm6 = 0.0;

    p.wely_cm3 = (b * h * h) / 6.0;
    p.welz_cm3 = (h * b * b) / 6.0;
    p.wply_cm3 = (b * h * h) * 0.25;
    p.wplz_cm3 = (h * b * b) * 0.25;

    p.ry_cm = h / std::sqrt(12.0);
    p.rz_cm = b / std::sqrt(12.0);
    p.peso_kg_m = p.area_m2() * density_kg_m3;
    p.masividad_am_v_m_inv = 2.0 * (b_mm + h_mm) * 1.0e-3 / std::max(1e-9, p.area_m2());
    return p;
}

ProfileSectionProperties ProfileLibraryManager::compute_solid_circ(
    std::string_view ref,
    std::string_view serie,
    double d_mm,
    FamiliaMaterialPerfil familia,
    double density_kg_m3
) noexcept {
    ProfileSectionProperties p{};
    p.referencia = std::string(ref);
    p.serie = std::string(serie);
    p.familia = familia;
    p.tipo_forma = (familia == FamiliaMaterialPerfil::AceroLaminado)
        ? GeometriaSeccionTipo::RedondoMacizo
        : GeometriaSeccionTipo::CircularMacizo;
    p.h_mm = d_mm;
    p.b_mm = d_mm;
    p.tw_mm = d_mm;
    p.tf_mm = d_mm;

    const double d = d_mm * 0.1;
    p.area_cm2 = 0.25 * std::numbers::pi * d * d;
    p.avy_cm2 = 0.9 * p.area_cm2;
    p.avz_cm2 = 0.9 * p.area_cm2;
    p.iy_cm4 = (std::numbers::pi * d * d * d * d) / 64.0;
    p.iz_cm4 = p.iy_cm4;
    p.it_cm4 = 2.0 * p.iy_cm4;
    p.iw_cm6 = 0.0;
    p.wely_cm3 = (std::numbers::pi * d * d * d) / 32.0;
    p.welz_cm3 = p.wely_cm3;
    p.wply_cm3 = (d * d * d) / 6.0;
    p.wplz_cm3 = p.wply_cm3;
    p.ry_cm = 0.25 * d;
    p.rz_cm = 0.25 * d;
    p.peso_kg_m = p.area_m2() * density_kg_m3;
    p.masividad_am_v_m_inv = 4.0 / std::max(1e-6, d_mm * 1.0e-3);
    return p;
}

ProfileSectionProperties ProfileLibraryManager::compute_composite_section(
    const ProfileSectionProperties& base,
    DisposicionCompuestaPerfil disposicion,
    double separacion_mm,
    double espesor_platabanda_mm,
    double ancho_platabanda_mm
) noexcept {
    if (disposicion == DisposicionCompuestaPerfil::Simple) {
        return base;
    }

    ProfileSectionProperties comp = base;
    const double sep_cm = separacion_mm * 0.1;

    switch (disposicion) {
        case DisposicionCompuestaPerfil::DobleEnCajonSoldado:
        case DisposicionCompuestaPerfil::DobleEnCajonPresillas:
        case DisposicionCompuestaPerfil::DobleEnIConUnionContinua:
        case DisposicionCompuestaPerfil::DobleEnIConPresillas:
        case DisposicionCompuestaPerfil::DobleAngularEnT:
        case DisposicionCompuestaPerfil::DobleAngularEnCruz: {
            comp.referencia = "2x " + base.referencia;
            comp.b_mm = 2.0 * base.b_mm + separacion_mm;
            comp.area_cm2 = 2.0 * base.area_cm2;
            comp.avy_cm2 = 2.0 * base.avy_cm2;
            comp.avz_cm2 = 2.0 * base.avz_cm2;
            comp.iy_cm4 = 2.0 * base.iy_cm4;

            // Teorema de Steiner en eje Z para 2 perfiles separados
            const double dy_cm = 0.5 * (base.b_mm * 0.1 + sep_cm);
            comp.iz_cm4 = 2.0 * (base.iz_cm4 + base.area_cm2 * dy_cm * dy_cm);

            if (disposicion == DisposicionCompuestaPerfil::DobleEnCajonSoldado) {
                // Cajón cerrado soldado: torsión de Bredt muy superior a 2*It
                const double am_cm2 = std::max(1.0, (comp.b_mm * 0.1 - base.tw_mm * 0.1) * (base.h_mm * 0.1 - base.tf_mm * 0.1));
                const double perim_cm = 2.0 * ((comp.b_mm + base.h_mm) * 0.1);
                comp.it_cm4 = 4.0 * am_cm2 * am_cm2 * (base.tf_mm * 0.1) / perim_cm;
            } else {
                comp.it_cm4 = 2.0 * base.it_cm4;
            }

            comp.wely_cm3 = 2.0 * base.wely_cm3;
            comp.wply_cm3 = 2.0 * base.wply_cm3;
            comp.welz_cm3 = 2.0 * comp.iz_cm4 / std::max(1e-6, comp.b_mm * 0.1);
            comp.wplz_cm3 = 1.25 * comp.welz_cm3;
            comp.ry_cm = std::sqrt(comp.iy_cm4 / std::max(1e-6, comp.area_cm2));
            comp.rz_cm = std::sqrt(comp.iz_cm4 / std::max(1e-6, comp.area_cm2));
            comp.peso_kg_m = 2.0 * base.peso_kg_m;
            break;
        }
        case DisposicionCompuestaPerfil::VigaAlveolarCastelada: {
            // Viga Boyd / castelada: incremento de canto ~1.5*h con alvéolos en el alma
            comp.referencia = base.referencia + " (Boyd)";
            comp.h_mm = 1.5 * base.h_mm;
            comp.iy_cm4 = 1.85 * base.iy_cm4;
            comp.wely_cm3 = 2.0 * comp.iy_cm4 / (comp.h_mm * 0.1);
            comp.wply_cm3 = 1.12 * comp.wely_cm3;
            comp.ry_cm = std::sqrt(comp.iy_cm4 / std::max(1e-6, comp.area_cm2));
            break;
        }
        case DisposicionCompuestaPerfil::ConPlatabandasLaterales: {
            const double tp_cm = std::max(0.8, espesor_platabanda_mm * 0.1);
            const double bp_cm = (ancho_platabanda_mm > 0.0 ? ancho_platabanda_mm : base.b_mm) * 0.1;
            const double a_plates = 2.0 * bp_cm * tp_cm;
            const double d_z = 0.5 * (base.h_mm * 0.1 + tp_cm);
            comp.referencia = base.referencia + " + Platabandas";
            comp.h_mm = base.h_mm + 20.0 * tp_cm;
            comp.area_cm2 = base.area_cm2 + a_plates;
            comp.iy_cm4 = base.iy_cm4 + a_plates * d_z * d_z;
            comp.iz_cm4 = base.iz_cm4 + 2.0 * (tp_cm * bp_cm * bp_cm * bp_cm) / 12.0;
            comp.wely_cm3 = 2.0 * comp.iy_cm4 / (comp.h_mm * 0.1);
            comp.wply_cm3 = base.wply_cm3 + a_plates * d_z;
            comp.ry_cm = std::sqrt(comp.iy_cm4 / std::max(1e-6, comp.area_cm2));
            comp.rz_cm = std::sqrt(comp.iz_cm4 / std::max(1e-6, comp.area_cm2));
            comp.peso_kg_m = comp.area_m2() * 7850.0;
            break;
        }
        default:
            break;
    }
    return comp;
}

ProfileSectionProperties ProfileLibraryManager::compute_haunched_section_at_end(
    const ProfileSectionProperties& base,
    double canto_adicional_cartela_mm,
    bool doble_cartela
) noexcept {
    const double dh = std::max(0.0, canto_adicional_cartela_mm) * (doble_cartela ? 2.0 : 1.0);
    return compute_double_t(
        base.referencia + " (Cartela)",
        base.serie,
        base.h_mm + dh,
        base.b_mm,
        base.tw_mm,
        base.tf_mm,
        base.r_mm,
        base.familia
    );
}

ProfileLibraryManager::ProfileLibraryManager() {
    // 1. Serie IPE (IPE 80 a IPE 600 - EN 10365)
    struct DimDT { const char* ref; double h, b, tw, tf, r; };
    constexpr DimDT ipe_table[] = {
        {"IPE 80",  80,  46, 3.8, 5.2, 5.0},
        {"IPE 100", 100, 55, 4.1, 5.7, 7.0},
        {"IPE 120", 120, 64, 4.4, 6.3, 7.0},
        {"IPE 140", 140, 73, 4.7, 6.9, 7.0},
        {"IPE 160", 160, 82, 5.0, 7.4, 9.0},
        {"IPE 180", 180, 91, 5.3, 8.0, 9.0},
        {"IPE 200", 200, 100, 5.6, 8.5, 12.0},
        {"IPE 220", 220, 110, 5.9, 9.2, 12.0},
        {"IPE 240", 240, 120, 6.2, 9.8, 15.0},
        {"IPE 270", 270, 135, 6.6, 10.2, 15.0},
        {"IPE 300", 300, 150, 7.1, 10.7, 15.0},
        {"IPE 330", 330, 160, 7.5, 11.5, 18.0},
        {"IPE 360", 360, 170, 8.0, 12.7, 18.0},
        {"IPE 400", 400, 180, 8.6, 13.5, 21.0},
        {"IPE 450", 450, 190, 9.4, 14.6, 21.0},
        {"IPE 500", 500, 200, 10.2, 16.0, 21.0},
        {"IPE 550", 550, 210, 11.1, 17.2, 24.0},
        {"IPE 600", 600, 220, 12.0, 19.0, 24.0}
    };
    for (const auto& d : ipe_table) {
        catalog_.push_back(compute_double_t(d.ref, "IPE", d.h, d.b, d.tw, d.tf, d.r));
    }

    // 2. Serie HEB (HEB 100 a HEB 600)
    constexpr DimDT heb_table[] = {
        {"HEB 100", 100, 100, 6.0, 10.0, 12.0},
        {"HEB 120", 120, 120, 6.5, 11.0, 12.0},
        {"HEB 140", 140, 140, 7.0, 12.0, 12.0},
        {"HEB 160", 160, 160, 8.0, 13.0, 15.0},
        {"HEB 180", 180, 180, 8.5, 14.0, 15.0},
        {"HEB 200", 200, 200, 9.0, 15.0, 18.0},
        {"HEB 220", 220, 220, 9.5, 16.0, 18.0},
        {"HEB 240", 240, 240, 10.0, 17.0, 21.0},
        {"HEB 260", 260, 260, 10.0, 17.5, 24.0},
        {"HEB 280", 280, 280, 10.5, 18.0, 24.0},
        {"HEB 300", 300, 300, 11.0, 19.0, 27.0},
        {"HEB 320", 320, 300, 11.5, 20.5, 27.0},
        {"HEB 340", 340, 300, 12.0, 21.5, 27.0},
        {"HEB 360", 360, 300, 12.5, 22.5, 27.0},
        {"HEB 400", 400, 300, 13.5, 24.0, 27.0},
        {"HEB 450", 450, 300, 14.0, 26.0, 27.0},
        {"HEB 500", 500, 300, 14.5, 28.0, 27.0},
        {"HEB 550", 550, 300, 15.0, 29.0, 27.0},
        {"HEB 600", 600, 300, 15.5, 30.0, 27.0}
    };
    for (const auto& d : heb_table) {
        catalog_.push_back(compute_double_t(d.ref, "HEB", d.h, d.b, d.tw, d.tf, d.r));
    }

    // 3. Serie HEA (HEA 100 a HEA 600)
    constexpr DimDT hea_table[] = {
        {"HEA 100", 96,  100, 5.0, 8.0,  12.0},
        {"HEA 120", 114, 120, 5.0, 8.0,  12.0},
        {"HEA 140", 133, 140, 5.5, 8.5,  12.0},
        {"HEA 160", 152, 160, 6.0, 9.0,  15.0},
        {"HEA 180", 171, 180, 6.0, 9.5,  15.0},
        {"HEA 200", 190, 200, 6.5, 10.0, 18.0},
        {"HEA 220", 210, 220, 7.0, 11.0, 18.0},
        {"HEA 240", 230, 240, 7.5, 12.0, 21.0},
        {"HEA 260", 250, 260, 7.5, 12.5, 24.0},
        {"HEA 280", 270, 280, 8.0, 13.0, 24.0},
        {"HEA 300", 290, 300, 8.5, 14.0, 27.0},
        {"HEA 320", 310, 300, 9.0, 15.5, 27.0},
        {"HEA 340", 330, 300, 9.5, 16.5, 27.0},
        {"HEA 360", 350, 300, 10.0, 17.5, 27.0},
        {"HEA 400", 390, 300, 11.0, 19.0, 27.0},
        {"HEA 450", 440, 300, 11.5, 21.0, 27.0},
        {"HEA 500", 490, 300, 12.0, 23.0, 27.0},
        {"HEA 600", 590, 300, 13.0, 25.0, 27.0}
    };
    for (const auto& d : hea_table) {
        catalog_.push_back(compute_double_t(d.ref, "HEA", d.h, d.b, d.tw, d.tf, d.r));
    }

    // 4. Serie UPN (UPN 80 a UPN 400)
    constexpr DimDT upn_table[] = {
        {"UPN 80",  80,  45, 6.0, 8.0,  8.0},
        {"UPN 100", 100, 50, 6.0, 8.5,  8.5},
        {"UPN 120", 120, 55, 7.0, 9.0,  9.0},
        {"UPN 140", 140, 60, 7.0, 10.0, 10.0},
        {"UPN 160", 160, 65, 7.5, 10.5, 10.5},
        {"UPN 180", 180, 70, 8.0, 11.0, 11.0},
        {"UPN 200", 200, 75, 8.5, 11.5, 11.5},
        {"UPN 220", 220, 80, 9.0, 12.5, 12.5},
        {"UPN 240", 240, 85, 9.5, 13.0, 13.0},
        {"UPN 260", 260, 90, 10.0, 14.0, 14.0},
        {"UPN 300", 300, 100, 10.0, 16.0, 16.0},
        {"UPN 400", 400, 110, 14.0, 18.0, 18.0}
    };
    for (const auto& d : upn_table) {
        catalog_.push_back(compute_channel_upn(d.ref, "UPN", d.h, d.b, d.tw, d.tf, d.r));
    }

    // 5. Angulares L simétricos y desiguales LD
    catalog_.push_back(compute_angle_l("L 40x40x4", "L", 40, 40, 4, 6));
    catalog_.push_back(compute_angle_l("L 50x50x5", "L", 50, 50, 5, 7));
    catalog_.push_back(compute_angle_l("L 60x60x6", "L", 60, 60, 6, 8));
    catalog_.push_back(compute_angle_l("L 80x80x8", "L", 80, 80, 8, 10));
    catalog_.push_back(compute_angle_l("L 100x100x10", "L", 100, 100, 10, 12));
    catalog_.push_back(compute_angle_l("L 120x120x12", "L", 120, 120, 12, 13));
    catalog_.push_back(compute_angle_l("LD 80x40x6", "LD", 80, 40, 6, 7));
    catalog_.push_back(compute_angle_l("LD 100x50x8", "LD", 100, 50, 8, 8));
    catalog_.push_back(compute_angle_l("LD 120x80x10", "LD", 120, 80, 10, 10));

    // 6. Perfil T simple y Redondo macizo (tirantes R)
    {
        auto t_prof = compute_double_t("T 100x100x11", "T", 100, 100, 11, 11, 8);
        t_prof.tipo_forma = GeometriaSeccionTipo::TeSimple;
        t_prof.area_cm2 *= 0.55;
        catalog_.push_back(t_prof);
    }
    catalog_.push_back(compute_solid_circ("R 12", "R", 12.0));
    catalog_.push_back(compute_solid_circ("R 16", "R", 16.0));
    catalog_.push_back(compute_solid_circ("R 20", "R", 20.0));
    catalog_.push_back(compute_solid_circ("R 25", "R", 25.0));

    // 7. Tubos estructurales huecos SHS, RHS y CHS
    catalog_.push_back(compute_hollow_rect("SHS 60x60x4", "SHS", 60, 60, 4));
    catalog_.push_back(compute_hollow_rect("SHS 80x80x4", "SHS", 80, 80, 4));
    catalog_.push_back(compute_hollow_rect("SHS 100x100x5", "SHS", 100, 100, 5));
    catalog_.push_back(compute_hollow_rect("SHS 120x120x6", "SHS", 120, 120, 6));
    catalog_.push_back(compute_hollow_rect("SHS 150x150x8", "SHS", 150, 150, 8));
    catalog_.push_back(compute_hollow_rect("SHS 200x200x10", "SHS", 200, 200, 10));

    catalog_.push_back(compute_hollow_rect("RHS 100x50x4", "RHS", 100, 50, 4));
    catalog_.push_back(compute_hollow_rect("RHS 120x60x5", "RHS", 120, 60, 5));
    catalog_.push_back(compute_hollow_rect("RHS 160x80x6", "RHS", 160, 80, 6));
    catalog_.push_back(compute_hollow_rect("RHS 200x100x8", "RHS", 200, 100, 8));

    catalog_.push_back(compute_hollow_circ("CHS 88.9x4.0", "CHS", 88.9, 4.0));
    catalog_.push_back(compute_hollow_circ("CHS 114.3x5.0", "CHS", 114.3, 5.0));
    catalog_.push_back(compute_hollow_circ("CHS 139.7x6.3", "CHS", 139.7, 6.3));
    catalog_.push_back(compute_hollow_circ("CHS 168.3x8.0", "CHS", 168.3, 8.0));
    catalog_.push_back(compute_hollow_circ("CHS 219.1x10.0", "CHS", 219.1, 10.0));

    // 8. Perfiles de acero conformado en frío (CF, ZF, UF, OMEGA)
    catalog_.push_back(compute_cold_formed("CF 160x60x20x2.0", "CF", GeometriaSeccionTipo::ConformadoC, 160, 60, 20, 2.0));
    catalog_.push_back(compute_cold_formed("CF 200x60x20x2.5", "CF", GeometriaSeccionTipo::ConformadoC, 200, 60, 20, 2.5));
    catalog_.push_back(compute_cold_formed("CF 250x70x25x3.0", "CF", GeometriaSeccionTipo::ConformadoC, 250, 70, 25, 3.0));
    catalog_.push_back(compute_cold_formed("ZF 180x65x20x2.0", "ZF", GeometriaSeccionTipo::ConformadoZ, 180, 65, 20, 2.0));
    catalog_.push_back(compute_cold_formed("ZF 200x70x20x2.5", "ZF", GeometriaSeccionTipo::ConformadoZ, 200, 70, 20, 2.5));
    catalog_.push_back(compute_cold_formed("UF 150x50x3.0", "UF", GeometriaSeccionTipo::ConformadoU, 150, 50, 0.0, 3.0));
    catalog_.push_back(compute_cold_formed("OMEGA 120x60x20x2.0", "OMEGA", GeometriaSeccionTipo::ConformadoOmega, 120, 60, 20, 2.0));

    // 9. Perfiles de Madera aserrada y laminada encolada (`pzmadera.dll`)
    catalog_.push_back(compute_solid_rect("RECT 100x200", "MADERA-RECT", 100, 200, FamiliaMaterialPerfil::Madera, 420.0));
    catalog_.push_back(compute_solid_rect("RECT 150x300", "MADERA-RECT", 150, 300, FamiliaMaterialPerfil::Madera, 420.0));
    catalog_.push_back(compute_solid_rect("GL 140x320", "MADERA-GL", 140, 320, FamiliaMaterialPerfil::Madera, 440.0));
    catalog_.push_back(compute_solid_rect("GL 200x600", "MADERA-GL", 200, 600, FamiliaMaterialPerfil::Madera, 440.0));
    catalog_.push_back(compute_solid_circ("CIRC D200", "MADERA-CIRC", 200, FamiliaMaterialPerfil::Madera, 420.0));

    // 10. Perfiles de Aluminio extruido (EN 1999)
    catalog_.push_back(compute_double_t("ALU-I 120x60x6", "ALU-I", 120, 60, 6, 8, 6, FamiliaMaterialPerfil::Aluminio, 2700.0));
    catalog_.push_back(compute_hollow_rect("ALU-RHS 100x50x4", "ALU-RHS", 100, 50, 4, FamiliaMaterialPerfil::Aluminio, 2700.0));
    catalog_.push_back(compute_hollow_circ("ALU-CHS 100x4", "ALU-CHS", 100, 4, FamiliaMaterialPerfil::Aluminio, 2700.0));
}

std::vector<std::string> ProfileLibraryManager::list_series(std::optional<FamiliaMaterialPerfil> familia) const {
    std::vector<std::string> out;
    for (const auto& p : catalog_) {
        if (familia.has_value() && p.familia != *familia) {
            continue;
        }
        if (std::find(out.begin(), out.end(), p.serie) == out.end()) {
            out.push_back(p.serie);
        }
    }
    return out;
}

std::vector<ProfileSectionProperties> ProfileLibraryManager::profiles_in_series(std::string_view serie_name) const {
    std::vector<ProfileSectionProperties> out;
    for (const auto& p : catalog_) {
        if (p.serie == serie_name) {
            out.push_back(p);
        }
    }
    return out;
}

const ProfileSectionProperties* ProfileLibraryManager::find_profile(std::string_view referencia) const noexcept {
    for (const auto& p : catalog_) {
        if (p.referencia == referencia) {
            return &p;
        }
    }
    return nullptr;
}

void ProfileLibraryManager::register_custom_profile(const ProfileSectionProperties& prop) {
    for (auto& existing : catalog_) {
        if (existing.referencia == prop.referencia) {
            existing = prop;
            return;
        }
    }
    catalog_.push_back(prop);
}

// ============================================================================
// P-06.3: Propiedades Mecánicas de Barras (`elements`, `compflecha`, `pushover`)
// ============================================================================

namespace {
core::Vec2d cardinal_point_offset_m(PuntoAjusteSeccion pt, double b_m, double h_m) noexcept {
    switch (pt) {
        case PuntoAjusteSeccion::CentroGravedad:
        case PuntoAjusteSeccion::MedioCentro:
            return {0.0, 0.0};
        case PuntoAjusteSeccion::SuperiorIzquierda:
            return {+0.5 * b_m, -0.5 * h_m};
        case PuntoAjusteSeccion::SuperiorCentro:
            return {0.0, -0.5 * h_m};
        case PuntoAjusteSeccion::SuperiorDerecha:
            return {-0.5 * b_m, -0.5 * h_m};
        case PuntoAjusteSeccion::MedioIzquierda:
            return {+0.5 * b_m, 0.0};
        case PuntoAjusteSeccion::MedioDerecha:
            return {-0.5 * b_m, 0.0};
        case PuntoAjusteSeccion::InferiorIzquierda:
            return {+0.5 * b_m, +0.5 * h_m};
        case PuntoAjusteSeccion::InferiorCentro:
            return {0.0, +0.5 * h_m};
        case PuntoAjusteSeccion::InferiorDerecha:
            return {-0.5 * b_m, +0.5 * h_m};
    }
    return {0.0, 0.0};
}
} // namespace

core::Vec2d DisposicionSeccionBarra::offset_cdg_extremo_inicial_m(double b_m, double h_m) const noexcept {
    const core::Vec2d base = cardinal_point_offset_m(punto_ajuste, b_m, h_m);
    return {base.x + desp_y_ini_m, base.y + desp_z_ini_m};
}

core::Vec2d DisposicionSeccionBarra::offset_cdg_extremo_final_m(double b_m, double h_m) const noexcept {
    const core::Vec2d base = cardinal_point_offset_m(punto_ajuste, b_m, h_m);
    return {base.x + desp_y_fin_m, base.y + desp_z_fin_m};
}

double DatosPandeoBarra::longitud_efectiva_xy_m(double longitud_barra_m) const noexcept {
    if (modo_xy == ModoCalculoPandeo::SinPandeo) return 0.0;
    if (modo_xy == ModoCalculoPandeo::LongitudEfectivaLkUsuario && lk_xy_m > 0.0) return lk_xy_m;
    return std::max(0.0, beta_xy * longitud_barra_m);
}

double DatosPandeoBarra::longitud_efectiva_xz_m(double longitud_barra_m) const noexcept {
    if (modo_xz == ModoCalculoPandeo::SinPandeo) return 0.0;
    if (modo_xz == ModoCalculoPandeo::LongitudEfectivaLkUsuario && lk_xz_m > 0.0) return lk_xz_m;
    return std::max(0.0, beta_xz * longitud_barra_m);
}

double DatosPandeoBarra::vpandeo_beta_intraslacional(double ga, double gb) noexcept {
    // Fórmula clásica de Wood / Eurocódigo 3 / CTE DB SE-A para pórticos intraslacionales:
    // ga, gb son coeficientes de flexibilidad en extremos [0 = empotrado, 1 = articulado] o rigideces relativas G
    // Normalizamos ga, gb (flexibilidades eta_A, eta_B en [0, 1]: eta = G / (G + 1) si G > 1, o directo si ya está en [0, 1])
    const auto to_eta = [](double g) noexcept {
        const double gc = std::max(0.0, g);
        return (gc <= 1.0) ? gc : (gc / (gc + 1.0));
    };
    const double eta1 = to_eta(ga);
    const double eta2 = to_eta(gb);
    const double num = 1.0 + 0.145 * (eta1 + eta2) - 0.265 * eta1 * eta2;
    const double den = 2.0 - 0.364 * (eta1 + eta2) - 0.247 * eta1 * eta2;
    return std::clamp(num / std::max(0.1, den), 0.5, 1.0);
}

double DatosPandeoBarra::vpandeo_beta_traslacional(double ga, double gb) noexcept {
    // Fórmula de Wood / EC3 para pórticos traslacionales:
    const auto to_eta = [](double g) noexcept {
        const double gc = std::max(0.0, g);
        return (gc <= 1.0) ? gc : (gc / (gc + 1.0));
    };
    const double eta1 = to_eta(ga);
    const double eta2 = to_eta(gb);
    const double num = 1.0 - 0.20 * (eta1 + eta2) - 0.12 * eta1 * eta2;
    const double den = std::max(0.02, 1.0 - 0.80 * (eta1 + eta2) + 0.60 * eta1 * eta2);
    return std::clamp(std::sqrt(num / den), 1.0, 10.0);
}

double DatosPandeoLateralBarra::compute_mcr_knm(
    const ProfileSectionProperties& profile,
    double longitud_barra_m,
    bool ala_superior,
    double e_mpa,
    double g_mpa
) const noexcept {
    if (ala_superior && !comprobar_ala_superior) return 1.0e9;
    if (!ala_superior && !comprobar_ala_inferior) return 1.0e9;

    const double lc_base = ala_superior
        ? (lc_ala_sup_m > 0.0 ? lc_ala_sup_m : longitud_barra_m * beta_lt_ala_sup)
        : (lc_ala_inf_m > 0.0 ? lc_ala_inf_m : longitud_barra_m * beta_lt_ala_inf);
    const double llt = std::max(0.05, lc_base);

    const double e_kn_m2 = e_mpa * 1000.0;
    const double g_kn_m2 = g_mpa * 1000.0;
    const double iz = profile.iz_m4();
    const double it = profile.it_m4();
    const double iw = profile.iw_m6();

    const double euler_z = (std::numbers::pi * std::numbers::pi * e_kn_m2 * iz) / (llt * llt);
    const double warp_term = (iw / std::max(1e-12, iz)) + ((llt * llt * g_kn_m2 * it) / std::max(1e-12, std::numbers::pi * std::numbers::pi * e_kn_m2 * iz));
    return std::max(0.5, coeficiente_c1) * euler_z * std::sqrt(std::max(0.0, warp_term));
}

double LimiteFlechaEspecificacion::flecha_admisible_mm(double longitud_referencia_m) const noexcept {
    const double l_mm = std::max(0.0, longitud_referencia_m * 1000.0);
    const double lim_rel = (divisor_relativo_l_n > 1.0) ? (l_mm / divisor_relativo_l_n) : 1.0e6;
    const double lim_abs = (limite_absoluto_mm > 0.0) ? limite_absoluto_mm : 1.0e6;

    switch (criterio) {
        case CriterioLimiteFlecha::RelativoLN:
            return lim_rel;
        case CriterioLimiteFlecha::AbsolutoMm:
            return lim_abs;
        case CriterioLimiteFlecha::MenorDeAmbos:
            return std::min(lim_rel, lim_abs);
    }
    return lim_rel;
}

// ============================================================================
// P-06.4: Pilares y Vigas de Hormigón Armado (`pilares`, `porvigas`, `comppilhorm`, `discap`)
// ============================================================================

namespace {
double rebar_area_cm2(std::uint32_t count, std::uint32_t diam_mm) noexcept {
    const double d_cm = static_cast<double>(diam_mm) * 0.1;
    return static_cast<double>(count) * (0.25 * std::numbers::pi * d_cm * d_cm);
}
} // namespace

double ArmadoTramoPilar::area_total_longitudinal_cm2(bool es_circular) const noexcept {
    if (es_circular) {
        return rebar_area_cm2(num_barras_circular, diametro_circular_mm);
    }
    return rebar_area_cm2(num_barras_esquina, diametro_esquina_mm)
         + 2.0 * rebar_area_cm2(num_barras_cara_x, diametro_cara_x_mm)
         + 2.0 * rebar_area_cm2(num_barras_cara_y, diametro_cara_y_mm);
}

double ArmadoTramoPilar::cuantia_geometrica(double area_hormigon_cm2, bool es_circular) const noexcept {
    if (area_hormigon_cm2 <= 1e-6) return 0.0;
    return area_total_longitudinal_cm2(es_circular) / area_hormigon_cm2;
}

double TramoPilarPlanta::area_bruta_hormigon_cm2() const noexcept {
    if (tipo_seccion == TipoSeccionPilar::HormigonCircular || tipo_seccion == TipoSeccionPilar::MixtoTubularRellenoHormigon) {
        const double d_cm = diametro_m * 100.0;
        return 0.25 * std::numbers::pi * d_cm * d_cm;
    }
    return (ancho_x_m * 100.0) * (ancho_y_m * 100.0);
}

double TramoPilarPlanta::capacidad_axil_plastica_kn(
    double fcd_mpa,
    double fyd_mpa,
    double fy_perfil_mpa,
    double area_perfil_cm2
) const noexcept {
    const bool circular = (tipo_seccion == TipoSeccionPilar::HormigonCircular || tipo_seccion == TipoSeccionPilar::MixtoTubularRellenoHormigon);
    const double ac_m2 = std::max(0.0, (area_bruta_hormigon_cm2() - area_perfil_cm2)) * 1.0e-4;
    const double as_m2 = armado.area_total_longitudinal_cm2(circular) * 1.0e-4;
    const double ap_m2 = area_perfil_cm2 * 1.0e-4;

    const double factor_conf = (tipo_seccion == TipoSeccionPilar::MixtoTubularRellenoHormigon) ? 1.00 : 0.85;
    return (factor_conf * fcd_mpa * ac_m2 + fyd_mpa * as_m2 + fy_perfil_mpa * ap_m2) * 1000.0;
}

double ArmadoVanoViga::area_superior_apoyo_izq_cm2() const noexcept {
    return rebar_area_cm2(num_montaje_sup, diametro_montaje_sup_mm)
         + rebar_area_cm2(num_ref_sup_izq, diametro_ref_sup_izq_mm);
}

double ArmadoVanoViga::area_superior_apoyo_der_cm2() const noexcept {
    return rebar_area_cm2(num_montaje_sup, diametro_montaje_sup_mm)
         + rebar_area_cm2(num_ref_sup_der, diametro_ref_sup_der_mm);
}

double ArmadoVanoViga::area_inferior_centro_vano_cm2() const noexcept {
    return rebar_area_cm2(num_montaje_inf, diametro_montaje_inf_mm)
         + rebar_area_cm2(num_ref_inf_vano, diametro_ref_inf_vano_mm);
}

double ArmadoVanoViga::area_estribos_cm2_por_m(double separacion_cm) const noexcept {
    if (separacion_cm <= 1e-3) return 0.0;
    const double asw_1 = rebar_area_cm2(num_ramas_estribo, diametro_estribo_mm);
    return asw_1 * (100.0 / separacion_cm);
}

namespace {
double compute_beam_mrd_knm(
    double as_cm2,
    double b_comp_m,
    double h_m,
    double recub_m,
    double fcd_mpa,
    double fyd_mpa
) noexcept {
    const double d_m = std::max(0.05, h_m - recub_m - 0.016);
    const double as_m2 = as_cm2 * 1.0e-4;
    const double f_tens_kn = as_m2 * fyd_mpa * 1000.0;
    const double y_block_m = f_tens_kn / std::max(1.0, 0.85 * fcd_mpa * 1000.0 * b_comp_m);
    const double z_lever_m = std::max(0.50 * d_m, d_m - 0.5 * y_block_m);
    return f_tens_kn * z_lever_m;
}
} // namespace

double VanoVigaHormigon::momento_resistente_negativo_izq_knm(double fcd_mpa, double fyd_mpa) const noexcept {
    return compute_beam_mrd_knm(
        armado.area_superior_apoyo_izq_cm2() + area_armadura_activa_cm2,
        ancho_alma_b_m,
        canto_total_h_m,
        recubrimiento_geom_m,
        fcd_mpa,
        fyd_mpa
    );
}

double VanoVigaHormigon::momento_resistente_negativo_der_knm(double fcd_mpa, double fyd_mpa) const noexcept {
    return compute_beam_mrd_knm(
        armado.area_superior_apoyo_der_cm2() + area_armadura_activa_cm2,
        ancho_alma_b_m,
        canto_total_h_m,
        recubrimiento_geom_m,
        fcd_mpa,
        fyd_mpa
    );
}

double VanoVigaHormigon::momento_resistente_positivo_vano_knm(double fcd_mpa, double fyd_mpa) const noexcept {
    double b_ef = ancho_alma_b_m;
    if (tipo_seccion == TipoSeccionViga::SeccionEnT) {
        b_ef += ancho_ala_izquierda_m + ancho_ala_derecha_m;
    } else if (tipo_seccion == TipoSeccionViga::SeccionEnLDerecha) {
        b_ef += ancho_ala_derecha_m;
    } else if (tipo_seccion == TipoSeccionViga::SeccionEnLIzquierda) {
        b_ef += ancho_ala_izquierda_m;
    }
    return compute_beam_mrd_knm(
        armado.area_inferior_centro_vano_cm2() + area_armadura_activa_cm2,
        b_ef,
        canto_total_h_m,
        recubrimiento_geom_m,
        fcd_mpa,
        fyd_mpa
    );
}

ResultadoComprobacionPilar ConcreteStructureEngine::verificar_tramo_pilar(
    const TramoPilarPlanta& tramo,
    double sum_mrb_vigas_concurrentes_knm,
    double gamma_rd_capacidad,
    bool con_criterio_sismico,
    double fcd_mpa,
    double fyd_mpa
) {
    ResultadoComprobacionPilar res{};
    const bool circular = (tramo.tipo_seccion == TipoSeccionPilar::HormigonCircular ||
                           tramo.tipo_seccion == TipoSeccionPilar::MixtoTubularRellenoHormigon);

    // 1. Comprobación de dimensión mínima (`cphcscrcomp_*`)
    const double min_dim_m = circular ? tramo.diametro_m : std::min(tramo.ancho_x_m, tramo.ancho_y_m);
    const double req_min_dim_m = con_criterio_sismico ? 0.25 : 0.20;
    res.cumple_dimension_minima = (min_dim_m + 1e-6 >= req_min_dim_m);
    if (!res.cumple_dimension_minima) {
        res.mensajes_comprobacion.push_back("Dimensión menor del pilar inferior al mínimo normativo.");
    }

    // 2. Comprobación de cuantía longitudinal mínima y máxima (`cphcscrcomp_cuantia_*`)
    res.cuantia_minima_norma = con_criterio_sismico ? 0.010 : 0.004;
    res.cuantia_maxima_norma = 0.040;
    res.cuantia_dispuesta = tramo.armado.cuantia_geometrica(tramo.area_bruta_hormigon_cm2(), circular);
    res.cumple_cuantia_min_max =
        (res.cuantia_dispuesta + 1e-6 >= res.cuantia_minima_norma) &&
        (res.cuantia_dispuesta <= res.cuantia_maxima_norma + 1e-6);
    if (!res.cumple_cuantia_min_max) {
        res.mensajes_comprobacion.push_back("La cuantía longitudinal está fuera del rango [rho_min, rho_max].");
    }

    // 3. Separación máxima de estribos en zona central y zona crítica (`cphcscrcomp_separacion_*`)
    const double d_long_min_cm = static_cast<double>(circular ? tramo.armado.diametro_circular_mm : tramo.armado.diametro_esquina_mm) * 0.1;
    res.separacion_max_central_adm_cm = std::min({min_dim_m * 100.0, 15.0 * d_long_min_cm, 30.0});
    res.separacion_max_critica_adm_cm = con_criterio_sismico
        ? std::min({0.5 * min_dim_m * 100.0, 8.0 * d_long_min_cm, 15.0})
        : res.separacion_max_central_adm_cm;

    res.cumple_separacion_estribos =
        (tramo.armado.separacion_zona_central_cm <= res.separacion_max_central_adm_cm + 1e-6) &&
        (tramo.armado.separacion_zona_critica_cm <= res.separacion_max_critica_adm_cm + 1e-6);
    if (!res.cumple_separacion_estribos) {
        res.mensajes_comprobacion.push_back("Separación de estribos superior a la máxima admisible.");
    }

    // 4. Diseño por capacidad a flexión y cortante en pilares (`discap.dll`: `dccomp_append_comprobacion_*_capacidad_*_pilares`)
    const double h_libre_m = std::max(0.5, tramo.cota_sup_m - tramo.cota_inf_m);
    const double as_tot_m2 = tramo.armado.area_total_longitudinal_cm2(circular) * 1.0e-4;
    const double z_brazo_m = std::max(0.10, min_dim_m - 2.0 * tramo.recubrimiento_mecanico_m);
    const double mrc_pilar_extremo_knm = 0.5 * as_tot_m2 * (fyd_mpa * 1000.0) * z_brazo_m;
    const double sum_mrc_nudo = 2.0 * mrc_pilar_extremo_knm;

    if (con_criterio_sismico && sum_mrb_vigas_concurrentes_knm > 0.0) {
        res.cumple_capacidad_flexion_nudo = (sum_mrc_nudo >= gamma_rd_capacidad * sum_mrb_vigas_concurrentes_knm);
        if (!res.cumple_capacidad_flexion_nudo) {
            res.mensajes_comprobacion.push_back("No cumple criterio columna fuerte / viga débil (Sum Mrc < gamma_Rd * Sum Mrb).");
        }
    }

    res.cortante_capacidad_sismica_kn = gamma_rd_capacidad * (2.0 * mrc_pilar_extremo_knm) / h_libre_m;
    const double n_plast_kn = tramo.capacidad_axil_plastica_kn(fcd_mpa, fyd_mpa);
    res.aprovechamiento_maximo = std::clamp(res.cortante_capacidad_sismica_kn / std::max(1.0, 0.25 * n_plast_kn), 0.05, 1.50);

    res.cumple_global =
        res.cumple_dimension_minima &&
        res.cumple_cuantia_min_max &&
        res.cumple_separacion_estribos &&
        res.cumple_capacidad_flexion_nudo;
    return res;
}

ResultadoComprobacionVanoViga ConcreteStructureEngine::verificar_vano_viga(
    const VanoVigaHormigon& vano,
    double luz_clara_m,
    double carga_isostatica_q_kn_m,
    double gamma_rd_capacidad,
    bool con_criterio_sismico,
    double fcd_mpa,
    double fyd_mpa
) {
    ResultadoComprobacionVanoViga res{};
    res.mrd_neg_izq_knm = vano.momento_resistente_negativo_izq_knm(fcd_mpa, fyd_mpa);
    res.mrd_neg_der_knm = vano.momento_resistente_negativo_der_knm(fcd_mpa, fyd_mpa);
    res.mrd_pos_vano_knm = vano.momento_resistente_positivo_vano_knm(fcd_mpa, fyd_mpa);

    // 1. Armadura mínima de tracción por flexión (EC2 / CE: As,min >= 0.0015 * b * d)
    const double d_cm = std::max(10.0, (vano.canto_total_h_m - vano.recubrimiento_geom_m) * 100.0);
    const double as_min_cm2 = 0.0015 * (vano.ancho_alma_b_m * 100.0) * d_cm;
    res.cumple_armadura_minima_flexion =
        (vano.armado.area_superior_apoyo_izq_cm2() >= as_min_cm2) &&
        (vano.armado.area_superior_apoyo_der_cm2() >= as_min_cm2) &&
        (vano.armado.area_inferior_centro_vano_cm2() >= as_min_cm2);
    if (!res.cumple_armadura_minima_flexion) {
        res.errores_valoracion.push_back("Armadura longitudinal inferior a la cuantía mínima mecánica/geométrica.");
    }

    // 2. Armadura de piel en vigas de canto h >= 0.60 m (`ed3pvacarm_activar_valoracion_error`: armadura de piel)
    if (vano.canto_total_h_m >= 0.60 && vano.armado.num_barras_piel_por_cara == 0) {
        res.cumple_armadura_piel = false;
        res.errores_valoracion.push_back("Viga de canto >= 60 cm requiere armadura de piel en caras laterales.");
    }

    // 3. Cortante de diseño por capacidad (`dcdimen_esfuerzo_cortante_disenyo_capacidad`)
    const double l_clara = std::max(0.5, luz_clara_m);
    const double v_isostatico_kn = 0.5 * carga_isostatica_q_kn_m * l_clara;
    const double v_hiperestatico_cap_kn = gamma_rd_capacidad * (res.mrd_neg_izq_knm + res.mrd_neg_der_knm) / l_clara;
    res.cortante_disenyo_capacidad_kn = v_isostatico_kn + (con_criterio_sismico ? v_hiperestatico_cap_kn : 0.5 * v_hiperestatico_cap_kn);

    // Resistencia a cortante VRd,s en zona crítica extrema
    const double asw_s_cm2_m = vano.armado.area_estribos_cm2_por_m(vano.armado.sep_extremo_izq_cm);
    const double z_m = 0.9 * (vano.canto_total_h_m - vano.recubrimiento_geom_m);
    res.cortante_resistente_vrd_kn = (asw_s_cm2_m * 1.0e-4) * z_m * (fyd_mpa * 1000.0);

    res.cumple_cortante_capacidad = (res.cortante_resistente_vrd_kn + 1e-6 >= res.cortante_disenyo_capacidad_kn);
    if (!res.cumple_cortante_capacidad) {
        res.errores_valoracion.push_back("Agotamiento por cortante de diseño por capacidad en extremo de viga.");
    }

    res.cumple_global =
        res.cumple_armadura_minima_flexion &&
        res.cumple_armadura_piel &&
        res.cumple_cortante_capacidad;
    return res;
}

} // namespace cypeclon::model
