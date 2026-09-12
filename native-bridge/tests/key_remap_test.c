#include "key_remap.h"

#include <assert.h>
#include <stdio.h>

static void test_plain_tab_passes_through(void) {
    struct key_remap_state state = {0};
    assert(key_remap_apply(&state, WB_KEY_TAB, 1) == WB_KEY_TAB);
    assert(key_remap_apply(&state, WB_KEY_TAB, 0) == WB_KEY_TAB);
}

static void test_left_alt_tab_becomes_f12(void) {
    struct key_remap_state state = {0};
    assert(key_remap_apply(&state, WB_KEY_LEFTALT, 1) == WB_KEY_LEFTALT);
    assert(key_remap_apply(&state, WB_KEY_TAB, 1) == WB_KEY_F12);
    assert(key_remap_apply(&state, WB_KEY_TAB, 2) == WB_KEY_F12);
    assert(key_remap_apply(&state, WB_KEY_TAB, 0) == WB_KEY_F12);
    assert(key_remap_apply(&state, WB_KEY_LEFTALT, 0) == WB_KEY_LEFTALT);
}

static void test_right_alt_tab_becomes_f12(void) {
    struct key_remap_state state = {0};
    assert(key_remap_apply(&state, WB_KEY_RIGHTALT, 1) == WB_KEY_RIGHTALT);
    assert(key_remap_apply(&state, WB_KEY_TAB, 1) == WB_KEY_F12);
    assert(key_remap_apply(&state, WB_KEY_TAB, 0) == WB_KEY_F12);
    assert(key_remap_apply(&state, WB_KEY_RIGHTALT, 0) == WB_KEY_RIGHTALT);
}

static void test_tab_release_stays_f12_when_alt_releases_first(void) {
    struct key_remap_state state = {0};
    key_remap_apply(&state, WB_KEY_LEFTALT, 1);
    assert(key_remap_apply(&state, WB_KEY_TAB, 1) == WB_KEY_F12);
    key_remap_apply(&state, WB_KEY_LEFTALT, 0);
    assert(key_remap_apply(&state, WB_KEY_TAB, 0) == WB_KEY_F12);
}

static void test_left_meta_becomes_scroll_lock_for_full_win_key_bridge(void) {
    struct key_remap_state state = {0};
    assert(key_remap_apply(&state, 125, 1) == 70);
    assert(key_remap_apply(&state, 125, 2) == 70);
    assert(key_remap_apply(&state, 125, 0) == 70);
}

int main(void) {
    test_plain_tab_passes_through();
    test_left_alt_tab_becomes_f12();
    test_right_alt_tab_becomes_f12();
    test_tab_release_stays_f12_when_alt_releases_first();
    test_left_meta_becomes_scroll_lock_for_full_win_key_bridge();
    puts("key_remap_test: OK");
    return 0;
}
