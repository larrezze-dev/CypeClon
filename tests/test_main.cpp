#include "core/memory.hpp"
#include "core/string_utils.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

// Declaraciones de suites de prueba de Fases 1, 2, 3, 4 y 5
void run_math_tests();
void run_units_tests();
void run_io_undoredo_tests();
void run_gui_phase2_tests();
void run_cad_phase3_tests();
void run_codes_phase4_tests();
void run_structure_phase5_tests();

namespace {

void check(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define TEST_ASSERT(expr) check((expr), #expr, __FILE__, __LINE__)

void run_memory_and_string_tests() {
    using namespace cypeclon::core;

    MemoryTracker::inicia();
    const std::size_t base_bytes = MemoryTracker::memoria_actual_programa();

    int* arr = static_cast<int*>(MemoryTracker::callocate(16, sizeof(int), "test_arr"));
    TEST_ASSERT(arr != nullptr);
    TEST_ASSERT(MemoryTracker::es_memoria_reservada_valida(arr));
    TEST_ASSERT(MemoryTracker::msize(arr) == 16 * sizeof(int));
    TEST_ASSERT(MemoryTracker::memoria_actual_programa() == base_bytes + 16 * sizeof(int));

    arr = static_cast<int*>(MemoryTracker::reallocate(arr, 32 * sizeof(int), "test_arr_realloc"));
    TEST_ASSERT(arr != nullptr);
    TEST_ASSERT(MemoryTracker::msize(arr) == 32 * sizeof(int));

    MemoryTracker::free_pp(&arr);
    TEST_ASSERT(arr == nullptr);
    TEST_ASSERT(MemoryTracker::memoria_actual_programa() == base_bytes);

    // Pruebas de cypestr / StringUtils
    TEST_ASSERT(StringUtils::trim("   CYPE 3D 2027 \t\n ") == "CYPE 3D 2027");
    TEST_ASSERT(StringUtils::collapse_whitespace("  Acero   laminado   S275  ") == "Acero laminado S275");
    TEST_ASSERT(StringUtils::equals_ignore_case("Eurocodigo 3", "EUROCODIGO 3"));
    TEST_ASSERT(StringUtils::strip_accents("Cimentación y Hormigón") == "Cimentacion y Hormigon");

    const auto parts = StringUtils::split("IPE 200, HEB 300, UPN 140", ',', true);
    TEST_ASSERT(parts.size() == 3 && parts[0] == "IPE 200" && parts[1] == "HEB 300" && parts[2] == "UPN 140");

    const std::wstring wide = StringUtils::utf8_to_wide("Cálculo de Láminas");
    TEST_ASSERT(!wide.empty());
    TEST_ASSERT(StringUtils::wide_to_utf8(wide) == "Cálculo de Láminas");

    const std::uint8_t raw_bytes[] = {0x43, 0x59, 0x50, 0x45, 0x33, 0x44};
    const std::string hex = StringUtils::to_hex(raw_bytes, true);
    TEST_ASSERT(hex == "435950453344");
    const auto decoded_hex = StringUtils::from_hex(hex);
    TEST_ASSERT(decoded_hex.size() == 6 && decoded_hex[0] == 0x43 && decoded_hex[5] == 0x44);

    const std::string b64 = StringUtils::to_base64(raw_bytes);
    const auto decoded_b64 = StringUtils::from_base64(b64);
    TEST_ASSERT(decoded_b64.size() == 6 && decoded_b64[1] == 0x59 && decoded_b64[4] == 0x33);
}

} // namespace

int main() {
    std::cout << "=== Ejecutando Suite de Pruebas Unitarias CypeClon (Fases 1, 2, 3 y 4) ===" << std::endl;
    run_memory_and_string_tests();
    std::cout << "[PASS] cypemem + cypestr" << std::endl;

    run_math_tests();
    std::cout << "[PASS] cypemath + ejes2d + geometria 2D/3D" << std::endl;

    run_units_tests();
    std::cout << "[PASS] unidades + cypeud + cyudconf (SI / MKS / Imperial)" << std::endl;

    run_io_undoredo_tests();
    std::cout << "[PASS] cypesave (Binario + JSON) + undoredo + Obra + Octree3d" << std::endl;

    run_gui_phase2_tests();
    std::cout << "[PASS] Fase 2 GUI: entograf + componen + panelwin + ribbon + entomenu + ed3dlib + planocim" << std::endl;

    run_cad_phase3_tests();
    std::cout << "[PASS] Fase 3 CAD/MDI: dispoMDI + pndibposecam + ebaobcap + dxflib + cyplanobj + cyhlr + vistas3d" << std::endl;

    run_codes_phase4_tests();
    std::cout << "[PASS] Fase 4 Normas/Materiales/Acciones/Porticos: cypenorma + datmater + sestlim + combs + sismo + veviento + rstfuego + ed3genport" << std::endl;

    run_structure_phase5_tests();
    std::cout << "[PASS] Fase 5 Nudos/Barras/Perfiles/Pandeo/Flecha/Pilares/Vigas: calcesf + bibperf3 + seccion + elements + compflecha + pushover + pilares + porvigas + discap" << std::endl;

    std::cout << "=== TODAS LAS PRUEBAS DE FASES 1, 2, 3, 4 Y 5 SUPERADAS CON EXITO ===" << std::endl;
    return 0;
}
