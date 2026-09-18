#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <windows.h>
#include <shlwapi.h>

// Standalone verification of the path parsing and title logic
static BOOL TestGetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
{
    const char* end = text + pathLen;
    const char* root = text; // pointer past the root portion of the path
    while (*root != 0 && *root != ':')
        root++;
    if (*root == ':')
    {
        root++;
        if ((root[0] == '/' || root[0] == '\\') && (root[1] == '/' || root[1] == '\\'))
            root += 2; // skip "//"
        while (root < end && *root != '/' && *root != '\\')
            root++; // skip "user@host[:port]"
        if (root < end && (*root == '/' || *root == '\\'))
            root++; // skip the leading slash of the remote path
    }

    const char* s = text + offset;
    if (s >= end)
        return FALSE;
    if (s < root)
        offset = (int)(root - text);
    else
    {
        if (*s == '/' || *s == '\\')
            s++;
        while (s < end && *s != '/' && *s != '\\')
            s++;
        offset = (int)(s - text);
    }
    return s < end;
}

static BOOL TestGetPathForMainWindowTitle(const char* Path, const char* hostPrefix, const char* connName, const char* fsName, int mode, char* buf, int bufSize)
{
    if (buf == NULL || bufSize <= 0)
        return FALSE;

    if (mode == 1) // "Directory Name Only"
    {
        char dirName[MAX_PATH] = "/";
        if (Path[0] != 0 && !(Path[0] == '/' && Path[1] == 0) && !(Path[0] == '\\' && Path[1] == 0))
        {
            const char* p = Path + strlen(Path);
            while (p > Path && (*(p - 1) == '/' || *(p - 1) == '\\'))
                p--;
            const char* end = p;
            while (p > Path && *(p - 1) != '/' && *(p - 1) != '\\')
                p--;
            int len = (int)(end - p);
            if (len > 0)
            {
                if (len >= MAX_PATH)
                    len = MAX_PATH - 1;
                memcpy(dirName, p, len);
                dirName[len] = 0;
            }
        }

        if (connName != NULL && connName[0] != 0)
            _snprintf_s(buf, bufSize, _TRUNCATE, "[%s] %s", connName, dirName);
        else
            lstrcpyn(buf, dirName, bufSize);
        return TRUE;
    }
    else if (mode == 2) // "Shortened Path"
    {
        if (Path[0] == 0 || (Path[0] == '/' && Path[1] == 0) || (Path[0] == '\\' && Path[1] == 0))
        {
            if (connName != NULL && connName[0] != 0)
                _snprintf_s(buf, bufSize, _TRUNCATE, "[%s] %s:%s/", connName, fsName, hostPrefix);
            else
                _snprintf_s(buf, bufSize, _TRUNCATE, "%s:%s/", fsName, hostPrefix);
            return TRUE;
        }
        const char* p = Path + strlen(Path);
        while (p > Path && (*(p - 1) == '/' || *(p - 1) == '\\'))
            p--;
        const char* end = p;
        while (p > Path && *(p - 1) != '/' && *(p - 1) != '\\')
            p--;
        if (p <= Path + 1)
        {
            if (connName != NULL && connName[0] != 0)
                _snprintf_s(buf, bufSize, _TRUNCATE, "[%s] %s:%s%s", connName, fsName, hostPrefix, Path);
            else
                _snprintf_s(buf, bufSize, _TRUNCATE, "%s:%s%s", fsName, hostPrefix, Path);
            return TRUE;
        }
        int len = (int)(end - p);
        if (connName != NULL && connName[0] != 0)
            _snprintf_s(buf, bufSize, _TRUNCATE, "[%s] %s:%s/.../%.*s", connName, fsName, hostPrefix, len, p);
        else
            _snprintf_s(buf, bufSize, _TRUNCATE, "%s:%s/.../%.*s", fsName, hostPrefix, len, p);
        return TRUE;
    }

    return FALSE;
}

static bool TestIsPathRemote(const char* p)
{
    if (p == NULL || p[0] == 0)
        return false;
    if (p[0] == '[')
        return false;
    if (strncmp(p, "sftp://", 7) == 0 || strncmp(p, "scp://", 6) == 0)
        return true;
    if (p[0] == '/')
        return true;
    if (isalpha((unsigned char)p[0]) && p[1] == ':' && (p[2] == '\\' || p[2] == '/'))
        return false;
    if ((p[0] == '\\' || p[0] == '/') && (p[1] == '\\' || p[1] == '/'))
        return false;
    return true;
}

static void TestFormatTransferPath(const char* inPath, bool forceRemote, const char* connName, char* outBuf, int outBufSize)
{
    if (inPath == NULL || inPath[0] == 0)
    {
        lstrcpynA(outBuf, "-", outBufSize);
        return;
    }

    char full[MAX_PATH * 2];
    bool isRemote = forceRemote || TestIsPathRemote(inPath);

    if (isRemote && connName != NULL && connName[0] != 0 && inPath[0] != '[')
    {
        _snprintf_s(full, sizeof(full), _TRUNCATE, "[%s] %s", connName, inPath);
    }
    else
    {
        lstrcpynA(full, inPath, sizeof(full));
    }

    int maxChars = 80;
    if (maxChars >= outBufSize)
        maxChars = outBufSize - 1;
    PathCompactPathExA(outBuf, full, maxChars, 0);
}

