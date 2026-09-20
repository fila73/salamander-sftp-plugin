# Návod na vývoj pluginů pro Open Salamander (SFTP Plugin Reference & Best Practices)

Tento dokument slouží jako přehled architektury, vývojových pravidel a osvědčených postupů při vývoji a údržbě pluginu **SFTP/SCP pro Open Salamander** (x64).

---

## 1. Architektura a adresářová struktura pluginu

Plugin do Salamandera se skládá z:
- **`sftp.spl`** – Dynamická knihovna pluginu (Windows DLL přejmenovaná na `.spl`).
- **`english.slg`, `czech.slg`** – Jazykové knihovny s přeloženými texty a dialogy.

### Adresářová struktura:

```
salamander-sftp-plugin/
├── src/
│   ├── sftp.cpp            # Hlavní vstup, SalamanderPluginEntry2, registrace FS
│   ├── sftp.h              # Společné deklarace rozhraní a struktur
│   ├── sftp.def            # Exportní definice
│   ├── sftpconn.cpp/.h     # Komunikační vrstva nad libssh2 (SFTP + SCP)
│   ├── sftpglue.cpp/.h     # Propojení mezi Salamander FS a CSftpConnection, práce s cestami
│   ├── fs1.cpp             # Správa relací, přihlašovací dialog, ExecuteOnFS
│   ├── fs2.cpp             # Implementace CPluginFSInterface (ChangePath, List, Copy, Delete, chmod)
│   ├── menu.cpp            # Obsluha položek menu
│   ├── dialogs.cpp/.h      # Dialogy konzole pro spouštění příkazů
│   ├── precomp.h           # Precompiled headers se Salamander SDK
│   └── lang/
│       ├── lang.rh         # Společné resource ID pro jazyky
│       └── lang.rc         # Texty a dialogy
├── test/                   # Unit testy pro nezávislou engine vrstvu
├── Makefile.mingw          # Sestavení pomocí MinGW-w64 (GCC/G++)
├── README.md               # Dokumentace (EN)
├── README_CZ.md            # Dokumentace (CZ)
├── jobs_done.md            # Přehled dokončených úkolů
└── implementation_plan.md  # Implementační plán
```

---

## 2. Navigace v adresářovém stromu a zachování fokusu

Při navigaci do nadřazeného adresáře (`..`, `isDir == 2`) v `ExecuteOnFS` je nutné:
1. Získat aktuální cestu `fs->Path` a normalizovat ji (`SftpNormalize`).
2. Odstranit případné koncové lomítko.
3. Najít poslední komponentu cesty (název opouštěné podsložky).
4. Vypočítat cestu k nadřazenému adresáři (`SftpParent`).
5. Předat název opouštěné podsložky jako parametr `suggestedFocusName` do `SalamanderGeneral->ChangePanelPathToPluginFS`.

Tím Salamander automaticky nastaví kurzor/focus na složku, ze které uživatel právě vystoupil.

---

## 3. Sestavení a testování

### Kompilace přes MinGW-w64:
```powershell
mingw32-make -f Makefile.mingw CROSS_COMPILE=
```

### Spuštění testů:
```powershell
g++ test/test_utf8.cpp src/sftpconn.o src/sftpglue.o libssh2_static.a libcrypto-3-x64.a -lws2_32 -lshlwapi -lbcrypt -lcrypt32 -Isrc -Isrc/libssh2/include -o test_utf8.exe
```

---

## 4. Udržování spojení a detekce odpojení (Keepalive & Auto-Reconnect)

