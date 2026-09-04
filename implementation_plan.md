# Implementační plán – Oprava zachování fokusu při přechodu do nadřazené složky

## Cíl
Při přechodu do nadřazeného adresáře (`..`) v SFTP pluginu zachovat fokus na opuštěnou složku v nadřazeném výpisu souborů a adresářů.

## Navržené změny

### `src/fs1.cpp`
V metodě `CPluginInterfaceForFS::ExecuteOnFS`:
- Při `isDir == 2` (přechod na `..`):
  - Normalizovat stávající cestu `fs->Path`.
  - Odříznout případné koncové lomítko.
  - Najít poslední lomítko a získat název opouštěné složky `focusName`.
  - Vypočítat novou cestu `SftpParent`.
  - Zavolat `SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newPath, NULL, -1, focusName[0] ? focusName : NULL)`.

## Verifikace
- Úspěšný překlad pluginu `mingw32-make -f Makefile.mingw CROSS_COMPILE=`.
- Ověření extrakce `focusName` a výpočtu cesty `SftpParent`.
