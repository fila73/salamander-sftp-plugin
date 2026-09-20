// Copyright © 2026 Dupl3xx
//
// Background worker thread for asynchronous SFTP transfers.
#include "sftpworker.h"
#include <process.h>
#include <shlwapi.h>
#include <stdio.h>
#ifndef NO_OPENSSL
#include <openssl/crypto.h>
#endif

CSftpTransferWorker::CSftpTransferWorker()
    : ThreadHandle(NULL), ThreadId(0), WakeEvent(NULL), StopEvent(NULL),
      DlgHwnd(NULL), OverwriteAllDecision(-1), AttachedObserver(NULL), Initialized(true)
{
    InitializeCriticalSection(&QueueLock);
    State.Init();
    WakeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    memset(&Profile, 0, sizeof(Profile));
}

CSftpTransferWorker::~CSftpTransferWorker()
{
    Stop();
    Initialized = false;
    if (AttachedObserver != NULL)
    {
        AttachedObserver->DetachWorker();
        AttachedObserver = NULL;
    }
    DlgHwnd = NULL;
    if (WakeEvent)
    {
        CloseHandle(WakeEvent);
        WakeEvent = NULL;
    }
    if (StopEvent)
    {
        CloseHandle(StopEvent);
        StopEvent = NULL;
    }
    DeleteCriticalSection(&QueueLock);
    State.Destroy();
}

void CSftpTransferWorker::Reset()
{
    if (ThreadHandle != NULL)
    {
        DWORD exitCode = 0;
        if (GetExitCodeThread(ThreadHandle, &exitCode) && exitCode != STILL_ACTIVE)
        {
            CloseHandle(ThreadHandle);
            ThreadHandle = NULL;
            ThreadId = 0;
        }
    }

    EnterCriticalSection(&QueueLock);
    TaskQueue.clear();
    LeaveCriticalSection(&QueueLock);

    EnterCriticalSection(&State.Lock);
    State.Reset();
    LeaveCriticalSection(&State.Lock);

    OverwriteAllDecision = -1;
}

bool CSftpTransferWorker::Start(const CSftpProfile& profile, HWND dlgHwnd)
{
    if (ThreadHandle != NULL)
    {
        DWORD exitCode = 0;
        if (GetExitCodeThread(ThreadHandle, &exitCode) && exitCode != STILL_ACTIVE)
        {
            CloseHandle(ThreadHandle);
            ThreadHandle = NULL;
            ThreadId = 0;
        }
        else
        {
            return false; // already running
        }
    }

    Profile = profile;
    DlgHwnd = dlgHwnd;
    OverwriteAllDecision = -1;

    ResetEvent(StopEvent);
    ResetEvent(WakeEvent);

    EnterCriticalSection(&State.Lock);
    // Note: Do not wipe TotalItemsCount or TotalBytesExpected; they were set by EnqueueTask!
    State.CurrentLocalFile[0] = 0;
    State.CurrentRemoteFile[0] = 0;
    State.CurrentFileDone = 0;
    State.CurrentFileTotal = 0;
    State.CurrentItemIndex = 0;
    State.TotalBytesDone = 0;
    State.BytesPerSec = 0.0;
    State.Cancelled = false;
    State.HasError = false;
    State.ErrorMsg[0] = 0;
    State.IsRunning = true;
    State.StartTick = GetTickCount();
    State.LastUpdateTick = State.StartTick;
    LeaveCriticalSection(&State.Lock);

    ThreadHandle = (HANDLE)_beginthreadex(NULL, 0, ThreadEntryPoint, this, 0, (unsigned*)&ThreadId);
    if (!ThreadHandle)
    {
        EnterCriticalSection(&State.Lock);
        State.IsRunning = false;
        State.HasError = true;
        strcpy_s(State.ErrorMsg, "Failed to create worker thread");
        LeaveCriticalSection(&State.Lock);
        return false;
    }
    return true;
}

