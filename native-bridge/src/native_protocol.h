#ifndef WB_NATIVE_PROTOCOL_H
#define WB_NATIVE_PROTOCOL_H

#include <stddef.h>

int wb_native_write_description(char *buffer, size_t capacity);
int wb_native_write_ready(char *buffer, size_t capacity, const char *physical_path);

#endif
