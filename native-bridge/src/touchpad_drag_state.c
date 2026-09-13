#include "touchpad_drag_state.h"

#include <string.h>

static struct wb_touchpad_drag_output no_output(void) {
    const struct wb_touchpad_drag_output output = {WB_DRAG_NONE, 0, 0};
    return output;
}

static struct wb_touchpad_drag_output release_output(void) {
    const struct wb_touchpad_drag_output output = {WB_DRAG_LEFT_UP, 0, 0};
    return output;
}

static void reset_runtime(struct wb_touchpad_drag_state *state) {
    struct wb_touchpad_drag_config config = state->config;
    memset(state, 0, sizeof(*state));
    state->selected_slot = 0;
    state->config = config;
}

void wb_touchpad_drag_init(struct wb_touchpad_drag_state *state,
                           struct wb_touchpad_drag_config config) {
    if (config.activation_distance <= 0) {
        config.activation_distance = 48;
    }
    if (config.movement_numerator <= 0) {
        config.movement_numerator = 1;
    }
    if (config.movement_denominator <= 0) {
        config.movement_denominator = 1;
    }
    memset(state, 0, sizeof(*state));
    state->selected_slot = 0;
    state->config = config;
}

void wb_touchpad_drag_select_slot(struct wb_touchpad_drag_state *state, int slot) {
    state->selected_slot = slot >= 0 && slot < WB_TOUCHPAD_MAX_SLOTS ? slot : -1;
}

void wb_touchpad_drag_set_tracking_id(struct wb_touchpad_drag_state *state, int tracking_id) {
    if (state->selected_slot < 0) {
        return;
    }
    struct wb_touchpad_slot *slot = &state->slots[state->selected_slot];
    if ((!slot->active && tracking_id < 0)
            || (slot->active && tracking_id >= 0 && slot->tracking_id == tracking_id)) {
        return;
    }
    state->composition_changed = true;
    if (tracking_id < 0) {
        memset(slot, 0, sizeof(*slot));
        return;
    }
    slot->tracking_id = tracking_id;
    slot->active = true;
    slot->has_x = false;
    slot->has_y = false;
}

void wb_touchpad_drag_set_x(struct wb_touchpad_drag_state *state, int x) {
    if (state->selected_slot < 0) {
        return;
    }
    struct wb_touchpad_slot *slot = &state->slots[state->selected_slot];
    if (slot->active) {
        slot->x = x;
        slot->has_x = true;
    }
}

void wb_touchpad_drag_set_y(struct wb_touchpad_drag_state *state, int y) {
    if (state->selected_slot < 0) {
        return;
    }
    struct wb_touchpad_slot *slot = &state->slots[state->selected_slot];
    if (slot->active) {
        slot->y = y;
        slot->has_y = true;
    }
}

static void arm_at(struct wb_touchpad_drag_state *state, int x, int y) {
    state->phase = WB_TOUCHPAD_ARMED;
    state->start_x = x;
    state->start_y = y;
    state->last_x = x;
    state->last_y = y;
}

struct wb_touchpad_drag_output wb_touchpad_drag_finish_frame(
        struct wb_touchpad_drag_state *state) {
    int active_count = 0;
    int sum_x = 0;
    int sum_y = 0;
    bool coordinates_complete = true;
    for (int index = 0; index < WB_TOUCHPAD_MAX_SLOTS; ++index) {
        const struct wb_touchpad_slot *slot = &state->slots[index];
        if (!slot->active) {
            continue;
        }
        active_count++;
        if (!slot->has_x || !slot->has_y) {
            coordinates_complete = false;
        } else {
            sum_x += slot->x;
            sum_y += slot->y;
        }
    }

    bool composition_changed = state->composition_changed;
    state->composition_changed = false;

    if (active_count != 3) {
        bool was_dragging = state->phase == WB_TOUCHPAD_DRAGGING;
        state->phase = WB_TOUCHPAD_IDLE;
        return was_dragging ? release_output() : no_output();
    }
    if (composition_changed && state->phase == WB_TOUCHPAD_DRAGGING) {
        state->phase = WB_TOUCHPAD_IDLE;
        return release_output();
    }
    if (!coordinates_complete) {
        if (composition_changed) {
            state->phase = WB_TOUCHPAD_IDLE;
        }
        return no_output();
    }

    int center_x = sum_x / 3;
    int center_y = sum_y / 3;
    if (state->phase == WB_TOUCHPAD_IDLE
            || (state->phase == WB_TOUCHPAD_ARMED && composition_changed)) {
        arm_at(state, center_x, center_y);
        return no_output();
    }

    bool started_dragging = false;
    if (state->phase == WB_TOUCHPAD_ARMED) {
        long long delta_x = (long long)center_x - state->start_x;
        long long delta_y = (long long)center_y - state->start_y;
        long long threshold = state->config.activation_distance;
        if (delta_x * delta_x + delta_y * delta_y < threshold * threshold) {
            return no_output();
        }
        state->phase = WB_TOUCHPAD_DRAGGING;
        started_dragging = true;
    }

    int delta_x = center_x - state->last_x;
    int delta_y = center_y - state->last_y;
    state->last_x = center_x;
    state->last_y = center_y;
    delta_x = delta_x * state->config.movement_numerator
            / state->config.movement_denominator;
    delta_y = delta_y * state->config.movement_numerator
            / state->config.movement_denominator;

    struct wb_touchpad_drag_output output = {
        .actions = WB_DRAG_MOVE,
        .dx = delta_x,
        .dy = delta_y,
    };
    if (started_dragging) {
        output.actions |= WB_DRAG_LEFT_DOWN;
    }
    if (delta_x == 0 && delta_y == 0) {
        output.actions &= ~WB_DRAG_MOVE;
    }
    return output;
}

struct wb_touchpad_drag_output wb_touchpad_drag_cancel(
        struct wb_touchpad_drag_state *state) {
    bool was_dragging = state->phase == WB_TOUCHPAD_DRAGGING;
    reset_runtime(state);
    return was_dragging ? release_output() : no_output();
}
