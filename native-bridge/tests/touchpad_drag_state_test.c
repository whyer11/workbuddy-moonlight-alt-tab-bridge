#include <assert.h>
#include <stdio.h>

#include "touchpad_drag_state.h"

static const struct wb_touchpad_drag_config TEST_CONFIG = {
    .activation_distance = 48,
    .movement_numerator = 25,
    .movement_denominator = 100,
    .continuation_ms = 1000,
    .reacquire_ms = 150,
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

static void begin_three_contacts(struct wb_touchpad_drag_state *state, int dx, int dy) {
    begin_contact(state, 0, 100, 300 + dx, 300 + dy);
    begin_contact(state, 1, 101, 600 + dx, 300 + dy);
    begin_contact(state, 2, 102, 900 + dx, 300 + dy);
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
    begin_three_contacts(state, 0, 0);
    assert_output(wb_touchpad_drag_finish_frame(state, 0), WB_DRAG_NONE, 0, 0);
    move_three_contacts(state, 48, 0);
    assert_output(wb_touchpad_drag_finish_frame(state, 100),
                  WB_DRAG_LEFT_DOWN | WB_DRAG_MOVE, 12, 0);
}

static void test_three_finger_tap_does_nothing(void) {
    struct wb_touchpad_drag_state state;
    wb_touchpad_drag_init(&state, TEST_CONFIG);
    begin_three_contacts(&state, 0, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 0), WB_DRAG_NONE, 0, 0);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 100), WB_DRAG_NONE, 0, 0);
}

static void test_threshold_crossing_scales_initial_movement(void) {
    struct wb_touchpad_drag_state state;
    wb_touchpad_drag_init(&state, TEST_CONFIG);
    begin_three_contacts(&state, 0, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 0), WB_DRAG_NONE, 0, 0);
    move_three_contacts(&state, 47, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 50), WB_DRAG_NONE, 0, 0);
    move_three_contacts(&state, 48, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 100),
                  WB_DRAG_LEFT_DOWN | WB_DRAG_MOVE, 12, 0);
}

static void test_drag_continues_without_repeating_button_down(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    move_three_contacts(&state, 60, 6);
    assert_output(wb_touchpad_drag_finish_frame(&state, 150), WB_DRAG_MOVE, 3, 1);
}

static void test_default_scale_accumulates_four_positive_single_steps(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    for (int step = 1; step <= 3; ++step) {
        move_three_contacts(&state, 48 + step, 0);
        assert_output(wb_touchpad_drag_finish_frame(&state, 100 + step),
                      WB_DRAG_NONE, 0, 0);
    }
    move_three_contacts(&state, 52, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 104), WB_DRAG_MOVE, 1, 0);
}

static void test_default_scale_accumulates_four_negative_single_steps(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    for (int step = 1; step <= 3; ++step) {
        move_three_contacts(&state, 48 - step, 0);
        assert_output(wb_touchpad_drag_finish_frame(&state, 100 + step),
                      WB_DRAG_NONE, 0, 0);
    }
    move_three_contacts(&state, 44, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 104), WB_DRAG_MOVE, -1, 0);
}

static void test_direction_reversal_consumes_existing_remainder(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    move_three_contacts(&state, 49, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 101), WB_DRAG_NONE, 0, 0);
    move_three_contacts(&state, 48, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 102), WB_DRAG_NONE, 0, 0);
    move_three_contacts(&state, 52, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 106), WB_DRAG_MOVE, 1, 0);
}

static void test_lift_enters_grace_without_releasing(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    assert(state.phase == WB_TOUCHPAD_GRACE);
    assert(state.button_down);
}

static void test_resume_at_999ms_rebases_without_up_or_second_down(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 2, 202, 948, 300);
    assert_output(wb_touchpad_drag_finish_frame(&state, 1199), WB_DRAG_NONE, 0, 0);
    assert(state.phase == WB_TOUCHPAD_DRAGGING);
    move_three_contacts(&state, 52, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 1200), WB_DRAG_MOVE, 1, 0);
}

static void test_resume_frame_never_moves_cursor(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 2, 202, 1200, 600);
    assert_output(wb_touchpad_drag_finish_frame(&state, 500), WB_DRAG_NONE, 0, 0);
}

