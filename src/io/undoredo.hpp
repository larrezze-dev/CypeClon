#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::io {

struct UndoOperation {
    std::string label;
    std::function<void()> redo_fn;
    std::function<void()> undo_fn;
};

struct UndoTransaction {
    std::uint64_t id{0};
    std::string description;
    std::vector<UndoOperation> operations;
};

struct AutoSaveConfig {
    bool enabled{true};
    bool save_before_calc{true}; ///< cyautosave_graba_obra_al_calcular
    bool create_backup{true};    ///< cyautosave_graba_obra_con_backup
    std::uint32_t interval_minutes{10};
};

/**
 * @brief Motor transaccional Undo/Redo equivalente a undoredo.dll (ebaundo_* y desreh_*).
 *
 * Soporta transacciones anidadas o simples (`begin_transaction`, `commit_transaction`, `rollback`),
 * registro de operaciones reversibles (`record_action`), capturas de estado por valor (`record_value_change`)
 * y capturas binarias serializadas (`record_binary_snapshot`, equivalente a `ebaundo_nousar_undo_redo_ST_CYPESAVE`).
 */
class UndoRedoManager {
public:
    using ChangeCallback = std::function<void()>;

    explicit UndoRedoManager(std::size_t max_depth = 64);

    // Control de transacciones (ebaundo_inicia_undo_redo, ebaundo_finaliza_undo_redo_aceptando, cancelando)
    void begin_transaction(std::string_view description);
    bool commit_transaction();
    void rollback();
    bool in_transaction() const noexcept { return !active_stack_.empty(); }

    // Registro de operaciones dentro de una transacción o como transacción atómica
    void record_action(std::string_view label, std::function<void()> redo_fn, std::function<void()> undo_fn, bool execute_now = true);

    template <typename T>
    void record_value_change(std::string_view label, T& target_ref, T new_value) {
        T old_value = target_ref;
        T* ptr = &target_ref;
        record_action(
            label,
            [ptr, new_value]() { *ptr = new_value; },
            [ptr, old_value]() { *ptr = old_value; },
            true
        );
    }

    // Ejecución de Deshacer / Rehacer (desreh_*)
    bool can_undo() const noexcept { return !in_transaction() && !undo_stack_.empty(); }
    bool can_redo() const noexcept { return !in_transaction() && !redo_stack_.empty(); }

    bool undo();
    bool redo();

    std::string undo_description() const;
    std::string redo_description() const;

    std::size_t undo_count() const noexcept { return undo_stack_.size(); }
    std::size_t redo_count() const noexcept { return redo_stack_.size(); }
    std::size_t max_depth() const noexcept { return max_depth_; }
    void set_max_depth(std::size_t depth);

    // Estado de cambios de la obra (desreh_hay_cambios / desreh_reset_cambios / ebaundo_vacia_pila)
    void clear();
    bool has_unsaved_changes() const noexcept { return saved_tx_id_ != current_tx_id(); }
    void mark_saved() noexcept { saved_tx_id_ = current_tx_id(); }

    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    bool is_enabled() const noexcept { return enabled_; }

    void add_change_listener(ChangeCallback cb);

    const AutoSaveConfig& autosave_config() const noexcept { return autosave_; }
    void set_autosave_config(const AutoSaveConfig& cfg) noexcept { autosave_ = cfg; }

private:
    std::uint64_t current_tx_id() const noexcept {
        return undo_stack_.empty() ? 0 : undo_stack_.back().id;
    }
    void notify_listeners();

    std::size_t max_depth_{64};
    bool enabled_{true};
    std::uint64_t next_tx_id_{1};
    std::uint64_t saved_tx_id_{0};

    std::vector<UndoTransaction> active_stack_;
    std::vector<UndoTransaction> undo_stack_;
    std::vector<UndoTransaction> redo_stack_;
    std::vector<ChangeCallback> listeners_;
    AutoSaveConfig autosave_{};
};

} // namespace cypeclon::io

