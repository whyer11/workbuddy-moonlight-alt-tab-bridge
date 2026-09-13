#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/native_protocol.h"

static void test_description_reports_real_capabilities(void) {
    char buffer[256];
    assert(wb_native_write_description(buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
            "WB_NATIVE_V2 version=0.9.1 protocol=2 rules=alt_tab,windows_key,arrow_keys,three_finger_drag") == 0);
}

static void test_ready_reports_validated_physical_device(void) {
    char buffer[256];
    assert(wb_native_write_ready(buffer, sizeof(buffer), "/dev/input/event18") > 0);
    assert(strcmp(buffer,
            "WB_NATIVE_READY_V2 version=0.9.1 protocol=2 rules=alt_tab,windows_key,arrow_keys,three_finger_drag physical=/dev/input/event18") == 0);
}

static void test_invalid_arguments_and_paths_are_rejected(void) {
    char buffer[256];
    assert(wb_native_write_description(NULL, sizeof(buffer)) == -1);
    assert(wb_native_write_description(buffer, 8) == -1);
    assert(wb_native_write_ready(buffer, sizeof(buffer), NULL) == -1);
    assert(wb_native_write_ready(buffer, sizeof(buffer), "/dev/input/event 18") == -1);
    assert(wb_native_write_ready(buffer, sizeof(buffer), "/dev/input/event18\nBAD") == -1);
    assert(wb_native_write_ready(buffer, 8, "/dev/input/event18") == -1);
}

int main(void) {
    test_description_reports_real_capabilities();
    test_ready_reports_validated_physical_device();
    test_invalid_arguments_and_paths_are_rejected();
    puts("native_protocol_test: OK");
    return 0;
}