Pro zajištění stability spojení na nestabilních sítích a proti timeoutům routerů/firewallů a serverů typu TrueNAS / OpenSSH:
1. **TCP Keepalive**: Socket má aktivovaný `SO_KEEPALIVE` s nastavením `SIO_KEEPALIVE_VALS` (15 s nečinnost, 5 s interval opakování).
2. **Proaktivní FS Timer Keepalive**: V `ChangePath` je registrován časovač `SalamanderGeneral->AddPluginFSTimer(8000, this, SFTP_TIMER_KEEPALIVE)`. Během nečinnosti uživatele (kdy Salamander čeká v message loop) se každých 8 s volá `SendKeepalive()`, což odbavuje `ClientAliveInterval` dotazy serveru (např. TrueNAS) a posílá keepalive sondu s `want_reply = 0`.
3. **Detekce stavu v `IsConnected()`**: Neblokující kontrola `select` s `recv(MSG_PEEK)` detekuje vzdálené uzavření spojení (FIN), reset (RST) nebo síťovou chybu ještě před zahájením další operace.
4. **Transparentní Reconnect & Retry**: `SftpEnsureConnected()` při zjištění odpojení automaticky obnoví spojení. Operace čtení adresářů `ListCurrentPath` při selhání provede transparentní znovunavázání a opakování operace.

---

## 5. Výpočet velikosti složek na serveru (Calc Size) a klávesové zkratky

Při výpočtu velikosti složek (`Calculate Size (server)`):
1. **Server-side optimalizace (`FastDirSize`)**: Nejprve se pokusí spustit rychlý výpočet na serveru přes SSH exec (`du -sb` / `du -sk` + `find`), což proběhne v milisekundách bez stahování výpisu souborů po síti.
2. **Bezpečný fallback na SFTP rekurzi**: Pokud server neumožňuje spuštění shellových příkazů, proběhne rekurzivní procházení podsložek s ochranou proti cyklení na symbolických odkazech.
3. **Nezamrzající dialog s Cancel**: Po celou dobu běhu je zobrazen dialog s průběžným stavem skenování a možností výpočet kdykoliv zrušit (klávesa Escape / tlačítko Storno).
4. **Aktualizace panelu**: Vypočtená velikost se zapíše do `CFileData` a panel se okamžitě překreslí.
5. **Klávesová zkratka `Ctrl+Shift+F10` & Kontextové menu**:
   - V `sftp.cpp` je pro položku `Calculate &Size (server)` registrována zkratka `SALHOTKEY(VK_F10, HOTKEYF_CONTROL | HOTKEYF_SHIFT)`.
   - V `fs2.cpp` (`ContextMenu()`) a v `GetSupportedServices()` (`FS_SERVICE_CALCULATEOCCUPIEDSPACE`) jsou standardní příkazy Salamandera `SALCMD_CALCDIRSIZES` a `SALCMD_OCCUPIEDSPACE` povoleny a přesměrovány na obsluhu `MENUCMD_CALCSIZE`.
6. **Řešení mezerníku (Spacebar) přes Windows Message Hook**:
   - V jádře Open Salamandera (`fileswn0.cpp:1082-1087`) je stisk mezerníku (`VK_SPACE`) pro pluginy (`ptPluginFS`) označen poznámkou `// to be implemented` a stisk mezerníku pouze invertuje výběr položky jako klávesa Insert bez volání pluginu.
   - K překonání tohoto omezení plugin instaluje vláknový Windows Message Hook (`SetWindowsHookEx(WH_GETMESSAGE, GetMsgHookProc, NULL, GetCurrentThreadId())`).
   - Hook zachytí `WM_KEYDOWN` s `VK_SPACE`, ověří, že uživatel nepíše do editboxu a že je aktivní panel s naším pluginem (`InterfaceForFS.IsOurFS(activeFS)`).
   - Zprávu zkonzumuje (`pMsg->message = WM_NULL`) a provede `SftpOnSpacePressedOnFolder`: invertuje výběr složky (`SelectPanelItem`), spočítá velikost přes `FastDirSize`, zapíše velikost do `CFileData`, překreslí panel (`RepaintChangedItems`) a posune kurzor na další položku.

---

## 6. Statické linkování a distribuce na jiné počítače

