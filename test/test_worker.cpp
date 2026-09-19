#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "sftpworker.h"

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


int main()
{
    printf("Running SFTP Transfer Worker unit tests...\n");

    CSftpTransferWorker worker;

    assert(!worker.IsRunning());
    assert(!worker.HasTasks());

    CSftpTransferTask task1;
    task1.TaskType = CSftpTransferTask::TaskDownload;
    task1.RemotePath = "/mnt/data/test1.zip";
    task1.LocalPath = "C:\\Downloads\\test1.zip";
    task1.FileSize = 10485760; // 10 MB
    task1.ResumeOffset = 0;
    task1.IsDirectory = false;
    task1.DeleteSourceOnSuccess = false;

    worker.EnqueueTask(task1);
    assert(worker.HasTasks());

    CSftpTransferTask task2;
    task2.TaskType = CSftpTransferTask::TaskUpload;
    task2.RemotePath = "/mnt/data/photos";
    task2.LocalPath = "C:\\Pictures\\photos";
    task2.FileSize = 20971520; // 20 MB
    task2.ResumeOffset = 0;
    task2.IsDirectory = true;
    task2.DeleteSourceOnSuccess = true;

    worker.EnqueueTask(task2);

    CSftpTransferState snap;
    worker.GetStateSnapshot(snap);

    assert(snap.TotalItemsCount == 2);
    assert(snap.TotalBytesExpected == 31457280);
    assert(!snap.Cancelled);
    assert(!snap.HasError);

    printf("  Task queue size: 2 items, expected bytes: %llu\n", snap.TotalBytesExpected);

    worker.Cancel();
    worker.GetStateSnapshot(snap);
    assert(snap.Cancelled);
    printf("  Worker cancellation flag verified successfully.\n");

    worker.Stop();
    assert(!worker.IsRunning());

    printf("ALL SFTP TRANSFER WORKER UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
