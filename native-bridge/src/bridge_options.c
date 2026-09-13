#include "bridge_options.h"

#include "generated_bridge_schema.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void wb_bridge_options_init(struct wb_bridge_options *options) {
    if (options == NULL) {
        return;
    }
    options->keyboard_path = NULL;
    options->three_finger_speed_percent =
            WB_SCHEMA_THREE_FINGER_SPEED_PERCENT_DEFAULT;
    options->three_finger_continuation_ms =
            WB_SCHEMA_THREE_FINGER_CONTINUATION_MS_DEFAULT;
    options->three_finger_reacquire_ms =
            WB_SCHEMA_THREE_FINGER_REACQUIRE_MS_DEFAULT;
}

static int parse_integer(const char *text, int minimum, int maximum, int *result) {
    if (text == NULL || text[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0'
            || value < minimum || value > maximum
            || value < INT_MIN || value > INT_MAX) {
        errno = EINVAL;
        return -1;
    }
    *result = (int)value;
    return 0;
}

int wb_bridge_options_parse(struct wb_bridge_options *options,
                            int argc,
                            char **argv) {
    if (options == NULL || argc < 1 || argv == NULL) {
        errno = EINVAL;
        return -1;
    }

    bool saw_speed = false;
    bool saw_continuation = false;
    bool saw_reacquire = false;
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        if (strcmp(argument, "--three-finger-speed-percent") == 0) {
            if (saw_speed || ++index >= argc
                    || parse_integer(argv[index],
                            WB_SCHEMA_THREE_FINGER_SPEED_PERCENT_MIN,
                            WB_SCHEMA_THREE_FINGER_SPEED_PERCENT_MAX,
                            &options->three_finger_speed_percent) != 0) {
                errno = EINVAL;
                return -1;
            }
            saw_speed = true;
        } else if (strcmp(argument, "--three-finger-continuation-ms") == 0) {
            if (saw_continuation || ++index >= argc
                    || parse_integer(argv[index],
                            WB_SCHEMA_THREE_FINGER_CONTINUATION_MS_MIN,
                            WB_SCHEMA_THREE_FINGER_CONTINUATION_MS_MAX,
                            &options->three_finger_continuation_ms) != 0) {
                errno = EINVAL;
                return -1;
            }
            saw_continuation = true;
        } else if (strcmp(argument, "--three-finger-reacquire-ms") == 0) {
            if (saw_reacquire || ++index >= argc
                    || parse_integer(argv[index],
                            WB_SCHEMA_THREE_FINGER_REACQUIRE_MS_MIN,
                            WB_SCHEMA_THREE_FINGER_REACQUIRE_MS_MAX,
                            &options->three_finger_reacquire_ms) != 0) {
                errno = EINVAL;
                return -1;
            }
            saw_reacquire = true;
        } else if (argument[0] == '-' || options->keyboard_path != NULL) {
            errno = EINVAL;
            return -1;
        } else {
            options->keyboard_path = argument;
        }
    }
    return 0;
}
