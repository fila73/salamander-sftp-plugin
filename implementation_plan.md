# Implementační plán – Keepalive a detekce stavu spojení (Auto-Reconnect)

## Cíl
Zajistit spolehlivé udržení spojení se vzdáleným SFTP/SCP serverem při nečinnosti (ochrana proti timeoutu firewallů a NAT routerů), spolehlivou detekci pádu socketu a transparentní automatické znovupřipojení.

## Navržené a realizované změny

### 1. `src/sftpconn.h`
- Změna inline metody `IsConnected()` na plnohodnotnou metodu deklarovanou v hlavičkovém souboru a implementovanou v `sftpconn.cpp`.

### 2. `src/sftpconn.cpp`
- **TCP Keepalive**: V `Connect()` zapnut socket keepalive (`SO_KEEPALIVE`) a nastaveny parametry přes `SIO_KEEPALIVE_VALS` (15 s idle, 5 s probe interval).
- **SSH Keepalive**: V `Connect()` nakonfigurován SSH keepalive interval (`libssh2_keepalive_config(Session, 1, 15)`).
- **Detekce živosti socketu v `IsConnected()`**:
  - Kontrola platnosti handle socketu a relací.
  - Neblokující `select` na `efd` a `rfd` s `recv(MSG_PEEK)` k okamžité detekci vzdáleného ukončení spojení (FIN / RST / síťové chyby).
  - Odeslání periodického keepalive paketu přes `libssh2_keepalive_send(Session, nullptr)` a ověření funkčnosti transportní vrstvy.

### 3. `src/sftpglue.cpp`
- V `SftpEnsureConnected()`: Pokud `IsConnected()` detekuje odpojený socket, provede se znovunavázání spojení pomocí uloženého aktivního profilu (`SftpProfile`).

### 4. `Makefile.mingw` a audit čistoty závislostí
- Obnoven přepínač `-static` v `LDFLAGS`, který zajišťuje plně statické slinkování `libwinpthread.a` spolu s `libstdc++.a`, `libgcc.a` a `libssh2_static.a`. Tím byla odstraněna nechtěná dynamická závislost na `libwinpthread-1.dll`.
- Odstranění drobných varování identifikovaných statickou analýzou Cppcheck v `src/sftpconn.cpp` a `src/dialogs.cpp`.

## Verifikace
- Úspěšný překlad pluginu `mingw32-make -f Makefile.mingw CROSS_COMPILE=`.
- Kontrola importů DLL přes `objdump -p sftp.spl | Select-String "DLL Name"` – plugin závisí výhradně na standardních systémových DLL Windows a bundled `libcrypto-3-x64.dll`.
- Proveden běh linteru `Cppcheck 2.21.0` s `--enable=warning,performance,portability,style`.
- Sestavení a úspěšný běh unit testu `test/test_isconnected.cpp`.
