#pragma once

#include "model/obra.hpp"

namespace cypeclon::solver {

struct SolverSummary {
    bool converged{false};
    std::size_t dof_count{0};
    double max_displacement_m{0.0};
};

class SolverEngine {
public:
    static SolverSummary analyze_linear_static(const model::Obra& obra) noexcept;
};

} // namespace cypeclon::solver

