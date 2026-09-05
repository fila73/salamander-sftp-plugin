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

Pro zajištění stability spojení na nestabilních sítích a proti timeoutům routerů/firewallů:
1. **TCP Keepalive**: Socket má aktivovaný `SO_KEEPALIVE` s nastavením `SIO_KEEPALIVE_VALS` (15 s nečinnost, 5 s interval opakování).
2. **SSH Keepalive**: Nastaven přes `libssh2_keepalive_config` a aktivně kontrolován při volání `libssh2_keepalive_send`.
3. **Detekce stavu v `IsConnected()`**: Neblokující kontrola `select` s `recv(MSG_PEEK)` detekuje vzdálené uzavření spojení (FIN), reset (RST) nebo síťovou chybu ještě před zahájením další operace.
4. **Transparentní Reconnect**: `SftpEnsureConnected()` při zjištění odpojení automaticky obnoví spojení pomocí aktivního profilu bez nutnosti ručního zásahu uživatele.

---

## 5. Statické linkování a distribuce na jiné počítače

Pro maximální přenositelnost bez nutnosti instalovat MinGW/GCC runtimes:
- **Statické runtimes**: `Makefile.mingw` používá `-static -static-libgcc -static-libstdc++`, což eliminuje závislosti na `libwinpthread-1.dll`, `libgcc_s_seh-1.dll` i `libstdc++-6.dll`.
- **Statický libssh2**: Slinkován ze statického archivu `libssh2_static.a`.
- **OpenSSL (`libcrypto-3-x64.dll`)**: Z důvodu dynamického načítání a aktualizací kryptografických modulů se `libcrypto-3-x64.dll` distribuuje jako samostatná 64bitová DLL přímo ve složce pluginu (`plugins\sftp\`), odkud ji `sftp.cpp` při startu přednostně načítá (`LOAD_WITH_ALTERED_SEARCH_PATH`).
- **Ověření závislostí**:
  ```powershell
  objdump -p sftp.spl | Select-String "DLL Name"
  ```
  Výstup smí obsahovat pouze standardní Windows systémové knihovny a `libcrypto-3-x64.dll`.
- **Statická analýza**:
  ```powershell
  cppcheck --enable=warning,performance,portability,style src/
  ```

