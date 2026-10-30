#include "foundation/foundation_engine.hpp"

namespace cypeclon::foundation {

std::size_t FoundationEngine::count_elements(const model::Obra& obra) noexcept {
    return obra.cimentacion().elementos.size();
}

} // namespace cypeclon::foundation
