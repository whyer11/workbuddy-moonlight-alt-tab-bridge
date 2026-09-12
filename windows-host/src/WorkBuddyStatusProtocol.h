#ifndef WORKBUDDY_STATUS_PROTOCOL_H
#define WORKBUDDY_STATUS_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>

enum {
    WB_STATUS_PORT = 57420,
    WB_STATUS_NONCE_HEX_LENGTH = 32,
};

#define WB_STATUS_PROTOCOL_NAME "WB_STATUS_V1"
#define WB_STATUS_HOST_VERSION "0.6.0"
#define WB_STATUS_HOST_RULES "alt_tab,windows_key"

bool wb_status_parse_request(const char *data, size_t length,
                             char *nonce, size_t nonce_capacity);
int wb_status_format_response(char *buffer, size_t capacity, const char *nonce);

#endif
