# Aktualizace README a README_CZ pro Release (Fork od Dupl3xx)

Tento plán popisuje rozšíření a aktualizaci dokumentace v [README.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README.md) a [README_CZ.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README_CZ.md) a synchronizaci v návazných souborech ([jobs_done.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/jobs_done.md), [PLUGIN_DEV.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/PLUGIN_DEV.md)).

## Přehled cílů

1. **Uvedení původu (Fork attribution)**:
   - Jasně deklarovat, že repozitář je forkiem původního projektu [`Dupl3xx/salamander-sftp-plugin`](https://github.com/Dupl3xx/salamander-sftp-plugin).
   - Vzdát kredit původnímu autorovi (Dupl3xx) za položení základů a architekturu integrace do Open Salamandera.

2. **Strukturované shrnutí rozdílů naší verze oproti originálu**:
   - **Samostatný build a nulové runtime závislosti na MinGW DLL**:
     - Statické slinkování `libssh2` i C/C++ runtimu (`-static`), eliminace potřeby `libwinpthread-1.dll`, `libgcc_s_seh-1.dll` apod. Jediná externí knihovna je standardní `libcrypto-3-x64.dll`.
     - Plná podpora samostatného sestavení (`Makefile.mingw` a MSBuild `sftp.vcxproj`) bez nutnosti kompilace celého zdrojového stromu Salamandera.
   - **Vzdálené spouštění příkazů a streamovací konzole**:
     - Asynchronní neblokující provádění příkazů přes dedikované SSH spojení na pozadí (Salamander panel nezamrzá).
     - Nové nezávislé okno konzole (`ShowCommandExecDialog`) s monospace fontem Consolas, podporou resize a možností přerušení běžícího procesu (Cancel).
     - Integrace do kontextového menu panelu (`Execute`) a podpora spouštění z příkazové řádky Salamandera.
   - **Aktivní keepalive & auto-reconnect pro spolehlivost**:
     - Periodický aktivní FS timer keepalive (8 s) zabraňující odpojení relace při nečinnosti v UI na serverech TrueNAS CORE/SCALE a OpenSSH (`ClientAliveInterval`) a na stavových firewallech.
     - Transparentní auto-reconnect a opakování operací při výpadku spojení.
   - **Rychlý výpočet velikosti složek na serveru (`Ctrl+Shift+F10`)**:
     - Server-side kalkulace přes `du` s bezpečným rekurzivním fallbackem přes SFTP.
     - Nezamrzající dialog s možností kdykoliv výpočet zrušit (Cancel).
     - Ochrana proti cyklickým symlinkům a okamžitý zápis velikosti přímo do panelu Salamandera.
   - **Opravy stability a kompatibility**:
     - Oprava zacyklení při čtení velkých adresářů (directory listing loop fix).
     - Zachování fokusu/kurzoru na právě opuštěné složce při přechodu do `..`.
     - Podpora vlastního příkazu `sftp-server` a správná expanze domovské složky (`~`).
     - Oprava zasekávání a pomalého pohybu oken (slow move fix) a korektní ANSI/Unicode notifikace v TreeView přihlašovacího dialogu.
   - **UI, Dark Mode & Lokalizace**:
     - Tlačítko pro zobrazení/skrytí hesla (ikona oka).
     - Plná podpora Dark Mode v souladu se Salamander tématem.
     - Kompletní lokalizace pro všech 11 jazyků Salamandera (`.slg` / `.slt`).
     - Kompatibilita s Altap Salamander 4.x i Open Salamander 5.x.

## Navrhované změny

### Dokumentace

#### [MODIFY] [README.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README.md)
- Přidat odkaz na upstream fork `https://github.com/Dupl3xx/salamander-sftp-plugin`.
- Přidat kapitolu **Fork & Key Differences from Upstream**, kde budou přehledně v odrážkách a srovnávací tabulce popsána všechna vylepšení naší verze.
- Aktualizovat sekci o sestavení, závislostech a funkcích.

#### [MODIFY] [README_CZ.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README_CZ.md)
- Přidat českou informaci o forku z `https://github.com/Dupl3xx/salamander-sftp-plugin`.
- Přidat kapitolu **Fork a rozdíly oproti původní verzi**, detailně popisující všechny výhody a úpravy naší verze v češtině.

#### [MODIFY] [jobs_done.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/jobs_done.md)
- Zaznamenat provedení aktualizace dokumentace pro release.

## Plán ověření

### Manuální a statické ověření
- Zkontrolovat správnost všech markdown odkazů a formátování tabulek.
- Zkontrolovat, že obě jazykové mutace (EN i CZ) jsou v plné shodě.
- Ověřit git status a provést logický commit do gitu dle uživatelských pravidel.
