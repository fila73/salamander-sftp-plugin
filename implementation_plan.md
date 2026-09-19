# Implementační plán: Fáze A – Informativní dialog přenosu (Transfer Progress) s podporou Dark Mode

Tento plán popisuje realizaci Fáze A pro plugin SFTP v Open Salamanderu: nahrazení dosavadního recyklovaného dialogu mazání (`CDeleteProgressDlg`) za plnohodnotný, přehledný a informativní dialog přenosu souborů (`CSftpTransferProgressDlg`) s kompletní podporou tmavého režimu (Dark Mode).

## Uživatelské přezkoumání a požadavky

> [!IMPORTANT]
> **Fáze A**:
> 1. Vytvoření dedikovaného dialogového okna `IDD_TRANSFERDLG` v anglických i českých zdrojích.
> 2. Oddělení klíčových informací:
>    - **Zdroj (From / Odkud)** a **Cíl (To / Kam)** s ošetřením dlouhých cest.
>    - **Aktuální soubor (Soubor)** a jeho velikost.
>    - **Statistika přenosu**: přeneseno / celkem pro aktuální soubor, okamžitá rychlost přenosu v MB/s a odhadovaný zbývající čas (ETA).
>    - **Dva progress bary**: postup aktuálního souboru (`IDP_TR_FILE_PROGRESS`) a celkový postup přenosu (`IDP_TR_TOTAL_PROGRESS`).
> 3. **Plná podpora Dark Mode**:
>    - Integrace `plugindarkmode.h` do dialogové procedury (`PluginDarkMode_ApplyTitleBar`, `PluginDarkMode_HandleCtlColor`, `PluginDarkMode_HandleThemeMessage`).
>    - Stylizace obou progress barů přes `SalamanderGUI->AttachProgressBar`.

---

## Navržené změny

### 1. Zdroje a identifikátory (Resources)

#### [MODIFY] [src/lang/lang.rh](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang.rh)
- Definice nových ID pro dialog a prvky:
  - `IDD_TRANSFERDLG`
  - `IDT_TR_FROM_LABEL`, `IDT_TR_FROM_PATH`
  - `IDT_TR_TO_LABEL`, `IDT_TR_TO_PATH`
  - `IDT_TR_FILE_LABEL`, `IDT_TR_FILE_NAME`
  - `IDT_TR_STATUS`
  - `IDP_TR_FILE_PROGRESS`
  - `IDT_TR_TOTAL_STATUS`
  - `IDP_TR_TOTAL_PROGRESS`

#### [MODIFY] [src/lang/lang_en.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_en.rc) & [src/lang/lang_cs.rc](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/lang/lang_cs.rc)
- Definice šablony dialogu `IDD_TRANSFERDLG` (EN i CZ verze).

---

### 2. Třída dialogu a engine přenosu

#### [MODIFY] [src/sftp.h](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/sftp.h)
- Deklarace třídy `CSftpTransferProgressDlg : public CCommonDialog`:
  - Ukazatele na progress bary (`FileProgressBar`, `TotalProgressBar`).
  - Cache proměnné pro texty, rychlost, ETA a procenta.
  - Metody:
    - `SetOperationInfo(bool upload, const char* fromPath, const char* toPath)`
    - `SetCurrentFile(const char* fileName, unsigned __int64 fileSize)`
    - `UpdateFileProgress(unsigned __int64 done, unsigned __int64 total)`
    - `UpdateTotalProgress(int fileIndex, int totalFiles, unsigned __int64 totalBytesDone, unsigned __int64 totalBytesExpected)`
    - `GetWantCancel()`

#### [MODIFY] [src/fs2.cpp](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/src/fs2.cpp)
- Implementace třídy `CSftpTransferProgressDlg` včetně:
  - Obsluha zpráv `WM_INITDIALOG`, `WM_COMMAND`, `WM_CTLCOLOR*`, `WM_THEMECHANGED`, `WM_SETTINGCHANGE`.
  - Výpočet klouzavé průměrné rychlosti a odhadu zbývajícího času (ETA).
  - Přepracování funkcí `SftpProgressBegin`, `SftpProgressCallback`, `SftpProgressEnd` na nový dialog.
  - Aktualizace `CopyOrMoveFromFS` a `CopyOrMoveToFS` pro předávání informací o zdroji, cíli, počtu položek a celkovém postupu.

---

### 3. Dokumentace

#### [MODIFY] [PLUGIN_DEV.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/PLUGIN_DEV.md)
#### [MODIFY] [jobs_done.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/jobs_done.md)
#### [MODIFY] [README_CZ.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README_CZ.md) & [README.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/README.md)
#### [MODIFY] [nice_to_have.md](file:///c:/Users/filip/AntigravityProjects/salamander-sftp-plugin/nice_to_have.md)

---

## Plán ověření

### Automatické sestavení a testy
1. `mingw32-make -f Makefile.mingw CROSS_COMPILE=` – kompilace bez chyb (slinkováno `sftp.spl`, `english.slg`, `czech.slg`).
2. Kontrola existence a velikosti výstupních knihoven `sftp.spl`, `english.slg`, `czech.slg`.
3. Spuštění unit testů (`test_path_hottrack.exe` - ALL TESTS PASSED).

---

## Stav realizace
- [x] Přidání resource ID do `src/lang/lang.rh`
- [x] Šablony dialogů `IDD_TRANSFERDLG` v `src/lang/lang_en.rc` a `src/lang/lang_cs.rc`
- [x] Deklarace `CSftpTransferProgressDlg` v `src/sftp.h`
- [x] Implementace `CSftpTransferProgressDlg` a Dark Mode v `src/fs2.cpp`
- [x] Předávání informací o operaci v `SftpSyncDir`, `CopyOrMoveFromFS`, `CopyOrMoveToFS`
- [x] Zobrazení jména konexe `[NAS]` ve vzdálených cestách (`From:` / `To:`) a v záhlaví okna přenosu
- [x] Kompilace a ověření buildů `sftp.spl`, `english.slg`, `czech.slg`
- [x] Zamezení vzniku 0B souborů při stornu & okamžité ukončení přenosových smyček bez dotazů (`src/sftpconn.cpp`, `src/fs2.cpp`)

---

## Ošetření storna přenosu a prevence vzniku 0-bajtových souborů
- **Problém**: Při stisku Storno v přenosovém dialogu vznikaly v cílovém umístění 0B prázdné soubory pro položky, které se ještě nezačaly přenášet, a smyčky pokračovaly v iteraci.
- **Řešení**:
  1. Kontrola `ReportProgress` před jakýmkoli vytvořením/zkrácením souboru (`fopen_s` / `libssh2_sftp_open`).
  2. Automatický úklid nedotčených souborů (`DeleteFileA` / `libssh2_sftp_unlink`), pokud došlo ke stornu před zápisem prvního bajtu (`done == 0 && resumeOffset == 0`).
  3. Zavedení `SftpIsCancelled()`, který detekuje jak dialog přepisu (`g_OvrCancel`), tak tlačítko Storno v progress dialogu (`g_ProgDlg->GetWantCancel()`, `g_ProgCancel`).
  4. Okamžité přerušení smyček (`break`) v `CopyOrMoveFromFS`, `CopyOrMoveFromDiskToFS`, `SftpDownloadRecursive` a `SftpUploadRecursive` bez dotazování uživatele na pokračování.



