#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::units {

/**
 * @brief Sistemas de unidades soportados por CYPE 3D (unidades.dll / cyudconf.dll).
 */
enum class UnitSystem : std::uint8_t {
    SI = 0,       ///< Sistema Internacional (kN, m, MPa, °C)
    MKS = 1,      ///< Sistema Técnico MKS (t / kp, m, kp/cm², °C)
    Imperial = 2  ///< Sistema Anglosajón Imperial (kip / lb, ft / in, ksi / psi, °F)
};

/**
 * @brief Magnitudes físicas tipadas modeladas según cypeud.dll y basicos.dll!magnitud_*.
 *
 * Convención de Unidad Interna Canónica (SI Estructural):
 * - Longitud / Distancia: m
 * - Dimensión de sección / Desplazamiento / Abertura de fisura: m (visualizado en mm/cm/in)
 * - Área de sección: m² (visualizado en cm²/mm²/in²)
 * - Inercia de sección: m⁴ (visualizado en cm⁴/in⁴)
 * - Módulo resistente: m³ (visualizado en cm³/in³)
 * - Módulo de alabeo: m⁶ (visualizado en cm⁶/in⁶)
 * - Fuerza / Carga puntual: kN
 * - Momento flector / torsor: kN·m
 * - Carga lineal: kN/m
 * - Carga superficial: kN/m²
 * - Tensión / Límite elástico / Módulo de elasticidad: MPa (N/mm² = MN/m²)
 * - Tensión del terreno: MPa (SI), kp/cm² (MKS), ksf (Imperial)
 * - Módulo de balasto: kN/m³
 * - Peso lineal de perfil: kN/m (o kg/m según magnitud)
 * - Masa: kg
 * - Densidad: kg/m³
 * - Temperatura: °C
 * - Incremento térmico (salto térmico): Δ°C
 * - Ángulo: rad
 * - Rigidez rotacional: kN·m/rad
 * - Rigidez traslacional: kN/m
 */
enum class Magnitude : std::uint8_t {
    Length = 0,
    SectionDimension,
    Displacement,
    CrackWidth,
    SectionArea,
    ReinforcementArea,
    MomentOfInertia,
    SectionModulus,
    WarpingConstant,
    Force,
    Moment,
    LinearLoad,
    SurfaceLoad,
    Stress,
    SoilBearingPressure,
    SubgradeModulus,
    LinearMass,
    Mass,
    Density,
    Temperature,
    TemperatureDelta,
    Angle,
    AngleMillirad,
    RotationalStiffness,
    TranslationalStiffness
};

/**
 * @brief Descriptor de unidad de visualización y precisión para una magnitud en un sistema dado
 *        (equivalente a udusada_t / cyudcuso_t en unidades.dll y cyudconf.dll).
 */
struct UnitDescriptor {
    Magnitude magnitude{Magnitude::Length};
    UnitSystem system{UnitSystem::SI};
    std::string symbol;         ///< Sufijo de unidad p.ej. "m", "mm", "kN", "MPa", "kp/cm²", "kip·ft"
    double internal_to_display_scale{1.0};  ///< display = internal * scale + offset
    double internal_to_display_offset{0.0}; ///< Para temperatura absoluta (°C -> °F)
    int default_decimals{2};
    double default_step{0.1};
    double min_internal{-1.0e12};
    double max_internal{1.0e12};

    double to_display(double internal_val) const noexcept;
    double to_internal(double display_val) const noexcept;
    double round_display(double internal_val, int decimals = -1) const noexcept;
    double round_internal(double internal_val, int decimals = -1) const noexcept;
};

struct FormatOptions {
    int decimals{-1};               ///< -1 usa default_decimals del descriptor
    bool include_units{true};       ///< Añade el sufijo de unidades ("12.50 kN")
    bool brackets_around_units{false}; ///< Formato "[kN]" (cyudcuso_texto_con_unidades_entre_corchetes)
    bool trim_trailing_zeros{false};   ///< cypeud_texto_sin_ceros_al_final_u
    std::string header_prefix;      ///< Prefijo opcional (cypeud_texto_con_cabecera_u)
};

/**
 * @brief Magnitud física fuertemente tipada (valor almacenado siempre en unidad interna canónica).
 */
