# Implementační plán: Body 9 + 11 – Per-instance konexe a asynchronní přenosy na pozadí

## Přehled

Tento plán pokrývá dvě úzce provázané položky z [nice_to_have.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/nice_to_have.md):

- **Bod 9**: Souběžné vícenásobné konexe (Per-instance `CPluginFSInterface`)
- **Bod 11**: Plně asynchronní nemodální přenosový dialog s během na pozadí (Phase B)

Bod 11 **závisí** na bodu 9, proto je plán strukturován sekvenčně: nejprve bod 9 (kroky 1–6), poté bod 11 (kroky 7–12).

---

## Uživatelské přezkoumání

> [!IMPORTANT]
> **Zlomová změna**: Bod 9 zásadně mění architekturu pluginu z jednoho globálního `SftpConn` singletonu na per-instance konexe. Veškerý kód, který dnes přistupuje ke globální `SftpConn` a `SftpProfile`, bude muset přistupovat k instanční proměnné přes `CPluginFSInterface*`.

> [!WARNING]
> **Zpětná kompatibilita registru**: Uložené profily se nezmění (zůstávají globální pole `SftpProfiles[]`). Každá FS instance pouze **kopíruje** profil při otevření relace. Žádná migrace konfigurace není potřeba.

> [!CAUTION]
> **Stávající progress callback je statický** (`CSftpConnection::SetProgressCallback` je `static`). Pro bod 11 (paralelní přenosy) bude nutné ho instancovat, nebo přepsat na per-connection callback.

---

## Soulad s architekturou FTP pluginu v Salamandru (`..\samandarin\src\plugins\ftp`)

Provedli jsme hloubkovou inspekci zdrojových kódů oficiálního FTP pluginu (`src/plugins/ftp/`):

1. **Bod 9 (Per-instance konexe)**:
   - **V souladu**: V FTP pluginu (`ftp.h`, `fs1.cpp`, `fs2.cpp`) vlastní každá instance `CPluginFSInterface` svůj `ControlConnection` (`CControlConnectionSocket*`), `Host`, `Port`, `User`, `Path` atd.
   - Globální seznam instancí: FTP plugin používá `TIndirectArray<CPluginFSInterface> FTPConnections`, náš plugin již má `ActiveFSList` v `CPluginInterfaceForFS`.
   - Předávání konfigurace: `ConnectFTPServer` nastaví `Config.UseConnectionDataFromConfig = TRUE` a zavolá `ChangePanelPathToPluginFS`. V `ChangePath` se konfigurace zkopíruje do instančních proměnných. Náš plán (`ConnectData.UseConnectData`) kopíruje tento zavedený pattern 1:1.
   - Odpojení (`DisconnectFS`): Přetypuje `pluginFS` na `CPluginFSInterface*` a odpojí konkrétní instanci.

2. **Bod 11 (Asynchronní přenosy na pozadí)**:
   - **V souladu – API kontrakt**: V `CopyOrMoveFromFS` a `CopyOrMoveFromDiskToFS` naplní FTP plugin frontu úloh (`CFTPQueue`), spustí operaci a vrátí `cancelOrHandlePath = FALSE; return TRUE;`. Tím se řízení ihned vrátí Salamandru a panel zůstane plně interaktivní.
   - **Zpřesnění – Notifikace o změně cesty**: Po dokončení souborů/přenosu FTP plugin volá `SalamanderGeneral->PostChangeOnPathNotification(path, includingSubdirs | flags)`. Náš plán krok 11.6 toto výslovně integruje pro správný refresh panelů.
   - **Zlepšení proti FTP pluginu (Worker Connection)**: FTP plugin u první úlohy "půjčoval" existující soket z panelu (`GiveConnectionToWorker`), aby ušetřil přihlašování, a vracel jej přes `FTPCMD_RETURNCONNECTION`. U SFTP/libssh2 (šifrovaná stavová SSH relace) je tato výměna mezi vlákny extrémně nebezpečná. Náš přístup – **otevření samostatné dedikované SSH relace pro worker thread (`WorkerConn`)** – je pro SFTP nesrovnatelně čistší, bezpečnější a umožňuje uživateli procházet adresáře v panelu bez jakýchkoli prodlev či swapování.
   - **Architektonická inspirace pro Dialog Thread**: FTP plugin spouští dialog operace v samostatném vlákně `COperationDlgThread` s vlastním `GetMessage` loopem. Díky tomu dialog nezamrzá, ani když Salamander na hlavním UI vlákně provádí modální operaci (např. otevřený dialog hledání nebo konfigurace). V kroku 11.4 tuto možnost zapracováváme.