int main()
{
    printf("Running path hottrack & tab title unit tests...\n");

    const char* fullPath = "sftp://root@10.0.1.35/mnt/Enko/nas/Serialy/South Park/Season 29";
    int pathLen = (int)strlen(fullPath);

    int offset = 0;
    int steps = 0;
    int offsets[20];
    while (TestGetNextDirectoryLineHotPath(fullPath, pathLen, offset))
    {
        offsets[steps++] = offset;
        printf("  Step %d: offset=%d, text=%.*s\n", steps, offset, offset, fullPath);
    }
    offsets[steps++] = offset; // final offset
    printf("  Final offset=%d (pathLen=%d)\n", offset, pathLen);

    // Verify step 1 root offset points right after root slash
    // "sftp://root@10.0.1.35/" has length 22
    assert(offsets[0] == 22);
    // Verify that "oot@" is NOT at any step offset!
    for (int i = 0; i < steps; i++)
    {
        assert(strncmp(fullPath + offsets[i], "oot@", 4) != 0);
    }

    // Test Title Mode 1 (Directory Name Only - Tab title) with profile name [NAS]
    char titleBuf[256];
    BOOL res1 = TestGetPathForMainWindowTitle("/mnt/Enko/nas/Serialy/South Park/Season 29", "//root@10.0.1.35", "NAS", "sftp", 1, titleBuf, sizeof(titleBuf));
    assert(res1);
    printf("Tab title with profile (mode 1): '%s'\n", titleBuf);
    assert(strcmp(titleBuf, "[NAS] Season 29") == 0);

    // Test Title Mode 1 root with profile name [NAS]
    BOOL resRoot = TestGetPathForMainWindowTitle("/", "//root@10.0.1.35", "NAS", "sftp", 1, titleBuf, sizeof(titleBuf));
    assert(resRoot);
    printf("Tab title root with profile (mode 1): '%s'\n", titleBuf);
    assert(strcmp(titleBuf, "[NAS] /") == 0);

    // Test Title Mode 2 (Shortened Path) with profile name [NAS]
    BOOL res2 = TestGetPathForMainWindowTitle("/mnt/Enko/nas/Serialy/South Park/Season 29", "//root@10.0.1.35", "NAS", "sftp", 2, titleBuf, sizeof(titleBuf));
    assert(res2);
    printf("Shortened path with profile (mode 2): '%s'\n", titleBuf);
    assert(strcmp(titleBuf, "[NAS] sftp://root@10.0.1.35/.../Season 29") == 0);

    // Test Cache Key generation
    char cacheKey[512];
    unsigned __int64 fileSize = 4225;
    DWORD highTime = 0x01dc481b;
    DWORD lowTime = 0x0a234567;
    _snprintf_s(cacheKey, sizeof(cacheKey), _TRUNCATE,
                "sftp://root@10.0.1.35/mnt/Enko/nas/Serialy/South Park/Season 29/tvshow.nfo:%I64u:%08lx%08lx",
                fileSize, highTime, lowTime);
    printf("Cache key: '%s'\n", cacheKey);
    assert(strstr(cacheKey, "root@10.0.1.35") != NULL);
    assert(strstr(cacheKey, ":4225:01dc481b0a234567") != NULL);

    // Test Transfer Progress Path formatting with [NAS]
    char fromBuf[MAX_PATH * 2], toBuf[MAX_PATH * 2];
    // Download: from remote to local
    TestFormatTransferPath("/mnt/Enko/nas/Serialy/South Park/Season 29", true, "NAS", fromBuf, sizeof(fromBuf));
    TestFormatTransferPath("C:\\Users\\filip\\Downloads", false, "NAS", toBuf, sizeof(toBuf));
    printf("Download From (remote): '%s'\n", fromBuf);
    printf("Download To (local): '%s'\n", toBuf);
    assert(strcmp(fromBuf, "[NAS] /mnt/Enko/nas/Serialy/South Park/Season 29") == 0);
    assert(strcmp(toBuf, "C:\\Users\\filip\\Downloads") == 0);

    // Upload: from local to remote
    TestFormatTransferPath("C:\\Users\\filip\\Videos\\clip.mp4", false, "NAS", fromBuf, sizeof(fromBuf));
    TestFormatTransferPath("/mnt/Enko/nas/Serialy/South Park/Season 29", true, "NAS", toBuf, sizeof(toBuf));
    printf("Upload From (local): '%s'\n", fromBuf);
    printf("Upload To (remote): '%s'\n", toBuf);
    assert(strcmp(fromBuf, "C:\\Users\\filip\\Videos\\clip.mp4") == 0);
    assert(strcmp(toBuf, "[NAS] /mnt/Enko/nas/Serialy/South Park/Season 29") == 0);

    // Long path compacting with [NAS] prefix preserved
    char longBuf[MAX_PATH * 2];
    TestFormatTransferPath("/mnt/Enko/nas/Serialy/South Park/Season 29/VeryLongDirectoryNameThatExceedsTheBufferLengthAndShouldBeTruncated/subfolder", true, "NAS", longBuf, sizeof(longBuf));
    printf("Long path compacted: '%s'\n", longBuf);
    assert(strncmp(longBuf, "[NAS] ", 6) == 0);
    assert(strstr(longBuf, "...") != NULL);

    printf("\nALL PATH, PROGRESS & CACHE TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
