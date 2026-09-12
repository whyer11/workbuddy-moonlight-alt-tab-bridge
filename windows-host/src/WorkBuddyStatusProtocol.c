#include "WorkBuddyStatusProtocol.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

bool wb_status_parse_request(const char *data, size_t length,
                             char *nonce, size_t nonce_capacity) {
    static const char prefix[] = WB_STATUS_PROTOCOL_NAME " ";
    const size_t prefix_length = sizeof(prefix) - 1;
    const size_t expected_length = prefix_length + WB_STATUS_NONCE_HEX_LENGTH;

    if (data == NULL || nonce == NULL
            || nonce_capacity < WB_STATUS_NONCE_HEX_LENGTH + 1
            || length != expected_length
            || memcmp(data, prefix, prefix_length) != 0) {
        return false;
    }

    for (size_t index = 0; index < WB_STATUS_NONCE_HEX_LENGTH; ++index) {
        unsigned char value = (unsigned char)data[prefix_length + index];
        if (!isxdigit(value)) {
            return false;
        }
    }

    memcpy(nonce, data + prefix_length, WB_STATUS_NONCE_HEX_LENGTH);
    nonce[WB_STATUS_NONCE_HEX_LENGTH] = '\0';
    return true;
}

int wb_status_format_response(char *buffer, size_t capacity, const char *nonce) {
    if (buffer == NULL || nonce == NULL || capacity == 0
            || strlen(nonce) != WB_STATUS_NONCE_HEX_LENGTH) {
        return -1;
    }

    int length = snprintf(buffer, capacity, WB_STATUS_PROTOCOL_NAME " %s host="
            WB_STATUS_HOST_VERSION " rules=" WB_STATUS_HOST_RULES, nonce);
    if (length < 0 || (size_t)length >= capacity) {
        buffer[0] = '\0';
        return -1;
    }
    return length;
}
