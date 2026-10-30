#include "units/units.hpp"
#include "core/math.hpp"
#include "core/string_utils.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace cypeclon::units {

double UnitDescriptor::to_display(double internal_val) const noexcept {
    return internal_val * internal_to_display_scale + internal_to_display_offset;
}

double UnitDescriptor::to_internal(double display_val) const noexcept {
    if (std::abs(internal_to_display_scale) < 1.0e-18) {
        return 0.0;
    }
    return (display_val - internal_to_display_offset) / internal_to_display_scale;
}

double UnitDescriptor::round_display(double internal_val, int decimals) const noexcept {
    const int dec = (decimals >= 0) ? decimals : default_decimals;
    return core::round_decimals(to_display(internal_val), dec);
}

double UnitDescriptor::round_internal(double internal_val, int decimals) const noexcept {
    return to_internal(round_display(internal_val, decimals));
}

namespace {

struct UnitEnvState {
    std::mutex mutex;
    UnitSystem global_system{UnitSystem::SI};
    std::vector<UnitSystem> stack;
};

UnitEnvState& env_state() {
    static UnitEnvState s;
    return s;
}

constexpr std::size_t kMagCount = 25;
constexpr std::size_t kSysCount = 3;

using DescriptorTable = std::array<std::array<UnitDescriptor, kMagCount>, kSysCount>;

DescriptorTable build_descriptor_table() {
    DescriptorTable tbl{};

    constexpr double kInPerM  = 1.0 / UnitManager::kMetersPerInch; // 39.37007874015748
    constexpr double kFtPerM  = 1.0 / UnitManager::kMetersPerFoot; // 3.280839895013123
    constexpr double kIn2PerM2 = kInPerM * kInPerM;
    constexpr double kIn3PerM3 = kIn2PerM2 * kInPerM;
    constexpr double kIn4PerM4 = kIn2PerM2 * kIn2PerM2;
    constexpr double kIn6PerM6 = kIn3PerM3 * kIn3PerM3;

    constexpr double kTnPerKN  = 1.0 / UnitManager::kKilonewtonsPerTonf; // 0.10197162129779283
    constexpr double kKipPerKN = 1.0 / UnitManager::kKilonewtonsPerKip; // 0.2248089430997105
    constexpr double kKipFtPerKNm = kKipPerKN * kFtPerM;
    constexpr double kKipPerKN_div_FtPerM = kKipPerKN / kFtPerM; // kip/ft por kN/m
    constexpr double kKsfPerKNm2 = kKipPerKN / (kFtPerM * kFtPerM); // kip/ft² por kN/m²
    constexpr double kKgfCm2PerMPa = 1.0 / UnitManager::kMPaPerKgfCm2; // 10.197162129779283
    constexpr double kKsiPerMPa = 1.0 / UnitManager::kMPaPerKsi; // 0.14503773773020923
    constexpr double kKsfPerMPa = kKsfPerKNm2 * 1000.0; // 1 MPa = 1000 kN/m² -> 20.885434233 ksf
    constexpr double kPciPerKNm3 = (kKipPerKN * 1000.0) / kIn3PerM3; // lb/in³ por kN/m³
    constexpr double kLbFtPerKgM = 2.2046226218487757 / kFtPerM;
    constexpr double kLbPerKg = 2.2046226218487757;
    constexpr double kLbFt3PerKgM3 = kLbPerKg / (kFtPerM * kFtPerM * kFtPerM);
    constexpr double kKipInPerKNm = kKipPerKN / kInPerM; // kip/in por kN/m

    auto reg = [&](UnitSystem sys, Magnitude mag, const char* sym, double scale, double offset, int dec, double step) {
        tbl[static_cast<std::size_t>(sys)][static_cast<std::size_t>(mag)] = UnitDescriptor{
            mag, sys, sym, scale, offset, dec, step, -1.0e12, 1.0e12
        };
    };

    // 1. Sistema Internacional (SI)
    reg(UnitSystem::SI, Magnitude::Length,                 "m",        1.0,       0.0, 2, 0.05);
    reg(UnitSystem::SI, Magnitude::SectionDimension,       "mm",       1000.0,    0.0, 1, 1.0);
    reg(UnitSystem::SI, Magnitude::Displacement,           "mm",       1000.0,    0.0, 2, 0.1);
    reg(UnitSystem::SI, Magnitude::CrackWidth,             "mm",       1000.0,    0.0, 2, 0.05);
    reg(UnitSystem::SI, Magnitude::SectionArea,            "cm²",      1.0e4,     0.0, 2, 0.1);
    reg(UnitSystem::SI, Magnitude::ReinforcementArea,      "cm²",      1.0e4,     0.0, 2, 0.1);
    reg(UnitSystem::SI, Magnitude::MomentOfInertia,        "cm⁴",      1.0e8,     0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::SectionModulus,         "cm³",      1.0e6,     0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::WarpingConstant,        "cm⁶",      1.0e12,    0.0, 0, 10.0);
    reg(UnitSystem::SI, Magnitude::Force,                  "kN",       1.0,       0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::Moment,                 "kN·m",     1.0,       0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::LinearLoad,             "kN/m",     1.0,       0.0, 2, 0.5);
    reg(UnitSystem::SI, Magnitude::SurfaceLoad,            "kN/m²",    1.0,       0.0, 2, 0.5);
    reg(UnitSystem::SI, Magnitude::Stress,                 "MPa",      1.0,       0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::SoilBearingPressure,    "MPa",      1.0,       0.0, 3, 0.05);
    reg(UnitSystem::SI, Magnitude::SubgradeModulus,        "kN/m³",    1.0,       0.0, 0, 1000.0);
    reg(UnitSystem::SI, Magnitude::LinearMass,             "kg/m",     1.0,       0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::Mass,                   "kg",       1.0,       0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::Density,                "kg/m³",    1.0,       0.0, 0, 50.0);
    reg(UnitSystem::SI, Magnitude::Temperature,            "°C",       1.0,       0.0, 1, 1.0);
    reg(UnitSystem::SI, Magnitude::TemperatureDelta,       "°C",       1.0,       0.0, 1, 1.0);
    reg(UnitSystem::SI, Magnitude::Angle,                  "°",        core::kRadToDeg, 0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::AngleMillirad,          "mrad",     1000.0,    0.0, 2, 1.0);
    reg(UnitSystem::SI, Magnitude::RotationalStiffness,    "kN·m/rad", 1.0,       0.0, 1, 100.0);
    reg(UnitSystem::SI, Magnitude::TranslationalStiffness, "kN/m",     1.0,       0.0, 1, 100.0);

    // 2. Sistema MKS Técnico (MKS)
    reg(UnitSystem::MKS, Magnitude::Length,                 "m",        1.0,            0.0, 2, 0.05);
    reg(UnitSystem::MKS, Magnitude::SectionDimension,       "mm",       1000.0,         0.0, 1, 1.0);
    reg(UnitSystem::MKS, Magnitude::Displacement,           "mm",       1000.0,         0.0, 2, 0.1);
    reg(UnitSystem::MKS, Magnitude::CrackWidth,             "mm",       1000.0,         0.0, 2, 0.05);
    reg(UnitSystem::MKS, Magnitude::SectionArea,            "cm²",      1.0e4,          0.0, 2, 0.1);
    reg(UnitSystem::MKS, Magnitude::ReinforcementArea,      "cm²",      1.0e4,          0.0, 2, 0.1);
    reg(UnitSystem::MKS, Magnitude::MomentOfInertia,        "cm⁴",      1.0e8,          0.0, 2, 1.0);
    reg(UnitSystem::MKS, Magnitude::SectionModulus,         "cm³",      1.0e6,          0.0, 2, 1.0);
    reg(UnitSystem::MKS, Magnitude::WarpingConstant,        "cm⁶",      1.0e12,         0.0, 0, 10.0);
    reg(UnitSystem::MKS, Magnitude::Force,                  "t",        kTnPerKN,       0.0, 2, 0.1);
    reg(UnitSystem::MKS, Magnitude::Moment,                 "t·m",      kTnPerKN,       0.0, 2, 0.1);
    reg(UnitSystem::MKS, Magnitude::LinearLoad,             "t/m",      kTnPerKN,       0.0, 2, 0.05);
    reg(UnitSystem::MKS, Magnitude::SurfaceLoad,            "t/m²",     kTnPerKN,       0.0, 2, 0.05);
    reg(UnitSystem::MKS, Magnitude::Stress,                 "kp/cm²",   kKgfCm2PerMPa,  0.0, 1, 10.0);
    reg(UnitSystem::MKS, Magnitude::SoilBearingPressure,    "kp/cm²",   kKgfCm2PerMPa,  0.0, 2, 0.1);
    reg(UnitSystem::MKS, Magnitude::SubgradeModulus,        "t/m³",     kTnPerKN,       0.0, 0, 100.0);
    reg(UnitSystem::MKS, Magnitude::LinearMass,             "kg/m",     1.0,            0.0, 2, 1.0);
    reg(UnitSystem::MKS, Magnitude::Mass,                   "kg",       1.0,            0.0, 2, 1.0);
    reg(UnitSystem::MKS, Magnitude::Density,                "kg/m³",    1.0,            0.0, 0, 50.0);
    reg(UnitSystem::MKS, Magnitude::Temperature,            "°C",       1.0,            0.0, 1, 1.0);
    reg(UnitSystem::MKS, Magnitude::TemperatureDelta,       "°C",       1.0,            0.0, 1, 1.0);
    reg(UnitSystem::MKS, Magnitude::Angle,                  "°",        core::kRadToDeg, 0.0, 2, 1.0);
    reg(UnitSystem::MKS, Magnitude::AngleMillirad,          "mrad",     1000.0,         0.0, 2, 1.0);
    reg(UnitSystem::MKS, Magnitude::RotationalStiffness,    "t·m/rad",  kTnPerKN,       0.0, 1, 10.0);
    reg(UnitSystem::MKS, Magnitude::TranslationalStiffness, "t/m",      kTnPerKN,       0.0, 1, 10.0);

    // 3. Sistema Imperial (kip, ft, in, ksi, °F)
    reg(UnitSystem::Imperial, Magnitude::Length,                 "ft",         kFtPerM,                0.0,  2, 0.5);
    reg(UnitSystem::Imperial, Magnitude::SectionDimension,       "in",         kInPerM,                0.0,  2, 0.125);
    reg(UnitSystem::Imperial, Magnitude::Displacement,           "in",         kInPerM,                0.0,  3, 0.05);
    reg(UnitSystem::Imperial, Magnitude::CrackWidth,             "in",         kInPerM,                0.0,  4, 0.002);
    reg(UnitSystem::Imperial, Magnitude::SectionArea,            "in²",        kIn2PerM2,              0.0,  2, 0.1);
    reg(UnitSystem::Imperial, Magnitude::ReinforcementArea,      "in²",        kIn2PerM2,              0.0,  2, 0.05);
    reg(UnitSystem::Imperial, Magnitude::MomentOfInertia,        "in⁴",        kIn4PerM4,              0.0,  2, 1.0);
    reg(UnitSystem::Imperial, Magnitude::SectionModulus,         "in³",        kIn3PerM3,              0.0,  2, 0.5);
    reg(UnitSystem::Imperial, Magnitude::WarpingConstant,        "in⁶",        kIn6PerM6,              0.0,  1, 1.0);
    reg(UnitSystem::Imperial, Magnitude::Force,                  "kip",        kKipPerKN,              0.0,  2, 0.5);
    reg(UnitSystem::Imperial, Magnitude::Moment,                 "kip·ft",     kKipFtPerKNm,           0.0,  2, 0.5);
    reg(UnitSystem::Imperial, Magnitude::LinearLoad,             "kip/ft",     kKipPerKN_div_FtPerM,   0.0,  3, 0.05);
    reg(UnitSystem::Imperial, Magnitude::SurfaceLoad,            "ksf",        kKsfPerKNm2,            0.0,  3, 0.01);
    reg(UnitSystem::Imperial, Magnitude::Stress,                 "ksi",        kKsiPerMPa,             0.0,  2, 0.5);
    reg(UnitSystem::Imperial, Magnitude::SoilBearingPressure,    "ksf",        kKsfPerMPa,             0.0,  2, 0.5);
    reg(UnitSystem::Imperial, Magnitude::SubgradeModulus,        "pci",        kPciPerKNm3,            0.0,  1, 10.0);
    reg(UnitSystem::Imperial, Magnitude::LinearMass,             "lb/ft",      kLbFtPerKgM,            0.0,  2, 1.0);
    reg(UnitSystem::Imperial, Magnitude::Mass,                   "lb",         kLbPerKg,               0.0,  2, 1.0);
    reg(UnitSystem::Imperial, Magnitude::Density,                "lb/ft³",     kLbFt3PerKgM3,          0.0,  1, 5.0);
    reg(UnitSystem::Imperial, Magnitude::Temperature,            "°F",         1.8,                    32.0, 1, 1.0);
    reg(UnitSystem::Imperial, Magnitude::TemperatureDelta,       "°F",         1.8,                    0.0,  1, 1.0);
    reg(UnitSystem::Imperial, Magnitude::Angle,                  "°",          core::kRadToDeg,        0.0,  2, 1.0);
    reg(UnitSystem::Imperial, Magnitude::AngleMillirad,          "mrad",       1000.0,                 0.0,  2, 1.0);
    reg(UnitSystem::Imperial, Magnitude::RotationalStiffness,    "kip·ft/rad", kKipFtPerKNm,           0.0,  1, 50.0);
    reg(UnitSystem::Imperial, Magnitude::TranslationalStiffness, "kip/in",     kKipInPerKNm,           0.0,  2, 10.0);

    return tbl;
}

const DescriptorTable& table() {
    static const DescriptorTable t = build_descriptor_table();
    return t;
}

} // namespace

