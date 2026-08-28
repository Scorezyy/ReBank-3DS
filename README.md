# ReBank

ReBank is a Nintendo 3DS homebrew prototype with a versioned backend API intended to be reusable by a future Nintendo Switch client.

## Implemented

- Native Citro2D interface with stereoscopic animated intro and Pokemon preview
- 3DS system-language detection for English, German, French, Spanish, Italian, and Portuguese
- Username/password login, username/email/password registration, email password reset, and native software keyboard
- Argon2id password hashing, rate limits, short-lived JWT access tokens, and hashed rotating refresh tokens
- Native background login, registration, password reset, and automatic login from private CIA savedata
- Build-embedded client server address that cannot be changed from the installed app
- Client log console on `SELECT` plus a per-session `sdmc:/3ds/ReBank/rebank.log`
- Readable structured Windows server logs with passwords, tokens, and authorization headers redacted
- Generated OpenAPI 3.0 contract in `middleend/openapi.json`
- PKSM-Core-backed parsing for the 17 generation 4-7 main-series DS and 3DS games
- Direct reading of installed 3DS titles, CTR cartridges, and DS cartridge SPI saves
- Validated SD-export fallback under `sdmc:/3ds/ReBank/saves/`
- Real trainer name, trainer ID, play time, Pokedex count, box names, and Pokemon metadata
- D-pad and Circle Pad box cursor with single/multi selection tools and green transfer state
- Local and online 6x5 box views on the top screen with compact Pokemon details on the touchscreen
- Asynchronous transfer animation, PKHeX check status, and upload progress bar
- 50 cloud boxes for standard accounts and a 100-box database limit for special accounts
- Authenticated atomic batch uploads with fail-closed PKHeX legality checks through local-gpss
- AES-256-GCM Pokemon payload encryption at rest after every Pokemon in the batch passes legality
- Optional looping background music from `app/romfs/assets/music.ogg`
- Reproducible `.3dsx`, `.cia`, and `.3ds` packaging

## Not Yet Implemented

- Save write-back; game saves are currently opened read-only
- Alternate-form sprite indexing and rigged 3D Pokemon models
- Signed GitHub release checks and user-confirmed updates
- Password-reset email delivery

Automatic CIA installation is intentionally excluded from the prototype. It requires custom firmware privileges and must only happen after signature and SHA-256 verification with explicit user confirmation.

## Windows Setup

Requirements: Node.js 22+, Docker Desktop, and devkitPro installed at `C:\devkitPro`.

```powershell
.\tools\setup-windows.ps1
```

## Client Server Address

Edit `app/config/server.mk` before every release build:

```makefile
REBANK_SERVER_SCHEME := https
REBANK_SERVER_HOST := 88.99.242.28
REBANK_SERVER_PORT := 6969
```

These values are compiled into the executable. Users cannot change the server or port after installation. Use the Windows PC's LAN IPv4 address, not `127.0.0.1`, because the 3DS is a separate device.

The production client trusts the private ReBank CA embedded at `app/romfs/assets/rebank-ca.der`.
It does not fall back to unencrypted HTTP when the certificate is missing or invalid.

## Build

Install devkitPro with the `3ds-dev` group and `3ds-libvorbisidec`, then run:

```powershell
.\tools\package.ps1
```

Outputs are written to `output/`.

## Save Loading

Select the matching game while its cartridge is inserted or its digital copy is installed. ReBank
opens the save read-only and rejects a save whose detected generation does not match the selection.

For emulator testing or when direct archive permissions are unavailable, place an exported save at
`sdmc:/3ds/ReBank/saves/<game-code>/main` or `sdmc:/3ds/ReBank/saves/<game-code>.sav`. Game codes are
listed in `app/source/GameCatalog.cpp`.

## Storage Controls

- D-pad or Circle Pad: move the box cursor
- `A`: select with the active single/multi tool
- `Y`: switch between single and multi selection
- `L` / `R`: change the active local or online box
- Up from the first local row: stage selected Pokemon in the online box
- Down or `B`: cancel a staged transfer
- `X`: confirm the staged upload and start the PKHeX legitimacy check

Nothing is stored when one Pokemon is illegal or the legality service is unavailable.

## Server

```powershell
.\tools\start-server.ps1
```

This starts PostgreSQL and a pinned self-hosted FlagBrew local-gpss/PKHeX service, generates private local JWT/data-encryption secrets under ignored `Server/.runtime/`, builds the API, and opens the server log console. The first start builds the legality container and can take several minutes. Runtime bind address, API port, database URL, and legality URL are controlled by `Server/config/server.json`.

The PKHeX service is bound to `127.0.0.1:8080`; it is not exposed to the LAN. ReBank sends raw PK4-PK7 data to it only from the API server. Uploads fail closed if the service cannot return a valid legality result.

Stop PostgreSQL with:

```powershell
.\tools\stop-server.ps1
```

Swagger UI is available at `http://127.0.0.1:3000/documentation`. Client and server ports must match, but changing only `Server/config/server.json` does not alter an already built 3DS client.

## Production Deployment

The public API is available over pinned HTTPS at `https://88.99.242.28:6969`. nginx terminates TLS and proxies only to the API loopback listener. PostgreSQL listens only on `10.77.68.1:6968` inside a dedicated WireGuard tunnel whose only peer is the authentication VM. It is not reachable through the database server's public address.

Non-secret production templates are under `deploy/`. Environment files, WireGuard private keys, JWT secrets, database credentials, and TLS private keys must remain outside the repository.