static void test_new_single_contact_after_empty_grace_releases_immediately(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 0, 200, 400, 400);
    assert_output(wb_touchpad_drag_finish_frame(&state, 300), WB_DRAG_NONE, 0, 0);
    assert_output(wb_touchpad_drag_tick(&state, 449), WB_DRAG_NONE, 0, 0);
    assert_output(wb_touchpad_drag_tick(&state, 450), WB_DRAG_LEFT_UP, 0, 0);
    assert(state.phase == WB_TOUCHPAD_IDLE);
}

static void test_new_two_contacts_after_empty_grace_release_immediately(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 0, 200, 400, 400);
    begin_contact(&state, 1, 201, 700, 400);
    assert_output(wb_touchpad_drag_finish_frame(&state, 300), WB_DRAG_NONE, 0, 0);
    assert_output(wb_touchpad_drag_tick(&state, 450), WB_DRAG_LEFT_UP, 0, 0);
}

static void test_two_finger_motion_during_grace_releases_immediately(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    move_contact(&state, 0, 360, 300);
    move_contact(&state, 1, 660, 300);
    assert_output(wb_touchpad_drag_finish_frame(&state, 300),
                  WB_DRAG_LEFT_UP, 0, 0);
}

static void test_staggered_lift_without_new_input_preserves_grace(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    end_contact(&state, 1);
    assert_output(wb_touchpad_drag_finish_frame(&state, 210), WB_DRAG_NONE, 0, 0);
    end_contact(&state, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 220), WB_DRAG_NONE, 0, 0);
    assert(state.phase == WB_TOUCHPAD_GRACE);
    assert(state.button_down);
}

static void test_three_contacts_after_empty_grace_resume_without_second_down(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_three_contacts(&state, 100, 100);
    assert_output(wb_touchpad_drag_finish_frame(&state, 300), WB_DRAG_NONE, 0, 0);
    assert(state.phase == WB_TOUCHPAD_DRAGGING);
    assert(state.button_down);
}

static void test_staggered_three_contact_reacquire_resumes_within_tolerance(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 0, 200, 400, 400);
    assert_output(wb_touchpad_drag_finish_frame(&state, 300), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 1, 201, 700, 400);
    assert_output(wb_touchpad_drag_finish_frame(&state, 350), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 2, 202, 1000, 400);
    assert_output(wb_touchpad_drag_finish_frame(&state, 449), WB_DRAG_NONE, 0, 0);
    assert(state.phase == WB_TOUCHPAD_DRAGGING);
    assert(state.button_down);
}

static void test_zero_reacquire_tolerance_preserves_strict_mode(void) {
    struct wb_touchpad_drag_state state;
    struct wb_touchpad_drag_config strict = TEST_CONFIG;
    strict.reacquire_ms = 0;
    wb_touchpad_drag_init(&state, strict);
    begin_three_contacts(&state, 0, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 0), WB_DRAG_NONE, 0, 0);
    move_three_contacts(&state, 48, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 100),
                  WB_DRAG_LEFT_DOWN | WB_DRAG_MOVE, 12, 0);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 0, 200, 400, 400);
    assert_output(wb_touchpad_drag_finish_frame(&state, 300),
                  WB_DRAG_LEFT_UP, 0, 0);
}

static void test_reacquire_timeout_never_extends_continuation_deadline(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 0, 200, 400, 400);
    assert_output(wb_touchpad_drag_finish_frame(&state, 1190), WB_DRAG_NONE, 0, 0);
    assert(wb_touchpad_drag_next_timeout_ms(&state, 1190, 500) == 10);
    assert_output(wb_touchpad_drag_tick(&state, 1200), WB_DRAG_LEFT_UP, 0, 0);
}

static void test_invalid_grace_input_releases_only_once(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 0);
    end_contact(&state, 1);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 0, 200, 400, 400);
    assert_output(wb_touchpad_drag_finish_frame(&state, 300), WB_DRAG_NONE, 0, 0);
    assert_output(wb_touchpad_drag_tick(&state, 450), WB_DRAG_LEFT_UP, 0, 0);
    assert_output(wb_touchpad_drag_tick(&state, 1800), WB_DRAG_NONE, 0, 0);
}

