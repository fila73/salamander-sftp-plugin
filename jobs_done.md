# Zpráva o realizaci (Jobs Done) – SFTP Plugin

Dokument rekapituluje realizované úkoly a opravy v pluginu **SFTP/SCP pro Open Salamander**.

---

## 📋 Přehled realizovaných úkolů

| Oblast | Popis řešení | Klíčové soubory |
|---|---|---|
| **Udržování spojení & Auto-reconnect** | Zavedení TCP keepalive sond (15s idle / 5s interval) a SSH keepalive pakety. Implementace přesné detekce živosti socketu v `IsConnected()` (`select` + `MSG_PEEK`) a transparentní reconnect v `SftpEnsureConnected`. | `src/sftpconn.cpp`, `src/sftpconn.h`, `src/sftpglue.cpp` |
| **Navigace & Focus na `..`** | Při opuštění složky přes `..` (`isDir == 2`) se extrahuje název opouštěné podsložky a předá se jako `suggestedFocusName` do `ChangePanelPathToPluginFS`. Salamander tak správně vyfokusuje složku v nadřazeném výpisu. | `src/fs1.cpp` |
| **Protokoly SFTP & SCP** | Plná podpora SFTP v3 a SCP přenosů přes libssh2 + OpenSSL. | `src/sftpconn.cpp`, `src/fs2.cpp` |
| **Přihlašovací dialog** | Správa uložených relací, profily připojení, šifrování hesel. | `src/fs1.cpp`, `src/sftpglue.cpp` |
| **Příkazová konzole** | Spouštění příkazů na serveru s reálným výstupem v okně konzole. | `src/dialogs.cpp`, `src/menu.cpp` |