---

## Rozhodnutí k dřívějším otázkám

1. **Maximální počet současně otevřených konexí**: **Libovolný** (bez umělého omezení, limitováno pouze pamětí a limity OS/serveru).
2. **Přenosy mezi dvěma SFTP servery (server-to-server)**: **Odloženo do budoucna** (zaevidováno v [nice_to_have.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/nice_to_have.md) jako bod 12).
3. **Persistence FS instance přes restart Salamandera**: **Odloženo do budoucna** (zaevidováno v [nice_to_have.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/nice_to_have.md) jako bod 13).

---

## BOD 9: Per-instance konexe

### Krok 9.1 – Přesun `CSftpConnection` a `CSftpProfile` z globálních proměnných do `CPluginFSInterface`

**Cíl**: Každá FS instance vlastní svůj objekt `CSftpConnection` a svůj `CSftpProfile`.

#### [MODIFY] [sftpglue.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpglue.h)
- Odstranit `extern CSftpConnection SftpConn;` (řádek 7)
- Odstranit `extern CSftpProfile SftpProfile;` (řádek 26)
- Ponechat `CSftpProfile` struct definici a `SftpProfiles[]` / `SftpProfileCount` (ty zůstávají globální – jsou to **uložené** profily)

#### [MODIFY] [sftpglue.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpglue.cpp)
- Odstranit definice `CSftpConnection SftpConn;` a `CSftpProfile SftpProfile = {...};` (řádky 6–7)
- Funkci `SftpEnsureConnected(HWND parent)` přepsat na `SftpEnsureConnected(HWND parent, CSftpConnection& conn, CSftpProfile& profile)` – přijímá odkaz na instanční konexe

#### [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h)
- Do třídy `CPluginFSInterface` přidat členské proměnné:
  ```cpp
  CSftpConnection Conn;      // per-instance SSH/SFTP connection
  CSftpProfile    Profile;   // per-instance connection profile (copied from dialog)
  ```
- Přidat metodu `CSftpConnection& GetConn()` a `CSftpProfile& GetProfile()`

**Ověření**: Kompilace projde (zatím se nic nevolá přes nové členy, staré globální proměnné jsou odstraněny = kompilační chyby ve všech spotřebitelích → to se řeší v kroku 9.2).

---

### Krok 9.2 – Přepojení všech referencí `SftpConn` / `SftpProfile` na instanční členské proměnné

**Cíl**: Všechna místa v `fs2.cpp`, `fs1.cpp`, `sftpglue.cpp`, `menu.cpp` a `dialogs.cpp`, kde se přistupuje ke globálním `SftpConn` a `SftpProfile`, přesměrovat na `this->Conn` / `this->Profile` (v metodách `CPluginFSInterface`) nebo na `fs->Conn` / `fs->Profile` (v externích funkcích, kde je FS předán jako parametr).

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- Všechna volání `SftpEnsureConnected(parent)` → `SftpEnsureConnected(parent, Conn, Profile)`
- Všechna `SftpConn.XYZ(...)` → `Conn.XYZ(...)` (v metodách `CPluginFSInterface`)
- Všechna `SftpProfile.Name` / `.Host` / `.Port` atd. → `Profile.Name` / `.Host` / `.Port`
- Statické globální proměnné pro progress (`g_ProgDlg`, `g_ProgMainWnd`, `g_ProgFile`, `g_ProgStartTick`, `g_ProgCancel`, `g_OvrCancel`, `g_OvrParent`) – zatím ponechat globální (budou instancovány až v kroku 11.x)

#### [MODIFY] [fs1.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs1.cpp)
- V `ExecuteOnFS` – přetypovat `pluginFS` na `CPluginFSInterface*` a přistupovat k `fs->Conn` / `fs->Profile`
- V `ExecuteChangeDriveMenuItem` – přihlašovací dialog naplní `ConnectData` a profil se zkopíruje do `fs->Profile` v `ChangePath`
- V `DisconnectFS` – volat `fs->Conn.Disconnect()` místo `SftpConn.Disconnect()`

