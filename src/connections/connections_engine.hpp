#pragma once

#include "model/obra.hpp"

namespace cypeclon::connections {

class ConnectionsEngine {
public:
    static std::size_t count_candidate_nodes(const model::Obra& obra) noexcept;
};

} // namespace cypeclon::connections
