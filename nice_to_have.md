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

### 8. Hlavní heslo (Master Password) pro šifrování profilů
- **Popis**: Možnost zabezpečit uložené relace a hesla silnou šifrou (např. AES-256-GCM) chráněnou jedním Master heslem jako alternativa k DPAPI.
- **Přínos**: Bezpečnost při přenosu konfigurace mezi různými profily či počítači.

---
