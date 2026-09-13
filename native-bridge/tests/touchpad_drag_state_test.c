#include <assert.h>
#include <stdio.h>

#include "touchpad_drag_state.h"

static const struct wb_touchpad_drag_config TEST_CONFIG = {
    .activation_distance = 48,
    .movement_numerator = 1,
    .movement_denominator = 1,
};

static void begin_contact(struct wb_touchpad_drag_state *state,
                          int slot,
                          int tracking_id,
                          int x,
                          int y) {
    wb_touchpad_drag_select_slot(state, slot);
    wb_touchpad_drag_set_tracking_id(state, tracking_id);
    wb_touchpad_drag_set_x(state, x);
    wb_touchpad_drag_set_y(state, y);
}

static void move_contact(struct wb_touchpad_drag_state *state,
                         int slot,
                         int x,
                         int y) {
    wb_touchpad_drag_select_slot(state, slot);
    wb_touchpad_drag_set_x(state, x);
    wb_touchpad_drag_set_y(state, y);
}

static void end_contact(struct wb_touchpad_drag_state *state, int slot) {
    wb_touchpad_drag_select_slot(state, slot);
    wb_touchpad_drag_set_tracking_id(state, -1);
}

static void begin_three_contacts(struct wb_touchpad_drag_state *state) {
    begin_contact(state, 0, 100, 300, 300);
    begin_contact(state, 1, 101, 600, 300);
    begin_contact(state, 2, 102, 900, 300);
}

static void move_three_contacts(struct wb_touchpad_drag_state *state,
                                int dx,
                                int dy) {
    move_contact(state, 0, 300 + dx, 300 + dy);
    move_contact(state, 1, 600 + dx, 300 + dy);
    move_contact(state, 2, 900 + dx, 300 + dy);
}

static void assert_output(struct wb_touchpad_drag_output output,
                          unsigned int actions,
                          int dx,
                          int dy) {
    assert(output.actions == actions);
    assert(output.dx == dx);
    assert(output.dy == dy);
}

static void start_drag(struct wb_touchpad_drag_state *state) {
    wb_touchpad_drag_init(state, TEST_CONFIG);
    begin_three_contacts(state);
    assert_output(wb_touchpad_drag_finish_frame(state), WB_DRAG_NONE, 0, 0);
    move_three_contacts(state, 48, 0);
    assert_output(wb_touchpad_drag_finish_frame(state),
                  WB_DRAG_LEFT_DOWN | WB_DRAG_MOVE, 48, 0);
}

static void test_three_finger_tap_does_nothing(void) {
    struct wb_touchpad_drag_state state;
    wb_touchpad_drag_init(&state, TEST_CONFIG);
    begin_three_contacts(&state);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_NONE, 0, 0);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_NONE, 0, 0);
}

static void test_threshold_crossing_presses_then_moves(void) {
    struct wb_touchpad_drag_state state;
    wb_touchpad_drag_init(&state, TEST_CONFIG);
    begin_three_contacts(&state);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_NONE, 0, 0);
    move_three_contacts(&state, 47, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_NONE, 0, 0);
    move_three_contacts(&state, 48, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state),
                  WB_DRAG_LEFT_DOWN | WB_DRAG_MOVE, 48, 0);
}

static void test_drag_continues_without_repeating_button_down(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    move_three_contacts(&state, 60, 6);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_MOVE, 12, 6);
}

static void test_lifting_one_finger_releases_button(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_LEFT_UP, 0, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_NONE, 0, 0);
}

static void test_fourth_finger_cancels_and_releases(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    begin_contact(&state, 3, 103, 1200, 300);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_LEFT_UP, 0, 0);
}

static void test_replacing_a_finger_releases_before_rearming(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    begin_contact(&state, 2, 202, 948, 300);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_LEFT_UP, 0, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_NONE, 0, 0);
    move_three_contacts(&state, 96, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state),
                  WB_DRAG_LEFT_DOWN | WB_DRAG_MOVE, 48, 0);
}

static void test_cancel_releases_only_an_active_drag(void) {
    struct wb_touchpad_drag_state state;
    wb_touchpad_drag_init(&state, TEST_CONFIG);
    begin_three_contacts(&state);
    assert_output(wb_touchpad_drag_finish_frame(&state), WB_DRAG_NONE, 0, 0);
    assert_output(wb_touchpad_drag_cancel(&state), WB_DRAG_NONE, 0, 0);

    start_drag(&state);
    assert_output(wb_touchpad_drag_cancel(&state), WB_DRAG_LEFT_UP, 0, 0);
    assert_output(wb_touchpad_drag_cancel(&state), WB_DRAG_NONE, 0, 0);
}

int main(void) {
    test_three_finger_tap_does_nothing();
    test_threshold_crossing_presses_then_moves();
    test_drag_continues_without_repeating_button_down();
    test_lifting_one_finger_releases_button();
    test_fourth_finger_cancels_and_releases();
    test_replacing_a_finger_releases_before_rearming();
    test_cancel_releases_only_an_active_drag();
    puts("touchpad_drag_state_test: OK");
    return 0;
}
