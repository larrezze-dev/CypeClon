#pragma once

#include <string_view>

namespace cypeclon::codes {

class NormativaEngine {
public:
    static std::string_view active_steel_code() noexcept { return "CTE DB SE-A / Eurocódigo 3"; }
    static std::string_view active_concrete_code() noexcept { return "Código Estructural / Eurocódigo 2"; }
};

} // namespace cypeclon::codes

