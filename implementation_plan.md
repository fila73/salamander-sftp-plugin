# Implementační plán: Plnohodnotná podpora dialogů a operací po vzoru FTP pluginu

Na základě analýzy zdrojových kódů FTP pluginu v `samandarin/src/plugins/ftp` (zejména `IDD_OPERATIONDLG`, `IDD_SOLVEITEMERRSIMPLEEX`, `dialogs5.cpp`, `dialogs6.cpp` a `dialogs7.cpp`) a screenshotů uživatele navrhujeme kompletní architekturu odpovídající standardu Open Salamander:

---

## 1. Dialog "Cílový soubor již existuje" (`IDD_SFTP_CONFLICTDLG` / `IDD_SOLVEITEMERRSIMPLEEX`)

Místo tichého automatického navázání nebo přepsání se při detekci existujícího souboru zobrazí dialog pro vyřešení konfliktu:
- **Zobrazení cest a názvů**:
  - Zdrojová cesta a název (read-only)
  - Cílová cesta (read-only) a Cílový název (editovatelný editbox)
- **Tlačítko "Opakovat" se split/dropdown šipkou** (`SalamanderGUI->AttachButton(HWindow, IDOK, BTF_DROPDOWN)`):
  - Při kliknutí na šipku nebo stisku rozbalovacího tlačítka se otevře kontextové menu:
    1. **Opakovat** (`IDOK`): zkusí operaci znova od začátku.
    2. **Pokračovat** (`CM_SIED_RESUME`): naváže na existující soubor od jeho velikosti (`resumeOffset = existingSize`).
    3. **Pokračovat nebo přepsat** (`CM_SIED_RESUMEOROVR`): pokud je cílový soubor menší, naváže; pokud je stejný nebo větší, přepíše.
    4. **Použít alternativní název** (`CM_SCRD_USEALTNAME`): automaticky vygeneruje např. `nazev (1).ext` do editboxu cílového názvu.
- **Tlačítka dialogu**:
  - **Přepsat** (`CM_SIED_OVERWRITE`) – výchozí fokus
  - **Přepsat vše** (`CM_SIED_OVERWRITEALL`) – zapamatuje přepsání pro všechny další konflikty
  - **Přeskočit** (`IDB_SCRD_SKIP`) – přeskočí aktuální soubor a pokračuje dalším
  - **Storno** (`IDCANCEL`) – ukončí celou operaci
  - **Nápověda** (`IDHELP`)
- **Checkbox "Pamatovat si volbu..."** (`IDC_SCRD_APPLYTOALL`):
  - Zapamatuje zvolenou akci (např. Resume All, Overwrite All, Skip All) pro všechny další konflikty v rámci této přenosové operace.

---

## 2. Dialog průběhu přenosu (`IDD_TRANSFERDLG` / `IDD_OPERATIONDLG`)

Dialog bude podporovat **dva plnohodnotné režimy**:

### A. Kompaktní režim (Simple look – výchozí)
- **Hlavička s informacemi o přenosu**:
  - `Zdrojová: <cesta nebo [profil] cesta>`
  - `Cílová cesta: <cesta nebo [profil] cesta>`
  - `Zbývající čas: <odhadovaný čas, (neznámý), (čekám na uživatele) nebo (hotovo)>`
  - `Uplynulý čas: <X sek / X min Y sek>`
  - `Stav: <X B z Y MB, případně N chyba / chyb>`
  - **Progress bar** se stavem v procentech
  - Titulek okna: `(0 %) Kopírování Soubor "..." do ...`, `(chyby) ...`, `(hotovo) ...`
- **Volba uzavření okna**:
  - Checkbox `[x] Po skončení operace zavřít toto okno` (s perzistencí v konfiguraci pluginu).
  - Pokud není zaškrtnuto nebo došlo k chybě: okno se nezavře, tlačítko "Storno" se změní na "Zavřít" (`BS_DEFPUSHBUTTON`), tlačítko "Pauza" i "Na pozadí" se zakáží a zobrazí se souhrnný stav `(hotovo)`.
- **Ovládací tlačítka v kompaktním režimu**:
  - `Detaily >>`: zvětší okno směrem dolů a odhalí detailní sekce Spojení a Operace.
  - `Chyby >>`: **aktivní pouze při výskytu chyby nebo čekání na uživatele**. Po stisku automaticky přepne do detailního režimu, zaškrtne `[x] Zobrazit jen chyby` a nastaví fokus a kurzor na chybující položku.
  - `Pauza` / `Pokračovat`: pozastaví a znovu spustí přenos.
  - `Storno` / `Zavřít`: storno operace nebo zavření dokončeného okna.
  - `Nápověda`.

