#ifndef WB_TOUCHPAD_BRIDGE_H
#define WB_TOUCHPAD_BRIDGE_H

#include <stdbool.h>

#include "touchpad_drag_state.h"

struct wb_touchpad_bridge {
    int physical_fd;
    int virtual_fd;
    bool virtual_created;
    char physical_path[64];
    struct wb_touchpad_drag_state drag_state;
};

int wb_touchpad_bridge_open(struct wb_touchpad_bridge *bridge,
                            struct wb_touchpad_drag_config config);
int wb_touchpad_bridge_fd(const struct wb_touchpad_bridge *bridge);
const char *wb_touchpad_bridge_path(const struct wb_touchpad_bridge *bridge);
int wb_touchpad_bridge_drain(struct wb_touchpad_bridge *bridge);
void wb_touchpad_bridge_close(struct wb_touchpad_bridge *bridge);

#endif
