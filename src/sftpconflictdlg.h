// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "sftp.h"

enum ESftpConflictAction
{
    SFTP_CONFLICT_RETRY,                // Retry from scratch (IDOK)
    SFTP_CONFLICT_RESUME,               // Resume from existing size (CM_SIED_RESUME)
    SFTP_CONFLICT_RESUME_OR_OVERWRITE,  // Resume if smaller, else overwrite (CM_SIED_RESUMEOROVR)
    SFTP_CONFLICT_OVERWRITE,            // Overwrite single (CM_SIED_OVERWRITE)
    SFTP_CONFLICT_OVERWRITE_ALL,        // Overwrite all (CM_SIED_OVERWRITEALL)
    SFTP_CONFLICT_SKIP,                 // Skip this file (IDB_SCRD_SKIP)
    SFTP_CONFLICT_CANCEL                // Cancel entire transfer (IDCANCEL)
};

class CSftpConflictDlg : public CCommonDialog
{
public:
    CSftpConflictDlg(HWND parent,
                     const char* srcPath, const char* srcName,
                     const char* tgtPath, const char* tgtName,
                     unsigned __int64 srcSize, unsigned __int64 tgtSize);

    INT_PTR ExecuteDlg(ESftpConflictAction& outAction, BOOL& outApplyToAll, char* outNewName, int maxNewName);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    void GenerateAlternateName(char* outBuf, int maxLen);

    HWND CenterWnd;
    char SrcPath[MAX_PATH * 2];
    char SrcName[MAX_PATH];
    char TgtPath[MAX_PATH * 2];
    char TgtName[MAX_PATH];
    char ResultName[MAX_PATH];
    unsigned __int64 SrcSize;
    unsigned __int64 TgtSize;
    ESftpConflictAction Action;
    BOOL ApplyToAll;
};