void CSftpTransferWorker::Stop()
{
    if (AttachedObserver != NULL)
    {
        AttachedObserver->DetachWorker();
        AttachedObserver = NULL;
    }
    DlgHwnd = NULL;

    if (!ThreadHandle)
        return;

    if (Initialized)
    {
        EnterCriticalSection(&State.Lock);
        State.Cancelled = true;
        LeaveCriticalSection(&State.Lock);
    }

    SetEvent(StopEvent);
    SetEvent(WakeEvent);

    if (WaitForSingleObject(ThreadHandle, 5000) == WAIT_TIMEOUT)
    {
        // Thread did not finish in time; terminate as last resort
        TerminateThread(ThreadHandle, 1);
    }
    CloseHandle(ThreadHandle);
    ThreadHandle = NULL;
    ThreadId = 0;

    WorkerConn.Disconnect();

    if (Initialized)
    {
        EnterCriticalSection(&State.Lock);
        State.IsRunning = false;
        LeaveCriticalSection(&State.Lock);
    }
}


void CSftpTransferWorker::EnqueueTask(const CSftpTransferTask& task)
{
    if (!Initialized)
        return;
    EnterCriticalSection(&QueueLock);
    TaskQueue.push_back(task);
    EnterCriticalSection(&State.Lock);
    State.TotalItemsCount++;
    State.TotalBytesExpected += task.FileSize;
    LeaveCriticalSection(&State.Lock);
    LeaveCriticalSection(&QueueLock);

    SetEvent(WakeEvent);
}

void CSftpTransferWorker::Cancel()
{
    if (!Initialized)
        return;
    EnterCriticalSection(&State.Lock);
    State.Cancelled = true;
    LeaveCriticalSection(&State.Lock);
    SetEvent(WakeEvent);
}

bool CSftpTransferWorker::IsRunning() const
{
    if (!Initialized)
        return false;
    EnterCriticalSection(const_cast<LPCRITICAL_SECTION>(&State.Lock));
    bool running = State.IsRunning;
    LeaveCriticalSection(const_cast<LPCRITICAL_SECTION>(&State.Lock));
    return running;
}

bool CSftpTransferWorker::HasTasks() const
{
    if (!Initialized)
        return false;
    EnterCriticalSection(const_cast<LPCRITICAL_SECTION>(&QueueLock));
    bool empty = TaskQueue.empty();
    LeaveCriticalSection(const_cast<LPCRITICAL_SECTION>(&QueueLock));
    return !empty;
}

void CSftpTransferWorker::GetStateSnapshot(CSftpTransferState& outState)
{
    if (!Initialized)
        return;
    EnterCriticalSection(&State.Lock);
    memcpy(&outState.CurrentLocalFile, State.CurrentLocalFile, sizeof(State.CurrentLocalFile));
    memcpy(&outState.CurrentRemoteFile, State.CurrentRemoteFile, sizeof(State.CurrentRemoteFile));
    outState.CurrentFileDone = State.CurrentFileDone;
    outState.CurrentFileTotal = State.CurrentFileTotal;
    outState.CurrentItemIndex = State.CurrentItemIndex;
    outState.TotalItemsCount = State.TotalItemsCount;
    outState.TotalBytesDone = State.TotalBytesDone;
    outState.TotalBytesExpected = State.TotalBytesExpected;
    outState.StartTick = State.StartTick;
    outState.LastUpdateTick = State.LastUpdateTick;
    outState.BytesPerSec = State.BytesPerSec;
    outState.IsRunning = State.IsRunning;
    outState.Cancelled = State.Cancelled;
    outState.HasError = State.HasError;
    memcpy(&outState.ErrorMsg, State.ErrorMsg, sizeof(State.ErrorMsg));
    LeaveCriticalSection(&State.Lock);
}

unsigned __stdcall CSftpTransferWorker::ThreadEntryPoint(LPVOID param)
{
    CSftpTransferWorker* worker = static_cast<CSftpTransferWorker*>(param);
    worker->ThreadLoop();
    _endthreadex(0);
    return 0;
}

