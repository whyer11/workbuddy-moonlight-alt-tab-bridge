#include <winsock2.h>
#include <ws2tcpip.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/WorkBuddyStatusProtocol.h"
#include "../src/WorkBuddyStatusServer.h"

static SOCKET make_client(DWORD timeout_ms) {
    SOCKET socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(socket_handle != INVALID_SOCKET);
    assert(setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout_ms, sizeof(timeout_ms)) == 0);
    return socket_handle;
}

static int exchange(SOCKET client, unsigned short port, const char *request,
                    char *response, int response_capacity) {
    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int sent = sendto(client, request, (int)strlen(request), 0,
            (const struct sockaddr *)&target, sizeof(target));
    assert(sent == (int)strlen(request));
    return recvfrom(client, response, response_capacity, 0, NULL, NULL);
}

static void test_valid_request_gets_status_and_control_request_gets_nothing(void) {
    struct wb_status_server server;
    assert(wb_status_server_start(&server, 0));
    assert(wb_status_server_port(&server) != 0);

    SOCKET client = make_client(400);
    char response[160] = {0};
    const char *request = "WB_STATUS_V2 nonce=00112233445566778899aabbccddeeff";
    int received = exchange(client, wb_status_server_port(&server),
            request, response, (int)sizeof(response) - 1);
    assert(received > 0);
    response[received] = '\0';
    assert(strcmp(response,
            "WB_STATUS_V2 nonce=00112233445566778899aabbccddeeff "
            "host=0.7.0 protocol=2 core=ready rules=alt_tab,windows_key") == 0);

    const char *control = "RUN_KEY 00112233445566778899aabbccddeeff";
    received = exchange(client, wb_status_server_port(&server),
            control, response, (int)sizeof(response) - 1);
    assert(received == SOCKET_ERROR);
    assert(WSAGetLastError() == WSAETIMEDOUT);

    closesocket(client);
    wb_status_server_stop(&server);
}

static void test_stop_releases_bound_port(void) {
    struct wb_status_server server;
    assert(wb_status_server_start(&server, 0));
    unsigned short port = wb_status_server_port(&server);
    wb_status_server_stop(&server);

    SOCKET replacement = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(replacement != INVALID_SOCKET);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    assert(bind(replacement, (const struct sockaddr *)&address, sizeof(address)) == 0);
    closesocket(replacement);
}

int main(void) {
    WSADATA winsock;
    assert(WSAStartup(MAKEWORD(2, 2), &winsock) == 0);
    test_valid_request_gets_status_and_control_request_gets_nothing();
    test_stop_releases_bound_port();
    WSACleanup();
    puts("status_server_integration_test: OK");
    return 0;
}
