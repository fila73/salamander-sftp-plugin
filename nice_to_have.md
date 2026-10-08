# Zásobník vylepšení (Nice-to-Have & Backlog) – SFTP Plugin

Tento dokument shromažďuje nápady, náměty na rozšíření a potenciální budoucí vylepšení pluginu **SFTP/SCP pro Open Salamander**.

---

## 💡 Náměty a plánovaná rozšíření

### 1. Integrace s SSH Agenty (Pageant / Windows OpenSSH Agent)
- **Popis**: Umožnit autentizaci přes běžícího agenta (PuTTY Pageant nebo Windows OpenSSH `ssh-agent`) bez nutnosti zadávat heslo ke klíči nebo cestu k souboru na disku.
- **Přínos**: Výrazné zvýšení komfortu a bezpečnosti pro uživatele používající hardwarové klíče (YubiKey/FIDO2) nebo centrální správu klíčů.

### 2. Podpora Proxy a Jump Host (Bastion / SOCKS5 / HTTP CONNECT)
- **Popis**: Připojení na servery ve vnitřní síti přes SSH Jump host (ekvivalent `-J` / `ProxyJump` v OpenSSH) nebo přes firemní SOCKS5 / HTTP proxy.
- **Přínos**: Přístup k produkčním a staging serverům za privátní bránou přímo ze Salamandera.

### 3. Synchronizace adresářů (Directory Compare & Synchronize)
- **Popis**: Porovnání obsahu lokální a vzdálené složky podle velikosti a času modifikace s možností jednosměrné či obousměrné synchronizace.
- **Přínos**: Rychlé nasazování webů, zálohování a přenos pouze změněných souborů.

### 4. Oblíbené vzdálené cesty / Záložky (Bookmarks)
- **Popis**: Možnost definovat v profilu relace oblíbené podsložky pro rychlý skok do často navštěvovaných adresářů.
- **Přínos**: Urychlení navigace na rozsáhlých serverech s hlubokou adresářovou strukturou.

### 5. Volba kódování názvů souborů (Charset / Encoding)
- **Popis**: Možnost vynutit jiné kódování znaků než standardní UTF-8 (např. CP1250, ISO-8859-2) pro starší nebo specificky konfigurované Unix/Linux servery.
- **Přínos**: Bezproblémová práce s diakritikou na historických systémech.

### 6. Filtry přenosů a masky souborů (File Exclude / Include Masks)
- **Popis**: Pravidla pro vynechání vybraných souborů/složek při hromadném stahování a nahrávání (např. `.git`, `.DS_Store`, `node_modules`, `*.tmp`).
- **Přínos**: Zrychlení přenosů projektových složek a čistota přenášených dat.

### 7. Pokročilá správa symbolických odkazů (Symlinks)
- **Popis**: Volba chování při práci se symlinky – možnost stahovat cíl odkazu (dereference) nebo zachovat / ignorovat odkaz.
- **Přínos**: Větší kontrola nad přenosem složitých Linuxových stromů.

### 9. Souběžné vícenásobné konexe (Per-instance CPluginFSInterface) [HOTOVO – v1.3.0]
- **Popis**: Převést `CSftpConnection` a `CSftpProfile` z globálního singletonu na členské objekty `CPluginFSInterface` (podle vzoru vestavěného FTP pluginu v Salamanderu).
- **Stav**: Kompletně dokončeno (Kroky 9.1 až 9.6). Každá instance FS panelu vlastní své nezávislé SSH spojení `Conn`, profil `Profile`, per-instance keepalive timer a instanční progress callbacky. Odstraněn globální singleton `SftpConn`.
- **Přínos**: Umožňuje mít současně otevřené 2 a více různých SFTP serverů (např. v levém a pravém panelu, případně v různých tabech) bez vzájemného ovlivňování.

### 10. Barevné štítky profilů pro taby (Server Color Tags)
- **Popis**: Možnost přiřadit v nastavení připojení barvu profilu (např. červená pro produkci, oranžová pro staging, zelená pro dev/test) a při otevření relace automaticky obarvit příslušný tab v Samandarinu.
- **Přínos**: Okamžité vizuální rozlišení prostředí a prevence nechtěných zásahů na produkčních strojích.

