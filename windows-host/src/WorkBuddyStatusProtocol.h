#ifndef WORKBUDDY_STATUS_PROTOCOL_H
#define WORKBUDDY_STATUS_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>

#include "GeneratedBridgeSchema.h"

enum {
    WB_STATUS_PORT = WB_SCHEMA_STATUS_PORT,
    WB_STATUS_NONCE_HEX_LENGTH = WB_SCHEMA_NONCE_HEX_LENGTH,
};

#define WB_STATUS_PROTOCOL_NAME "WB_STATUS_V2"
#define WB_STATUS_HOST_VERSION WB_SCHEMA_PRODUCT_VERSION
#define WB_STATUS_HOST_RULES WB_SCHEMA_WINDOWS_RULES

bool wb_status_parse_request(const char *data, size_t length,
                             char *nonce, size_t nonce_capacity);
int wb_status_format_response(char *buffer, size_t capacity, const char *nonce);

#endif