Pro maximální přenositelnost bez nutnosti instalovat MinGW/GCC runtimes:
- **Statické runtimes**: `Makefile.mingw` používá `-static -static-libgcc -static-libstdc++`, což eliminuje závislosti na `libwinpthread-1.dll`, `libgcc_s_seh-1.dll` i `libstdc++-6.dll`.
- **Statický libssh2**: Slinkován přímo do `sftp.spl` ze statického archivu `libssh2_static.a` (není potřeba žádná externí `libssh2.dll` ani `z.dll`).
- **Knihovna OpenSSL (`libcrypto-3-x64.dll`)**: `sftp.spl` importuje `libcrypto-3-x64.dll` standardním PE importem. V Open Salamandru se tato knihovna nachází přímo v kořenovém adresáři aplikace (`C:\Apps\samandarin\libcrypto-3-x64.dll`). Starý kód `LoadBundledLibssh2()`, který ji načítal explicitně s `LOAD_WITH_ALTERED_SEARCH_PATH`, byl odstraněn, protože vedl k duplicitnímu načtení druhého OpenSSL runtime a poškození heapu (`0xc0000374`).
- **Ověření závislostí**:
  ```powershell
  objdump -p sftp.spl | Select-String "DLL Name"
  ```
  Výstup smí obsahovat pouze standardní Windows systémové knihovny a `libcrypto-3-x64.dll`.
- **Statická analýza**:
  ```powershell
  cppcheck --enable=warning,performance,portability,style src/
  ```

---

## 7. Titulky tabů (Tabs), adresní řádek a mezipaměť prohlížeče (Cache Invalidation)

### Titulky tabů a adresní řádek:
1. **`GetPathForMainWindowTitle`**:
   - `mode == 1` (**Directory Name Only**): Salamander volá pro titulky záložek (tabů). Plugin vrací název profilu v hranatých závorkách následovaný mezerou a názvem podsložky (např. `[NAS] Season 29` nebo `[NAS] /`). Pokud profil nemá jméno, použije se hostname nebo čistý název složky.
   - `mode == 2` (**Shortened Path**): Salamander volá pro záhlaví okna. Plugin vrací zkrácenou cestu `[NAS] sftp://user@host[:port]/.../podsložka`.
2. **`GetNextDirectoryLineHotPath`**:
   - Musí správně přeskočit prefix `sftp://user@host[:port]/` jako jeden celek (kořen) a následně rozdělovat cestu podle lomítek `/` i `\\`. Nesmí přeskakovat fixní počet znaků, aby nedošlo k poškození uživatelského jména (např. oříznutí `root` na `oot`).

### Zneplatnění diskové mezipaměti prohlížeče (F3 View):
- `uniqueFileName` předávaný do `AllocFileNameInCache` **musí** obsahovat kompletní identifikaci serveru (`user@host:port`), vzdálenou cestu a zároveň velikost a čas modifikace souboru:
  ```cpp
  _snprintf_s(uniqueFileName + len, sizeof(uniqueFileName) - len, _TRUNCATE,
              ":%I64u:%08lx%08lx", file.Size.Value,
              file.LastWrite.dwHighDateTime, file.LastWrite.dwLowDateTime);
  ```
- Tím se garantuje okamžité stažení nové verze při změně souboru na serveru i při přepnutí mezi různými servery se stejnou strukturou cest.

---

## 8. Přenosové dialogy a podpora Dark Mode (Transfer Dialogs & Theming)

### Přenosový dialog (`CSftpTransferProgressDlg` / `IDD_TRANSFERDLG`):
1. **Dva progress bary**:
   - `IDP_TR_FILE_PROGRESS` – průběh přenosu aktuálního souboru (0 až 1000 promile).
   - `IDP_TR_TOTAL_PROGRESS` – celkový průběh celé operace (stahování/nahrávání více položek).
2. **Připojení k tématu Salamandera**:
   - V `WM_INITDIALOG` se volá `SalamanderGUI->AttachProgressBar(HWindow, IDP_TR_FILE_PROGRESS)` a `SalamanderGUI->AttachProgressBar(HWindow, IDP_TR_TOTAL_PROGRESS)`, což zajistí správné vykreslování v nativním stylu a barvách Salamandera.