### B. Detailní režim (Detailed look – po stisku "Detaily >>")
Dialog se zvětší směrem dolů a pod dělící linkou (`IDC_DLGSPLITBAR`) odhalí:
1. **Sekce Spojení (Connections)**:
   - ListView (`IDL_CONNECTIONS`): sloupce `ID`, `Akce`, `Stav` (např. `ID 1`, Akce: `Kopíruji soubor.dat`, Stav: `Zpracovávám` / `Nečinný` / `Pauza`).
   - Tlačítka pod spojením: `Vyřešit chybu`, `Pauza` / `Zastavit`.
2. **Sekce Operace per soubor (Operations)**:
   - Nadpis: `Operace: (hotovo / celkem)` (např. `Operace: (0 / 1)` nebo `Operace: (3 / 3)`).
   - ListView (`IDL_OPERATIONS`):
     - Sloupec `Popis`: systémová ikona typu souboru + text operace (např. `Kopírovat soubor.ext (50 MB) z ... do ...`).
     - Sloupec `Stav`: aktuální stav položky (`Čeká`, `Zpracovávám`, `Dokončeno`, `Zeptat se uživatele: Cílový soubor již existuje`, `Chyba`, `Přeskočeno`).
   - Spodní lišta pod Operacemi:
     - Checkbox `[ ] Zobrazit jen chyby` (filtruje pouze problematické položky).
     - Tlačítka vpravo: `Vyřešit chybu` (otevře dialog pro řešení konfliktu daného souboru), `Opakovat`, `Přeskočit`.

---

## 3. Klávesnicová navigace (Tab, Shift+Tab, šipky, Enter, Esc)

- V obou dialozích nastavení `WS_TABSTOP` a `WS_GROUP` tak, aby:
  - Klávesa `Tab` a `Shift+Tab` plynule cyklovala mezi všemi tlačítky, seznamy a checkboxy.
  - Šipky `Left` / `Right` umožňovaly přechod mezi sousedními tlačítky ve skupině.
  - Šipky `Up` / `Down` umožňovaly procházení položek v ListView spojení a operací.
- V `sftp.cpp` v hooku `GetMsgHookProc` (`WH_GETMESSAGE`) volat `IsDialogMessage` pro aktivní dialog přenosu nebo dialog řešení konfliktu, čímž se zajistí plná funkčnost standardního dialogového manažeru Windows i v modeless režimu.

---

## Stav realizace změn v souborech

### 1. Resource šablony a lokalizace [HOTOVO – Commit 70b84a5]
- [MODIFY] [lang.rh](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang.rh): Přidána ID dialogů, ovládacích prvků a menu příkazů (`IDD_CONFLICTDLG`, `IDM_FILEEXISTSERRRETRY`, `IDL_CONNECTIONS`, `IDL_OPERATIONS`, `IDB_SHOWDETAILS`, `IDB_SHOWERRORS`, `IDB_PAUSERESUME`, `IDC_TR_CLOSE_ON_FINISH`, atd.).
- [MODIFY] [lang_cs.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_cs.rc), [lang_en.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_en.rc), [lang_cs.rc2](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_cs.rc2), [lang.rc2](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang.rc2):
  - Definice `IDD_TRANSFERDLG` v plné velikosti se všemi ovládacími prvky (kompaktní část nahoře, dělící linka, `IDL_CONNECTIONS`, `IDL_OPERATIONS`, tlačítka).
  - Definice dialogu `IDD_CONFLICTDLG` ("Cílový soubor již existuje" / "Target file already exists").
  - Definice popup menu `IDM_FILEEXISTSERRRETRY` (Retry, Resume, Resume or Overwrite, Use Alternate Name).
  - Přidány lokalizované textové řetězce.

### 2. Dialog řešení konfliktu (`CSftpConflictDlg`) [HOTOVO – Commit 6f21678]
- [NEW] [sftpconflictdlg.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconflictdlg.h) a [sftpconflictdlg.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconflictdlg.cpp):
  - Třída `CSftpConflictDlg` dědící z `CCommonDialog`.
  - Propojení dropdown tlačítka `IDOK` s `SalamanderGUI->AttachButton(..., BTF_DROPDOWN)`.
  - Obsluha `WM_USER_BUTTONDROPDOWN` s trackováním menu přes `SalamanderGUI->CreateMenuPopup()`.
  - Reakce na volby Overwrite, Overwrite All, Resume, Resume or Overwrite, AutoRename, Skip, Cancel.
  - Generování alternativního názvu `SftpGenerateAltName`.

