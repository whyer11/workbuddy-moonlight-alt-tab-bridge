#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "bridge_options.h"

static void assert_parse_ok(int argc,
                            char **argv,
                            const char *path,
                            int speed,
                            int continuation) {
    struct wb_bridge_options options;
    wb_bridge_options_init(&options);
    assert(wb_bridge_options_parse(&options, argc, argv) == 0);
    assert((path == NULL && options.keyboard_path == NULL)
            || (path != NULL && strcmp(path, options.keyboard_path) == 0));
    assert(options.three_finger_speed_percent == speed);
    assert(options.three_finger_continuation_ms == continuation);
}

static void assert_parse_fails(int argc, char **argv) {
    struct wb_bridge_options options;
    wb_bridge_options_init(&options);
    assert(wb_bridge_options_parse(&options, argc, argv) == -1);
}

static void test_defaults(void) {
    char *argv[] = {"bridge"};
    assert_parse_ok(1, argv, NULL, 25, 1000);
}

static void test_values_and_path_in_any_order(void) {
    char *first[] = {
        "bridge", "--three-finger-speed-percent", "10",
        "/dev/input/event18", "--three-finger-continuation-ms", "2000"
    };
    assert_parse_ok(6, first, "/dev/input/event18", 10, 2000);

    char *second[] = {
        "bridge", "--three-finger-continuation-ms", "0",
        "--three-finger-speed-percent", "100"
    };
    assert_parse_ok(5, second, NULL, 100, 0);
}

static void test_invalid_values_fail(void) {
    char *speed_low[] = {"bridge", "--three-finger-speed-percent", "9"};
    char *speed_high[] = {"bridge", "--three-finger-speed-percent", "101"};
    char *continuation_low[] = {"bridge", "--three-finger-continuation-ms", "-1"};
    char *continuation_high[] = {"bridge", "--three-finger-continuation-ms", "2001"};
    char *not_integer[] = {"bridge", "--three-finger-speed-percent", "25x"};
    assert_parse_fails(3, speed_low);
    assert_parse_fails(3, speed_high);
    assert_parse_fails(3, continuation_low);
    assert_parse_fails(3, continuation_high);
    assert_parse_fails(3, not_integer);
}

static void test_missing_duplicate_and_unknown_arguments_fail(void) {
    char *missing[] = {"bridge", "--three-finger-speed-percent"};
    char *duplicate[] = {
        "bridge", "--three-finger-speed-percent", "25",
        "--three-finger-speed-percent", "30"
    };
    char *two_paths[] = {"bridge", "/dev/input/event18", "/dev/input/event19"};
    char *unknown[] = {"bridge", "--unknown", "1"};
    assert_parse_fails(2, missing);
    assert_parse_fails(5, duplicate);
    assert_parse_fails(3, two_paths);
    assert_parse_fails(3, unknown);
}

int main(void) {
    test_defaults();
    test_values_and_path_in_any_order();
    test_invalid_values_fail();
    test_missing_duplicate_and_unknown_arguments_fail();
    puts("bridge_options_test: OK");
    return 0;
}