template <Magnitude Mag>
class PhysicalQuantity {
public:
    constexpr PhysicalQuantity() noexcept = default;
    constexpr explicit PhysicalQuantity(double internal_value) noexcept : value_(internal_value) {}

    static PhysicalQuantity from_internal(double val) noexcept { return PhysicalQuantity(val); }
    static PhysicalQuantity from_display(double display_val, UnitSystem sys) noexcept;

    constexpr double internal() const noexcept { return value_; }
    double in_system(UnitSystem sys) const noexcept;
    std::string format(UnitSystem sys, const FormatOptions& opts = {}) const;

    constexpr PhysicalQuantity operator+(const PhysicalQuantity& rhs) const noexcept { return PhysicalQuantity(value_ + rhs.value_); }
    constexpr PhysicalQuantity operator-(const PhysicalQuantity& rhs) const noexcept { return PhysicalQuantity(value_ - rhs.value_); }
    constexpr PhysicalQuantity operator*(double s) const noexcept { return PhysicalQuantity(value_ * s); }
    constexpr PhysicalQuantity operator/(double s) const noexcept { return PhysicalQuantity(value_ / s); }

private:
    double value_{0.0};
};

using LengthQty               = PhysicalQuantity<Magnitude::Length>;
using SectionDimQty           = PhysicalQuantity<Magnitude::SectionDimension>;
using DisplacementQty         = PhysicalQuantity<Magnitude::Displacement>;
using SectionAreaQty          = PhysicalQuantity<Magnitude::SectionArea>;
using MomentOfInertiaQty      = PhysicalQuantity<Magnitude::MomentOfInertia>;
using ForceQty                = PhysicalQuantity<Magnitude::Force>;
using MomentQty               = PhysicalQuantity<Magnitude::Moment>;
using LinearLoadQty           = PhysicalQuantity<Magnitude::LinearLoad>;
using SurfaceLoadQty          = PhysicalQuantity<Magnitude::SurfaceLoad>;
using StressQty               = PhysicalQuantity<Magnitude::Stress>;
using SoilPressureQty         = PhysicalQuantity<Magnitude::SoilBearingPressure>;
using TemperatureQty          = PhysicalQuantity<Magnitude::Temperature>;
using TemperatureDeltaQty     = PhysicalQuantity<Magnitude::TemperatureDelta>;
using RotationalStiffnessQty  = PhysicalQuantity<Magnitude::RotationalStiffness>;
using TranslationalStiffnessQty = PhysicalQuantity<Magnitude::TranslationalStiffness>;

/**
 * @brief Gestor de configuración global y pila de entorno de unidades (cyudcfgent_* / cypeud_* / magnitud_*).
 */
class UnitManager {
public:
    // Configuración activa y pila anidada (cyudcfgent_apila_unidades_entorno_anidado)
    static UnitSystem active_system() noexcept;
    static void set_active_system(UnitSystem sys) noexcept;
    static void push_environment_system(UnitSystem sys);
    static void pop_environment_system();
    static std::string_view system_name(UnitSystem sys) noexcept;

    // Obtención de descriptores por magnitud y sistema
    static const UnitDescriptor& descriptor(Magnitude mag, UnitSystem sys) noexcept;
    static const UnitDescriptor& active_descriptor(Magnitude mag) noexcept;

    // Conversión entre valor interno y externo (udusada_convertir_valor_externo_a_interno)
    static double to_display(Magnitude mag, double internal_val, UnitSystem sys) noexcept;
    static double to_internal(Magnitude mag, double display_val, UnitSystem sys) noexcept;

    static double to_active_display(Magnitude mag, double internal_val) noexcept {
        return to_display(mag, internal_val, active_system());
    }
    static double from_active_display(Magnitude mag, double display_val) noexcept {
        return to_internal(mag, display_val, active_system());
    }

    // Formateo de valores y unidades (cypeud_texto_*, cyudcuso_texto_*, convers_texto_*)
    static std::string format(Magnitude mag, double internal_val, UnitSystem sys, const FormatOptions& opts = {});
    static std::string format_active(Magnitude mag, double internal_val, const FormatOptions& opts = {}) {
        return format(mag, internal_val, active_system(), opts);
    }
    static std::string unit_symbol(Magnitude mag, UnitSystem sys) noexcept;