3. **Plná podpora Dark Mode**:
   - Využívá sdílený modul `plugindarkmode.h` (`../salamander-plugins/salamand/plugins/shared/plugindarkmode.o`).
   - V `WM_INITDIALOG`: `PluginDarkMode_ApplyTitleBar(HWindow);` (obarví záhlaví okna do tmavého tématu přes DwmSetWindowAttribute).
   - V dialogové proceduře:
     - `PluginDarkMode_HandleThemeMessage(HWindow, uMsg, wParam, lParam);`
     - Při `WM_CTLCOLORDLG` a `WM_CTLCOLORSTATIC`:
       ```cpp
       LRESULT lr = 0;
       if (PluginDarkMode_HandleCtlColor(uMsg, wParam, lParam, &lr))
           return (INT_PTR)lr;
       ```
   - Stejný postup je aplikován i pro `CCalcSizeProgressDlg` a `CDeleteProgressDlg`.
4. **Prefix aktivní konexe `[NAS]` v cestách a titulku**:
   - Vzdálené cesty v polích `From:` a `To:` jsou formátovány pomocí `SftpFormatTransferPath`: detekuje se vzdálená cesta (`SftpIsPathRemote`) a automaticky se předřadí jméno aktivního profilu `[NAS] /cesta` s následným zkrácením přes `PathCompactPathExA` (prefix `[NAS]` zůstává vždy zachován).
   - Titulek okna v `WM_INITDIALOG` je rovněž obohacen o prefix `[Jméno_konexe]`, což uživateli umožňuje okamžitě vidět, ke kterému serveru operace náleží.

---

## 9. Správné ošetření storna operací a prevence vzniku 0-bajtových souborů

Při přenosu více souborů nebo rekurzivním stahování/nahrávání musí plugin korektně reagovat na stisk tlačítka **Storno / Cancel** v progress dialogu (`CSftpTransferProgressDlg`):

1. **Centralizovaná detekce storna (`SftpIsCancelled`)**:
   - Funkce kontroluje jak `g_OvrCancel` (storno z dialogu dotazu na přepis/resume), tak stav progress dialogu `g_ProgDlg->GetWantCancel()`.
   - Stav storna se ukládá do perzistentního příznaku `g_ProgCancel`, který přetrvá i po destrukci okna progress dialogu v `SftpProgressEnd()`.
   - Všechny hlavní smyčky (`CopyOrMoveFromFS`, `CopyOrMoveFromDiskToFS`, `SftpDownloadRecursive`, `SftpUploadRecursive`) testují `SftpIsCancelled()` na začátku každé iterace. Při stornu se smyčka ihned ukončí (`break`), aniž by se dotazovala uživatele na *"Continue with other items?"*.

2. **Ověření storna před otevřením / vytvořením souboru**:
   - V `Download`, `Upload`, `ScpDownload` i `ScpUpload` se `ReportProgress(path, 0, total)` volá **před** jakýmkoliv voláním `fopen_s` nebo `libssh2_sftp_open(..., CREAT | TRUNC)`.
   - Pokud uživatel zrušil operaci dříve, `ReportProgress` vrátí `false` a soubor se vůbec neotevře ani nezaloží prázdný na cílovém úložišti.

3. **Úklid nedokončených 0-bajtových souborů**:
   - Pokud došlo k přerušení transferu a nebylo přeneseno nic (`!ok && done == 0 && resumeOffset == 0`), soubor se okamžitě smaže:
     - Lokálně: `DeleteFileA(localPath)`
     - Vzdáleně: `libssh2_sftp_unlink(Sftp, remotePath)`
   - Pokud již byla přenesena část dat (`done > 0`), data zůstávají na disku/serveru pro možnost budoucího navázání (resume).

---

## 10. Per-instance konexe a asynchronní přenosy na pozadí (Phase B)

Od verze **v1.3.0** plugin přechází z globálního singletonu na plně izolované per-instance konexe a asynchronní přenosy na pozadí:

### 1. Izolace instancí (`CPluginFSInterface`):
- Každý otevřený FS panel vlastní nezávislé SSH/SFTP spojení `Conn` (`CSftpConnection`) a konfiguraci `Profile` (`CSftpProfile`).
- Všechny operace ve virtuálním FS panelu přistupují výhradně k `this->Conn` a `this->Profile`.
- Keepalive časovač (`SFTP_TIMER_KEEPALIVE`) a odpojování panelů fungují zcela nezávisle na ostatních panelech či tabech.
- Progress callbacky v `CSftpConnection` jsou instancovány (každé spojení má svůj callback a kontext).

