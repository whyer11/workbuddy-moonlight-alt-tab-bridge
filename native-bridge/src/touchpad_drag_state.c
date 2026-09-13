#include "touchpad_drag_state.h"

#include <limits.h>
#include <string.h>

static struct wb_touchpad_drag_output no_output(void) {
    const struct wb_touchpad_drag_output output = {WB_DRAG_NONE, 0, 0};
    return output;
}

static struct wb_touchpad_drag_output release_output(void) {
    const struct wb_touchpad_drag_output output = {WB_DRAG_LEFT_UP, 0, 0};
    return output;
}

static void clear_motion_runtime(struct wb_touchpad_drag_state *state) {
    state->grace_deadline_ms = 0;
    state->remainder_x = 0;
    state->remainder_y = 0;
    state->grace_observed_no_contacts = false;
}

static void enter_idle(struct wb_touchpad_drag_state *state) {
    state->phase = WB_TOUCHPAD_IDLE;
    clear_motion_runtime(state);
}

static struct wb_touchpad_drag_output release_if_pressed(
        struct wb_touchpad_drag_state *state) {
    bool was_pressed = state->button_down;
    state->button_down = false;
    enter_idle(state);
    return was_pressed ? release_output() : no_output();
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
        config.movement_numerator = 25;
    }
    if (config.movement_denominator <= 0) {
        config.movement_denominator = 100;
    }
    if (config.continuation_ms < 0) {
        config.continuation_ms = 1000;
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
    state->frame_contact_started = true;
}

void wb_touchpad_drag_set_x(struct wb_touchpad_drag_state *state, int x) {
    if (state->selected_slot < 0) {
        return;
    }
    struct wb_touchpad_slot *slot = &state->slots[state->selected_slot];
    if (slot->active) {
        slot->x = x;
        slot->has_x = true;
        state->frame_position_updated = true;
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
        state->frame_position_updated = true;
    }
}

static void arm_at(struct wb_touchpad_drag_state *state, int x, int y) {
    state->phase = WB_TOUCHPAD_ARMED;
    state->start_x = x;
    state->start_y = y;
    state->last_x = x;
    state->last_y = y;
    clear_motion_runtime(state);
}

static void resume_at(struct wb_touchpad_drag_state *state, int x, int y) {
    state->phase = WB_TOUCHPAD_DRAGGING;
    state->last_x = x;
    state->last_y = y;
    clear_motion_runtime(state);
}

static struct wb_touchpad_drag_output enter_grace(
        struct wb_touchpad_drag_state *state, int64_t now_ms) {
    clear_motion_runtime(state);
    if (state->config.continuation_ms == 0) {
        return release_if_pressed(state);
    }
    state->phase = WB_TOUCHPAD_GRACE;
    state->grace_deadline_ms = now_ms + state->config.continuation_ms;
    return no_output();
}

static int scale_delta(int delta,
                       int numerator,
                       int denominator,
                       long long *remainder) {
    long long scaled = (long long)delta * numerator + *remainder;
    long long output = scaled / denominator;
    *remainder = scaled % denominator;
    if (output > INT_MAX) return INT_MAX;
    if (output < INT_MIN) return INT_MIN;
    return (int)output;
}

struct wb_touchpad_drag_output wb_touchpad_drag_tick(
        struct wb_touchpad_drag_state *state, int64_t now_ms) {
    if (state->phase == WB_TOUCHPAD_GRACE
            && now_ms >= state->grace_deadline_ms) {
        return release_if_pressed(state);
    }
    return no_output();
}

int wb_touchpad_drag_next_timeout_ms(
        const struct wb_touchpad_drag_state *state,
        int64_t now_ms,
        int fallback_ms) {
    if (fallback_ms < 0) {
        fallback_ms = 0;
    }
    if (state->phase != WB_TOUCHPAD_GRACE) {
        return fallback_ms;
    }
    int64_t remaining = state->grace_deadline_ms - now_ms;
    if (remaining <= 0) {
        return 0;
    }
    if (remaining >= fallback_ms) {
        return fallback_ms;
    }
    return (int)remaining;
}

struct wb_touchpad_drag_output wb_touchpad_drag_finish_frame(
        struct wb_touchpad_drag_state *state, int64_t now_ms) {
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
    bool contact_started = state->frame_contact_started;
    bool position_updated = state->frame_position_updated;
    state->composition_changed = false;
    state->frame_contact_started = false;
    state->frame_position_updated = false;

    if (active_count >= 4) {
        return release_if_pressed(state);
    }

    struct wb_touchpad_drag_output expired = wb_touchpad_drag_tick(state, now_ms);
    if ((expired.actions & WB_DRAG_LEFT_UP) != 0) {
        if (active_count == 3 && coordinates_complete) {
            arm_at(state, sum_x / 3, sum_y / 3);
        }
        return expired;
    }

    bool entered_grace_this_frame = false;
    if (state->phase == WB_TOUCHPAD_DRAGGING
            && (active_count != 3 || composition_changed)) {
        struct wb_touchpad_drag_output output = enter_grace(state, now_ms);
        if (output.actions != WB_DRAG_NONE) {
            return output;
        }
        entered_grace_this_frame = true;
    }

    if (state->phase == WB_TOUCHPAD_GRACE) {
        if (active_count == 0) {
            state->grace_observed_no_contacts = true;
            return no_output();
        }
        if (active_count == 3 && coordinates_complete) {
            resume_at(state, sum_x / 3, sum_y / 3);
            return no_output();
        }
        if (!entered_grace_this_frame
                && active_count < 3
                && (state->grace_observed_no_contacts
                    || contact_started
                    || position_updated)) {
            return release_if_pressed(state);
        }
        return no_output();
    }

    if (active_count != 3) {
        if (state->phase == WB_TOUCHPAD_ARMED) {
            enter_idle(state);
        }
        return no_output();
    }
    if (!coordinates_complete) {
        if (composition_changed && state->phase == WB_TOUCHPAD_ARMED) {
            enter_idle(state);
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
        state->button_down = true;
        started_dragging = true;
    }

    int delta_x = center_x - state->last_x;
    int delta_y = center_y - state->last_y;
    state->last_x = center_x;
    state->last_y = center_y;
    delta_x = scale_delta(delta_x,
            state->config.movement_numerator,
            state->config.movement_denominator,
            &state->remainder_x);
    delta_y = scale_delta(delta_y,
            state->config.movement_numerator,
            state->config.movement_denominator,
            &state->remainder_y);

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
    bool was_pressed = state->button_down;
    reset_runtime(state);
    return was_pressed ? release_output() : no_output();
}
