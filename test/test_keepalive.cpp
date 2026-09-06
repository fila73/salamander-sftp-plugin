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
    if (c.SendKeepalive())
    {
        printf("FAIL: SendKeepalive should return false on unconnected session\n");
        return 1;
    }
    printf("PASS: SendKeepalive returns false when not connected\n");

    unsigned __int64 bytes = 0;
    int files = 0, dirs = 0;
    if (c.FastDirSize("/tmp", bytes, files, dirs))
    {
        printf("FAIL: FastDirSize should return false on unconnected session\n");
        return 1;
    }
    printf("PASS: FastDirSize returns false when not connected\n");

    CSftpConnection::GlobalExit();
    printf("All keepalive and FastDirSize unit tests passed!\n");
    return 0;
}
