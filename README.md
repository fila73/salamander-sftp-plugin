# SFTP/SCP Plugin for Open Salamander (x64)

A full-featured **SFTP and SCP** client as a file-system plugin for [Open Salamander 5.0](https://github.com/OpenSalamander/salamander) (x64). Built on top of **libssh2 + OpenSSL**, providing modern cryptography and high performance.

> Copyright © 2026 Dupl3xx  
> Based on the SDK template of Open Salamander (SPDX headers retained in SDK files). The core implementation (`sftpconn.*`, `sftpglue.*`, `dialogs.*`) is original work.

---

## Features

### Protocols
- **SFTP** (SSH File Transfer Protocol) – default
- **SCP** – directory listing via shell (`ls`/`stat`), file transfer via `libssh2_scp_*`, operations (`mkdir`/`rm`/`mv`/`chmod`) via shell
- **Fallback SCP** – automatic fallback to SCP when the server does not support the SFTP subsystem

### Cryptography (via OpenSSL Backend)
Negotiated automatically based on server capabilities:
- **Key Exchange**: curve25519-sha256, ECDH (nistp256/384/521), DH group14/16/18
- **Key Types**: ed25519, ECDSA, RSA (rsa-sha2-256/512)
- **Ciphers**: ChaCha20-Poly1305, AES-GCM, AES-CTR
- **Compression**: zlib (optional)

### Authentication
- **Password**
- **Private Key** – OpenSSH/PEM and **PuTTY `.ppk`** (RSA + ed25519, v2/v3, including encrypted keys – v2 SHA1/AES, v3 Argon2id via OpenSSL)
- **Keyboard-Interactive** – including 2FA / MFA (first prompt auto-filled with password, subsequent prompts handled via dialog)

### Security
- **Host Key Verification** against `known_hosts` (`%APPDATA%\OpenSalamander-SFTP\known_hosts`)
- Trust prompt for unknown servers (Save / Just once / Reject), warning on key change (MITM detection)
- Fingerprint display (SHA256 / SHA1), key type, and server identification banner

### File Operations & Features
- Remote file browsing (permissions, owner, group columns), downloading, uploading
- **Progress with transfer speed**, overwrite confirmations
- **Resume interrupted transfers** – byte-exact resume from last position (SFTP)
- Delete, create directory, rename, **change permissions (`chmod`)**, properties
- **Edit file on server** (F4 – downloads to temp, opens default editor, automatically re-uploads on save)
- **Calculate directory size**
- **Remote Command Execution**:
  - Direct execution via Open Salamander command line bar below panels
  - Context menu item **`Execute`** (located right after `Open`)
  - Real-time command output console dialog with clean monospace font (Consolas) and cancel/interrupt support

### Connection Management & UI
- Login dialog with categorized tree view and saved connections (New / Edit / Delete / Rename / Set as Default)
- **Keepalive & Connection Stability**: TCP and SSH keepalive probes (15s) prevent idle disconnects; non-blocking socket health check and automatic transparent reconnect
- Password visibility toggle (eye icon)
- Dark mode support matching Open Salamander dark theme
- Multi-language localization (`.slg` modules for English, Czech, and all 11 Salamander languages)

---

## Source Code Structure (`src/`)

### Plugin Core
| File | Purpose |
|------|---------|
| **`sftpconn.h/.cpp`** | **Connection layer over libssh2.** Connect (handshake, host key verification, authentication), ListDir, Download/Upload (with resume + progress), SCP operations, chmod/stat, known_hosts verification, PuTTY `.ppk` parser, streaming command exec. |
| **`sftpglue.h/.cpp`** | **Glue** between Open Salamander FS and `CSftpConnection`. POSIX path helpers, `SftpEnsureConnected`, KBI dialog callbacks, connection profiles and saved sessions. |
| **`dialogs.h/.cpp`** | Real-time command execution console dialog (`ShowCommandExecDialog`), worker streaming thread, custom font, and dark mode handling. |

### Salamander Integration
| File | Purpose |
|------|---------|
| `sftp.cpp` | Plugin entry point, registration (FS name `dfs`), menu command routing, registry configuration loading/saving. |
| `fs1.cpp` | **Login dialog** (`ConnectDlgProc`) – category tree, connection profiles, saved sessions management. |
| `fs2.cpp` | **FS interface implementation** – ChangePath, ListCurrentPath, copy/download/upload (with resume/overwrite dialogs), Delete, CreateDir, QuickRename, ChangeAttributes (chmod), ShowProperties, ExecuteCommandLine, context menu. |
| `menu.cpp` | Menu command handlers (Edit file, Calculate size, Disconnect, Execute). |
| `sftp.h` | Shared declarations, `CFSData` (column attributes), command constants. |

### Resources and Build Files
| File | Purpose |
|------|---------|
| `lang/lang.rc`, `lang.rc2`, `lang.rh` | Dialog templates, string tables, Czech/English translations compiled into `.slg`. |
| `res/fs.ico`, `dir.ico`, `file.ico` | Icons for filesystem and dialogs. |
| `versinfo.rh2` | Version, copyright, and description resource header. |
| `Makefile.mingw` | GNU Make / GCC build script for MinGW-w64 (WSL/MSYS2 cross-compilation). |
| `vcxproj/sftp.vcxproj` | Visual Studio MSBuild project file. |

---

## Building

### Option A: Using MinGW-w64 (via WSL or MSYS2)
The plugin can be built cleanly using the included `Makefile.mingw`:

```bash
cd src/plugins/sftp
make -f Makefile.mingw
```

This compiles `sftp.spl`, `English.slg`, and `Czech.slg` statically linked against `libssh2` and OpenSSL.

### Option B: Using Visual Studio (MSBuild)
**Requirements:** Visual Studio 2022 (x64), libssh2 + OpenSSL via vcpkg.

```powershell
MSBuild src\vcxproj\sftp.vcxproj /p:Configuration=Release /p:Platform=x64
```

---

## License
Based on Open Salamander SDK (SPDX / GPL-2.0-or-later). Core custom components Copyright © 2026 Dupl3xx.
