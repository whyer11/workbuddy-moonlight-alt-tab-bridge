#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/WorkBuddyStatusProtocol.h"

static void test_valid_request_round_trips_nonce(void) {
    const char *request = "WB_STATUS_V2 nonce=00112233445566778899aabbccddeeff";
    char nonce[WB_STATUS_NONCE_HEX_LENGTH + 1];

    assert(wb_status_parse_request(request, strlen(request), nonce, sizeof(nonce)));
    assert(strcmp(nonce, "00112233445566778899aabbccddeeff") == 0);
}

static void test_unknown_or_malformed_requests_are_rejected(void) {
    char nonce[WB_STATUS_NONCE_HEX_LENGTH + 1];
    const char *control = "RUN_KEY 00112233445566778899aabbccddeeff";
    const char *short_nonce = "WB_STATUS_V2 nonce=00112233";
    const char *not_hex = "WB_STATUS_V2 nonce=00112233445566778899aabbccddeefg";
    const char *trailing = "WB_STATUS_V2 nonce=00112233445566778899aabbccddeeff extra=x";
    const char *v1 = "WB_STATUS_V1 00112233445566778899aabbccddeeff";

    assert(!wb_status_parse_request(control, strlen(control), nonce, sizeof(nonce)));
    assert(!wb_status_parse_request(short_nonce, strlen(short_nonce), nonce, sizeof(nonce)));
    assert(!wb_status_parse_request(not_hex, strlen(not_hex), nonce, sizeof(nonce)));
    assert(!wb_status_parse_request(trailing, strlen(trailing), nonce, sizeof(nonce)));
    assert(!wb_status_parse_request(v1, strlen(v1), nonce, sizeof(nonce)));
}

static void test_response_has_fixed_version_rules_and_matching_nonce(void) {
    char response[160];

    assert(wb_status_format_response(
            response, sizeof(response), "00112233445566778899aabbccddeeff") > 0);
    assert(strcmp(response,
            "WB_STATUS_V2 nonce=00112233445566778899aabbccddeeff "
            "host=0.7.0 protocol=2 core=ready rules=alt_tab,windows_key") == 0);
    assert(wb_status_format_response(
            response, 8, "00112233445566778899aabbccddeeff") == -1);
}

int main(void) {
    test_valid_request_round_trips_nonce();
    test_unknown_or_malformed_requests_are_rejected();
    test_response_has_fixed_version_rules_and_matching_nonce();
    puts("status_protocol_test: OK");
    return 0;
}