### 3. Zapojení řešení konfliktu do rekurzivních přenosů [HOTOVO – Commit 5b13dc4]
- [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp):
  - Zavedena pomocná funkce `AskOverwriteWorker`, která při existenci cílového souboru zobrazí `CSftpConflictDlg` (pokud nebyla akce trvale zapamatována pro všechny soubory).
  - Zapojeno do `DoDownloadRecursive` a `DoUploadRecursive` s podporou navázání (`resumeOffset = existingSize`) i přejmenování na alternativní název.

### 4. Worker s podporou Pause/Resume, TaskStatus a snapshotem úloh [HOTOVO – Commit f367a4e]
- [MODIFY] [sftpworker.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.h) a [sftpworker.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.cpp):
  - Zaveden `TaskStatus` (`StatusWaiting`, `StatusRunning`, `StatusDone`, `StatusError`, `StatusSkipped`).
  - Podpora Pause/Resume přes `RunEvent`, metody `SetPaused(bool)` a `IsPaused()`.
  - Thread-safe metoda `GetTasksSnapshot(std::vector<CSftpTransferTask>& out)` pro krmení ListView operací.

### 5. Modeless dialog keyboard navigation hook & perzistence [HOTOVO – Commit e1a70dc]
- [MODIFY] [sftp.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.cpp):
  - Hook zpráv `GetMsgHookProc` (`WH_GETMESSAGE`) monitoruje `g_TransferDlgHwnds` a volá `IsDialogMessage`, čímž umožňuje plynulou navigaci klávesami `Tab`, `Shift+Tab` i šipkami.
  - Ukládání a načítání konfigurace `CloseTransferDlgOnFinish` v `LoadConfiguration` a `SaveConfiguration`.

### 6. Přenosový dialog se 2 módy, Detaily, Chyby, Pauza/Pokračovat [HOTOVO – Commit 414d788]
- [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h) a [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp):
  - Metody `ToggleSimpleLook()` a `ShowControlsAndChangeSize(BOOL simple)` pro dynamické zvětšování/zmenšování okna.
  - `InitListViews()` a `RefreshListViews()` pro seznam Spojení (`IDL_CONNECTIONS`) a Operace (`IDL_OPERATIONS`) s malými systémovými ikonami souborů.
  - Zpracování `WM_NOTIFY` s `LVN_GETDISPINFOA` a `LVN_GETDISPINFOW`.
  - Tlačítko `Chyby >>` (`ShowNextError()`) s aktivací pouze při chybě.
  - Tlačítko `Pauza` / `Pokračovat` napojené na `Worker->SetPaused()`.
  - Ošetření checkboxu `[x] Po skončení operace zavřít toto okno` při `WM_APP_SFTP_WORKER_FINISHED`. Pokud není zaškrtnuto, dialog zůstane otevřený, tlačítko Storno se změní na Zavřít s fokusem, tlačítka Pauza a Na pozadí se zakáží a stav ukáže `(hotovo)`.
  - Aktualizace unit testů `test/test_worker.cpp`.

### 7. Oprava zjištěných chyb a stabilizace UI [HOTOVO – Hotovo]
- [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp):
  - **Odstranění pádu na Stack Overflow**: V `case LVN_GETDISPINFOW` odstraněno rekurzivní volání `SendMessage` a texty i ikony se plní přímo na místě.
  - **Odstranění černých ploch po rozbalení detailů**: Nastaveny barvy a témata `DarkMode_Explorer` a `DarkMode_ItemsView` pro oba ListView; v `WM_CTLCOLORBTN` se již nevrací tmavý štětec pro standardní tlačítka s vizuálním stylem.
  - **Zamezení rušení menu a kurzoru na pozadí**: V `UpdateFromWorker` doplněna pojistka `if (IsBackground || !IsWindowVisible(HWindow)) return;`, takže skrytý dialog zbytečně neposílá aktualizace a nezhasíná menu Salamandera.
- [MODIFY] [sftp.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.cpp):
  - **Odblokování pohybu okna za záhlaví**: V `GetMsgHookProc` striktně omezeno `IsDialogMessage` pouze na klávesové zprávy (`WM_KEYFIRST` až `WM_KEYLAST`) a viditelná okna, což obnovilo možnost posouvat okno za lištu a klikat myší.
