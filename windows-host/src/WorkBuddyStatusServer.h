#ifndef WORKBUDDY_STATUS_SERVER_H
#define WORKBUDDY_STATUS_SERVER_H

#include <winsock2.h>
#include <windows.h>

#include <stdbool.h>

struct wb_status_server {
    SOCKET socket_handle;
    HANDLE thread_handle;
    HANDLE stop_event;
    unsigned short port;
    bool winsock_started;
    bool started;
};

bool wb_status_server_start(struct wb_status_server *server, unsigned short port);
void wb_status_server_stop(struct wb_status_server *server);
unsigned short wb_status_server_port(const struct wb_status_server *server);

#endif