### 2. Architektura worker threadu (`CSftpTransferWorker`):
- Přenosy souborů a celých složek (stahování i nahrávání) jsou vyčleněny do samostatného pracovního vlákna `CSftpTransferWorker`.
- **Dedikovaná SSH relace (`WorkerConn`)**: Worker thread otevírá vlastní SSH spojení se stejným profilem, takže hlavní panelové spojení `Conn` zůstává ihned volné pro plynulé procházení a práci v panelech Salamandera.
- **Fronta úloh (`TaskQueue`)**: Úlohy typu `CSftpTransferTask` jsou bezpečně řazeny pod zámkem `QueueLock` a probouzeny signálem `WakeEvent`.
- **Thread-safe stav (`CSftpTransferState`)**: Sdílený stav uchovává informace o přenesených bajtech, rychlosti v MB/s, ETA a celkovém počtu položek.
- **Životní cyklus a Reset (`Reset()` / `Stop()`)**: Po dokončení každé přenosové operace je vlákno ukončeno a `WM_APP_SFTP_WORKER_FINISHED` volá `Worker->Stop()`. Před zahájením nové operace se volá `Worker->Reset()`, což zabraňuje nežádoucí akumulaci čítačů a počtů položek z předchozích přenosů.

### 3. Nemodální dialog s tokom na pozadí:
- Dialog `CSftpTransferProgressDlg` běží nemodálně a neblokuje hlavní okno správce souborů.
- **Tlačítko „Na pozadí" (`IDB_BACKGROUND`)**: Uživatel může dialog kdykoli minimalizovat či skrýt (`SW_HIDE`), přičemž přenos pokračuje plnou rychlostí v pozadí.
- **Znovuzobrazení dialogu**: V menu pluginu **Moduly ➔ SFTP ➔ Zobrazit přenosy...** (`MENUCMD_SHOWTRANSFERS`) lze dialog kdykoliv přenést zpět do popředí. Příkaz je přístupný ze všech panelů (včetně lokálních disků) a prohledává všechny aktivní i odpojené FS relace. Výchozí klávesová zkratka byla ponechána volná (hotkey `0`), aby nekolidovala s klávesovou zkratkou Salamandera pro správu tabů (`Ctrl+Shift+T`). Uživatel si ji může volitelně nastavit v konfiguraci Salamandera.
- **Celkový postup podle objemu dat (MB/kB)**: Celkový progress bar se počítá na základě poměru přenesených bajtů vůči celkovému očekávanému objemu dat (`TotalDoneBytes / TotalExpectedBytes`), nikoli pouhým počtem souborů.
- **Notifikace změn**: Po dokončení všech úloh ve frontě worker dialog automaticky zavolá `SalamanderGeneral->PostChangeOnPathNotification` pro cíl i zdroj (u operací přesunutí / Move).


### 4. Null-safety při volání virtuálního FS (`ChangePath`, `IsCurrentPath`, `IsOurPath`):
- Salamander při volání `ChangePanelPathToPluginFS` s prázdnou cestou (např. po stisku Login v Connect dialogu) předává do `ChangePath` parametr `userPart = NULL`.
- Všechny metody FS musí striktně ověřovat `if (userPart == NULL) userPart = "";` před jakoukoli dereferencí řetězce nebo předáním do stringových a path-helper funkcí (`SftpStripHost`, `SftpIsSamePath`, `SftpIsRoot`, `SftpJoin`, `SftpParent`).
- `SftpStripHost` při `NULL` vždy vrací prázdný řetězec `""` a nikdy `NULL`.

