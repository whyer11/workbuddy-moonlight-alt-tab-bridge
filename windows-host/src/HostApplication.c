#include "HostApplication.h"

#include <string.h>

bool wb_host_application_start(struct wb_host_application *application,
                               HINSTANCE instance, HWND message_window,
                               unsigned short status_port) {
    if (application == NULL || message_window == NULL) return false;
    memset(application, 0, sizeof(*application));
    application->status.socket_handle = INVALID_SOCKET;
    application->status_port = status_port;
    application->keyboard_ready = wb_keyboard_core_start(
            &application->keyboard, instance, message_window);
    if (!application->keyboard_ready) return false;
    application->status_ready = wb_status_server_start(&application->status, status_port);
    return true;
}

bool wb_host_application_retry_status(struct wb_host_application *application) {
    if (application == NULL || !application->keyboard_ready) return false;
    if (application->status_ready) return true;
    application->status_ready = wb_status_server_start(
            &application->status, application->status_port);
    return application->status_ready;
}

bool wb_host_application_handle_message(struct wb_host_application *application,
                                        const MSG *message) {
    return application != NULL && application->keyboard_ready
            && wb_keyboard_core_handle_message(&application->keyboard, message);
}

bool wb_host_application_set_paused(struct wb_host_application *application,
                                    bool paused) {
    return application != NULL && application->keyboard_ready
            && wb_keyboard_core_set_paused(&application->keyboard, paused);
}

void wb_host_application_stop(struct wb_host_application *application) {
    if (application == NULL) return;
    if (application->status_ready) wb_status_server_stop(&application->status);
    if (application->keyboard_ready) wb_keyboard_core_stop(&application->keyboard);
    memset(application, 0, sizeof(*application));
}
