#ifndef WORKBUDDY_HOST_APPLICATION_H
#define WORKBUDDY_HOST_APPLICATION_H

#include "WorkBuddyStatusServer.h"
#include "KeyboardBridgeCore.h"

struct wb_host_application {
    struct wb_keyboard_core keyboard;
    struct wb_status_server status;
    bool keyboard_ready;
    bool status_ready;
    unsigned short status_port;
};

bool wb_host_application_start(struct wb_host_application *application,
                               HINSTANCE instance, unsigned short status_port);
bool wb_host_application_handle_message(struct wb_host_application *application,
                                        const MSG *message);
bool wb_host_application_retry_status(struct wb_host_application *application);
void wb_host_application_stop(struct wb_host_application *application);

#endif