### 5. Správa životního cyklu nemodálních dialogů a Observer pattern (`ISftpTransferDlgObserver`):
- Nemodální dialogy jako `CSftpTransferProgressDlg` využívají Windows časovač (`WM_TIMER` 101, 100 ms) pro periodický dotaz na snapshot stavu workeru (`GetStateSnapshot`).
- Pokud uživatel zavře nebo opustí SFTP panel, instance `CPluginFSInterface` je zničena včetně členské proměnné `CSftpTransferWorker TransferWorker`, která v destruktoru volá `DeleteCriticalSection`.
- **Riziko pádu (Use-After-Free / `0x24` Access Violation)**: Pokud by dialog přežil zničení FS/workeru, při dalším ticku timeru by zavolal `EnterCriticalSection` na smazanou kritickou sekci, což v `ntdll.dll!RtlEnterCriticalSection` způsobí `access violation write on 0x0000000000000024` (dereference vynulovaného `DebugInfo`).
- **Architektonické řešení**:
  1. Zavedeno rozhraní `ISftpTransferDlgObserver` s čistě virtuální metodou `virtual void DetachWorker() = 0;`.
  2. Dialog toto rozhraní implementuje a v `DetachWorker()` okamžitě zabíjí časovač (`KillTimer(HWindow, 101)`) a nuluje ukazatel `Worker = NULL`.
  3. `CSftpTransferWorker` nese příznak `Initialized` a v metodách `Stop()` a v destruktoru ihned notifikuje pozorovatele `AttachedObserver->DetachWorker()` a nuluje `DlgHwnd`. Metoda `GetStateSnapshot` bezpečně ověřuje `if (!Initialized) return;` ještě před dotykem kritické sekce.
  4. Destruktor `CPluginFSInterface::~CPluginFSInterface()` bezpečně odpojí a zničí okno dialogu `ActiveTransferDlg`, aby nezůstávaly viset žádné osiřelé časovače ani okna.

### 6. Opuštění panelu, dotaz na odpojení (`TryCloseOrDetach`) a Detached FS:
- Podle vzoru oficiálního FTP pluginu Salamandera implementuje virtuální FS metodu `TryCloseOrDetach`:
  ```cpp
  virtual BOOL WINAPI TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason);
  ```
- **Obsluha `FSTRYCLOSE_CHANGEPATH`**:
  - Pokud uživatel opouští panel (změna disku, navigace pryč), dialog `SalMessageBoxEx` nabídne:
    - **Odpojit** (`DIALOG_YES`): `detach = FALSE; return TRUE;` -> spojení se korektně uzavře a FS se zruší.
    - **Ponechat** (`DIALOG_NO`): `detach = TRUE; return TRUE;` -> spojení přejde do režimu Detached FS. Zůstává živé v paměti Salamandera a uživatel se k němu může kdykoliv vrátit z nabídky Změna disku (`Alt+F1`/`Alt+F2`) nebo nástrojové lišty disků.
    - **Storno** (`IDCANCEL`): `return FALSE;` -> změna cesty je zrušena a panel zůstává v SFTP.
- **Pojmenování odpojených relací v `GetChangeDriveOrDisconnectItem`**:
  - Do nabídky Změna disku se vkládá řetězec `\tSFTP:[Název_Profilu] /vzdálena/cesta\t` se jménem aktivního profilu, což uživateli umožňuje snadnou orientaci mezi více odpojenými servery.
- **Perzistence volby v registru (`LeavePanelAction`)**:
  - `0` = Ptát se (výchozí chování).
  - `1` = Vždy odpojit.
  - `2` = Vždy ponechat na pozadí (Detached FS).

---

## 11. Tmavý režim (Dark Mode) pro dialogy pluginu

Salamander od verze 5.0 integruje celoaplikační tmavý režim řízený přes konfigurační parametr `SALCFG_USEWINDOWSDARKMODE`. Pro správné zobrazení dialogů pluginu v tmavém i světlém režimu platí následující pravidla:

1. **Inicializace stavu hostitele (`SftpInitDarkMode`)**:
   - Helper `plugindarkmode.cpp` vyžaduje explicitní informaci o politice hostitele:
     ```cpp
     DWORD useDark = 0;
     if (general->GetConfigParameter(SALCFG_USEWINDOWSDARKMODE, &useDark, sizeof(useDark), NULL))
     {
         PluginDarkMode_SetHostPolicyAvailable(TRUE, useDark ? TRUE : FALSE);
         if (useDark)
             PluginDarkMode_SetHostResolvedColors(RGB(220, 220, 220), RGB(32, 32, 32), RGB(220, 220, 220));
         else
             PluginDarkMode_SetHostResolvedColors(CLR_INVALID, CLR_INVALID, CLR_INVALID);
     }
     ```
   - **Důležité**: Pro dialogy nelze použít barvy `SALCOL_ITEM_FG_NORMAL` a `SALCOL_ITEM_BK_NORMAL`, protože ty reprezentují barvy položek panelu souborů (které mohou mít světlé pozadí např. `RGB(240, 240, 240)`), což by způsobilo bílé rámečky pod statickými texty (`WM_CTLCOLORSTATIC`). Volá se proto `PluginDarkMode_SetHostResolvedColors` s nativními dialogovými tmavými barvami `RGB(32, 32, 32)` a textem `RGB(220, 220, 220)`.
