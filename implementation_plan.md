# Implementační plán – Stabilizace spojení s TrueNAS a vylepšení výpočtu velikosti složek (Calc Size)

## 🎯 Cíle

1. **Vyřešit odpojování od TrueNAS / OpenSSH serverů**:
   - TrueNAS používá výchozí konfiguraci OpenSSH s `ClientAliveInterval` (server posílá sondy `keepalive@openssh.com` klientovi).
   - Když uživatel v Salamanderu neprovádí žádnou akci, plugin byl v nečinnosti a neodpovídal na transportní vrstvě libssh2, což vedlo k tomu, že TrueNAS po vypršení `ClientAliveCountMax` spojení jednostranně ukončil (FIN/RST).
   - Zároveň `want_reply = 1` u `libssh2_keepalive_send` vyvolával ze strany OpenSSH zbytečné odpovědi `SSH_MSG_REQUEST_FAILURE`.
   - Chyběla automatická obnova (reconnect & retry) přímo při selhání jednotlivých FS operací (`ListCurrentPath`, `PathType`, `Download`, ...).

2. **Výrazně vylepšit výpočet velikosti složky na serveru (`Calc Size`)**:
   - Nahradit blokující synchronní zamrznutí UI interaktivním průběžným dialogem (`OpenProgressDialog` / `ProgressDialogCheckCancel`) s možností zrušení (**Cancel**).
   - Implementovat rychlou server-side kalkulaci přes SSH exec (`du -sb` / `du -sk`) tam, kde je dostupný shell, s bezpečným fallbackem na rekurzivní SFTP skenování.
   - Ochrana proti zacyklení na cyklických symbolických odkazech (symlinks).
   - Okamžitá aktualizace velikosti v panelu Salamandera.

---

## 🛠️ Navržené změny

### 1. Řešení odpojování od TrueNAS & OpenSSH

#### [MODIFY] [`src/sftpconn.h`](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconn.h) & [`src/sftpconn.cpp`](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconn.cpp)
- Přidat metodu `SendKeepalive()`:
  - Zavolá `libssh2_keepalive_send(Session, ...)` a zpracuje příchozí pakety ze socketu bez blokování.
  - Změnit konfiguraci na `libssh2_keepalive_config(Session, 0, 10)` (`want_reply = 0`), aby OpenSSH negenerovalo `SSH_MSG_REQUEST_FAILURE`.
- Vylepšit `IsConnected()` o detekci stavu socketu před i po transportním zápisu.
- Do `ListDir`, `PathType`, `Download`, `Upload`, `StatFull` přidat detekci odpojení a možnost automatického znovupřipojení a opakování operace.

#### [MODIFY] [`src/fs2.cpp`](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- V `CPluginFSInterface::ChangePath` po úspěšném připojení zaregistrovat periodický FS časovač:
  `SalamanderGeneral->AddPluginFSTimer(8000, this, SFTP_TIMER_KEEPALIVE);`
- V `CPluginFSInterface::Event`:
  - Při `event == FSE_TIMER` a `param == SFTP_TIMER_KEEPALIVE`:
    - Zavolat `SftpConn.SendKeepalive()`.
    - Znovu naplánovat `SalamanderGeneral->AddPluginFSTimer(8000, this, SFTP_TIMER_KEEPALIVE)`.
  - Tím je zaručeno, že i při dlouhé nečinnosti uživatele (kdy Salamander čeká v message loop) se každých 8 sekund odešle keepalive a odbaví příchozí `ClientAliveInterval` sondy z TrueNAS.
- V `ListCurrentPath`: pokud `ListDir` selže z důvodu odpojeného socketu, provést transparentní `SftpEnsureConnected` a 1x opakovat čtení složky.

---

### 2. Vylepšení výpočtu velikosti složek (`Calc Size`)

#### [MODIFY] [`src/sftpconn.h`](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconn.h) & [`src/sftpconn.cpp`](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconn.cpp)
- Přidat metodu `FastDirSize(const char* remotePath, unsigned __int64& outBytes, int& outFiles, int& outDirs)`:
  - Pokusí se o rychlý server-side výpočet přes SSH exec (`du -sb` nebo POSIX `find`/`wc`).
  - Pokud server příkaz nepodporuje nebo je v režimu omezeného SFTP subsystému, vrátí `false` a použije se SFTP rekurze.

#### [MODIFY] [`src/fs2.cpp`](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp) & [`src/sftp.cpp`](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.cpp)
- Přepracovat `SftpCalcSize`:
  - Použít `SalamanderGeneral->OpenProgressDialog` s textem "Počítání velikosti na serveru...".
  - Během rekurze pravidelně kontrolovat `SalamanderGeneral->ProgressDialogCheckCancel()`.
  - Pokud uživatel stiskne Storno / Cancel, výpočet se bezpečně přeruší bez pádu a bez zamrznutí.
  - Zobrazovat aktuálně procházenou složku a mezisoučty přes `SalamanderGeneral->ProgressDialogAddText`.
  - Zamezit cyklickému zanoření symlinků (symlinky nezanořovat, počítat pouze jejich velikost).
  - Po dokončení aktualizovat velikost položek v panelu a zavolat `SalamanderGeneral->RepaintChangedItems(panel)`.
- Zpřístupnit `Ctrl+Shift+F10` a kontextové menu:
  - V `src/sftp.cpp` přiřadit klávesovou zkratku `SALHOTKEY(VK_F10, HOTKEYF_CONTROL | HOTKEYF_SHIFT)` položce `Calculate &Size (server)`.
  - V `src/fs2.cpp` přidat `FS_SERVICE_CALCULATEOCCUPIEDSPACE` a povolit příkazy `SALCMD_CALCDIRSIZES` a `SALCMD_OCCUPIEDSPACE` v `ContextMenu()` s namapováním na `MENUCMD_CALCSIZE`.

---

## 🧪 Verifikační plán

### 1. Automatické a integrační testy
- Kompilace pluginu přes `mingw32-make -f Makefile.mingw CROSS_COMPILE=`.
- Sestavení a spuštění testu `test/test_isconnected.cpp` a nového testu `test/test_keepalive.cpp`.
- Spuštění statické analýzy `cppcheck --enable=warning,performance,portability,style src/`.

### 2. Manuální ověření
- Test nečinnosti na TrueNAS / OpenSSH serveru po dobu několika minut – ověření, že spojení zůstává aktivní a nevypadává.
- Test `Calculate Size (server)` na složce s mnoha podsložkami a soubory – ověření dialogu průběhu, tlačítka Cancel a správného zobrazení velikosti v panelu Salamandera.