- [MODIFY] [sftpconflictdlg.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconflictdlg.cpp), [lang_cs.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_cs.rc), [lang_en.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_en.rc):
  - Editboxu v `IDD_CONFLICTDLG` nastaven Dark Mode přes `WM_CTLCOLOREDIT` a `DarkMode_CFD`.
  - Rozšířen popisek ze šířky 52 na 60, čímž byl vyřešen oříznutý text „Zdrojový ná".

### 8. Vlastní UI vlákno dialogu (`CSftpProgressDlgThread`), dynamický layout a bohaté informace po vzoru FTP [HOTOVO – Commit 85c5ba6]
- [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h) a [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp):
  - **Izolované UI vlákno**: Implementována třída `CSftpProgressDlgThread`, která spouští modeless přenosový dialog ve vlastním vlákně s vlastní smyčkou `GetMessage`. Hlavní UI vlákno Salamandera není nijak zatěžováno ani rušeno (rozbalené menu nezhasíná, kurzor neproblikává, i když je okno na pozadí či bez fokusu).
  - **Dynamický layout a resizing**: Přidána obsluha `WM_SIZE` a `WM_GETMINMAXINFO`. Metoda `LayoutDialog()` pomocí `BeginDeferWindowPos` dynamicky roztahuje horní část i oba seznamy, které si proporcionálně dělí volnou vertikální výšku (35 % pro Spojení, 65 % pro Operace). Tlačítka jsou ukotvena vpravo dole a metoda `SetColumnWidths()` automaticky přizpůsobuje šířky sloupců šířce okna.
  - **Plné informace o přenosech (dle vzoru FTP)**:
    - *Spojení*: Akce formátuje `Kopíruji <soubor>`, Stav formátuje `X MB z Y MB (za Z MB/s), P %, zbývající čas: ETA`, přidáno tlačítko `Zastavit` (`IDB_OPCONSSTOP`).
    - *Operace*: Nadpis nese `Operace: (hotovo / celkem)`, Popis formátuje detailní text `Kopírovat <soubor> (<velikost>) z <odkud> do <kam> / v režimu přenosu SFTP`, Stavy ukazují `Čeká`, `Zpracovávám`, `Dokončeno`, `Chyba: <popis>`, `Přeskočeno`.
    - *Horní stav*: `Stav: X MB z Y MB, celková rychlost přenosu: Z MB/s`.
- [MODIFY] [sftp.rh2](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.rh2), [lang_cs.rc2](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_cs.rc2), [lang.rc2](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang.rc2), [lang_cs.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_cs.rc), [lang_en.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_en.rc):
  - Přidány identifikátory, lokalizované šablony textů a tlačítko `Zastavit` pod seznamem spojení.

---

## Verifikační plán

### Automatizované testy
- Kompilace celého projektu pomocí `mingw32-make CROSS_COMPILE= all`.
- Spuštění existujících unit testů (`test_worker.exe`, `test_path_hottrack.exe`, `test_multithread_handshake.exe`).

### Manuální ověření
1. **Ověření dialogu "Cílový soubor již existuje"**:
   - Spustit stahování souboru, který již v cíli existuje.
   - Ověřit, že se zobrazí dialog konfliktu se správnou cestou a názvem.
   - Otestovat rozkliknutí tlačítka `Opakovat` -> volby `Pokračovat` (naváže od existující velikosti), `Pokračovat nebo přepsat`, `Použít alternativní název`.
   - Otestovat tlačítka `Přepsat`, `Přepsat vše`, `Přeskočit`, `Storno`.
2. **Ověření 2 módů v Progress dialogu**:
   - Spustit přenos více souborů.
   - Výchozí je kompaktní zobrazení.
   - Kliknout na `Detaily >>` -> dialog se zvětší, zobrazí se seznam Spojení a seznam Operací se stavy per soubor.
   - Kliknout na `Detaily <<` -> dialog se zmenší zpět.
3. **Ověření tlačítka "Chyby >>"**:
   - Při běžícím bezchybném přenosu je tlačítko zakázané.
   - Při chybě (nebo dotazu) se povolí; po kliknutí se dialog rozbalí a vyfiltrují se chybující operace.
4. **Ověření klávesnice**:
   - Ověřit procházení prvků klávesou `Tab`, `Shift+Tab` a šipkami.