2. **Aplikace na dialogové okno (`SftpApplyDarkModeToWindow`)**:
   - Při `WM_INITDIALOG` a při změně motivu (`WM_THEMECHANGED`, `WM_SETTINGCHANGE`) se volá pomocná funkce:
     - `PluginDarkMode_ApplyTitleBar(hwnd)` – zajistí tmavý titulek okna Windows (DWM).
     - `PluginDarkMode_ApplyListTreeThemeRecursive(hwnd)` – nastaví tmavý režim pro podřízené ovládací prvky.
     - Pro tlačítka typu `BS_PUSHBUTTON` / `BS_DEFPUSHBUTTON` je v tmavém režimu nutné explicitně zavolat `SetWindowTheme(child, L"DarkMode_Explorer", NULL);`, aby tlačítka převzala moderní tmavý vizuální styl systému Windows namísto bílého rámečku.
3. **Obsluha barev v dialogové proceduře & oprava `SS_TYPEMASK`**:
   - Ve zprávách `WM_CTLCOLORDLG`, `WM_CTLCOLORSTATIC` a `WM_CTLCOLORBTN` se předává řízení do `PluginDarkMode_HandleCtlColor(uMsg, (HDC)wParam, (HWND)lParam)`.
   - **Kritický gotcha v `plugindarkmode.cpp`**: V původním kódu byla podmínka `if ((style & (SS_ICON | ...)) != 0) return FALSE;`. V rozhraní Win32 však typ statického prvku není bitmaska, nýbrž enum v rozsahu `style & SS_TYPEMASK`. Protože hodnota masky byla `0x0F`, prvky `SS_RIGHT` (2) i `SS_LEFTNOWORDWRAP` (12) vracely `FALSE` a nebyly dark módem obslouženy. Kód byl zafixován na `LONG_PTR type = style & SS_TYPEMASK; if (type == SS_ICON || ...) return FALSE;`.
   - Současně byla do `DialogProc` dialogů (`CSftpTransferProgressDlg`, `CDeleteProgressDlg`, `CCalcSizeProgressDlg`) doplněna pojistka: pokud by `PluginDarkMode_HandleCtlColor` vrátil `FALSE`, ale `PluginDarkMode_ShouldUseDark()` je aktivní, dialog přímo nastaví `SetBkMode(hdc, TRANSPARENT)`, `SetTextColor(hdc, RGB(220, 220, 220))`, `SetBkColor(hdc, RGB(32, 32, 32))` a vrátí tmavý štětec `s_darkBgBrush`.

---

## 12. Přenosy mezi servery (Server-to-Server Copy)

