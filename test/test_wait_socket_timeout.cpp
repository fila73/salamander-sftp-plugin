#define LIBSSH2_OPENSSL 1
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
extern "C" {
#include "libssh2.h"
#include "libssh2_priv.h"
int ssh2_wait_socket(LIBSSH2_SESSION *session, ssh2_time_t start_time);
}

int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    libssh2_init(0);

    // Create a local TCP loopback connection
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    bind(listenSock, (sockaddr*)&addr, sizeof(addr));
    listen(listenSock, 1);

    int addrLen = sizeof(addr);
    getsockname(listenSock, (sockaddr*)&addr, &addrLen);

    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(clientSock, (sockaddr*)&addr, sizeof(addr));
    SOCKET serverSock = accept(listenSock, NULL, NULL);

    LIBSSH2_SESSION* session = libssh2_session_init();
    session->socket_fd = clientSock;
    libssh2_session_set_blocking(session, 1);

    // Configure keepalive interval to 2s and simulate recent keepalive sent
    libssh2_keepalive_config(session, 0, 2);
    session->keepalive_last_sent = ssh2_now() + 2000000;

    // Pretend socket is blocked waiting for inbound read (no data sent by server)
    session->socket_state = SSH2_SOCKET_CONNECTED;
    session->socket_block_directions = LIBSSH2_SESSION_BLOCK_INBOUND;

    printf("now=%lld, last_sent=%lld, interval=%lld\n", (long long)ssh2_now(), (long long)session->keepalive_last_sent, (long long)session->keepalive_interval);
    printf("Calling ssh2_wait_socket with keepalive interval=2s, api_timeout=0...\n");
    ssh2_time_t start = ssh2_now();
    int rc = ssh2_wait_socket(session, start);
    ssh2_time_t duration = (ssh2_now() - start) / 1000000;

    printf("ssh2_wait_socket returned rc=%d (duration=%ld s)\n", rc, (long)duration);

    if (rc == LIBSSH2_ERROR_TIMEOUT)
    {
        printf("FAIL: ssh2_wait_socket prematurely aborted with LIBSSH2_ERROR_TIMEOUT due to keepalive!\n");
        return 1;
    }
    if (rc == 0)
    {
        printf("PASS: ssh2_wait_socket returned 0 (ready to try again), keeping transfer alive.\n");
    }

    // Now test with explicit api_timeout=5000ms and keepalive interval=2s
    libssh2_session_set_timeout(session, 5000);
    printf("Calling ssh2_wait_socket with keepalive interval=2s, api_timeout=5000ms...\n");
    session->keepalive_last_sent = ssh2_now() + 2000000;
    start = ssh2_now();
    // First call (2s keepalive) should return 0 (duration ~2s < 5s)
    rc = ssh2_wait_socket(session, start);
    printf("1st call rc=%d (duration=%ld s)\n", rc, (long)((ssh2_now() - start) / 1000000));
    if (rc != 0)
    {
        printf("FAIL: Expected 0 on first keepalive interval, got %d\n", rc);
        return 1;
    }

    // Second call after full 5s has elapsed should return timeout
    Sleep(3200);
    session->keepalive_last_sent = ssh2_now() + 2000000;
    rc = ssh2_wait_socket(session, start);
    printf("2nd call after api_timeout expired rc=%d (duration=%ld s)\n", rc, (long)((ssh2_now() - start) / 1000000));
    if (rc == LIBSSH2_ERROR_TIMEOUT)
    {
        printf("PASS: ssh2_wait_socket correctly returned timeout only after full api_timeout expired.\n");
    }
    else
    {
        printf("FAIL: Expected LIBSSH2_ERROR_TIMEOUT after api_timeout expired, got %d\n", rc);
        return 1;
    }

    libssh2_session_free(session);
    closesocket(clientSock);
    closesocket(serverSock);
    closesocket(listenSock);
    libssh2_exit();
    WSACleanup();

    printf("ALL TESTS PASSED!\n");
    return 0;
}