    // Parseo y validación de entrada numérica con sufijo opcional (udusada_valida_texto_numero)
    static std::optional<double> parse_to_internal(Magnitude mag, std::string_view text, UnitSystem sys) noexcept;

    // Factores de conversión directa equivalentes a basicos.dll!magnitud_* y cyudconf.dll!cyudctipo_*
    static constexpr double kGravity = 9.80665; // m/s² (1 t = 9.80665 kN; en CYPE MKS clásico también se provee factor exacto)
    static constexpr double kMetersPerFoot = 0.3048;
    static constexpr double kMetersPerInch = 0.0254;
    static constexpr double kPoundsPerKip  = 1000.0;
    static constexpr double kKilonewtonsPerKip = 4.4482216152605;
    static constexpr double kKilonewtonsPerTonf = 9.80665;
    static constexpr double kMPaPerKgfCm2 = 0.0980665;
    static constexpr double kMPaPerKsi = 6.894757293168361;
    static constexpr double kMPaPerPsi = 0.006894757293168361;

    static constexpr double kN_to_Tn(double kn) noexcept { return kn / kKilonewtonsPerTonf; }
    static constexpr double Tn_to_kN(double tn) noexcept { return tn * kKilonewtonsPerTonf; }
    static constexpr double kN_to_kip(double kn) noexcept { return kn / kKilonewtonsPerKip; }
    static constexpr double kip_to_kN(double kip) noexcept { return kip * kKilonewtonsPerKip; }
    static constexpr double m_to_ft(double m) noexcept { return m / kMetersPerFoot; }
    static constexpr double ft_to_m(double ft) noexcept { return ft * kMetersPerFoot; }
    static constexpr double m_to_in(double m) noexcept { return m / kMetersPerInch; }
    static constexpr double in_to_m(double in_val) noexcept { return in_val * kMetersPerInch; }
    static constexpr double MPa_to_kgf_cm2(double mpa) noexcept { return mpa / kMPaPerKgfCm2; }
    static constexpr double kgf_cm2_to_MPa(double kgcm2) noexcept { return kgcm2 * kMPaPerKgfCm2; }
    static constexpr double MPa_to_ksi(double mpa) noexcept { return mpa / kMPaPerKsi; }
    static constexpr double ksi_to_MPa(double ksi) noexcept { return ksi * kMPaPerKsi; }
    static constexpr double MPa_to_psi(double mpa) noexcept { return mpa / kMPaPerPsi; }
    static constexpr double psi_to_MPa(double psi) noexcept { return psi * kMPaPerPsi; }
    static constexpr double celsius_to_fahrenheit(double c) noexcept { return c * 1.8 + 32.0; }
    static constexpr double fahrenheit_to_celsius(double f) noexcept { return (f - 32.0) / 1.8; }
    static constexpr double delta_c_to_delta_f(double dc) noexcept { return dc * 1.8; }
    static constexpr double delta_f_to_delta_c(double df) noexcept { return df / 1.8; }
};

/**
 * @brief Guardián RAII para apilar/desapilar un sistema de unidades de entorno
 *        (equivalente a cyudcfgent_apila_unidades_entorno_anidado / desapila).
 */
class ScopedUnitSystem {
public:
    explicit ScopedUnitSystem(UnitSystem sys) {
        UnitManager::push_environment_system(sys);
    }
    ~ScopedUnitSystem() {
        UnitManager::pop_environment_system();
    }
    ScopedUnitSystem(const ScopedUnitSystem&) = delete;
    ScopedUnitSystem& operator=(const ScopedUnitSystem&) = delete;
};

template <Magnitude Mag>
PhysicalQuantity<Mag> PhysicalQuantity<Mag>::from_display(double display_val, UnitSystem sys) noexcept {
    return PhysicalQuantity<Mag>(UnitManager::to_internal(Mag, display_val, sys));
}

template <Magnitude Mag>
double PhysicalQuantity<Mag>::in_system(UnitSystem sys) const noexcept {
    return UnitManager::to_display(Mag, value_, sys);
}

template <Magnitude Mag>
std::string PhysicalQuantity<Mag>::format(UnitSystem sys, const FormatOptions& opts) const {
    return UnitManager::format(Mag, value_, sys, opts);
}

} // namespace cypeclon::units