### 11. Plně asynchronní nemodální přenosový dialog s během na pozadí & souběžné přenosy (Phase B) [HOTOVO – v1.3.1]
- **Popis**: Rozšíření přenosového dialogu o běh na pozadí (tlačítko 'Na pozadí' / 'Background') a vyčlenění přenosů do dedikovaného pracovního vlákna (`CSftpTransferWorker`) s frontou úloh. Salamander panely zůstávají plně interaktivní i během stahování a nahrávání velkých objemů dat.
- **Stav**: Kompletně dokončeno (Kroky 11.1 až 11.10). Dedikovaná worker SSH relace `WorkerConn`, fronta `CSftpTransferTask`, thread-safe stav `CSftpTransferState`, nemodální dialog s tlačítkem „Na pozadí", znovuzobrazení přes menu „Show Transfers...", automatické notifikace panelů Salamandera přes `PostChangeOnPathNotification`. Nyní rozšířeno o **plnou souběžnost více přenosů (Per-transfer Worker instances)**: Každá operace má vlastní nezávislý `CSftpTransferWorker` a dialog s příznakem `OwnsWorker`, `CPluginFSInterface` eviduje více oken v `ActiveTransferDlgs` a zamezuje kolizím či pádům při zavření aplikace. Přenosová okna jsou nezávislá na hlavním okně (`Parent = NULL`), takže nezůstávají nuceně navrchu při kliknutí do Salamandera, mají vlastní tlačítko na hlavním panelu a podporují minimalizaci (`WS_MINIMIZEBOX`). Zajištěna stoprocentní stabilita vícevláknových přenosů napojením SSH nonces na Windows CSPRNG (`BCryptGenRandom`) a inicializací worker vláken přes CRT `_beginthreadex`.

### 12. Přenosy mezi dvěma SFTP servery (Server-to-Server Copy) [HOTOVO – v1.3.1]
- **Popis**: Podpora přímého kopírování/přesouvání souborů mezi dvěma otevřenými SFTP panely (např. server A v levém panelu, server B v pravém panelu) pomocí transparentního dočasného lokálního bufferu (streaming download z A -> upload na B).
- **Stav**: Kompletně dokončeno. `CopyOrMoveFromFS` analyzuje cílovou URL (`sftp://user@host:port/path`), detekuje odlišný server, vyhledá aktivní profil v `InterfaceForFS.GetActiveFSList()` nebo `SftpProfiles`, naváže/použije cílové spojení `targetConn`, v dialogu zobrazuje odděleně `From: [HOP Test]` a `To: [HOP Stage]`, provede stažení do `%TEMP%` a nahrání na cílový server a následně odešle `PostChangeOnPathNotification` pro oba panely.
- **Přínos**: Pohodlný přesun dat mezi vzdálenými servery bez nutnosti ručního mezistahování na disk uživatele a bez rizika nechtěného přepsání souborů na zdrojovém serveru.

### 13. Persistence FS instance přes restart Salamandera
- **Popis**: Podpora automatického obnovení otevřených SFTP panelů/tabů po restartu aplikace Salamander (implementace `SavePathForMainWindowTitle` a obnovení relace ze serializované cesty `sftp://user@host:port/path`).
- **Přínos**: Zachování rozpracovaného stavu a otevřených složek po ukončení a novém spuštění správce souborů.

### 14. Opuštění panelu, dotaz na odpojení a Detached FS (`TryCloseOrDetach`) [HOTOVO – v1.3.0]
- **Popis**: Implementace metody `TryCloseOrDetach` a `GetChangeDriveOrDisconnectItem` podle vzoru vestavěného FTP pluginu Salamandera.
- **Stav**: Kompletně dokončeno. Při opuštění SFTP panelu (`FSTRYCLOSE_CHANGEPATH`) se uživatele plugin zeptá přes `SalMessageBoxEx` na **Odpojit** / **Ponechat** / **Storno**. Při volbě Ponechat přejde FS do režimu Detached FS (`detach = TRUE`), běží dál na pozadí včetně keepalive a lze se k němu vrátit z `Alt+F1`/`Alt+F2` se zobrazeným jménem profilu `[NAS]`. Volbu lze trvale uložit v registru (`LeavePanelAction`).

