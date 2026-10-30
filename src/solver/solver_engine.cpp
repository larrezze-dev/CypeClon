#include "solver/solver_engine.hpp"

namespace cypeclon::solver {

SolverSummary SolverEngine::analyze_linear_static(const model::Obra& obra) noexcept {
    SolverSummary summary;
    summary.dof_count = obra.estructura().nudos.size() * 6;
    summary.converged = !obra.estructura().nudos.empty();
    summary.max_displacement_m = 0.0;
    return summary;
}

} // namespace cypeclon::solver

