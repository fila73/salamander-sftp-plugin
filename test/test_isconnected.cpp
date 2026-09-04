#include "sftpconn.h"
#include <stdio.h>

int main()
{
    if (!CSftpConnection::GlobalInit())
    {
        printf("GlobalInit failed\n");
        return 1;
    }

    CSftpConnection c;
    if (c.IsConnected())
    {
        printf("FAIL: Expected IsConnected() == false for uninitialized connection\n");
        return 1;
    }
    printf("PASS: IsConnected() == false when not connected\n");

    // Try connecting to closed/invalid port to test error handling
    bool res = c.Connect("127.0.0.1", 59999, "test", "test");
    if (res)
    {
        printf("FAIL: Connected to non-existent port\n");
        return 1;
    }
    if (c.IsConnected())
    {
        printf("FAIL: Expected IsConnected() == false after failed connect\n");
        return 1;
    }
    printf("PASS: IsConnected() == false after failed connect\n");

    CSftpConnection::GlobalExit();
    printf("All IsConnected unit tests passed!\n");
    return 0;
}