static void test_tick_at_1000ms_releases_exactly_once(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    assert_output(wb_touchpad_drag_tick(&state, 1199), WB_DRAG_NONE, 0, 0);
    assert_output(wb_touchpad_drag_tick(&state, 1200), WB_DRAG_LEFT_UP, 0, 0);
    assert_output(wb_touchpad_drag_tick(&state, 1201), WB_DRAG_NONE, 0, 0);
}

static void test_resume_at_1000ms_releases_then_arms_new_drag(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 2, 202, 948, 300);
    assert_output(wb_touchpad_drag_finish_frame(&state, 1200),
                  WB_DRAG_LEFT_UP, 0, 0);
    assert(state.phase == WB_TOUCHPAD_ARMED);
    move_three_contacts(&state, 96, 0);
    assert_output(wb_touchpad_drag_finish_frame(&state, 1201),
                  WB_DRAG_LEFT_DOWN | WB_DRAG_MOVE, 12, 0);
}

static void test_fourth_finger_releases_immediately_during_drag(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    wb_touchpad_drag_select_slot(&state, 3);
    wb_touchpad_drag_set_tracking_id(&state, 103);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200),
                  WB_DRAG_LEFT_UP, 0, 0);
}

static void test_fourth_finger_releases_immediately_during_grace(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    begin_contact(&state, 2, 202, 948, 300);
    begin_contact(&state, 3, 203, 1200, 300);
    assert_output(wb_touchpad_drag_finish_frame(&state, 300),
                  WB_DRAG_LEFT_UP, 0, 0);
}

static void test_cancel_during_grace_releases_exactly_once(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    assert_output(wb_touchpad_drag_cancel(&state), WB_DRAG_LEFT_UP, 0, 0);
    assert_output(wb_touchpad_drag_cancel(&state), WB_DRAG_NONE, 0, 0);
}

static void test_next_timeout_is_minimum_of_deadline_and_fallback(void) {
    struct wb_touchpad_drag_state state;
    start_drag(&state);
    assert(wb_touchpad_drag_next_timeout_ms(&state, 100, 500) == 500);
    end_contact(&state, 2);
    assert_output(wb_touchpad_drag_finish_frame(&state, 200), WB_DRAG_NONE, 0, 0);
    assert(wb_touchpad_drag_next_timeout_ms(&state, 300, 500) == 500);
    assert(wb_touchpad_drag_next_timeout_ms(&state, 900, 500) == 300);
    assert(wb_touchpad_drag_next_timeout_ms(&state, 1200, 500) == 0);
}

int main(void) {
    test_three_finger_tap_does_nothing();
    test_threshold_crossing_scales_initial_movement();
    test_drag_continues_without_repeating_button_down();
    test_default_scale_accumulates_four_positive_single_steps();
    test_default_scale_accumulates_four_negative_single_steps();
    test_direction_reversal_consumes_existing_remainder();
    test_lift_enters_grace_without_releasing();
    test_resume_at_999ms_rebases_without_up_or_second_down();
    test_resume_frame_never_moves_cursor();
    test_new_single_contact_after_empty_grace_releases_immediately();
    test_new_two_contacts_after_empty_grace_release_immediately();
    test_two_finger_motion_during_grace_releases_immediately();
    test_staggered_lift_without_new_input_preserves_grace();
    test_three_contacts_after_empty_grace_resume_without_second_down();
    test_staggered_three_contact_reacquire_resumes_within_tolerance();
    test_zero_reacquire_tolerance_preserves_strict_mode();
    test_reacquire_timeout_never_extends_continuation_deadline();
    test_invalid_grace_input_releases_only_once();
    test_tick_at_1000ms_releases_exactly_once();
    test_resume_at_1000ms_releases_then_arms_new_drag();
    test_fourth_finger_releases_immediately_during_drag();
    test_fourth_finger_releases_immediately_during_grace();
    test_cancel_during_grace_releases_exactly_once();
    test_next_timeout_is_minimum_of_deadline_and_fallback();
    puts("touchpad_drag_state_test: OK");
    return 0;
}
