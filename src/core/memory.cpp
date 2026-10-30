#include "core/memory.hpp"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace cypeclon::core {

namespace {

struct TrackerState {
    std::mutex mutex;
    bool initialized{true};
    bool heap_check{false};
    std::size_t active_bytes{0};
    std::size_t peak_bytes{0};
    std::uint64_t total_allocs{0};
    std::uint64_t total_frees{0};
    std::uint64_t next_seq{1};
    std::unordered_map<const void*, AllocationRecord> records;
};

TrackerState& state() {
    static TrackerState s;
    return s;
}

} // namespace

void MemoryTracker::inicia() noexcept {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.initialized = true;
}

void MemoryTracker::fin() noexcept {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.initialized = false;
}

bool MemoryTracker::con_memleak() noexcept {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return !s.records.empty();
}

void MemoryTracker::activa_comprobacion_heapchk(bool enable) noexcept {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.heap_check = enable;
}

bool MemoryTracker::heapchk() noexcept {
#ifdef _WIN32
    const int status = _heapchk();
    return status == _HEAPOK || status == _HEAPEMPTY;
#else
    return true;
#endif
}

void* MemoryTracker::allocate(std::size_t size, const char* tag) noexcept {
    const std::size_t actual_size = (size == 0) ? 1 : size;
    void* ptr = std::malloc(actual_size);
    if (!ptr) {
        return nullptr;
    }

    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.initialized) {
        s.active_bytes += actual_size;
        if (s.active_bytes > s.peak_bytes) {
            s.peak_bytes = s.active_bytes;
        }
        ++s.total_allocs;
        s.records[ptr] = AllocationRecord{
            ptr,
            actual_size,
            tag ? tag : "cypemem",
            s.next_seq++
        };
    }
    return ptr;
}

void* MemoryTracker::callocate(std::size_t count, std::size_t size, const char* tag) noexcept {
    const std::size_t total = (count == 0 || size == 0) ? 1 : (count * size);
    void* ptr = allocate(total, tag);
    if (ptr) {
        std::memset(ptr, 0, total);
    }
    return ptr;
}

void* MemoryTracker::reallocate(void* ptr, std::size_t new_size, const char* tag) noexcept {
    if (!ptr) {
        return allocate(new_size, tag);
    }
    if (new_size == 0) {
        deallocate(ptr);
        return nullptr;
    }

    auto& s = state();
    std::size_t old_size = 0;
    std::string old_tag = tag ? tag : "cypemem";
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        auto it = s.records.find(ptr);
        if (it != s.records.end()) {
            old_size = it->second.size;
            if (!tag) {
                old_tag = it->second.tag;
            }
            s.records.erase(it);
            s.active_bytes -= old_size;
        }
    }

    void* new_ptr = std::realloc(ptr, new_size);
    if (!new_ptr) {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (old_size > 0) {
            s.active_bytes += old_size;
            s.records[ptr] = AllocationRecord{ptr, old_size, old_tag, s.next_seq++};
        }
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.active_bytes += new_size;
        if (s.active_bytes > s.peak_bytes) {
            s.peak_bytes = s.active_bytes;
        }
        ++s.total_allocs;
        s.records[new_ptr] = AllocationRecord{new_ptr, new_size, old_tag, s.next_seq++};
    }
    return new_ptr;
}

void MemoryTracker::deallocate(void* ptr) noexcept {
    if (!ptr) {
        return;
    }
    {
        auto& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        auto it = s.records.find(ptr);
        if (it != s.records.end()) {
            if (s.active_bytes >= it->second.size) {
                s.active_bytes -= it->second.size;
            } else {
                s.active_bytes = 0;
            }
            s.records.erase(it);
        }
        ++s.total_frees;
    }
    std::free(ptr);
}

bool MemoryTracker::es_memoria_reservada_valida(const void* ptr) noexcept {
    if (!ptr) {
        return false;
    }
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.records.find(ptr) != s.records.end();
}

std::size_t MemoryTracker::msize(const void* ptr) noexcept {
    if (!ptr) {
        return 0;
    }
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto it = s.records.find(ptr);
    return (it != s.records.end()) ? it->second.size : 0;
}

std::size_t MemoryTracker::memoria_actual_programa() noexcept {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.active_bytes;
}

MemoryStats MemoryTracker::stats() noexcept {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return MemoryStats{
        s.records.size(),
        s.active_bytes,
        s.peak_bytes,
        s.total_allocs,
        s.total_frees,
        s.initialized,
        s.heap_check
    };
}

std::vector<AllocationRecord> MemoryTracker::array_direcciones_memoria() {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    std::vector<AllocationRecord> result;
    result.reserve(s.records.size());
    for (const auto& [addr, rec] : s.records) {
        result.push_back(rec);
    }
    return result;
}

std::string MemoryTracker::volcado_diferencia_memoria(std::uint64_t since_sequence_id) {
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    std::ostringstream oss;
    std::size_t leaked_count = 0;
    std::size_t leaked_bytes = 0;
    for (const auto& [addr, rec] : s.records) {
        if (rec.sequence_id > since_sequence_id) {
            ++leaked_count;
            leaked_bytes += rec.size;
            oss << "[cypespy] seq=" << rec.sequence_id
                << " addr=" << rec.address
                << " bytes=" << rec.size
                << " tag=" << rec.tag << "\n";
        }
    }
    oss << "[cypespy] Resumen: " << leaked_count << " bloques activos (" << leaked_bytes << " bytes)";
    return oss.str();
}

} // namespace cypeclon::core