void CSftpTransferWorker::ThreadLoop()
{
    // 1. Connect dedicated session
    const char* keyFile = (Profile.KeyFile[0] != 0) ? Profile.KeyFile : nullptr;
    const char* sftpServer = (Profile.SftpServer[0] != 0) ? Profile.SftpServer : nullptr;

    bool connected = WorkerConn.Connect(
        Profile.Host, Profile.Port, Profile.User, Profile.Password,
        keyFile, Profile.UseCompression, Profile.Protocol,
        Profile.ScpFallback, sftpServer
    );

    if (!connected)
    {
        EnterCriticalSection(&State.Lock);
        State.HasError = true;
        State.IsRunning = false;
        strncpy_s(State.ErrorMsg, WorkerConn.LastError(), _TRUNCATE);
        LeaveCriticalSection(&State.Lock);

        if (DlgHwnd)
            PostMessage(DlgHwnd, WM_APP_SFTP_WORKER_FINISHED, 0, 0);
        return;
    }

    WorkerConn.SetProgressCallback(StaticProgressCallback, this);

    HANDLE waitHandles[2] = { StopEvent, WakeEvent };

    while (1)
    {
        if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
            break;

        CSftpTransferTask currentTask;
        bool haveTask = false;

        EnterCriticalSection(&QueueLock);
        if (!TaskQueue.empty())
        {
            currentTask = TaskQueue.front();
            TaskQueue.pop_front();
            haveTask = true;
        }
        LeaveCriticalSection(&QueueLock);

        if (!haveTask)
        {
            // Wait for new task or stop signal
            DWORD wr = WaitForMultipleObjects(2, waitHandles, FALSE, 200);
            if (wr == WAIT_OBJECT_0) // StopEvent
                break;
            if (wr == WAIT_TIMEOUT)
            {
                // Check if we are done (queue empty and no more items being added)
                EnterCriticalSection(&QueueLock);
                bool stillEmpty = TaskQueue.empty();
                LeaveCriticalSection(&QueueLock);
                if (stillEmpty)
                {
                    // If no tasks left and running, we can finish or wait
                    // For now, if queue is empty and was previously processed, we break
                    break;
                }
            }
            continue;
        }

        EnterCriticalSection(&State.Lock);
        if (State.Cancelled)
        {
            LeaveCriticalSection(&State.Lock);
            break;
        }
        State.CurrentItemIndex++;
        LeaveCriticalSection(&State.Lock);

        bool success = ExecuteTask(currentTask);
        if (!success)
        {
            EnterCriticalSection(&State.Lock);
            if (State.Cancelled)
            {
                LeaveCriticalSection(&State.Lock);
                break;
            }
            State.HasError = true;
            strncpy_s(State.ErrorMsg, WorkerConn.LastError(), _TRUNCATE);
            LeaveCriticalSection(&State.Lock);
            break;
        }
    }

    WorkerConn.Disconnect();

    EnterCriticalSection(&State.Lock);
    State.IsRunning = false;
    LeaveCriticalSection(&State.Lock);

    if (DlgHwnd)
        PostMessage(DlgHwnd, WM_APP_SFTP_WORKER_FINISHED, 0, 0);

#ifndef NO_OPENSSL
    OPENSSL_thread_stop();
#endif
}

bool CSftpTransferWorker::ExecuteTask(const CSftpTransferTask& task)
{
    bool ok = false;
    if (task.TaskType == CSftpTransferTask::TaskDownload)
    {
        ok = DoDownloadRecursive(task.RemotePath, task.LocalPath, task.IsDirectory);
        if (ok && task.DeleteSourceOnSuccess)
        {
            if (task.IsDirectory)
                WorkerConn.RemoveDir(task.RemotePath.c_str());
            else
                WorkerConn.RemoveFile(task.RemotePath.c_str());
        }
    }
    else if (task.TaskType == CSftpTransferTask::TaskUpload)
    {
        ok = DoUploadRecursive(task.LocalPath, task.RemotePath, task.IsDirectory);
        if (ok && task.DeleteSourceOnSuccess)
        {
            if (task.IsDirectory)
                RemoveDirectoryA(task.LocalPath.c_str());
            else
                DeleteFileA(task.LocalPath.c_str());
        }
    }
    return ok;
}