Při operacích přenosu v `CPluginFSInterface::CopyOrMoveFromFS` dochází k vyhodnocení cílové cesty:
1. **Lokální disk**: Cíl začíná písmenem jednotky (`C:\...`) nebo `\\UNC\`. Spouští se asynchronní download z SFTP na lokální disk.
2. **Stejný SFTP server**: Cíl je virtuální cesta začínající `sftp:` a cíl ukazuje na tentýž hostitel a port jako aktivní panel (`Profile.Host`). Provádí se server-side kopírování/přesun v rámci jednoho spojení.
3. **Odlišný SFTP server (Server-to-Server)**:
   - Cíl má tvar `sftp://user@remotehost:port/path` nebo `//user@remotehost:port/path` s odlišným serverem než `this->Profile.Host`.
   - **Vyhledání cílového připojení**:
     - Plugin prohledá aktivní FS panely přes `InterfaceForFS.GetActiveFSList()`. Pokud nalezne odpovídající instanci se stejným hostitelem a uživatelem, převezme její aktivní profil i otevřené spojení `pTargetConn`.
     - Pokud aktivní panel neexistuje, prohledá uložené profily `SftpProfiles`.
     - V případě potřeby naváže spojení k cíli přes `SftpEnsureConnected(parent, localTargetConn, targetProfile)`.
   - **Přenosový dialog a identifikace profilů**:
     - Do dialogu `CSftpTransferProgressDlg` se předávají názvy zdrojového i cílového profilu:
       - `From: [Zdrojový_Profil] /cesta/zdroj`
       - `To: [Cílový_Profil] /cesta/cil`
       - Titulek okna přenosu: `[[Zdroj] -> [Cíl]] SFTP Transfer`.
   - **Streamovaný přenos**:
     - Pro každou označenou položku se soubor stáhne ze zdroje (`this->Conn`) do bezpečného dočasného umístění v lokálním `%TEMP%` a ihned se nahraje na cílový server (`*pTargetConn`).
     - Dočasný soubor je smazán.
     - V případě přesunu (`!copy`) se zdrojový soubor po úspěšném nahrání na cíl smaže ze zdroje.
   - **Dvoustranná notifikace změn**:
     - Po dokončení se vyvolá `SalamanderGeneral->PostChangeOnPathNotification` pro zdrojovou i cílovou cestu, což zajistí okamžitou aktualizaci obou otevřených panelů.

---

## 13. Nezávislé instance workerů pro souběžné přenosy na pozadí & životní cyklus dialogů

Při souběžném spuštění více přenosů (stahování i nahrávání) v rámci téhož panelu nebo profilu nesmí docházet ke sdílení jediné instance `CSftpTransferWorker`:
1. **Per-transfer Worker alokace**:
   - V metodách `CopyOrMoveFromFS` a `CopyOrMoveFromDiskToFS` se pro každou operaci dynamicky vytvoří nový worker:
     ```cpp
     CSftpTransferWorker* worker = new CSftpTransferWorker();
     ```
   - Úlohy se zařadí do tohoto workeru a ten se předá dialogu s příznakem vlastnictví:
     ```cpp
     dlg->AttachWorker(worker, true);
     RegisterTransferDlg(dlg);
     worker->Start(Profile, dlg->HWindow);
     ```
2. **Vlastnictví workeru dialogem (`OwnsWorker`)**:
   - `CSftpTransferProgressDlg` nese příznak `OwnsWorker`.
   - V metodě `DetachWorker()`, při destrukci okna (`WM_DESTROY`) i při dokončení přenosu (`WM_APP_SFTP_WORKER_FINISHED`):
     - Zruší se časovač (`KillTimer(HWindow, 101)`).
     - Workeru se vynuluje HWND i observer.
     - Pokud `OwnsWorker == true`, zavolá se `w->Stop()` a `delete w;`.
     - Ukazatele se vynulují před uvolněním pro zamezení rekurzivního volání / double free.
3. **Evidence dialogů (`ActiveTransferDlgs`) v FS rozhraní**:
   - `CPluginFSInterface` udržuje `std::vector<CSftpTransferProgressDlg*> ActiveTransferDlgs`.
   - Dialog se při spuštění registruje přes `FS->RegisterTransferDlg(dlg)` a při zániku odregistruje přes `FS->UnregisterTransferDlg(dlg)`.
   - Metoda `ShowTransferDialog(HWND parent)` a příkaz `MENUCMD_SHOWTRANSFERS` iterují přes všechny aktivní dialogy a obnovují všechna minimalizovaná/skrytá okna.
4. **Korektní teardown při zavření Salamandera (`~CPluginFSInterface`)**:
   - Při ukončení Salamandera (`WM_USER_CLOSE_MAINWND`) probíhá destrukce všech instancí `CPluginFSInterface`.
   - Destruktor prochází `ActiveTransferDlgs`, odpojuje worker dialogy (`dlg->SetFS(NULL)`, `dlg->DetachWorker()`) a bezpečně ničí okna (`DestroyWindow`), čímž je zamezeno pádům na neplatné ukazatele po uvolnění pluginu.