#### [MODIFY] [sftpglue.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpglue.cpp)
- `SftpEnsureConnected` přijímá `CSftpConnection&` a `CSftpProfile&`

#### [MODIFY] [menu.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/menu.cpp) (pokud používá `SftpConn` / `SftpProfile`)
- Přesměrovat na aktivní FS panel: `SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE)` → cast na `CPluginFSInterface*`

**Ověření**: `mingw32-make -f Makefile.mingw CROSS_COMPILE=` – kompilace bez chyb. Funkční test: otevřít SFTP panel, procházet adresáře, stahovat soubor.

---

### Krok 9.3 – Inicializace profilu v `ChangePath` a `OpenFS`

**Cíl**: Při otevření nového FS panelu se profil korektně zkopíruje z globálního `ConnectData` do instančního `Profile`.

#### [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h)
- Konstruktor `CPluginFSInterface()` inicializuje `Profile` na nulový stav (`.Valid = false`)

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp) – `ChangePath`
- Na začátku `ChangePath`, pokud `!Profile.Valid && ConnectData.UseConnectData`:
  - Zkopírovat profil z přihlašovacího dialogu do `this->Profile`
  - Nastavit `Profile.Valid = true`
  - Vymazat `ConnectData.UseConnectData = FALSE`

**Ověření**: Otevřít dva panely (levý + pravý) s různými SFTP servery. Každý panel má svůj vlastní `Profile`.

---

### Krok 9.4 – Keepalive timer per-instance

**Cíl**: Každá FS instance registruje svůj vlastní keepalive timer.

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- V `ChangePath`: timer `SFTP_TIMER_KEEPALIVE` je již registrován s `this` jako parametrem – **žádná změna** potřeba (timer je per-instance díky `AddPluginFSTimer(..., this, ...)`).
- V `Event(FSE_TIMER, SFTP_TIMER_KEEPALIVE)`: volat `this->Conn.SendKeepalive()` (místo `SftpConn.SendKeepalive()`)

**Ověření**: Dva otevřené panely na různé servery, oba udržují spojení.

---

### Krok 9.5 – Disconnect per-instance

**Cíl**: Příkazy „Disconnect" (F12 / menu) odpojují pouze vybranou instanci, nikoli globální konexe.

#### [MODIFY] [fs1.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs1.cpp)
- `DisconnectFS`: přetypovat `pluginFS` → `CPluginFSInterface*`, volat `fs->Conn.Disconnect()`
- Menu příkazy `MENUCMD_DISCONNECT_LEFT` / `_RIGHT` / `_ACTIVE`: získat FS z panelu, přetypovat, zavolat disconnect na konkrétní instanci

**Ověření**: Odpojit jeden panel – druhý zůstane připojený.

---

### Krok 9.6 – Instancování statických callbacků `CSftpConnection` (Progress, HostKey, Kbd)

**Cíl**: Callbacky `ProgressFn`, `HostKeyVerifyFn`, `KbdPromptFn` jsou nyní `static` v `CSftpConnection`. Pro paralelní konexe je to problém (sdílejí kontext). Zatím to funguje, protože Salamander volá vše v jednom UI vláknu, ale připravíme infrastrukturu.

#### [MODIFY] [sftpconn.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconn.h)
- `ProgressFn`, `ProgressCtx` přesunout ze `static` na instanční (member) proměnné
- Metodu `SetProgressCallback` změnit z `static` na instanční
- `HostKeyCb` a `KbdCb` **ponechat static** (jsou sdílené pro celý plugin – OK, protože Salamander je single-threaded UI)

#### [MODIFY] [sftpconn.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpconn.cpp)
- Přesunout definice `Progress` a `ProgressCtx` z `static` na member
- Inicializovat v konstruktoru

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- `SftpProgressBegin` a spol. volají `Conn.SetProgressCallback(...)` místo `CSftpConnection::SetProgressCallback(...)`

**Ověření**: Kompilace + jeden panel stahuje soubor s progress barem.

---

