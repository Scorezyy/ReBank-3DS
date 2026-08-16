# ReBank 3DS

ReBank is a Nintendo 3DS homebrew Pokemon bank client for generation 4 through generation 7 games.

## Features

- Native Citro2D interface with stereoscopic presentation
- Username-based authentication and automatic session refresh
- Direct save access for supported DS and 3DS titles
- PKSM-Core-backed PK4-PK7 parsing and generation-aware conversion
- Local and cloud box views with atomic, legality-checked transfers
- Verified save writes and cloud deletion only after local persistence
- English, German, French, Spanish, Italian, and Portuguese UI

## Security

Production requests use pinned HTTPS at `https://88.99.242.28:6969`. The trusted public ReBank CA is embedded in the ROMFS. The client fails closed if the certificate is missing or invalid and never falls back to plaintext HTTP.

No server source, database configuration, deployment topology, credentials, or private keys are included in this repository.

## Build

Requirements:

- devkitPro with the `3ds-dev` group
- `3ds-libvorbisidec`
- PowerShell on Windows for the packaging script

```powershell
.\tools\package.ps1
```

Release outputs are written to `output/` as `.3dsx`, `.cia`, and `.3ds` files.

The endpoint compiled into a build is configured in `app/config/server.mk`.

## Controls

- D-pad or Circle Pad: move the box cursor
- `A`: select with the active single/multi tool
- `Y`: switch between single and multi selection
- `L` / `R`: change the active local or cloud box
- Up from the first local row: stage selected Pokemon in the cloud box
- Down or `B`: cancel a staged transfer
- `X`: confirm a staged upload
- `SELECT`: open the in-app log console

The current session log is stored at `sdmc:/3ds/ReBank/rebank.log` and is replaced at the next app start.