UnitSystem UnitManager::active_system() noexcept {
    auto& s = env_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.stack.empty() ? s.global_system : s.stack.back();
}

void UnitManager::set_active_system(UnitSystem sys) noexcept {
    auto& s = env_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.global_system = sys;
}

void UnitManager::push_environment_system(UnitSystem sys) {
    auto& s = env_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.stack.push_back(sys);
}

void UnitManager::pop_environment_system() {
    auto& s = env_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (!s.stack.empty()) {
        s.stack.pop_back();
    }
}

std::string_view UnitManager::system_name(UnitSystem sys) noexcept {
    switch (sys) {
        case UnitSystem::SI:       return "Sistema Internacional (S.I.)";
        case UnitSystem::MKS:      return "Sistema Técnico (M.K.S.)";
        case UnitSystem::Imperial: return "Sistema Anglosajón (Imperial)";
    }
    return "Sistema Internacional (S.I.)";
}

const UnitDescriptor& UnitManager::descriptor(Magnitude mag, UnitSystem sys) noexcept {
    const auto sys_idx = static_cast<std::size_t>(sys);
    const auto mag_idx = static_cast<std::size_t>(mag);
    return table()[sys_idx < kSysCount ? sys_idx : 0][mag_idx < kMagCount ? mag_idx : 0];
}