## BOD 11: Asynchronní přenosy na pozadí (Phase B)

> [!IMPORTANT]
> Bod 11 **závisí na dokončení bodu 9**. Bez per-instance konexí nelze provádět paralelní operace.

### Krok 11.1 – Worker thread infrastruktura (`CSftpTransferWorker`)

**Cíl**: Vytvořit třídu worker threadu, která provádí přenosové operace v pozadí.

#### [NEW] [sftpworker.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.h)
- Třída `CSftpTransferWorker`:
  - Vlastní vlákno (`HANDLE ThreadHandle`)
  - Vlastní `CSftpConnection WorkerConn` – **dedicatedá konexe** otevřená se stejným profilem jako FS instance (nutné, protože hlavní `Conn` musí zůstat k dispozici pro procházení adresářů)
  - Fronta úloh (`std::queue<CSftpTransferTask>`)
  - Synchronizace: `CRITICAL_SECTION QueueLock`, `HANDLE WakeEvent`
  - Stav: `volatile bool Running`, `volatile bool Cancelled`
  - Metody:
    - `Start(CSftpProfile& profile)` – spustí vlákno a otevře druhou konexe
    - `Stop()` – čistě ukončí vlákno
    - `EnqueueTask(CSftpTransferTask task)` – přidá úlohu do fronty
    - `Cancel()` – nastaví `Cancelled`, úlohy se přeskočí

#### [NEW] [sftpworker.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.cpp)
- Thread proc: smyčka `WaitForSingleObject(WakeEvent)` → vytáhne úlohu z fronty → provede stažení/nahrání → aktualizuje sdílený stav

**Ověření**: Unit test – vytvoření workera, přidání dummy úlohy, korektní ukončení vlákna.

---

### Krok 11.2 – Struktura přenosové úlohy (`CSftpTransferTask`)

**Cíl**: Definovat datovou strukturu popisující jednu přenosovou operaci.

#### [MODIFY] [sftpworker.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.h)
```cpp
struct CSftpTransferTask
{
    enum Type { Download, Upload };
    Type TaskType;
    std::string RemotePath;
    std::string LocalPath;
    unsigned __int64 FileSize;       // expected size (for progress)
    unsigned __int64 ResumeOffset;   // 0 = start from beginning
    bool IsDirectory;                // true = recursive
};
```

**Ověření**: Kompilace.

---

### Krok 11.3 – Sdílený stav přenosu (`CSftpTransferState`)

**Cíl**: Struktura pro thread-safe sdílení stavu z worker threadu do UI.

#### [MODIFY] [sftpworker.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.h)
```cpp
struct CSftpTransferState
{
    CRITICAL_SECTION Lock;
    
    // Aktuální soubor
    char CurrentFile[MAX_PATH];
    unsigned __int64 FileDone;
    unsigned __int64 FileTotal;
    
    // Celkový postup
    int CurrentIndex;
    int TotalCount;
    unsigned __int64 TotalDone;
    unsigned __int64 TotalExpected;
    
    // Stav
    bool IsRunning;
    bool HasError;
    char ErrorMsg[512];
    DWORD StartTick;
};
```

**Ověření**: Kompilace.

---

### Krok 11.4 – Nemodální přenosový dialog (`CSftpBgTransferDlg`)

**Cíl**: Nový nemodální dialog, který zobrazuje stav přenosu a může být „schován" (minimalizován / přesunut do systémové lišty).

