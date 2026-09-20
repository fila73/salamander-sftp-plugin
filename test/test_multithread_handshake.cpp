#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <assert.h>
#include "libssh2.h"
#include "sftpconn.h"

// Stubs for Salamander SDK symbols referenced by sftpglue.o
void* SalamanderGeneral = nullptr;
const char* LoadStr(int) { return ""; }
extern "C" char* _sal_lstrcpynA(char* d, const char* s, int n)
{
    if (!d || n <= 0) return d;
    if (!s) { *d = 0; return d; }
    lstrcpynA(d, s, n);
    return d;
}
bool SftpInputDialog(HWND, const char*, bool, char*, int) { return false; }

extern "C" int ssh2_random(unsigned char *buf, size_t len);

static volatile LONG g_ThreadSuccessCount = 0;

unsigned __stdcall WorkerThreadProc(void* param)
{
    int threadNum = (int)(intptr_t)param;
    unsigned char randomBuf[64];
    memset(randomBuf, 0, sizeof(randomBuf));

    // Test calling ssh2_random from background thread
    for (int i = 0; i < 50; i++)
    {
        int rc = ssh2_random(randomBuf, sizeof(randomBuf));
        assert(rc == 0);
    }

    // Verify non-zero bytes were generated
    bool hasNonZero = false;
    for (size_t i = 0; i < sizeof(randomBuf); i++)
    {
        if (randomBuf[i] != 0) { hasNonZero = true; break; }
    }
    assert(hasNonZero);

    InterlockedIncrement(&g_ThreadSuccessCount);
    return 0;
}

int main()
{
    printf("Starting multi-threaded RNG & OpenSSL thread test...\n");

    bool ok = CSftpConnection::GlobalInit();
    assert(ok);
    printf("GlobalInit succeeded.\n");

    const int NUM_THREADS = 8;
    HANDLE threads[NUM_THREADS];
    unsigned threadIds[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
    {
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThreadProc, (void*)(intptr_t)i, 0, &threadIds[i]);
        assert(threads[i] != NULL);
    }

    WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);

    for (int i = 0; i < NUM_THREADS; i++)
    {
        CloseHandle(threads[i]);
    }

    assert(g_ThreadSuccessCount == NUM_THREADS);
    printf("All %d threads generated random bytes concurrently without crash!\n", NUM_THREADS);

    CSftpConnection::GlobalExit();
    printf("GlobalExit succeeded.\n");
    printf("TEST COMPLETED SUCCESSFULLY!\n");
    return 0;
}
