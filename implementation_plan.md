# Tmavý režim (Dark Mode) pro dialogy & Přenosy mezi SFTP servery (Server-to-Server Copy)

Tento plán řeší dva problémy zachycené na uživatelském screenshotu:
1. **Světlý dialog v tmavém režimu**: Přenosový dialog `CSftpTransferProgressDlg` byl bílý, protože plugin nenačítal stav `SALCFG_USEWINDOWSDARKMODE` ze Salamandera a `plugindarkmode.cpp` proto zůstával ve výchozím světlém režimu.
2. **Kopírování mezi servery**: Při kopírování mezi dvěma panely s různými SFTP servery (`[HOP Test]` -> `[HOP Stage]`) se cílová adresa ořízla na pouhý adresář a operace se nahrávala zpět na zdrojový server (vyvolalo se *Confirm File Overwrite* na témže souboru a dialog ukazoval `From: [HOP Test]` i `To: [HOP Test]`).

---

## Navržené změny

### 1. Tmavý režim (Dark Mode) pro všechny dialogy pluginu

#### [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h) & [sftp.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.cpp)
- Zavést funkci `SftpInitDarkMode(CSalamanderGeneralAbstract* general)`:
  - Dotázat se na `general->GetConfigParameter(SALCFG_USEWINDOWSDARKMODE, &useDark, sizeof(useDark), NULL)`.
  - Pokud je hodnota k dispozici, zavolat:
    ```cpp
    PluginDarkMode_SetHostPolicyAvailable(TRUE, useDark);
    COLORREF fg = general->GetCurrentColor(SALCOL_ITEM_FG_NORMAL);
    COLORREF bg = general->GetCurrentColor(SALCOL_ITEM_BK_NORMAL);
    PluginDarkMode_SetHostColors(fg, bg);
    ```
- Volat `SftpInitDarkMode(SalamanderGeneral)` v `CPluginInterface::Connect` a při inicializaci dialogů.

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- V dialogových procedurách `CSftpTransferProgressDlg::DialogProc`, `CDeleteProgressDlg::DialogProc` a `CCalcSizeProgressDlg::DialogProc`:
  - V `WM_INITDIALOG`:
    - Zavolat `SftpInitDarkMode(SalamanderGeneral)`.
    - Zavolat `PluginDarkMode_ApplyTitleBar(HWindow)`.
    - Zavolat `PluginDarkMode_ApplyListTreeThemeRecursive(HWindow)`.
  - V `WM_THEMECHANGED` a `WM_SETTINGCHANGE`:
    - Obnovit konfiguraci přes `SftpInitDarkMode(SalamanderGeneral)`.
    - Zavolat `PluginDarkMode_HandleThemeMessage(HWindow, uMsg, lParam)`.
    - Zavolat `PluginDarkMode_ApplyTitleBar(HWindow)`.
    - `InvalidateRect(HWindow, NULL, TRUE)`.
  - V `WM_CTLCOLORDLG`, `WM_CTLCOLORSTATIC`, `WM_CTLCOLORBTN`:
    - Zachovat a zajistit korektní návrat štětce z `PluginDarkMode_HandleCtlColor`.

---

### 2. Podpora Server-to-Server Copy (přenos mezi 2 SFTP servery)

#### [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h)
- Rozšířit `CSftpTransferProgressDlg`:
  - Přidat podporu pro oddělená jména profilů pro zdroj i cíl:
    ```cpp
    char FromConnName[128];
    char ToConnName[128];
    void SetOperationInfo(bool upload, const char* fromPath, const char* toPath,
                          int totalFiles, unsigned __int64 totalExpectedBytes,
                          const char* fromConnName = NULL, const char* toConnName = NULL);
    ```

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- Upravit `CSftpTransferProgressDlg::SetOperationInfo`:
  - Používat `FromConnName` pro `fromPath` a `ToConnName` pro `toPath`.
  - Dialog tak zobrazí:
    - `From: [HOP Test] /opt/hop`
    - `To: [HOP Stage] /opt/hop`
- Přepracovat větev `!diskPath` v `CPluginFSInterface::CopyOrMoveFromFS`:
  1. **Parsování cílové adresy**:
     - Cíl může mít tvar `sftp:sftp://user@host:port/path` nebo `sftp://user@host:port/path` nebo `sftp:/path`.
     - Analyzovat hostitele, uživatele a port cíle.
  2. **Detekce cílového serveru**:
     - Porovnat cílového hostitele s `this->Profile.Host`.
     - Pokud se shoduje: provést kopírování/přesun v rámci téhož serveru (`this->Conn`).
     - Pokud se liší:
       - Prohledat `InterfaceForFS.GetActiveFSList()`, zda existuje aktivní panel pro cílový server.
       - Pokud existuje, získat jeho profil `targetProf` (obsahující název např. `"HOP Stage"`, heslo/klíč apod.).
       - Pokud neexistuje v aktivních panelech, vyhledat odpovídající profil v uložených profilech `SftpProfiles`.
       - Navázat nebo použít spojení k cílovému serveru `targetConn`.
  3. **Přenos položek**:
     - Pro každý označený soubor/adresář:
       - Stáhnout ze zdrojového serveru `this->Conn` do lokálního `%TEMP%`.
       - Nahrát z lokálního `%TEMP%` na cílový server `targetConn`.
       - Smazat dočasný lokální soubor.
       - V případě přesunu (`!copy`) smazat zdroj ze zdrojového serveru `this->Conn`.
  4. **Notifikace a refresh**:
     - Po dokončení přenosu odeslat `PostChangeOnPathNotification` pro zdrojovou i cílovou cestu, aby se zaktualizovaly oba panely Salamandera.