#### [MODIFY] [lang.rh](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang.rh)
- Nové ID: `IDD_BGTRANSFERDLG`, `IDB_BACKGROUND` (tlačítko „Na pozadí")

#### [MODIFY] [lang_en.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_en.rc) & [lang_cs.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_cs.rc)
- Šablona dialogu: stejný layout jako `IDD_TRANSFERDLG`, navíc tlačítko „Background" / „Na pozadí"

#### [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h)
- Třída `CSftpBgTransferDlg` – nemodální dialog:
  - Čte stav z `CSftpTransferState` (přes `WM_TIMER` polling 100–200 ms)
  - Tlačítko „Na pozadí" skryje dialog, ale worker běží dál
  - Tlačítko „Storno" nastaví `Worker.Cancel()`
  - Při dokončení všech úloh dialog sám zmizí

**Ověření**: Dialog se zobrazí, aktualizuje se z dummy stavu.

---

### Krok 11.5 – Napojení worker threadu na `CopyOrMoveFromFS` / `CopyOrMoveFromDiskToFS`

**Cíl**: Při zahájení kopírování/přesunutí se úlohy vloží do fronty worker threadu místo synchronního provádění.

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- V `CopyOrMoveFromFS` (stahování z SFTP na disk):
  1. Enumerovat vybrané soubory (stávající logika)
  2. Místo synchronního `SftpDownloadRecursive` → `Worker.EnqueueTask(...)` pro každý soubor/adresář
  3. Otevřít nemodální `CSftpBgTransferDlg`
  4. Vrátit se okamžitě (`return TRUE` s `cancelOrHandlePath = FALSE`) – Salamander panel zůstane interaktivní
  
- V `CopyOrMoveFromDiskToFS` (nahrávání z disku na SFTP):
  - Analogicky

#### [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h)
- `CPluginFSInterface` získá členskou proměnnou `CSftpTransferWorker* Worker` (lazy-initialized)
- Destruktor `~CPluginFSInterface` zastaví worker

**Ověření**: Zahájit stahování → dialog se zobrazí nemodálně → panel je navigovatelný → soubory se stáhnou.

---

### Krok 11.6 – Storno, chybové stavy a dokončení

**Cíl**: Korektní ošetření storna, chyb a dokončení přenosu.

#### [MODIFY] [sftpworker.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.cpp)
- Worker po každém souboru kontroluje `Cancelled`
- Při chybě nastaví `State.HasError = true`, `State.ErrorMsg = ...`
- Při dokončení nastaví `State.IsRunning = false`

#### [MODIFY] [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp) – dialog & notifikace
- Dialog při `!State.IsRunning`:
  - Pokud `State.HasError` → zobrazí chybovou hlášku
  - Pokud OK → zavře se
  - Po dokončení přenosu zavolat notifikaci pro aktualizaci panelů:
    ```cpp
    SalamanderGeneral->PostChangeOnPathNotification(targetPath, TRUE);
    if (isMove)
        SalamanderGeneral->PostChangeOnPathNotification(sourcePath, TRUE);
    ```
    (přesně podle vzoru `CFTPOperation::PostChangeOnPathNotifications` v FTP pluginu).

**Ověření**: Storno během přenosu → zbývající soubory se nevytvoří. Chyba → dialog zobrazí hlášku.

---

### Krok 11.7 – Tlačítko „Na pozadí" a obnovení dialogu

**Cíl**: Uživatel může dialog schovat a znovu otevřít.

#### [MODIFY] [sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h) / [fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- Tlačítko „Na pozadí" → `ShowWindow(HWindow, SW_HIDE)`, dialog zůstává v paměti
- V menu pluginu přidat „Show transfers..." / „Zobrazit přenosy..." pro znovuotevření
- Nebo: Salamander status bar indikátor (pokud API dovolí)

**Ověření**: Schovat dialog → přenos pokračuje → znovu otevřít → progress aktuální.

---

### Krok 11.8 – Oddělená konexe worker threadu

**Cíl**: Worker thread otevře svou vlastní `CSftpConnection` se stejnými credentials, aby procházení v panelu neblokoval přenos.

#### [MODIFY] [sftpworker.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.cpp)
- V `Start(CSftpProfile& profile)`:
  - `WorkerConn.Connect(profile.Host, profile.Port, ...)` – otevře separátní SSH session
  - Pokud se nepodaří připojit → chyba v dialogu
- V `Stop()`:
  - `WorkerConn.Disconnect()`

**Ověření**: Přenos běží na pozadí + souběžné procházení adresářů v panelu bez prodlevy.

---

### Krok 11.9 – Overwrite/Resume dialog z worker threadu

**Cíl**: Při kolizi souborů (soubor již existuje) se uživateli zobrazí dotaz.

#### [MODIFY] [sftpworker.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftpworker.cpp)
- Worker thread nemůže přímo otevřít dialog (není UI thread)
- Řešení: `PostMessage` / `SendMessage` do `CSftpBgTransferDlg` s požadavkem na overwrite dialog
- Worker thread čeká na `HANDLE OverwriteEvent` (set by UI thread po odpovědi uživatele)
- Volby: Overwrite / Resume / Skip / Skip all / Cancel

**Ověření**: Existující soubor → dialog se zobrazí → overwrite funguje.

---

### Krok 11.10 – Aktualizace dokumentace

#### [MODIFY] [PLUGIN_DEV.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/PLUGIN_DEV.md)
- Nová sekce: „10. Per-instance konexe a asynchronní přenosy"

#### [MODIFY] [README.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README.md) & [README_CZ.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README_CZ.md)
- Aktualizovat seznam funkcí

#### [MODIFY] [jobs_done.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/jobs_done.md)
- Nové řádky pro dokončené úkoly

#### [MODIFY] [nice_to_have.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/nice_to_have.md)
- Označit body 9 a 11 jako realizované

---

## Plán ověření

### Automatické testy
```powershell
mingw32-make -f Makefile.mingw CROSS_COMPILE=
```
- Kompilace bez chyb a warningů
- Unit testy (existující + nové pro worker thread)

### Manuální testování
1. **Dva SFTP panely současně** (levý panel = server A, pravý panel = server B):
   - Procházení adresářů nezávisle
   - Keepalive na obou spojeních
   - Disconnect jednoho – druhý zůstane
2. **Kopírování z SFTP na disk** (stahování):
   - Dialog se zobrazí nemodálně
   - Panel zůstane navigovatelný
   - Progress se aktualizuje
   - Storno funguje
3. **Kopírování z disku na SFTP** (nahrávání):
   - Analogicky
4. **Tlačítko „Na pozadí"**:
   - Přenos pokračuje po schování dialogu
   - Znovuotevření zobrazí aktuální stav
5. **Overwrite dialog** při kolizi souborů
6. **Kopírování mezi dvěma SFTP panely** (server A → server B):
   - Dočasný lokální buffer (stáhne z A, nahraje na B)

---

## Diagram závislostí kroků

```mermaid
graph TD
    A["9.1 Přesun SftpConn/SftpProfile<br>do CPluginFSInterface"] --> B["9.2 Přepojení všech<br>referencí v fs2/fs1"]
    B --> C["9.3 Inicializace profilu<br>v ChangePath/OpenFS"]
    C --> D["9.4 Keepalive<br>per-instance"]
    D --> E["9.5 Disconnect<br>per-instance"]
    E --> F["9.6 Instancování<br>static callbacků"]
    F --> G["11.1 Worker thread<br>infrastruktura"]
    G --> H["11.2 Struktura<br>TransferTask"]
    H --> I["11.3 Sdílený stav<br>TransferState"]
    I --> J["11.4 Nemodální<br>dialog BgTransfer"]
    J --> K["11.5 Napojení na<br>CopyOrMoveFrom*"]
    K --> L["11.6 Storno, chyby,<br>dokončení"]
    L --> M["11.7 Tlačítko<br>'Na pozadí'"]
    M --> N["11.8 Oddělená konexe<br>worker threadu"]
    N --> O["11.9 Overwrite/Resume<br>z worker threadu"]
    O --> P["11.10 Dokumentace"]
```

---

## Odhad náročnosti

| Krok | Složitost | Odhad |
|------|-----------|-------|
| 9.1 | Střední | ~1 h |
| 9.2 | Vysoká (hodně přepisování) | ~3 h |
| 9.3 | Nízká | ~30 min |
| 9.4 | Nízká (už funguje) | ~15 min |
| 9.5 | Nízká | ~30 min |
| 9.6 | Střední | ~1 h |
| 11.1 | Vysoká (nový soubor, threading) | ~3 h |
| 11.2 | Nízká | ~15 min |
| 11.3 | Střední | ~30 min |
| 11.4 | Střední (nový dialog) | ~2 h |
| 11.5 | Vysoká (refaktoring přenosů) | ~4 h |
| 11.6 | Střední | ~1 h |
| 11.7 | Nízká | ~30 min |
| 11.8 | Střední | ~1 h |
| 11.9 | Vysoká (cross-thread UI) | ~2 h |
| 11.10 | Nízká | ~30 min |
| **Celkem** | | **~20 h** |
