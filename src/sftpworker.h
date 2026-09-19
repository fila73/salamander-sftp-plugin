// Copyright © 2026 Dupl3xx
//
// Background worker thread for asynchronous SFTP transfers.
#pragma once
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <vector>
#include <deque>
#include "sftpconn.h"
#include "sftpglue.h"

// Custom window messages for worker -> UI notifications
#define WM_APP_SFTP_WORKER_UPDATE        (WM_APP + 101)
#define WM_APP_SFTP_WORKER_FINISHED      (WM_APP + 102)
#define WM_APP_SFTP_WORKER_ASK_OVERWRITE (WM_APP + 103)

// Single transfer task in the worker queue
struct CSftpTransferTask
{
    enum Type { TaskDownload, TaskUpload };
    Type TaskType;
    std::string RemotePath;
    std::string LocalPath;
    unsigned __int64 FileSize;      // expected file size (or 0 if unknown)
    unsigned __int64 ResumeOffset;  // 0 = start from beginning
    bool IsDirectory;               // true = recursive directory
    bool DeleteSourceOnSuccess;     // true for Move operation
};

// Thread-safe shared state snapshot
struct CSftpTransferState
{
    CRITICAL_SECTION Lock;

    char CurrentLocalFile[MAX_PATH];
    char CurrentRemoteFile[MAX_PATH];
    unsigned __int64 CurrentFileDone;
    unsigned __int64 CurrentFileTotal;

    int CurrentItemIndex;
    int TotalItemsCount;
    unsigned __int64 TotalBytesDone;
    unsigned __int64 TotalBytesExpected;

    DWORD StartTick;
    DWORD LastUpdateTick;
    double BytesPerSec;

    volatile bool IsRunning;
    volatile bool Cancelled;
    volatile bool HasError;
    char ErrorMsg[512];

    void Init()
    {
        InitializeCriticalSection(&Lock);
        Reset();
    }

    void Destroy()
    {
        DeleteCriticalSection(&Lock);
    }

    void Reset()
    {
        CurrentLocalFile[0] = 0;
        CurrentRemoteFile[0] = 0;
        CurrentFileDone = 0;
        CurrentFileTotal = 0;
        CurrentItemIndex = 0;
        TotalItemsCount = 0;
        TotalBytesDone = 0;
        TotalBytesExpected = 0;
        StartTick = 0;
        LastUpdateTick = 0;
        BytesPerSec = 0.0;
        IsRunning = false;
        Cancelled = false;
        HasError = false;
        ErrorMsg[0] = 0;
    }
};

// Background worker thread managing dedicated CSftpConnection
class CSftpTransferWorker
{
public:
    CSftpTransferWorker();
    ~CSftpTransferWorker();

    // Start worker thread with target connection profile
    bool Start(const CSftpProfile& profile, HWND dlgHwnd = NULL);
    // Gracefully stop worker and wait for thread termination
    void Stop();
    // Add transfer task to queue
    void EnqueueTask(const CSftpTransferTask& task);
    // Cancel ongoing and queued transfers
    void Cancel();

    bool IsRunning() const;
    bool HasTasks() const;
    void SetDlgHwnd(HWND hwnd) { DlgHwnd = hwnd; }

    // Take a thread-safe snapshot of the current state
    void GetStateSnapshot(CSftpTransferState& outState);

    // Get reference to worker connection profile
    const CSftpProfile& GetProfile() const { return Profile; }

private:
    static DWORD WINAPI ThreadEntryPoint(LPVOID param);
    void ThreadLoop();

    bool ExecuteTask(const CSftpTransferTask& task);
    bool DoDownloadRecursive(const std::string& remote, const std::string& local, bool isDir);
    bool DoUploadRecursive(const std::string& local, const std::string& remote, bool isDir);

    static bool StaticProgressCallback(void* ctx, const char* name, unsigned __int64 done, unsigned __int64 total);
    bool ReportProgress(const char* name, unsigned __int64 done, unsigned __int64 total);

    HANDLE ThreadHandle;
    DWORD ThreadId;
    HANDLE WakeEvent;
    HANDLE StopEvent;
    HWND DlgHwnd;

    CRITICAL_SECTION QueueLock;
    std::deque<CSftpTransferTask> TaskQueue;

    CSftpConnection WorkerConn;
    CSftpProfile Profile;
    CSftpTransferState State;

    // Overwrite decision handling in worker
    int OverwriteAllDecision; // -1 = ask, 0 = skip, 1 = overwrite all, 2 = resume all
    int AskOverwriteWorker(const char* path, bool isLocal, unsigned __int64 existingSize, unsigned __int64 newSize, unsigned __int64& outResumeOffset);
};
