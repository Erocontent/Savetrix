# Changelog

## 2.3.1 (hotfix candidate)

- Restored SKSE plugin discovery metadata for AE, including Steam 1.6.1170 (based on PR #1 by JackOfSpeed-01).
- Fixed reading MCM Helper UTF-8 BOM settings, including remapped and unbound keyboard hotkeys.
- Added content to the initial MCM page and blocked hotkeys while menus are open.
- Added visible export/import progress and result notifications, plus post-import restart instructions.
- Unified DLL/profile/build versioning and added settings, input, package and DLL metadata checks.

This candidate still requires validation inside Skyrim before a public release. Migration safety policies and profile schema are unchanged.

## 2.0.0

- Added profile schema V2 with quest snapshots.
- Added automatic campaign discovery for official Skyrim/Dawnguard/Dragonborn quest categories.
- Added conservative storyline whitelist for Main Quest, College, Thieves Guild, Dark Brotherhood, Companions, Dawnguard and Dragonborn.
- Added completed-milestone replay through Skyrim's quest stage command path.
- Added no-regression checks for destination quest progress.
- Added protection for in-progress quests, Civil War, tutorial-sensitive main quests, Season Unending and Paarthurnax.
- Preserved V1 character transfer behavior and non-destructive inventory import.
- Added Windows GitHub Actions build and installable ZIP artifact.
