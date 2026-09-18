# Implementační plán: Oprava tabů a zneplatnění mezipaměti prohlížeče (Cache Invalidation)

Tento plán řeší dva klíčové nedostatky v pluginu SFTP pro Open Salamander:
1. **Ořezávání názvu záložky (tabu) a chybná detekce podsložek**: Oprava parsování SFTP URL v `GetNextDirectoryLineHotPath()` a implementace `GetPathForMainWindowTitle()`.
2. **Vracení zastaralého obsahu při přepnutí serveru / změně souboru**: Oprava klíče diskové mezipaměti v `ViewFile()`.

---

## Uživatelské přezkoumání a rozhodnutí

> [!IMPORTANT]
> **Vícenásobné konexe (2 panely / taby na různé servery):**
> V současné architektuře pluginu existuje pouze jedna globální instance `CSftpConnection SftpConn` a `CSftpProfile SftpProfile`. Připojení k jinému serveru provede `Disconnect()` a přepíše aktivní relaci. Plugin proto v tuto chvíli neumí držet 2 různé servery současně.
> Tento plán řeší bezpečné zneplatnění cache (aby se při přepnutí serveru nezobrazoval obsah předchozího serveru). Architektonický převod spojení na per-instance objekt `CPluginFSInterface` (pro souběžné připojení k více serverům) byl zařazen do `nice_to_have.md`.

---

## Provedené změny

### Jádro SFTP pluginu

#### [MODIFY] [src/sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h)
- Nahrazena inline dummy implementace `virtual BOOL WINAPI GetPathForMainWindowTitle(...) { return FALSE; }` za plnohodnotnou deklaraci metody.

#### [MODIFY] [src/fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
1. **Oprava `GetNextDirectoryLineHotPath()`**:
   - Nahrazena zastaralá logika přeskakování znaků (`while (*++root != 0 && --c)`), která chybně odřezávala písmeno `r` z uživatelského jména `root` (vznikalo `oot@...`).
   - Implementováno korektní přeskočení prefixu `sftp://user@host[:port]/`.
   - Zohledněna lomítka `/` (POSIX styl SFTP) pro rozpad cesty na jednotlivé klikatelné segmenty adresního řádku.
2. **Implementace `GetPathForMainWindowTitle()`**:
   - `mode == 1` (**Directory Name Only** – výchozí režim pro taby): vrací čistý název aktuální složky (např. `Season 29`, nebo `/` pro root).
   - `mode == 2` (**Shortened Path**): vrací zkrácenou cestu s vypuštěnými středovými složkami (např. `sftp://root@10.0.1.35/.../Season 29`).
   - Vrací `TRUE`, čímž Salamander převezme přesný a čistý formát titulku tabu i okna.
3. **Oprava mezipaměti prohlížeče v `ViewFile()`**:
   - Namísto dosavadního `sftp:` + `Path` + `file.Name` (kde zcela chyběl hostitel i uživatel) je klíč cache sestaven pomocí `GetFullName(file, 0, ...)` (zahrnuje `//user@host:port/path/file`).
   - Do klíče cache jsou připojena metadata souboru (velikost a čas modifikace): `_snprintf_s(..., ":%I64u:%08lx%08lx", file.Size.Value, file.LastWrite.dwHighDateTime, file.LastWrite.dwLowDateTime)`.
   - Tím se zaručuje, že soubory ze serveru A a serveru B se stejnou cestou budou mít odlišný klíč v cache a při modifikaci souboru na serveru se stáhne aktuální verze.

---

### Dokumentace

#### [MODIFY] [jobs_done.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/jobs_done.md)
- Zaznamenána oprava parsování cest pro taby a oprava cache prohlížeče.

#### [MODIFY] [PLUGIN_DEV.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/PLUGIN_DEV.md)
- Zdokumentována sekce 7 (titulky tabů a zneplatnění mezipaměti).

#### [MODIFY] [README_CZ.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README_CZ.md) & [README.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README.md)
- Aktualizována srovnávací tabulka vylepšení.

#### [MODIFY] [nice_to_have.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/nice_to_have.md)
- Přidána položka 9 (více souběžných konexí na různé servery) a 10 (barevné štítky profilů pro taby).

---

## Ověření

### Sestavení
- Kompilace přes MinGW dokončena s kódem 0 (`sftp.spl` vygenerován).
- Unit testy v `test/test_path_hottrack.cpp` úspěšně otestovaly tokenizaci cest, formátování tabů pro oba režimy i formát klíče cache.
