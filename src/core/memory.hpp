#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cypeclon::core {

struct AllocationRecord {
    const void* address{nullptr};
    std::size_t size{0};
    std::string tag;
    std::uint64_t sequence_id{0};
};

struct MemoryStats {
    std::size_t active_allocations{0};
    std::size_t active_bytes{0};
    std::size_t peak_bytes{0};
    std::uint64_t total_allocations{0};
    std::uint64_t total_frees{0};
    bool tracking_enabled{true};
    bool heap_check_enabled{false};
};

/**
 * @brief Subsistema de gestión y diagnóstico de memoria equivalente a cypemem.dll (cypespy_*).
 */
class MemoryTracker {
public:
    static void inicia() noexcept;
    static void fin() noexcept;
    static bool con_memleak() noexcept;
    static void activa_comprobacion_heapchk(bool enable) noexcept;
    static bool heapchk() noexcept;

    static void* allocate(std::size_t size, const char* tag = "cypemem") noexcept;
    static void* callocate(std::size_t count, std::size_t size, const char* tag = "cypemem") noexcept;
    static void* reallocate(void* ptr, std::size_t new_size, const char* tag = "cypemem") noexcept;
    static void deallocate(void* ptr) noexcept;

    template <typename T>
    static void free_pp(T** pp) noexcept {
        if (pp && *pp) {
            deallocate(static_cast<void*>(*pp));
            *pp = nullptr;
        }
    }

    static bool es_memoria_reservada_valida(const void* ptr) noexcept;
    static std::size_t msize(const void* ptr) noexcept;
    static std::size_t memoria_actual_programa() noexcept;
    static MemoryStats stats() noexcept;
    static std::vector<AllocationRecord> array_direcciones_memoria();
    static std::string volcado_diferencia_memoria(std::uint64_t since_sequence_id = 0);
};

} // namespace cypeclon::core

