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

### 9. Souběžné vícenásobné konexe (Per-instance CPluginFSInterface)
- **Popis**: Převést `CSftpConnection` a `CSftpProfile` z globálního singletonu na členské objekty `CPluginFSInterface` (podle vzoru vestavěného FTP pluginu v Salamanderu).
- **Přínos**: Umožní mít současně otevřené 2 a více různých SFTP serverů (např. v levém a pravém panelu, případně v různých tabech) a přenášet soubory mezi servery.

### 10. Barevné štítky profilů pro taby (Server Color Tags)
- **Popis**: Možnost přiřadit v nastavení připojení barvu profilu (např. červená pro produkci, oranžová pro staging, zelená pro dev/test) a při otevření relace automaticky obarvit příslušný tab v Samandarinu.
- **Přínos**: Okamžité vizuální rozlišení prostředí a prevence nechtěných zásahů na produkčních strojích.

### 11. Plně asynchronní nemodální přenosový dialog s během na pozadí (Phase B)
- **Popis**: Rozšíření přenosového dialogu o běh na pozadí (tlačítko 'Na pozadí' / 'Hide') a vyčlenění přenosů do dedikovaného pracovního vlákna (worker thread) s frontou úloh. Salamander panely zůstanou plně interaktivní i během stahování a nahrávání velkých objemů dat.
- **Závislost**: Vyžaduje per-instance konexe (viz bod 9), aby bylo možné při přenosu paralelně procházet a provádět operace ve vzdálených panelech bez blokování jediného SSH/SFTP socketu.

