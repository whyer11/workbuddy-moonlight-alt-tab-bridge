#include "WorkBuddyStatusServer.h"

#include "WorkBuddyStatusProtocol.h"

#include <stdint.h>
#include <string.h>

static DWORD WINAPI status_server_thread(void *context) {
    struct wb_status_server *server = (struct wb_status_server *)context;
    char request[WB_SCHEMA_MAX_DATAGRAM_BYTES + 1];

    while (WaitForSingleObject(server->stop_event, 0) == WAIT_TIMEOUT) {
        struct sockaddr_in sender;
        int sender_length = sizeof(sender);
        int received = recvfrom(server->socket_handle, request, sizeof(request), 0,
                (struct sockaddr *)&sender, &sender_length);
        if (received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
                continue;
            }
            if (WaitForSingleObject(server->stop_event, 0) != WAIT_TIMEOUT) {
                break;
            }
            continue;
        }
        if (received > WB_SCHEMA_MAX_DATAGRAM_BYTES) {
            continue;
        }

        char nonce[WB_STATUS_NONCE_HEX_LENGTH + 1];
        if (!wb_status_parse_request(request, (size_t)received,
                nonce, sizeof(nonce))) {
            continue;
        }

        char response[WB_SCHEMA_MAX_DATAGRAM_BYTES];
        int response_length = wb_status_format_response(
                response, sizeof(response), nonce);
        if (response_length > 0) {
            sendto(server->socket_handle, response, response_length, 0,
                    (const struct sockaddr *)&sender, sender_length);
        }
    }
    return 0;
}

static void cleanup_failed_start(struct wb_status_server *server) {
    if (server->thread_handle != NULL) {
        CloseHandle(server->thread_handle);
    }
    if (server->stop_event != NULL) {
        CloseHandle(server->stop_event);
    }
    if (server->socket_handle != INVALID_SOCKET) {
        closesocket(server->socket_handle);
    }
    if (server->winsock_started) {
        WSACleanup();
    }
    memset(server, 0, sizeof(*server));
    server->socket_handle = INVALID_SOCKET;
}

bool wb_status_server_start(struct wb_status_server *server, unsigned short port) {
    if (server == NULL) {
        return false;
    }

    memset(server, 0, sizeof(*server));
    server->socket_handle = INVALID_SOCKET;

    WSADATA winsock;
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        return false;
    }
    server->winsock_started = true;

    server->socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server->socket_handle == INVALID_SOCKET) {
        cleanup_failed_start(server);
        return false;
    }

    BOOL exclusive = TRUE;
    if (setsockopt(server->socket_handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            (const char *)&exclusive, sizeof(exclusive)) != 0) {
        cleanup_failed_start(server);
        return false;
    }

    DWORD timeout_ms = 100;
    if (setsockopt(server->socket_handle, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout_ms, sizeof(timeout_ms)) != 0) {
        cleanup_failed_start(server);
        return false;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server->socket_handle,
            (const struct sockaddr *)&address, sizeof(address)) != 0) {
        cleanup_failed_start(server);
        return false;
    }

    int address_length = sizeof(address);
    if (getsockname(server->socket_handle,
            (struct sockaddr *)&address, &address_length) != 0) {
        cleanup_failed_start(server);
        return false;
    }
    server->port = ntohs(address.sin_port);

    server->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (server->stop_event == NULL) {
        cleanup_failed_start(server);
        return false;
    }

    server->thread_handle = CreateThread(
            NULL, 0, status_server_thread, server, 0, NULL);
    if (server->thread_handle == NULL) {
        cleanup_failed_start(server);
        return false;
    }

    server->started = true;
    return true;
}

void wb_status_server_stop(struct wb_status_server *server) {
    if (server == NULL || !server->started) {
        return;
    }

    SetEvent(server->stop_event);
    closesocket(server->socket_handle);
    server->socket_handle = INVALID_SOCKET;
    WaitForSingleObject(server->thread_handle, 2000);
    CloseHandle(server->thread_handle);
    CloseHandle(server->stop_event);
    if (server->winsock_started) {
        WSACleanup();
    }

    memset(server, 0, sizeof(*server));
    server->socket_handle = INVALID_SOCKET;
}

unsigned short wb_status_server_port(const struct wb_status_server *server) {
    return server == NULL ? 0 : server->port;
}
