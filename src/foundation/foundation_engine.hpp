#pragma once

#include "model/obra.hpp"

namespace cypeclon::foundation {

class FoundationEngine {
public:
    static std::size_t count_elements(const model::Obra& obra) noexcept;
};

} // namespace cypeclon::foundation
