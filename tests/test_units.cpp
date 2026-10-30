#include "units/units.hpp"
#include "core/math.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void assert_units(bool cond, const char* expr, int line) {
    if (!cond) {
        std::cerr << "[FAIL test_units.cpp:" << line << "] " << expr << "\n";
        std::exit(1);
    }
}
#define UNITS_ASSERT(expr) assert_units((expr), #expr, __LINE__)
} // namespace

void run_units_tests() {
    using namespace cypeclon::units;
    using namespace cypeclon::core;

    UnitManager::set_active_system(UnitSystem::SI);
    UNITS_ASSERT(UnitManager::active_system() == UnitSystem::SI);

    // 1. Prueba de Fuerza en SI (kN), MKS (t) e Imperial (kip)
    const ForceQty f_si = ForceQty::from_internal(98.0665); // 98.0665 kN = 10.00 t
    UNITS_ASSERT(son_iguales(f_si.in_system(UnitSystem::SI), 98.0665, 1.0e-4));
    UNITS_ASSERT(son_iguales(f_si.in_system(UnitSystem::MKS), 10.0, 1.0e-4));
    UNITS_ASSERT(f_si.format(UnitSystem::MKS) == "10.00 t");
    UNITS_ASSERT(f_si.format(UnitSystem::SI, {2, true, true, false, ""}) == "98.07 [kN]");
    UNITS_ASSERT(f_si.format(UnitSystem::MKS, {2, true, false, true, "N ="}) == "N = 10 t");

    // 2. Prueba de Tensión (MPa <-> kp/cm² <-> ksi)
    const StressQty fy_s275 = StressQty::from_internal(275.0); // 275 MPa
    UNITS_ASSERT(fy_s275.format(UnitSystem::SI) == "275.00 MPa");
    UNITS_ASSERT(son_iguales(fy_s275.in_system(UnitSystem::MKS), 2804.22, 0.05));
    UNITS_ASSERT(son_iguales(fy_s275.in_system(UnitSystem::Imperial), 39.885, 0.01));

    // 3. Prueba de Longitud y Dimensión de Sección (m, mm, ft, in)
    const LengthQty span_ft = LengthQty::from_display(10.0, UnitSystem::Imperial); // 10 ft = 3.048 m
    UNITS_ASSERT(son_iguales(span_ft.internal(), 3.048, 1.0e-6));
    UNITS_ASSERT(span_ft.format(UnitSystem::SI, {3, true, false, false, ""}) == "3.048 m");

    const SectionDimQty canto_ipe200 = SectionDimQty::from_display(200.0, UnitSystem::SI); // 200 mm = 0.20 m
    UNITS_ASSERT(son_iguales(canto_ipe200.internal(), 0.20, 1.0e-6));
    UNITS_ASSERT(canto_ipe200.format(UnitSystem::SI) == "200.0 mm");

    // 4. Prueba de Temperatura absoluta (°C <-> °F) y Salto Térmico (Δ°C <-> Δ°F)
    const TemperatureQty temp_20c = TemperatureQty::from_internal(20.0);
    UNITS_ASSERT(son_iguales(temp_20c.in_system(UnitSystem::Imperial), 68.0, 1.0e-5));
    UNITS_ASSERT(temp_20c.format(UnitSystem::Imperial) == "68.0 °F");

    const TemperatureDeltaQty delta_30c = TemperatureDeltaQty::from_internal(30.0);
    UNITS_ASSERT(son_iguales(delta_30c.in_system(UnitSystem::Imperial), 54.0, 1.0e-5));
    UNITS_ASSERT(delta_30c.format(UnitSystem::Imperial) == "54.0 °F");

    // 5. Pila de unidades de entorno anidado (ScopedUnitSystem / cyudcfgent_apila_unidades_entorno_anidado)
    {
        ScopedUnitSystem mks_scope(UnitSystem::MKS);
        UNITS_ASSERT(UnitManager::active_system() == UnitSystem::MKS);
        UNITS_ASSERT(UnitManager::format_active(Magnitude::Force, 9.80665) == "1.00 t");
        {
            ScopedUnitSystem imp_scope(UnitSystem::Imperial);
            UNITS_ASSERT(UnitManager::active_system() == UnitSystem::Imperial);
            UNITS_ASSERT(UnitManager::format_active(Magnitude::Length, 0.3048) == "1.00 ft");
        }
        UNITS_ASSERT(UnitManager::active_system() == UnitSystem::MKS);
    }
    UNITS_ASSERT(UnitManager::active_system() == UnitSystem::SI);

    // 6. Parseo de textos con unidades (udusada_valida_texto_numero)
    const auto parsed_len = UnitManager::parse_to_internal(Magnitude::Length, "250 cm", UnitSystem::SI);
    UNITS_ASSERT(parsed_len.has_value() && son_iguales(*parsed_len, 2.50));

    const auto parsed_force = UnitManager::parse_to_internal(Magnitude::Force, "15,5 [kN]", UnitSystem::SI);
    UNITS_ASSERT(parsed_force.has_value() && son_iguales(*parsed_force, 15.5));
}

