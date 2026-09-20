#include "precomp.h"
#include <stdio.h>
#include <assert.h>
#include "sftpworker.h"
#include "sftpconflictdlg.h"

CSalamanderGeneralAbstract* SalamanderGeneral = nullptr;
CSalamanderGUIAbstract* SalamanderGUI = nullptr;
HINSTANCE HLanguage = NULL;
char* LoadStr(int) { static char empty[] = ""; return empty; }
bool SftpInputDialog(HWND, const char*, bool, char*, int) { return false; }
void SftpApplyDarkModeToWindow(HWND) {}
BOOL PluginDarkMode_HandleThemeMessage(HWND, UINT, LPARAM) { return FALSE; }
BOOL PluginDarkMode_HandleCtlColor(UINT, WPARAM, LPARAM, LRESULT*) { return FALSE; }

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, hParent, origin) {}
INT_PTR CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) { return CDialog::DialogProc(uMsg, wParam, lParam); }
void CCommonDialog::NotifDlgJustCreated() {}


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

    // Test Reset()
    worker.Reset();
    assert(!worker.HasTasks());
    worker.GetStateSnapshot(snap);
    assert(snap.TotalItemsCount == 0);
    assert(snap.TotalBytesExpected == 0);
    assert(!snap.Cancelled);
    printf("  Worker Reset() verified successfully.\n");

    // Test fresh second transfer
    CSftpTransferTask task3;
    task3.TaskType = CSftpTransferTask::TaskDownload;
    task3.RemotePath = "/mnt/data/single.txt";
    task3.LocalPath = "C:\\Downloads\\single.txt";
    task3.FileSize = 1024;
    task3.ResumeOffset = 0;
    task3.IsDirectory = false;
    task3.DeleteSourceOnSuccess = false;

    worker.EnqueueTask(task3);
    assert(worker.HasTasks());
    worker.GetStateSnapshot(snap);
    assert(snap.TotalItemsCount == 1);
    assert(snap.TotalBytesExpected == 1024);
    // Test Pause / Resume and task snapshot
    assert(!worker.IsPaused());
    worker.SetPaused(true);
    assert(worker.IsPaused());
    worker.GetStateSnapshot(snap);
    assert(snap.IsPaused);
    worker.SetPaused(false);
    assert(!worker.IsPaused());
    printf("  Worker Pause/Resume control verified successfully.\n");

    std::vector<CSftpTransferTask> tasksSnap;
    worker.GetTasksSnapshot(tasksSnap);
    assert(tasksSnap.size() == 1);
    assert(tasksSnap[0].Status == CSftpTransferTask::StatusWaiting);
    printf("  Worker task snapshot verified successfully (size %zu).\n", tasksSnap.size());

    worker.Reset();
    printf("ALL SFTP TRANSFER WORKER UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
