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

## Verifikace
- Úspěšný překlad pluginu `mingw32-make -f Makefile.mingw CROSS_COMPILE=`.
- Sestavení a úspěšný běh unit testu `test/test_isconnected.cpp` (ověření stavů před připojením i po neúspěšném pokusu o spojení).