const UnitDescriptor& UnitManager::active_descriptor(Magnitude mag) noexcept {
    return descriptor(mag, active_system());
}

double UnitManager::to_display(Magnitude mag, double internal_val, UnitSystem sys) noexcept {
    return descriptor(mag, sys).to_display(internal_val);
}

double UnitManager::to_internal(Magnitude mag, double display_val, UnitSystem sys) noexcept {
    return descriptor(mag, sys).to_internal(display_val);
}

std::string UnitManager::unit_symbol(Magnitude mag, UnitSystem sys) noexcept {
    return descriptor(mag, sys).symbol;
}

std::string UnitManager::format(Magnitude mag, double internal_val, UnitSystem sys, const FormatOptions& opts) {
    const auto& desc = descriptor(mag, sys);
    const int dec = (opts.decimals >= 0) ? opts.decimals : desc.default_decimals;
    double val = desc.round_display(internal_val, dec);
    if (std::abs(val) < 1.0e-12) {
        val = 0.0; // Evitar "-0.00"
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(dec) << val;
    std::string num_str = oss.str();

    if (opts.trim_trailing_zeros && dec > 0) {
        const auto dot_pos = num_str.find('.');
        if (dot_pos != std::string::npos) {
            while (!num_str.empty() && num_str.back() == '0') {
                num_str.pop_back();
            }
            if (!num_str.empty() && num_str.back() == '.') {
                num_str.pop_back();
            }
        }
    }

    std::string result;
    if (!opts.header_prefix.empty()) {
        result += opts.header_prefix;
        if (result.back() != ' ') {
            result.push_back(' ');
        }
    }
    result += num_str;

    if (opts.include_units && !desc.symbol.empty()) {
        result.push_back(' ');
        if (opts.brackets_around_units) {
            result.push_back('[');
            result += desc.symbol;
            result.push_back(']');
        } else {
            result += desc.symbol;
        }
    }
    return result;
}

std::optional<double> UnitManager::parse_to_internal(Magnitude mag, std::string_view text, UnitSystem sys) noexcept {
    std::string cleaned = core::StringUtils::trim(text);
    if (cleaned.empty()) {
        return std::nullopt;
    }

    // Permitir coma decimal o punto decimal
    for (char& ch : cleaned) {
        if (ch == ',') ch = '.';
    }

    char* end_ptr = nullptr;
    const double parsed = std::strtod(cleaned.c_str(), &end_ptr);
    if (end_ptr == cleaned.c_str()) {
        return std::nullopt;
    }

    std::string suffix = core::StringUtils::trim(std::string_view(end_ptr));
    const auto& desc = descriptor(mag, sys);
    if (!suffix.empty()) {
        // Si tiene corchetes [kN], quitarlos
        if (suffix.size() >= 2 && suffix.front() == '[' && suffix.back() == ']') {
            suffix = core::StringUtils::trim(std::string_view(suffix).substr(1, suffix.size() - 2));
        }
        if (!core::StringUtils::equals_ignore_case(suffix, desc.symbol)) {
            // Permitir conversión explícita para sufijos alternativos frecuentes en longitud o fuerza
            if (mag == Magnitude::Length) {
                if (suffix == "mm") return parsed * 0.001;
                if (suffix == "cm") return parsed * 0.01;
                if (suffix == "m")  return parsed;
                if (suffix == "ft") return ft_to_m(parsed);
                if (suffix == "in") return in_to_m(parsed);
            } else if (mag == Magnitude::Force) {
                if (core::StringUtils::equals_ignore_case(suffix, "kN")) return parsed;
                if (core::StringUtils::equals_ignore_case(suffix, "N"))  return parsed * 0.001;
                if (core::StringUtils::equals_ignore_case(suffix, "t") ||
                    core::StringUtils::equals_ignore_case(suffix, "Tn")) return Tn_to_kN(parsed);
                if (core::StringUtils::equals_ignore_case(suffix, "kip") ||
                    core::StringUtils::equals_ignore_case(suffix, "kips")) return kip_to_kN(parsed);
            }
            return std::nullopt;
        }
    }

    return desc.to_internal(parsed);
}

} // namespace cypeclon::units