bool CSftpTransferWorker::DoDownloadRecursive(const std::string& remote, const std::string& local, bool isDir)
{
    {
        EnterCriticalSection(&State.Lock);
        if (State.Cancelled)
        {
            LeaveCriticalSection(&State.Lock);
            return false;
        }
        strncpy_s(State.CurrentRemoteFile, remote.c_str(), _TRUNCATE);
        strncpy_s(State.CurrentLocalFile, local.c_str(), _TRUNCATE);
        State.CurrentFileDone = 0;
        State.CurrentFileTotal = 0;
        LeaveCriticalSection(&State.Lock);
    }

    if (!isDir)
    {
        unsigned __int64 resumeOffset = 0;
        if (GetFileAttributesA(local.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            // Local file exists - determine whether to resume, overwrite, or skip
            WIN32_FILE_ATTRIBUTE_DATA fad;
            unsigned __int64 localSize = 0;
            if (GetFileAttributesExA(local.c_str(), GetFileExInfoStandard, &fad))
            {
                ULARGE_INTEGER uli;
                uli.LowPart = fad.nFileSizeLow;
                uli.HighPart = fad.nFileSizeHigh;
                localSize = uli.QuadPart;
            }

            unsigned __int64 remoteSize = 0;
            WorkerConn.RemoteFileSize(remote.c_str(), remoteSize);

            int decision = AskOverwriteWorker(local.c_str(), true, localSize, remoteSize, resumeOffset);
            if (decision == 0) // Skip
                return true;
            if (decision < 0)  // Cancel
            {
                Cancel();
                return false;
            }
        }

        EnterCriticalSection(&State.Lock);
        if (State.Cancelled)
        {
            LeaveCriticalSection(&State.Lock);
            return false;
        }
        LeaveCriticalSection(&State.Lock);

        return WorkerConn.Download(remote.c_str(), local.c_str(), resumeOffset);
    }

    // Recursive directory download
    CreateDirectoryA(local.c_str(), NULL);
    std::vector<CSftpEntry> entries;
    if (!WorkerConn.ListDir(remote.c_str(), entries))
        return false;

    for (size_t i = 0; i < entries.size(); i++)
    {
        EnterCriticalSection(&State.Lock);
        if (State.Cancelled)
        {
            LeaveCriticalSection(&State.Lock);
            return false;
        }
        LeaveCriticalSection(&State.Lock);

        char r[MAX_PATH], l[MAX_PATH];
        SftpJoin(remote.c_str(), entries[i].Name.c_str(), r, MAX_PATH);
        lstrcpynA(l, local.c_str(), MAX_PATH);
        PathAppendA(l, entries[i].Name.c_str());

        if (!DoDownloadRecursive(r, l, entries[i].IsDir))
            return false;
    }
    return true;
}

bool CSftpTransferWorker::DoUploadRecursive(const std::string& local, const std::string& remote, bool isDir)
{
    {
        EnterCriticalSection(&State.Lock);
        if (State.Cancelled)
        {
            LeaveCriticalSection(&State.Lock);
            return false;
        }
        strncpy_s(State.CurrentLocalFile, local.c_str(), _TRUNCATE);
        strncpy_s(State.CurrentRemoteFile, remote.c_str(), _TRUNCATE);
        State.CurrentFileDone = 0;
        State.CurrentFileTotal = 0;
        LeaveCriticalSection(&State.Lock);
    }

    if (!isDir)
    {
        unsigned __int64 resumeOffset = 0;
        if (WorkerConn.PathType(remote.c_str()) == 1) // Remote file exists
        {
            unsigned __int64 remoteSize = 0;
            WorkerConn.RemoteFileSize(remote.c_str(), remoteSize);

            WIN32_FILE_ATTRIBUTE_DATA fad;
            unsigned __int64 localSize = 0;
            if (GetFileAttributesExA(local.c_str(), GetFileExInfoStandard, &fad))
            {
                ULARGE_INTEGER uli;
                uli.LowPart = fad.nFileSizeLow;
                uli.HighPart = fad.nFileSizeHigh;
                localSize = uli.QuadPart;
            }

            int decision = AskOverwriteWorker(remote.c_str(), false, remoteSize, localSize, resumeOffset);
            if (decision == 0) // Skip
                return true;
            if (decision < 0)  // Cancel
            {
                Cancel();
                return false;
            }
        }

        EnterCriticalSection(&State.Lock);
        if (State.Cancelled)
        {
            LeaveCriticalSection(&State.Lock);
            return false;
        }
        LeaveCriticalSection(&State.Lock);

        return WorkerConn.Upload(local.c_str(), remote.c_str(), resumeOffset);
    }

    // Recursive directory upload
    WorkerConn.MakeDir(remote.c_str());

    char mask[MAX_PATH];
    lstrcpynA(mask, local.c_str(), MAX_PATH);
    PathAppendA(mask, "*");

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(mask, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return true;

    bool ok = true;
    do
    {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        EnterCriticalSection(&State.Lock);
        if (State.Cancelled)
        {
            LeaveCriticalSection(&State.Lock);
            ok = false;
            break;
        }
        LeaveCriticalSection(&State.Lock);

        char l[MAX_PATH], r[MAX_PATH];
        lstrcpynA(l, local.c_str(), MAX_PATH);
        PathAppendA(l, fd.cFileName);
        SftpJoin(remote.c_str(), fd.cFileName, r, MAX_PATH);

        bool subIsDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!DoUploadRecursive(l, r, subIsDir))
        {
            ok = false;
            break;
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return ok;
}

int CSftpTransferWorker::AskOverwriteWorker(const char* path, bool isLocal, unsigned __int64 existingSize, unsigned __int64 newSize, unsigned __int64& outResumeOffset)
{
    outResumeOffset = 0;
    if (OverwriteAllDecision == 1) // Overwrite all
        return 1;
    if (OverwriteAllDecision == 0) // Skip all
        return 0;
    if (OverwriteAllDecision == 2) // Resume all
    {
        if (existingSize > 0 && existingSize < newSize)
            outResumeOffset = existingSize;
        return 1;
    }

    // By default for non-interactive / background: resume if partial, overwrite otherwise
    if (existingSize > 0 && existingSize < newSize)
    {
        outResumeOffset = existingSize;
        return 1; // Resume
    }
    return 1; // Overwrite
}

bool CSftpTransferWorker::StaticProgressCallback(void* ctx, const char* name, unsigned __int64 done, unsigned __int64 total)
{
    CSftpTransferWorker* worker = static_cast<CSftpTransferWorker*>(ctx);
    return worker->ReportProgress(name, done, total);
}

bool CSftpTransferWorker::ReportProgress(const char* name, unsigned __int64 done, unsigned __int64 total)
{
    EnterCriticalSection(&State.Lock);
    if (State.Cancelled)
    {
        LeaveCriticalSection(&State.Lock);
        return false;
    }

    DWORD now = GetTickCount();
    DWORD elapsed = now - State.StartTick;
    unsigned __int64 prevDone = State.CurrentFileDone;
    State.CurrentFileDone = done;
    State.CurrentFileTotal = total;

    if (done >= prevDone)
        State.TotalBytesDone += (done - prevDone);

    if (elapsed > 200 && State.TotalBytesDone > 0)
    {
        State.BytesPerSec = (double)State.TotalBytesDone / ((double)elapsed / 1000.0);
    }

    bool notify = (now - State.LastUpdateTick >= 100);
    if (notify)
        State.LastUpdateTick = now;

    LeaveCriticalSection(&State.Lock);

    if (notify && DlgHwnd)
    {
        PostMessage(DlgHwnd, WM_APP_SFTP_WORKER_UPDATE, 0, 0);
    }

    return true;
}