### 15. Plnohodnotný dialog konfliktu (přepsat/navázat), 2-módový Progress dialog (Simple/Detailed) a klávesnicová navigace [HOTOVO – v1.4.0]
- **Popis**: Kompletní přiblížení chování a možností vestavěnému FTP pluginu v Open Salamanderu (`IDD_OPERATIONDLG` a `IDD_SOLVEITEMERRSIMPLEEX`):
  1. Dialog řešení konfliktu existujícího souboru (`IDD_CONFLICTDLG`): dropdown split tlačítko „Opakovat" s popup menu (*Opakovat*, *Pokračovat / navázat*, *Pokračovat nebo přepsat*, *Použít alternativní název*), editbox cílového jména, tlačítka Přepsat, Přepsat vše, Přeskočit, Storno, Nápověda a checkbox pro zapamatování volby pro celou operaci.
  2. 2-módový Progress dialog: kompaktní zobrazení s ETA odpočtem, uplynulým časem, stavem a progress barem, dynamické rozbalení po stisku **„Detaily >>"** nebo **„Chyby >>"** odhalující seznam Spojení a seznam Operací per soubor s ikonami a stavy, filtr `[ ] Zobrazit jen chyby`, tlačítko **Pauza / Pokračovat** a checkbox `[x] Po skončení operace zavřít toto okno` s perzistencí v registru.
  3. Klávesnicová navigace: plná podpora `Tab`, `Shift+Tab` a šipek přes dialogový hook `IsDialogMessage`.
- **Stav**: Kompletně dokončeno a nasazeno ve verzi v1.4.0.

### 16. Dedikované UI vlákno transfer dialogu (`CSftpProgressDlgThread`), dynamický resizing a plnohodnotné zobrazení po vzoru FTP [HOTOVO – v1.4.0]
- **Popis**:
  1. **Izolace dialogu do samostatného UI threadu (`CSftpProgressDlgThread`)**: Přenosový dialog běží ve vlastním dedikovaném vlákně s vlastní `GetMessage` smyčkou po vzoru `COperationDlgThread` z FTP pluginu. Žádné zprávy ani ticky timeru neruší hlavní vlákno Salamandera (rozbalené menu nezhasíná, kurzor neproblikává, i když je okno na pozadí či bez fokusu).
  2. **Dynamický layout a resizing (`LayoutDialog` / `SetColumnWidths`)**: Dialog podporuje změnu velikosti (`WM_SIZE`). Seznamy Spojení a Operace si proporcionálně dělí volnou vertikální výšku (35 % / 65 %), ovládací tlačítka jsou ukotvena vpravo dole a šířky sloupců se dynamicky přepočítávají.
  3. **Bohaté informace o přenosech (dle vzoru FTP)**: Akce ve Spojení zobrazuje `Kopíruji <soubor>`, stav formátuje přenesený objem, celkový objem, rychlost v MB/s, procenta a zbývající čas. Sekce Operace nese nadpis `Operace: (hotovo / celkem)`, detailní popis `Kopírovat <soubor> (<velikost>) z <odkud> do <kam> / v režimu přenosu SFTP`, stavy (Čeká, Zpracovávám, Dokončeno, Chyba, Přeskočeno) a tlačítko `Zastavit` (`IDB_OPCONSSTOP`).
- **Stav**: Kompletně dokončeno a nasazeno ve verzi v1.4.0.

### 17. Oprava timeoutu zápisu u velkých uploadů (`Timed out waiting on socket`) [HOTOVO – v1.4.0]
- **Popis**: Oprava v `libssh2` (`_libssh2_wait_socket`), kde vypršení periody keepalive způsobovalo falešné shození přenosu na `LIBSSH2_ERROR_TIMEOUT`. Nastaven velkorysý operační timeout 60 s a `TCP_NODELAY`.
- **Stav**: Kompletně dokončeno a nasazeno ve verzi v1.4.0.
