# Savetrix V2

**Savetrix** is an SKSE/CommonLibSSE-NG mod for **Skyrim Special Edition / Anniversary Edition** that exports a portable character profile to JSON and applies it to another save.

## What V2 transfers

### Character

- player level
- global XP / level-up threshold
- base Health, Magicka and Stamina
- Dragon Souls
- unspent perk points
- all 18 skills: level, XP, threshold and Legendary count
- player perks
- spells
- shouts and known words
- inventory by base item + quantity, including gold

### Campaign milestones

V2 also exports quest snapshots for official campaign-like quests from:

- Main Quest
- Companions
- College of Winterhold
- Thieves Guild
- Dark Brotherhood
- Dawnguard
- Dragonborn
- Civil War

Automatic restoration is intentionally conservative. V2 only attempts to replay **completed** whitelisted storyline milestones that are considered safe enough for automatic restoration.

Civil War is exported for reference but is not automatically restored. `MQ101`, `MQ102`, `MQ302` and `MQPaarthurnax` are also blocked from automatic replay.

V2 never regresses a destination quest that is already further ahead, and it does not attempt to rebuild quests that were still in progress in the source save.

## Controls

- **F10** — export the current character + campaign profile
- **F11** — import `profile.json` into the current save

The profile is stored at:

`Skyrim Special Edition/Data/SKSE/Plugins/Savetrix/profile.json`

## Safety model

Savetrix does **not** copy or directly edit `.ess` save files. It reads the loaded character, writes an external profile, and applies portable data to the destination save.

Inventory import is non-destructive: if the destination already contains at least the exported amount of an item, Savetrix does not remove or duplicate it. If it contains less, Savetrix only adds the difference.

Forms are stored using `plugin + local FormID`, with EditorID as fallback, which makes the profile more resilient to load-order changes. Runtime-generated/dynamic forms are ignored by V2.

The campaign layer is a **milestone replayer**, not a complete save-state clone. It does not claim to reconstruct every alias, Papyrus variable, NPC position, cell state, script state, or decision-specific world mutation.

## End-user requirements

1. Skyrim SE/AE.
2. SKSE64 matching the installed Skyrim runtime.
3. Address Library for SKSE Plugins matching the runtime.
4. `Savetrix.dll` installed at `Data/SKSE/Plugins/`.

## Basic usage

1. Load the source save and wait until the game is fully loaded.
2. Press **F10** and wait for the export notification.
3. Load or create the destination save.
4. Make a manual backup save before the first import.
5. Press **F11**.
6. Make a new manual save and reload it after the import.
7. Check `Documents/My Games/Skyrim Special Edition/SKSE/Savetrix.log` if something was skipped or missing.

## Build on Windows

Requirements:

- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.24+
- Git
- vcpkg
- `VCPKG_ROOT` configured

PowerShell:

```powershell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
.\scripts\build.ps1
```

Expected DLL:

`build/dist/Data/SKSE/Plugins/Savetrix.dll`

Create the installable package with:

```powershell
.\scripts\package.ps1
```

Expected package:

`Savetrix_V2_Release.zip`

## GitHub Actions

The repository already contains:

`.github/workflows/build.yml`

The workflow builds on Windows Server 2022 using MSVC, validates the profile schema, verifies `Savetrix.dll`, packages the mod, and publishes two GitHub Actions artifacts:

- `Savetrix-V2-Windows`
- `Savetrix-V2-DLL`

You can run it manually from **Actions → Build Savetrix V2 → Run workflow**.

## Source compatibility

The project pins **CommonLibSSE-NG v6.2.0**, enables Skyrim SE and AE targets, and disables VR.

## Profile format

See `docs/PROFILE_SCHEMA.md` and `sample/profile.example.json`.

## License

GPL-3.0-or-later. See `LICENSE`.
