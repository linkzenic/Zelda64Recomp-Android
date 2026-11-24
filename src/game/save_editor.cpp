#include "zelda_save_editor.h"

#include <array>
#include <mutex>

namespace zelda64::save_editor {

namespace {
    std::mutex save_editor_mutex;
    std::array<int32_t, ValueCount> snapshot_values{};
    std::array<int32_t, ValueCount> pending_values{};
    bool snapshot_ready = false;
    bool pending_initialized = false;
    bool pending_apply = false;

    bool valid_id(ValueId id) {
        return id >= 0 && id < ValueCount;
    }
}

void set_snapshot_value(ValueId id, int32_t value) {
    if (!valid_id(id)) {
        return;
    }

    std::lock_guard lock{ save_editor_mutex };
    snapshot_ready = true;
    snapshot_values[id] = value;
    if (!pending_initialized) {
        pending_values[id] = value;
    }
}

int32_t get_snapshot_value(ValueId id) {
    if (!valid_id(id)) {
        return 0;
    }

    std::lock_guard lock{ save_editor_mutex };
    return snapshot_values[id];
}

void set_pending_value(ValueId id, int32_t value) {
    if (!valid_id(id)) {
        return;
    }

    std::lock_guard lock{ save_editor_mutex };
    if (!pending_initialized) {
        pending_values = snapshot_values;
        pending_initialized = true;
    }
    pending_values[id] = value;
    pending_apply = true;
}

int32_t get_pending_value(ValueId id) {
    if (!valid_id(id)) {
        return 0;
    }

    std::lock_guard lock{ save_editor_mutex };
    return pending_initialized ? pending_values[id] : snapshot_values[id];
}

bool should_apply_pending() {
    std::lock_guard lock{ save_editor_mutex };
    return pending_apply;
}

bool has_snapshot() {
    std::lock_guard lock{ save_editor_mutex };
    return snapshot_ready;
}

void clear_pending() {
    std::lock_guard lock{ save_editor_mutex };
    pending_apply = false;
    pending_initialized = false;
    pending_values = snapshot_values;
}

void reset_pending_to_snapshot() {
    std::lock_guard lock{ save_editor_mutex };
    pending_apply = false;
    pending_initialized = false;
    pending_values = snapshot_values;
}

}
