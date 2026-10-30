#include "connections/connections_engine.hpp"

namespace cypeclon::connections {

std::size_t ConnectionsEngine::count_candidate_nodes(const model::Obra& obra) noexcept {
    return obra.estructura().nudos.size();
}

} // namespace cypeclon::connections
