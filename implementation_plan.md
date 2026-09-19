# Implementace dotazu na odpojení při opuštění SFTP panelu (TryCloseOrDetach) & opravy přenosů [HOTOVO]

Tento plán navrhl a realizoval implementaci chování při opuštění SFTP panelu (změna disku, navigace pryč z virtuálního FS) podle osvědčeného vzoru oficiálního FTP pluginu v Open Salamandru, a následné opravy životního cyklu workeru a výpočtu progress baru.

## Stav realizace
- **Stav**: Dokončeno, otestováno a nasazeno do `C:\Apps\samandarin\plugins\sftp\`.
- **Výsledek**:
  1. Uživatel dostane na výběr mezi odpojením (**Odpojit**), ponecháním spojení na pozadí (**Ponechat**) nebo zrušením změny disku (**Storno**). Spojení na pozadí (Detached FS) zůstává živé a je dostupné z nabídky Změna disku (`Alt+F1`/`Alt+F2`) s prefixem profilu `[NAS]`.
  2. Opraveno nahrávání jazykových modulů z podsložky `plugins\sftp\lang\` a odstraněno hlášení „Error loading string".
  3. Implementován reset čítačů a fronty workeru (`CSftpTransferWorker::Reset()`), aby se hodnoty nepřenášely do dalšího kopírování.
  4. Celkový progress bar nyní přesně odráží objem přenesených dat (MB/kB) namísto pouhého počtu souborů.
  5. Okno přenosu na pozadí lze kdykoliv vyvolat z menu Moduly -> SFTP -> Zobrazit přenosy... z libovolného panelu (výchozí zkratka byla uvolněna, aby nekolidovala se zkratkou Salamandera pro taby `Ctrl+Shift+T`).



## Navržené změny

---

### Jazykové zdroje (Resources)

#### [MODIFY] [lang.rh](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang.rh)
- Přidat definice ID textů:
  - `IDS_CLOSECONINPANEL` – text dotazu při opuštění panelu
  - `IDS_DISCONNECTBUTTON` – popisek tlačítka „Odpojit" / „Disconnect"
  - `IDS_KEEPCONBUTTON` – popisek tlačítka „Ponechat" / „Keep"
  - `IDS_ALWAYSREMEMBER` – popisek checkboxu „Zapamatovat tuto volbu"
  - `IDS_WANTDISCONNECT` – text dotazu při nemožnosti detachovat („Opravdu se chcete odpojit?")

#### [MODIFY] [lang_en.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_en.rc)
- Přidat anglické texty:
  - `IDS_CLOSECONINPANEL`: `"You are leaving SFTP server in panel. Do you wish to disconnect or to keep connection to this server?\n\nIf you choose Keep, you can access this connection from the Change Drive menu later."`
  - `IDS_DISCONNECTBUTTON`: `"&Disconnect"`
  - `IDS_KEEPCONBUTTON`: `"&Keep"`
  - `IDS_ALWAYSREMEMBER`: `"&Remember this choice and do not ask again"`
  - `IDS_WANTDISCONNECT`: `"Do you want to disconnect from SFTP server?"`

#### [MODIFY] [lang_cs.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_cs.rc)
- Přidat české texty:
  - `IDS_CLOSECONINPANEL`: `"Opouštíte SFTP server v panelu. Přejete si odpojit spojení nebo jej ponechat aktivní?\n\nZvolíte-li Ponechat, spojení zůstane na pozadí a můžete se k němu kdykoliv vrátit z nabídky Změna disku (Alt+F1/Alt+F2)."`
  - `IDS_DISCONNECTBUTTON`: `"&Odpojit"`
  - `IDS_KEEPCONBUTTON`: `"&Ponechat"`
  - `IDS_ALWAYSREMEMBER`: `"&Zapamatovat tuto volbu a příště se již neptat"`
  - `IDS_WANTDISCONNECT`: `"Opravdu se chcete odpojit od SFTP serveru?"`

---

### Konfigurace a perzistence

#### [MODIFY] [sftpglue.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpglue.h) / [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h)
- Přidat globální proměnnou konfigurace:
  - `extern int SftpLeavePanelAction;`
    - `0` = Ptát se (Ask - výchozí hodnota)
    - `1` = Vždy odpojit (Always Disconnect)
    - `2` = Vždy ponechat odpojené na pozadí (Always Keep / Detach)

#### [MODIFY] [sftp.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.cpp)
- Načítání v `LoadConfiguration`:
  - `registry->GetValue(regKey, "LeavePanelAction", REG_DWORD, &SftpLeavePanelAction, sizeof(DWORD));`
- Ukládání v `SaveConfiguration`:
  - `registry->SetValue(regKey, "LeavePanelAction", REG_DWORD, &SftpLeavePanelAction, sizeof(DWORD));`

---

### Obsluha opuštění panelu a nabídka Změna disku

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- Přepracovat metodu `CPluginFSInterface::TryCloseOrDetach`:
  1. Kontrola `forceClose`, `CalledFromDisconnectDialog`, `FSTRYCLOSE_UNLOADCLOSEFS`, `FSTRYCLOSE_UNLOADCLOSEDETACHEDFS`, `FSTRYCLOSE_PLUGINCLOSEDETACHEDFS` a `SalamanderGeneral->IsCriticalShutdown()` -> bez ptaní `detach = FALSE; return TRUE;`.
  2. Pokud `reason == FSTRYCLOSE_CHANGEPATH`:
     - Pokud `SftpLeavePanelAction == 1` (Vždy odpojit): `detach = FALSE; return TRUE;`.
     - Pokud `SftpLeavePanelAction == 2 && canDetach` (Vždy ponechat): `detach = TRUE; return TRUE;`.
     - Pokud `SftpLeavePanelAction == 0` (Ptát se):
       - Je-li `canDetach == TRUE`:
         Zobrazit `SalamanderGeneral->SalMessageBoxEx` s volbami **Odpojit** (`DIALOG_YES`), **Ponechat** (`DIALOG_NO`), **Storno** (`IDCANCEL`) a volitelným zapamatováním volby (`rememberChoice`).
         - Při **Odpojit**: `detach = FALSE; ret = TRUE;`
         - Při **Ponechat**: `detach = TRUE; ret = TRUE;` (spojení přejde do režimu Detached FS v paměti Salamandera)
         - Při **Storno**: `return FALSE;` (změna cesty se zruší a uživatel zůstane v SFTP panelu)
       - Není-li `canDetach == TRUE`:
         Zobrazit dotaz `SalMessageBox` (Ano/Ne). Pokud Ne, `return FALSE`.
- Vylepšit metodu `CPluginFSInterface::GetChangeDriveOrDisconnectItem`:
  - Zahrnout název profilu `[NAS]` do textu položky pro nabídku Změna disku (Alt+F1/Alt+F2), např. `\tSFTP:[NAS] /cesta\t`, aby byla odpojená spojení na první pohled přehledná a identifikovatelná.

---

## Verifikační plán

### Automatizované testy
- Kompilace pluginu přes `mingw32-make -f Makefile.mingw CROSS_COMPILE=`.
- Spuštění stávajících unit testů:
  - `.\test\test_worker.exe`
  - `.\test\test_path_hottrack.exe`

### Manuální ověření v Open Salamandru
1. Otevřít SFTP panel a připojit se k serveru.
2. V panelu zvolit změnu disku (např. stisk `Alt+F1` a výběr `C:`).
3. Ověřit, že se zobrazí dialog s dotazem a třemi tlačítky:
   - Kliknutí na **Storno** -> uživatel zůstane v SFTP panelu.
   - Kliknutí na **Ponechat** -> panel se přepne na `C:`, ale spojení zůstane aktivní. V menu `Alt+F1` se objeví položka odpojeného SFTP serveru. Po kliknutí na ni se panel okamžitě vrátí do vzdálené složky bez nutnosti znovu zadávat heslo.
   - Kliknutí na **Odpojit** -> spojení se korektně ukončí a odpojí.
4. Ověřit fungování checkboxu pro zapamatování volby.
