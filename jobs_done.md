# Zpráva o realizaci (Jobs Done) – SFTP Plugin

Dokument rekapituluje realizované úkoly a opravy v pluginu **SFTP/SCP pro Open Salamander**.

---

## 📋 Přehled realizovaných úkolů

| Oblast | Popis řešení | Klíčové soubory |
|---|---|---|
| **Dokumentace & Release (Fork od Dupl3xx)** | Kompletní aktualizace [README.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README.md) a [README_CZ.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README_CZ.md) před vydáním releasu. Jasně uveden původ projektu jako fork z `https://github.com/Dupl3xx/salamander-sftp-plugin`, uvedeno poděkování původnímu autorovi a detailně zdokumentovány všechny klíčové rozdíly a vylepšení (statický build bez MinGW DLL, asynchronní konzole příkazů, aktivní keepalive pro TrueNAS, bleskový Calc Size `Ctrl+Shift+F10`, oprava focusu při návratu do `..`, synchronizace všech 11 jazyků). | `README.md`, `README_CZ.md`, `jobs_done.md`, `implementation_plan.md` |
| **Aktivní Keepalive & TrueNAS podpora** | Vyřešeno odpojování od TrueNAS / OpenSSH serverů způsobené nečinností v UI a `ClientAliveInterval`. Implementován periodický FS časovač (`AddPluginFSTimer` každých 8 s), neblokující `SendKeepalive()`, přepnuto na `want_reply = 0` u SSH keepalive sond (zamezení `SSH_MSG_REQUEST_FAILURE`), a přidán transparentní auto-reconnect & retry při čtení složek v `ListCurrentPath`. | `src/sftpconn.cpp`, `src/sftpconn.h`, `src/fs2.cpp` |
| **Vylepšený výpočet velikosti složek (`Calc Size`) & Klávesové zkratky** | Přepracován výpočet velikosti na serveru: přidána rychlá server-side kalkulace `FastDirSize` přes SSH exec (`du -sb` / `du -sk` / `find`), interaktivní a nezamrzající dialog průběhu s možností okamžitého zrušení (**Cancel** přes `CDeleteProgressDlg`), ochrana proti zacyklení na symlincích a automatický update velikostí v panelu Salamandera. Povolena a namapována standardní klávesová zkratka **`Ctrl+Shift+F10`** a položky v kontextovém menu (`SALCMD_CALCDIRSIZES`, `SALCMD_OCCUPIEDSPACE`). Analyzováno a zdokumentováno chování mezerníku (Spacebar) v jádře Salamandera. | `src/sftpconn.cpp`, `src/sftpconn.h`, `src/fs2.cpp`, `src/sftp.cpp` |
| **Udržování spojení & Auto-reconnect** | Zavedení TCP keepalive sond (15s idle / 5s interval) a SSH keepalive pakety. Implementace přesné detekce živosti socketu v `IsConnected()` (`select` + `MSG_PEEK`) a transparentní reconnect v `SftpEnsureConnected`. | `src/sftpconn.cpp`, `src/sftpconn.h`, `src/sftpglue.cpp` |
| **Navigace & Focus na `..`** | Při opuštění složky přes `..` (`isDir == 2`) se extrahuje název opouštěné podsložky a předá se jako `suggestedFocusName` do `ChangePanelPathToPluginFS`. Salamander tak správně vyfokusuje složku v nadřazeném výpisu. | `src/fs1.cpp` |
| **Protokoly SFTP & SCP** | Plná podpora SFTP v3 a SCP přenosů přes libssh2 + OpenSSL. | `src/sftpconn.cpp`, `src/fs2.cpp` |
| **Přihlašovací dialog** | Správa uložených relací, profily připojení, šifrování hesel. | `src/fs1.cpp`, `src/sftpglue.cpp` |
| **Statické linkování & Cppcheck audit** | Přidání `-static` do `Makefile.mingw` (odstranění dynamické závislosti na `libwinpthread-1.dll`), statické slinkování pthreads + libgcc + libstdc++ + libssh2. Provedení statické analýzy přes Cppcheck 2.21 a vyřešení nálezů. | `Makefile.mingw`, `src/sftpconn.cpp`, `src/dialogs.cpp` |
| **Příkazová konzole** | Spouštění příkazů na serveru s reálným výstupem v okně konzole. | `src/dialogs.cpp`, `src/menu.cpp` |
