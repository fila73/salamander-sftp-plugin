#include "sftpconn.h"
#include <windows.h>
#include <stdio.h>

void WrapCommandWithSftpServerPrefix(const char* sftpServer, const char* rawCmd, char* outBuf, size_t outSize)
{
    if (outBuf && outSize > 0) outBuf[0] = 0;
}

int main(int argc, char** argv)
{
    printf("1. Simulating Salamander loading C:\\Apps\\samandarin\\libcrypto-3-x64.dll...\n");
    HMODULE h1 = LoadLibraryA("C:\\Apps\\samandarin\\libcrypto-3-x64.dll");
    printf("   Loaded h1 = %p\n", h1);

    printf("2. Simulating LoadBundledLibssh2 loading plugins\\sftp\\libcrypto-3-x64.dll and libssh2.dll...\n");
    HMODULE h2 = LoadLibraryExA("C:\\Apps\\samandarin\\plugins\\sftp\\libcrypto-3-x64.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    HMODULE h3 = LoadLibraryExA("C:\\Apps\\samandarin\\plugins\\sftp\\libssh2.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    printf("   Loaded h2 = %p, h3 = %p\n", h2, h3);

    printf("3. Initializing CSftpConnection...\n");
    if (!CSftpConnection::GlobalInit()) {
        printf("GlobalInit failed!\n");
        return 1;
    }
    printf("GlobalInit succeeded.\n");

    CSftpConnection conn;
    printf("Connecting to 10.0.1.35:22...\n");
    bool res = conn.Connect("10.0.1.35", 22, "root", "dummy_pass_for_test", nullptr, false, 0, false, nullptr);
    printf("Connect result: %d (LastError: %s)\n", (int)res, conn.LastError());

    CSftpConnection::GlobalExit();
    return 0;
}