---

## Verifikační plán

### Automatizované testy
- Spuštění `test_worker.exe` a `test_path_hottrack.exe` (100% pass).
- Případné přidání unit testu pro parsování server-to-server URL a formátování From/To štítků.

### Manuální ověření
1. **Ověření Dark Mode**:
   - V Salamanderu s aktivním tmavým režimem vyvolat přenos (F5) nebo znovuzobrazit okno přenosu.
   - Ověřit, že okno přenosu má tmavé pozadí, světlé čitelné texty a tmavě laděná tlačítka.
2. **Ověření Server-to-Server Copy**:
   - Otevřít v levém panelu server A (`[HOP Stage]`) a v pravém panelu server B (`[HOP Test]`).
   - Zkopírovat soubor klávesou F5 z pravého do levého panelu.
   - Ověřit, že v přenosovém dialogu je:
     - `From: [HOP Test] /opt/hop`
     - `To: [HOP Stage] /opt/hop`
   - Ověřit, že se soubor skutečně přenesl na server A a nezpůsobil přepsání na serveru B.

---

## Stav realizace (Completed)
- [x] Implementace Dark Mode policy handshake se Salamanderem (`SftpInitDarkMode`).
- [x] Stylizace oken a podřízených tlačítek přes `SftpApplyDarkModeToWindow` a `DarkMode_Explorer`.
- [x] Podpora oddělených profilů `FromConnName` a `ToConnName` v dialogu přenosu a formátování titulku.
- [x] Rozpoznání a streaming Server-to-Server přenosů v `CPluginFSInterface::CopyOrMoveFromFS` s vyhledáním cílového profilu v aktivních relacích i uložených profilech.
- [x] Rekompilace `sftp.spl`, úspěšný běh všech testů (100% pass).
- [x] Nasazení binárek do `C:\Apps\samandarin\plugins\sftp\` i `lang\`.
- [x] Aktualizace dokumentace (`jobs_done.md`, `nice_to_have.md`, `PLUGIN_DEV.md`, `README_CZ.md`, `README.md`, `implementation_plan.md`).

---

# Nezávislé instance workerů pro souběžné přenosy na pozadí & oprava kolize dialogů a pádu při ukončení

## Problém
Při spuštění dvou souběžných operací kopírování v témže panelu/profilu:
1. Po dokončení prvního vlákna začal první dialog zobrazovat objem a průběh druhého přenosu a nikdy se nezavřel.
2. Druhý dialog běžel souběžně; stisk Storno zavřel jen jedno okno, staré zůstalo viset.
3. Při ukončení Salamandera došlo k pádu procesu na access violation (`50B4C3A8C1E0EAB4-AS50SAM0.15.1X64-260920-015421.TXT`).

## Příčina
`CPluginFSInterface` vlastnil jedinou instanci `CSftpTransferWorker TransferWorker` a ukazatel `CSftpTransferProgressDlg* ActiveTransferDlg`. Spuštění druhého přenosu zavolalo `TransferWorker.Reset()`, čímž se vymazaly úlohy prvního přenosu a notifikace se přesměrovaly na `dlg2`. `dlg1` byl osiřelý a pollingoval worker běžícího druhého přenosu. Při zavření aplikace došlo k UAF.

## Realizované změny
1. **Per-transfer worker**: V `CopyOrMoveFromFS` a `CopyOrMoveFromDiskToFS` se alokuje `new CSftpTransferWorker()`.
2. **Vlastnictví workeru dialogem (`OwnsWorker`)**: Dialog v `DetachWorker()`, `WM_DESTROY` a `WM_APP_SFTP_WORKER_FINISHED` bezpečně zastavuje a uvolňuje worker thread.
3. **Evidence více dialogů v FS (`ActiveTransferDlgs`)**: Metody `RegisterTransferDlg` a `UnregisterTransferDlg` udržují seznam běžících oken.
4. **Čisté ukončení**: V `~CPluginFSInterface` se bezpečně odpojují a ruší všechna zbývající okna a workery.
5. **Menu Show Transfers**: Obnovuje všechna aktivní okna ze seznamu.

## Stav
- [x] Implementace `src/sftp.h`, `src/fs2.cpp`, `src/menu.cpp`.
- [x] Úspěšné zkompilování a testy (100% pass).
- [x] Nasazení binárky do `C:\Apps\samandarin\plugins\sftp\sftp.spl`.
- [x] Dokumentace aktualizována.

---

# Nezávislá okna přenosů (Z-Order, kliknutí do hlavního okna & minimalizace)

## Problém
Přenosové dialogy zůstávaly trvale zobrazené navrchu (Always On Top) a překrývaly souborové panely i po kliknutí do hlavního okna Salamandera.

## Příčina
Předání `parent` (HWND hlavního okna Salamandera) do `CCommonDialog` vytvořilo dialog s Win32 vlastnictvím (`owned window`). Podle Win32 pravidel správce oken vždy drží vlastněné okno nad jeho vlastníkem v Z-pořadí, i když vlastník získá fokus.

## Realizované změny
1. **Unowned okno (`Parent = NULL`)**: Dialog `CSftpTransferProgressDlg` předává do `CCommonDialog` hodnotu `NULL` namísto HWND Salamandera (podle vzoru `COperationDlg` z FTP pluginu Salamandera).
2. **Vycentrování (`CenterToWnd`)**: Původní `parent` se uchová v proměnné `CenterToWnd` a v `WM_INITDIALOG` se okno jednorázově vycentruje vůči Salamanderu pomocí `SalamanderGeneral->MultiMonCenterWindow`.
3. **Taskbar a minimalizace (`WS_EX_APPWINDOW`, `WS_MINIMIZEBOX`)**: Dialog má styl `WS_EX_APPWINDOW` a v resource souborech `lang_cs.rc` i `lang_en.rc` styl `WS_MINIMIZEBOX`.
4. **Z-order chování**: Při kliknutí do Salamandera se Salamander bez problémů přenese do popředí a dialog jej nepřekrývá.

## Stav
- [x] Implementace v `src/sftp.h`, `src/fs2.cpp`, `src/lang/lang_cs.rc`, `src/lang/lang_en.rc`.
- [x] Úspěšná kompilace `sftp.spl`, `english.slg`, `czech.slg`.
- [x] Spuštění testů (100% pass).
- [x] Nasazení do `C:\Apps\samandarin\plugins\sftp\` a `lang\`.
- [x] Dokumentace aktualizována.

---

# Oprava pádu ihned po zahájení kopírování (Access violation v `AcquireSRWLockShared` & OpenSSL DRBG / Threading)

## Problém
Ihned po zahájení kopírování souborů z/do SFTP panelu došlo k pádu aplikace Open Salamander (`50B4C3A8C1E0EAB4-AS50SAM0.15.1X64-260920-105106.TXT`).

## Příčina
1. K výjimce `EXCEPTION_ACCESS_VIOLATION` (zápis na neplatnou adresu zámku `0x000001F03E690A90`) došlo v novém worker vlákně `CSftpTransferWorker` při volání `libssh2_session_handshake` -> `kex.c` -> `ssh2_random` -> `RAND_bytes` -> `AcquireSRWLockShared` uvnitř `libcrypto-3-x64.dll`.
2. OpenSSL 3 využívá komplexní hierarchii generátorů náhodných čísel (DRBG) s per-thread TLS kontexty a zámky (`rand_global->lock`). Při přenosu z nového vlákna se DRBG pokusil o zamčení na neplatné/dealokované adrese zámku.
3. Vlákno bylo vytvářeno přes Win32 `CreateThread` namísto C Runtime `_beginthreadex`, což vedlo k neúplné inicializaci struktur CRT/TLS pro worker vlákno.

## Realizované změny
1. **Windows CNG CSPRNG (`BCryptGenRandom`)**: V `src/libssh2/openssl.c` byla funkce `ssh2_random` na Windows přepojena na nativní systémové jádrové rozhraní Windows CNG `BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)`. Volání je 100% thread-safe, FIPS certifikované a zcela nezávislé na stavu a zámcích OpenSSL DRBG.
2. **Korektní CRT inicializace vláken (`_beginthreadex`)**: Spouštění worker threadu v `CSftpTransferWorker::Start` bylo přepnuto na `_beginthreadex` z `<process.h>` se signaturou `unsigned __stdcall ThreadEntryPoint` a voláním `_endthreadex(0)`.
3. **Úklid OpenSSL stavu vlákna (`OPENSSL_thread_stop`)**: Na konci worker vlákna i v `GlobalExit` je voláno `OPENSSL_thread_stop()`.
4. **Explicitní inicializace OpenSSL (`OPENSSL_init_crypto`)**: V `CSftpConnection::GlobalInit` doplněno explicitní volání `OPENSSL_init_crypto`.

## Stav
- [x] Implementace v `src/libssh2/openssl.c`, `src/sftpworker.cpp`, `src/sftpworker.h`, `src/sftpconn.cpp`.
- [x] Úspěšná čistá kompilace `sftp.spl`, `english.slg`, `czech.slg`.
- [x] Spuštění testů: `test_worker.exe`, `test_path_hottrack.exe`, `test_multithread_handshake.exe` (100% pass).
- [x] Nasazení binárek do `C:\Apps\samandarin\plugins\sftp\`.
- [x] Dokumentace aktualizována.


