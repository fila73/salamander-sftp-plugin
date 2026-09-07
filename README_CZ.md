# SFTP/SCP plugin pro Open Salamander (x64)

Plnohodnotný **SFTP a SCP** klient jako souborový (FS) doplněk pro [Open Salamander 5.0](https://github.com/OpenSalamander/salamander) (x64) a Altap Salamander 4.x. Postavený nad **libssh2 + OpenSSL**, podporuje moderní kryptografii, vysokou rychlost přenosu a maximální stabilitu.

> [!NOTE]
> **Informace o forku a poděkování**:  
> Tento projekt je rozšířeným a udržovaným forkem původního pluginu [Dupl3xx/salamander-sftp-plugin](https://github.com/Dupl3xx/salamander-sftp-plugin).  
> Velké uznání a poděkování patří původnímu autorovi (**Dupl3xx**) za návrh architektury, integraci se Salamander SDK a základní implementaci.  
> Autorská práva původního jádra: Copyright © 2026 Dupl3xx & přispěvatelé.

---

## 🚀 Hlavní rozdíly a vylepšení (oproti původní verzi)

Tento fork přináší řadu oprav stability, architektonických vylepšení, zvýšení výkonu a nových funkcí:

| Oblast / Funkce | Původní verze ([Dupl3xx](https://github.com/Dupl3xx/salamander-sftp-plugin)) | Tento rozšířený fork ([fila73](https://github.com/fila73/salamander-sftp-plugin)) |
|---|---|---|
| **Sestavení a závislosti** | Vyžadovalo externí `libssh2.dll` a dynamické MinGW runtime DLL (`libwinpthread-1.dll` atd.) | **Samostatný build bez závislostí**: `libssh2` je přímo staticky integrován; C/C++ runtime i pthreads jsou linkovány staticky (`-static`). Vyžaduje pouze standardní `libcrypto-3-x64.dll`. |
| **Vlastní SFTP server a přepnutí uživatele** | Pouze výchozí SFTP subsystém SSHD | **Vlastní příkaz `sftp-server` / `sudo` / `su`**: Možnost spustit SFTP subsystém s právy `root` přes `sudo`, přepnout se na servisního uživatele (např. `www-data`), zadat nestandardní cestu k binárce a automatické obalení příkazové řádky. |
| **Spouštění příkazů na serveru** | Základní provádění | **Asynchronní neblokující spouštění na pozadí** přes dedikované SSH spojení; nastavitelné okno konzole (Consolas font, zalamování textu, tlačítko Storno/Cancel). |
| **Udržování spojení (Keepalive)** | Základní TCP / idle obsluha | **Aktivní periodický FS timer keepalive** (interval 8 s) zabraňující odpojení na serverech TrueNAS / OpenSSH (`ClientAliveInterval`) a stavových firewallech; neblokující detekce zdraví socketu a transparentní auto-reconnect. |
| **Výpočet velikosti složek** | Standardní procházení | **Bleskový server-side výpočet** (`FastDirSize` přes SSH `du -sb`), nezamrzající přerušitelný dialog průběhu, ochrana proti symlinkovým cyklům, **stisk mezerníku na složce** s výpočtem a posunem kurzoru a integrace klávesové zkratky **`Ctrl+Shift+F10`** i kontextového menu. |
| **Spouštění souborů na serveru (Enter)** | Všechny soubory se stahují do Windows | **Spouštění souborů s `+x` na serveru při stisku Enter**: Pokud má soubor právo ke spuštění (skripty, binárky), stisk Enter na něm otevře konzoli a spustí `./soubor` na serveru (s podporou `sudo`). Běžné soubory se nadále otevírají lokálně ve Windows. Volitelné v profilu relace. |
| **Navigace v adresářích** | Reset fokusu při přechodu nahoru | **Zachování fokusu kurzoru** na opuštěné složce při přechodu do nadřazeného adresáře (`..`). |

---

## Co plugin umí

### Protokoly
- **SFTP** (SSH File Transfer Protocol) – výchozí, vysoce optimalizovaný protokol v3
- **SCP** – výpis adresářů přes shell (`ls`/`stat`), přenosy přes `libssh2_scp_*`, operace (`mkdir`/`rm`/`mv`/`chmod`) přes shell
- **Nouzové SCP (Fallback)** – automatický přechod na SCP v případě, že vzdálený server nepodporuje SFTP subsystém

### Vlastní příkaz SFTP serveru (Eskalace práv & Přepínání uživatelů)
V přihlašovacím dialogu lze pro profil nastavit pole **SFTP Server**, což umožňuje:
1. **Správu souborů jako `root` přes `sudo`**:
   - `sudo /usr/lib/openssh/sftp-server`
   - `sudo su -c /usr/lib/openssh/sftp-server`
   - Vhodné na serverech, kde je přímé přihlášení uživatele `root` přes SSH zakázáno (`PermitRootLogin no`). Přihlásíte se běžným uživatelem s klíčem a SFTP relace běží pod plnými právy root.
2. **Přepnutí na servisního / aplikačního uživatele**:
   - `sudo -u www-data /usr/lib/openssh/sftp-server`
   - Správa webových adresářů pod identitou webserveru (soubory mají okamžitě správného vlastníka `www-data:www-data`).
3. **Nestandardní cesty k binárce na NAS a jiných OS**:
   - Synology NAS: `/usr/syno/sbin/sftp-server`
   - QNAP / macOS / BSD: `/usr/libexec/sftp-server`
   - OpenWrt / BusyBox: `/usr/lib/ssh/sftp-server`
4. **Vlastní parametry SFTP serveru**:
   - `/usr/lib/openssh/sftp-server -u 0022` (nastavení výchozí umask)
   - `/usr/lib/openssh/sftp-server -l DEBUG3` (podrobné logování na serveru)
5. **Automatické obalení příkazů příkazové řádky**:
   - Příkazy spouštěné z příkazového řádku Salamandera pod panely automaticky přebírají prefix (např. `sudo su -c '<command>'`), takže běží pod stejným uživatelem jako celá relace.

### Kryptografie (přes OpenSSL backend)
Vše se vyjednává automaticky dle možností serveru:
- **Výměna klíčů**: curve25519-sha256, ECDH (nistp256/384/521), DH group14/16/18
- **Typy klíčů**: ed25519, ECDSA, RSA (rsa-sha2-256/512)
- **Šifry**: ChaCha20-Poly1305, AES-GCM, AES-CTR
- **Komprese**: zlib (volitelně)

### Autentizace
- **Heslo**
- **Privátní klíč** – OpenSSH/PEM i **PuTTY `.ppk`** (RSA + ed25519, v2/v3, včetně zašifrovaných klíčů – v2 SHA1/AES, v3 Argon2id přes OpenSSL)
- **Keyboard-Interactive** – včetně 2FA / MFA (první výzva automaticky vyplněna heslem, další výzvy interaktivně přes dialog)

### Bezpečnost
- **Ověření host key** proti souboru `known_hosts` (`%APPDATA%\OpenSalamander-SFTP\known_hosts`)
- Dotaz na důvěru u neznámých serverů (Uložit / Pouze jednou / Odmítnout), varování při změně klíče (ochrana proti MITM)
- Zobrazení otisků SHA256 / SHA1, typu klíče a uvítacího banneru serveru

### Souborové operace a funkce
- Procházení vzdálených souborů (sloupce práva, vlastník, skupina), stahování, nahrávání
- **Průběh přenosu s ukazatelem rychlosti**, potvrzení přepisu
- **Navazování přerušených přenosů (resume)** – pokračování od poslední pozice (SFTP)
- Mazání, vytváření adresářů, přejmenování, **změna oprávnění (`chmod`)**, vlastnosti souborů
- **Úprava souboru přímo na serveru** (`F4` – stáhne soubor do dočasné složky, otevře výchozí editor a po uložení automaticky nahraje zpět)
- **Výpočet velikosti složek na serveru** (`Ctrl+Shift+F10` / mezerník na složce, rychlý server-side `du` výpočet s rekurzivním fallbackem, nezamrzající dialog průběhu s tlačítkem Storno, ochrana proti symlinkovým cyklům a okamžitá aktualizace velikosti v panelu)
- **Spouštění příkazů na serveru**:
  - Přímo z příkazového řádku Salamandera pod panely (s automatickým `sudo`/`su` prefixem)
  - Kontextová položka **`Execute`** (umístěná přímo pod `Open`)
  - Okno konzole s reálným výstupem v reálném čase s přehledným neproporcionálním fontem (Consolas) a možností přerušení (Cancel)

### Správa připojení a UI
- Přihlašovací dialog se stromem relací ve stylu WinSCP (Nová relace / Upravit / Smazat / Přejmenovat / Výchozí)
- **Udržování spojení a stabilita**: Aktivní periodický FS timer keepalive (8 s) zabraňuje odpojení nečinných relací přes firewally a na serverech typu TrueNAS / OpenSSH (`ClientAliveInterval`); transparentní detekce živosti socketu a auto-reconnect & retry při operacích
- Zobrazení/skrytí hesla (ikona oka)
- Podpora tmavého režimu (Dark Mode) sjednoceného se Salamanderem
- Jazyková lokalizace (`.slg` moduly pro češtinu, angličtinu i všech 11 jazyků Salamandera)

---

## Struktura zdrojových kódů (`src/`)

### Jádro pluginu
| Soubor | Účel |
|--------|------|
| **`sftpconn.h/.cpp`** | **Komunikační vrstva nad libssh2.** Připojení, handshake, ověření host key, autentizace, procházení složek, stahování/nahrávání (resume + progress), SCP operace, chmod/stat, streaming výstupu příkazů, aktivní keepalive, rychlý výpočet velikosti složek, spouštění vlastního `sftp-server`. |
| **`sftpglue.h/.cpp`** | **Propojení (glue)** mezi Open Salamander FS a `CSftpConnection`. Pomocné POSIX funkce pro cesty, `SftpEnsureConnected`, `WrapCommandWithSftpServerPrefix`, správa profilů a uložených relací. |
| **`dialogs.h/.cpp`** | Dialog konzole pro spouštění příkazů (`ShowCommandExecDialog`), streamovací vlákno, nastavení monospace fontu a tmavého režimu. |

### Integrace do Salamandera
| Soubor | Účel |
|--------|------|
| `sftp.cpp` | Vstupní bod pluginu, registrace (FS název `dfs`), směrování příkazů menu, Windows message hook pro mezerník, registrace klávesových zkratek, načítání a ukládání relací do registru. |
| `fs1.cpp` | **Přihlašovací dialog** (`ConnectDlgProc`) – strom kategorií, profily připojení, pole vlastního SFTP serveru, správa relací, sledování aktivních FS, zachování fokusu při procházení do `..`. |
| `fs2.cpp` | **Implementace FS rozhraní** – ChangePath, ListCurrentPath, stahování a nahrávání souborů, mazání, tvorba složek, rychlé přejmenování, chmod, SftpOnSpacePressedOnFolder, kontextové menu. |
| `menu.cpp` | Obsluha položek menu pluginu (úprava souboru, výpočet velikosti, odpojení, spuštění). |
| `sftp.h` | Společné deklarace rozhraní a datových struktur. |

### Zdroje a sestavení
| Soubor | Účel |
|--------|------|
| `lang/lang.rc`, `lang.rc2`, `lang.rh` | Dialogy, textové řetězce a překlady kompilované do `.slg`. |
| `res/fs.ico`, `dir.ico`, `file.ico` | Ikony pluginu a dialogů. |
| `versinfo.rh2` | Informace o verzi a copyrightu. |
| `Makefile.mingw` | Skript pro samostatnou kompilaci přes GCC / MinGW-w64 (statický runtime). |
| `vcxproj/sftp.vcxproj` | Projektový soubor pro Microsoft Visual Studio (MSBuild). |

---

## Sestavení

### Možnost A: Přes MinGW-w64 (nativní GCC, MSYS2 nebo WSL)
Plugin lze jednoduše přeložit pomocí přiloženého `Makefile.mingw`:

```powershell
mingw32-make -f Makefile.mingw CROSS_COMPILE=
```

Výsledkem jsou zkompilované binárky `sftp.spl`, `english.slg` a `czech.slg` se staticky přilinkovaným `libssh2` i C/C++ runtimem (bez závislosti na MinGW DLL). Pro provoz na jiných PC stačí do složky pluginu přiložit pouze standardní 64bitovou `libcrypto-3-x64.dll`.

### Možnost B: Přes Visual Studio (MSBuild)
**Požadavky:** Visual Studio 2022 (x64), knihovny `libssh2` a `openssl` přes vcpkg.

```powershell
MSBuild src\vcxproj\sftp.vcxproj /p:Configuration=Release /p:Platform=x64
```

---

## Licence a poděkování
- **Původní repozitář**: [https://github.com/Dupl3xx/salamander-sftp-plugin](https://github.com/Dupl3xx/salamander-sftp-plugin)
- Postaveno na Open Salamander SDK (SPDX / GPL-2.0-or-later).
- Původní kód Copyright © 2026 Dupl3xx.
- Úpravy, rozšíření a údržba Copyright © 2026 fila73 & přispěvatelé.
