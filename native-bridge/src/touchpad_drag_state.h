#ifndef WB_TOUCHPAD_DRAG_STATE_H
#define WB_TOUCHPAD_DRAG_STATE_H

#include <stdbool.h>
#include <stdint.h>

#define WB_TOUCHPAD_MAX_SLOTS 8

enum wb_drag_action {
    WB_DRAG_NONE = 0,
    WB_DRAG_LEFT_DOWN = 1U << 0,
    WB_DRAG_MOVE = 1U << 1,
    WB_DRAG_LEFT_UP = 1U << 2
};

struct wb_touchpad_drag_config {
    int activation_distance;
    int movement_numerator;
    int movement_denominator;
    int continuation_ms;
    int reacquire_ms;
};

struct wb_touchpad_drag_output {
    unsigned int actions;
    int dx;
    int dy;
};

enum wb_touchpad_drag_phase {
    WB_TOUCHPAD_IDLE,
    WB_TOUCHPAD_ARMED,
    WB_TOUCHPAD_DRAGGING,
    WB_TOUCHPAD_GRACE
};

struct wb_touchpad_slot {
    int tracking_id;
    int x;
    int y;
    bool active;
    bool has_x;
    bool has_y;
};

struct wb_touchpad_drag_state {
    struct wb_touchpad_slot slots[WB_TOUCHPAD_MAX_SLOTS];
    int selected_slot;
    enum wb_touchpad_drag_phase phase;
    int start_x;
    int start_y;
    int last_x;
    int last_y;
    int64_t grace_deadline_ms;
    int64_t reacquire_deadline_ms;
    long long remainder_x;
    long long remainder_y;
    bool button_down;
    bool composition_changed;
    bool frame_contact_started;
    bool frame_existing_contact_moved;
    bool grace_observed_no_contacts;
    bool reacquire_pending;
    struct wb_touchpad_drag_config config;
};

void wb_touchpad_drag_init(struct wb_touchpad_drag_state *state,
                           struct wb_touchpad_drag_config config);
void wb_touchpad_drag_select_slot(struct wb_touchpad_drag_state *state, int slot);
void wb_touchpad_drag_set_tracking_id(struct wb_touchpad_drag_state *state, int tracking_id);
void wb_touchpad_drag_set_x(struct wb_touchpad_drag_state *state, int x);
void wb_touchpad_drag_set_y(struct wb_touchpad_drag_state *state, int y);
struct wb_touchpad_drag_output wb_touchpad_drag_finish_frame(
        struct wb_touchpad_drag_state *state, int64_t now_ms);
struct wb_touchpad_drag_output wb_touchpad_drag_tick(
        struct wb_touchpad_drag_state *state, int64_t now_ms);
int wb_touchpad_drag_next_timeout_ms(
        const struct wb_touchpad_drag_state *state,
        int64_t now_ms,
        int fallback_ms);
struct wb_touchpad_drag_output wb_touchpad_drag_cancel(
        struct wb_touchpad_drag_state *state);

#endif
