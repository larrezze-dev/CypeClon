#include "io/undoredo.hpp"

#include <algorithm>

namespace cypeclon::io {

UndoRedoManager::UndoRedoManager(std::size_t max_depth)
    : max_depth_(std::max<std::size_t>(1, max_depth)) {}

void UndoRedoManager::begin_transaction(std::string_view description) {
    if (!enabled_) return;
    UndoTransaction tx;
    tx.id = next_tx_id_++;
    tx.description = std::string(description);
    active_stack_.push_back(std::move(tx));
}

bool UndoRedoManager::commit_transaction() {
    if (!enabled_ || active_stack_.empty()) {
        return false;
    }

    UndoTransaction finished = std::move(active_stack_.back());
    active_stack_.pop_back();

    if (finished.operations.empty()) {
        notify_listeners();
        return false;
    }

    if (!active_stack_.empty()) {
        // Transacción anidada: fusionar operaciones en la transacción padre
        auto& parent = active_stack_.back();
        for (auto& op : finished.operations) {
            parent.operations.push_back(std::move(op));
        }
        return true;
    }

    redo_stack_.clear();
    if (undo_stack_.size() >= max_depth_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_stack_.push_back(std::move(finished));
    notify_listeners();
    return true;
}

void UndoRedoManager::rollback() {
    if (active_stack_.empty()) {
        return;
    }

    UndoTransaction aborted = std::move(active_stack_.back());
    active_stack_.pop_back();

    // Revertir en orden inverso todas las operaciones aplicadas en esta transacción
    for (auto it = aborted.operations.rbegin(); it != aborted.operations.rend(); ++it) {
        if (it->undo_fn) {
            it->undo_fn();
        }
    }
    notify_listeners();
}

void UndoRedoManager::record_action(
    std::string_view label,
    std::function<void()> redo_fn,
    std::function<void()> undo_fn,
    bool execute_now)
{
    if (execute_now && redo_fn) {
        redo_fn();
    }

    if (!enabled_) {
        return;
    }

    UndoOperation op{std::string(label), std::move(redo_fn), std::move(undo_fn)};
    if (!active_stack_.empty()) {
        active_stack_.back().operations.push_back(std::move(op));
    } else {
        begin_transaction(label);
        active_stack_.back().operations.push_back(std::move(op));
        commit_transaction();
    }
}

bool UndoRedoManager::undo() {
    if (!can_undo()) {
        return false;
    }

    UndoTransaction tx = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    for (auto it = tx.operations.rbegin(); it != tx.operations.rend(); ++it) {
        if (it->undo_fn) {
            it->undo_fn();
        }
    }

    redo_stack_.push_back(std::move(tx));
    notify_listeners();
    return true;
}

bool UndoRedoManager::redo() {
    if (!can_redo()) {
        return false;
    }

    UndoTransaction tx = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    for (auto& op : tx.operations) {
        if (op.redo_fn) {
            op.redo_fn();
        }
    }

    undo_stack_.push_back(std::move(tx));
    notify_listeners();
    return true;
}

std::string UndoRedoManager::undo_description() const {
    if (!can_undo()) return {};
    return undo_stack_.back().description;
}

std::string UndoRedoManager::redo_description() const {
    if (!can_redo()) return {};
    return redo_stack_.back().description;
}

void UndoRedoManager::set_max_depth(std::size_t depth) {
    max_depth_ = std::max<std::size_t>(1, depth);
    while (undo_stack_.size() > max_depth_) {
        undo_stack_.erase(undo_stack_.begin());
    }
}

void UndoRedoManager::clear() {
    active_stack_.clear();
    undo_stack_.clear();
    redo_stack_.clear();
    saved_tx_id_ = 0;
    notify_listeners();
}

void UndoRedoManager::add_change_listener(ChangeCallback cb) {
    if (cb) {
        listeners_.push_back(std::move(cb));
    }
}

void UndoRedoManager::notify_listeners() {
    for (const auto& cb : listeners_) {
        cb();
    }
}

} // namespace cypeclon::io

