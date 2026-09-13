#include "native_protocol.h"

#include "generated_bridge_schema.h"

#include <ctype.h>
#include <stdio.h>

static int finish_format(char *buffer, size_t capacity, int length) {
    if (buffer == NULL || capacity == 0 || length < 0 || (size_t)length >= capacity) {
        if (buffer != NULL && capacity > 0) {
            buffer[0] = '\0';
        }
        return -1;
    }
    return length;
}

int wb_native_write_description(char *buffer, size_t capacity) {
    if (buffer == NULL || capacity == 0) {
        return -1;
    }
    return finish_format(buffer, capacity, snprintf(buffer, capacity,
            "WB_NATIVE_V2 version=%s protocol=%d rules=%s",
            WB_SCHEMA_PRODUCT_VERSION, WB_SCHEMA_PROTOCOL_VERSION, WB_SCHEMA_PAD_RULES));
}

int wb_native_write_ready(char *buffer, size_t capacity, const char *physical_path) {
    if (buffer == NULL || capacity == 0 || physical_path == NULL || physical_path[0] == '\0') {
        return -1;
    }
    for (const unsigned char *cursor = (const unsigned char *)physical_path; *cursor != '\0'; ++cursor) {
        if (isspace(*cursor) || iscntrl(*cursor)) {
            return -1;
        }
    }
    return finish_format(buffer, capacity, snprintf(buffer, capacity,
            "WB_NATIVE_READY_V2 version=%s protocol=%d rules=%s physical=%s",
            WB_SCHEMA_PRODUCT_VERSION, WB_SCHEMA_PROTOCOL_VERSION,
            WB_SCHEMA_PAD_RULES, physical_path));
}
