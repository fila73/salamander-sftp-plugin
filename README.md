# SFTP/SCP Plugin for Open Salamander (x64)

A full-featured **SFTP and SCP** client as a file-system plugin for [Open Salamander 5.0](https://github.com/OpenSalamander/salamander) (x64) and Altap Salamander 4.x. Built on top of **libssh2 + OpenSSL**, providing modern cryptography, high transfer speed, and maximum stability.

> [!NOTE]
> **Fork Notice & Attribution**:  
> This project is an enhanced and maintained fork of [Dupl3xx/salamander-sftp-plugin](https://github.com/Dupl3xx/salamander-sftp-plugin).  
> All credit for the original plugin architecture, SDK integration, and base implementation goes to **Dupl3xx**.  
> Core custom components Copyright © 2026 Dupl3xx & contributors.

---

## 🚀 Key Differences & Enhancements (vs. Upstream)

This fork introduces significant stability fixes, architecture improvements, enhanced performance, and new features:

| Feature / Area | Original Upstream ([Dupl3xx](https://github.com/Dupl3xx/salamander-sftp-plugin)) | This Enhanced Fork ([fila73](https://github.com/fila73/salamander-sftp-plugin)) |
|---|---|---|
| **Build & Dependencies** | Required external `libssh2.dll` and dynamic MinGW runtime DLLs (`libwinpthread-1.dll`, etc.) | **Standalone zero-dependency build**: `libssh2` is statically embedded; C/C++ runtime & pthreads linked statically (`-static`). Only `libcrypto-3-x64.dll` is required. |
| **Custom SFTP Server & Elevation** | Only default SSHD SFTP subsystem | **Custom `sftp-server` command support**: Execute SFTP under `sudo` / `su -c` or specific user (e.g. `www-data`), custom binary paths on NAS/BSD, and automatic prefix propagation to command-line bar executions. |
| **Command Execution** | Basic execution | **Asynchronous non-blocking background execution** with dedicated SSH connection; resizable streaming console dialog (Consolas font, text wrap, Cancel button). |
| **Connection Keepalive** | Basic TCP / idle handling | **Active periodic FS timer keepalive** (8s interval) preventing disconnects on TrueNAS / OpenSSH (`ClientAliveInterval`) and stateful firewalls; non-blocking socket health check & auto-reconnect. |
| **Directory Size Calculation** | Standard manual traversal | **Fast server-side calculation** (`FastDirSize` via SSH `du -sb`), non-blocking cancelable progress dialog, symlink cycle protection, **Spacebar on folder** calculation with auto-advance, and **`Ctrl+Shift+F10`** hotkey + context menu integration. |
| **Directory Navigation** | Reset focus on parent entry | **Preserves cursor focus** on the exited folder when navigating up (`..`). |

---

## Features

### Protocols
- **SFTP** (SSH File Transfer Protocol) – default, high-performance v3 protocol
- **SCP** – directory listing via shell (`ls`/`stat`), file transfer via `libssh2_scp_*`, operations (`mkdir`/`rm`/`mv`/`chmod`) via shell
- **Fallback SCP** – automatic transparent fallback to SCP when the remote server does not support the SFTP subsystem

### Custom SFTP Server Command (Privilege Escalation & User Switching)
In the connection dialog, you can configure the **SFTP Server** field for any profile:
1. **Manage files as `root` via `sudo`**:
   - `sudo /usr/lib/openssh/sftp-server`
   - `sudo su -c /usr/lib/openssh/sftp-server`
   - Ideal for servers where direct SSH root login is disabled (`PermitRootLogin no`). Authenticate as a normal user with SSH keys, while the SFTP session operates with full root privileges.
2. **Switch to service / application user**:
   - `sudo -u www-data /usr/lib/openssh/sftp-server`
   - Manage web directories directly under the webserver identity (new files automatically receive `www-data:www-data` ownership).
3. **Non-standard binary paths on NAS & BSD systems**:
   - Synology NAS: `/usr/syno/sbin/sftp-server`
   - QNAP / macOS / BSD: `/usr/libexec/sftp-server`
   - OpenWrt / BusyBox: `/usr/lib/ssh/sftp-server`
4. **Custom server flags & options**:
   - `/usr/lib/openssh/sftp-server -u 0022` (set default umask for created files)
   - `/usr/lib/openssh/sftp-server -l DEBUG3` (verbose server-side logging)
5. **Automatic prefix propagation to command line**:
   - Commands executed from Open Salamander's bottom command line bar automatically inherit the `sudo` / `su` prefix (e.g. `sudo su -c '<command>'`), executing in the same target user context.

### Cryptography (via OpenSSL Backend)
Negotiated automatically based on server capabilities:
- **Key Exchange**: curve25519-sha256, ECDH (nistp256/384/521), DH group14/16/18
- **Key Types**: ed25519, ECDSA, RSA (rsa-sha2-256/512)
- **Ciphers**: ChaCha20-Poly1305, AES-GCM, AES-CTR
- **Compression**: zlib (optional)

### Authentication
- **Password**
- **Private Key** – OpenSSH/PEM and **PuTTY `.ppk`** (RSA + ed25519, v2/v3, including encrypted keys – v2 SHA1/AES, v3 Argon2id via OpenSSL)
- **Keyboard-Interactive** – including 2FA / MFA (first prompt auto-filled with password, subsequent prompts handled interactively via dialog)

### Security
- **Host Key Verification** against `known_hosts` (`%APPDATA%\OpenSalamander-SFTP\known_hosts`)
- Trust prompt for unknown servers (Save / Just once / Reject), warning on key change (MITM detection)
- Fingerprint display (SHA256 / SHA1), key type, and server identification banner

### File Operations & Features
- Remote file browsing (permissions, owner, group columns), downloading, uploading
- **Transfer progress with live speed gauge**, overwrite confirmations
- **Resume interrupted transfers** – byte-exact resume from last position (SFTP)
- Delete, create directory, rename, **change permissions (`chmod`)**, properties
- **Edit file on server** (`F4` – downloads to temp, opens configured editor, automatically re-uploads on save)
- **Calculate directory size** (`Ctrl+Shift+F10` / Spacebar on folder, fast server-side `du` with recursive fallback, non-blocking progress dialog with Cancel button, symlink cycle protection, and panel size updates)
- **Remote Command Execution**:
  - Direct execution via Open Salamander command line bar below panels (with automatic `sudo`/`su` prefix wrapping)
  - Context menu item **`Execute`** (located right after `Open`)
  - Real-time command output console dialog with clean monospace font (Consolas) and cancel/interrupt support

### Connection Management & UI
- Login dialog with categorized tree view and saved connections (New / Edit / Delete / Rename / Set as Default)
- **Keepalive & Connection Stability**: Active periodic FS timer keepalive (8s) prevents idle disconnects on firewalls and TrueNAS / OpenSSH servers (`ClientAliveInterval`); non-blocking socket health check and automatic transparent reconnect & retry
- Password visibility toggle (eye icon)
- Dark mode support matching Open Salamander dark theme
- Multi-language localization (`.slg` modules for English, Czech, and all 11 Salamander languages)

---

## Source Code Structure (`src/`)

### Plugin Core
| File | Purpose |
|------|---------|
| **`sftpconn.h/.cpp`** | **Connection layer over libssh2.** Connect (handshake, host key verification, authentication), ListDir, Download/Upload (with resume + progress), SCP operations, chmod/stat, known_hosts verification, PuTTY `.ppk` parser, streaming command exec, active keepalive, fast directory size, custom `sftp-server` execution. |
| **`sftpglue.h/.cpp`** | **Glue** between Open Salamander FS and `CSftpConnection`. POSIX path helpers, `SftpEnsureConnected`, `WrapCommandWithSftpServerPrefix`, KBI dialog callbacks, connection profiles and saved sessions. |
| **`dialogs.h/.cpp`** | Real-time command execution console dialog (`ShowCommandExecDialog`), worker streaming thread, custom monospace font, and dark mode handling. |

### Salamander Integration
| File | Purpose |
|------|---------|
| `sftp.cpp` | Plugin entry point, registration (FS name `dfs`), menu command routing, window message hook for Spacebar calculation, registry configuration loading/saving. |
| `fs1.cpp` | **Login dialog** (`ConnectDlgProc`) – category tree, connection profiles, custom SFTP server field, saved sessions management, active FS tracking, directory navigation focus preservation. |
| `fs2.cpp` | **FS interface implementation** – ChangePath, ListCurrentPath, copy/download/upload (with resume/overwrite dialogs), Delete, CreateDir, QuickRename, ChangeAttributes (chmod), ShowProperties, SftpOnSpacePressedOnFolder, context menu. |
| `menu.cpp` | Menu command handlers (Edit file, Calculate size, Disconnect, Execute). |
| `sftp.h` | Shared declarations, `CFSData` (column attributes), command constants. |

### Resources and Build Files
| File | Purpose |
|------|---------|
| `lang/lang.rc`, `lang.rc2`, `lang.rh` | Dialog templates, string tables, Czech/English translations compiled into `.slg`. |
| `res/fs.ico`, `dir.ico`, `file.ico` | Icons for filesystem and dialogs. |
| `versinfo.rh2` | Version, copyright, and description resource header. |
| `Makefile.mingw` | GNU Make / GCC build script for MinGW-w64 (standalone build, static runtime). |
| `vcxproj/sftp.vcxproj` | Visual Studio MSBuild project file. |

---

## Building

### Option A: Using MinGW-w64 (via MSYS2, WSL or Native GCC)
The plugin can be built cleanly using the included `Makefile.mingw`:

```powershell
mingw32-make -f Makefile.mingw CROSS_COMPILE=
```

This compiles `sftp.spl`, `english.slg`, and `czech.slg` with statically linked `libssh2` and C/C++ runtimes (no MinGW DLL dependencies). For deployment on other PCs, only the standard 64-bit `libcrypto-3-x64.dll` needs to be placed alongside `sftp.spl`.

### Option B: Using Visual Studio (MSBuild)
**Requirements:** Visual Studio 2022 (x64), libssh2 + OpenSSL via vcpkg.

```powershell
MSBuild src\vcxproj\sftp.vcxproj /p:Configuration=Release /p:Platform=x64
```

---

## License & Credits
- **Upstream Repository**: [https://github.com/Dupl3xx/salamander-sftp-plugin](https://github.com/Dupl3xx/salamander-sftp-plugin)
- Based on Open Salamander SDK (SPDX / GPL-2.0-or-later).
- Original core implementation Copyright © 2026 Dupl3xx.
- Enhancements and maintenance Copyright © 2026 fila73 & contributors.
