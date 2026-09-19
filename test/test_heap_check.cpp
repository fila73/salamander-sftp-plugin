#include "sftpconn.h"
#include <windows.h>
#include <stdio.h>

void WrapCommandWithSftpServerPrefix(const char* sftpServer, const char* rawCmd, char* outBuf, size_t outSize)
{
    if (outBuf && outSize > 0) outBuf[0] = 0;
}

void SftpTraceLog(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    BOOL ok = HeapValidate(GetProcessHeap(), 0, NULL);
    printf("[TRACE %s] %s\n", ok ? "OK" : "CORRUPT!", buf);
}

static void check_heap(const char* label)
{
    BOOL ok = HeapValidate(GetProcessHeap(), 0, NULL);
    printf("Heap status [%s]: %s\n", label, ok ? "VALID" : "CORRUPT!!!");
    if (!ok) {
        printf("TERMINATING DUE TO HEAP CORRUPTION AT [%s]\n", label);
        exit(1);
    }
}

int main()
{
    check_heap("initial");

    HMODULE h1 = LoadLibraryA("C:\\Apps\\samandarin\\libcrypto-3-x64.dll");
    printf("Loaded h1 (samandarin/libcrypto): %p\n", h1);
    check_heap("after h1");

    HMODULE hz = LoadLibraryExA("C:\\Apps\\samandarin\\plugins\\sftp\\z.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    printf("Loaded hz (plugins/z.dll): %p\n", hz);
    check_heap("after z.dll");

    HMODULE h2 = LoadLibraryExA("C:\\Apps\\samandarin\\plugins\\sftp\\libcrypto-3-x64.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    printf("Loaded h2 (plugins/libcrypto): %p\n", h2);
    check_heap("after h2");

    HMODULE h3 = LoadLibraryExA("C:\\Apps\\samandarin\\plugins\\sftp\\libssh2.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    printf("Loaded h3 (plugins/libssh2): %p\n", h3);
    check_heap("after h3");

    check_heap("before GlobalInit");
    CSftpConnection::GlobalInit();
    check_heap("after GlobalInit");

    CSftpConnection conn;
    check_heap("after CSftpConnection constructor");

    printf("Connecting to 10.0.1.35:22...\n");
    conn.Connect("10.0.1.35", 22, "root", "test", nullptr, false, 0, false, nullptr);
    check_heap("after Connect");

    CSftpConnection::GlobalExit();
    check_heap("after GlobalExit");
    printf("DONE - ALL HEAP CHECKS PASSED!\n");
    return 0;
}
