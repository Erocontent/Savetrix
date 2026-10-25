# Savetrix 2.3.1 Hotfix Implementation Plan

> Execute inline with superpowers:executing-plans; verify each change before publishing.

**Goal:** Address the reported loader, MCM/INI and feedback failures without changing migration policy.
**Architecture:** Preserve CommonLibSSE-NG and the existing MCM Helper configuration. Read settings with SimpleIni (already a dependency), export SKSE metadata, and use CommonLib's HUD message API. Keep changes on `hotfix/2.3.1`.
**Tech stack:** C++23, CommonLibSSE-NG 6.2.0, SimpleIni, CMake/MSVC, Python regression checks.
**Spec:** User request in this chat; no separate product specification.

## Constraints and evidence

- Main examined: 0352a09b51724901be590b4997503b6fbac716aa; PR #1: 7859cac2523b66376c4160d1b67521396c436242.
- Nexus displays 2.0 (archive uploaded 2026-08-22); source profile says 2.3.0; CMake/manifest say 2.0.0. Do not equate them.
- Reproduced on Windows: GetPrivateProfileIntW returns fallback for the first section of a UTF-8 BOM INI.
- Notify currently only logs. CommonLib 6.2.0 exposes SendHUDMessage::ShowHUDMessage, not DebugNotification.
- Preserve profile schema, selective import, duplicate protections and campaign restrictions.

## Task 1: Loader metadata and reproducible validation

Files: src/main.cpp, CMakeLists.txt, src/Version.h.in, src/Profile.h, vcpkg.json, scripts/verify_dll.py, tests/test_verify_dll.py, .github/workflows/build.yml.

- [x] Add negative PE verification tests (missing load/query/version, wrong version/flags/architecture).
- [x] Validate the PR's declaration against the pinned CommonLib headers and built binary.
- [x] Add metadata using the CMake version as the single source; keep PR attribution.
- [x] Run unit checks, Release build, and inspect actual PE exports/metadata.

## Task 2: MCM/INI and safe hotkeys

Files: src/Settings.h, src/InputSink.cpp, tests/settings_tests.cpp, tests/input_tests.cpp, tests/stubs/, scripts/test.ps1, CMakeLists.txt.

- [x] Prove failures for BOM first-section values, user precedence, missing/invalid values, disabled keys, collisions and live reload.
- [x] Replace Win32 profile reading with SimpleIni, preserving user-over-default precedence and safe fallbacks.
- [x] Block hotkeys during menus/remapping; accept keyboard only, including explicit unbinding.
- [x] Run tests with actual Settings and InputSink code; validate MCM IDs/defaults and ESP structure.

## Task 3: Feedback, package and release record

Files: src/TransferService.cpp, Data/MCM/Config/Savetrix/config.json, README.md, CHANGELOG.md, docs/HOTFIX_VALIDATION.md, scripts/build.ps1, scripts/package.ps1.

- [x] Make notifications visible through the supported HUD API; short English start/success/error messages without fixed F10/F11 assumptions.
- [x] Keep detailed counters in the log/report and show restart instructions after import.
- [x] Ensure build failure cannot be reported as success or package stale/missing exports.
- [x] Run checks/build/package; inspect packaged files and record exact revisions/hashes and manual Skyrim checks still pending.
- [x] Prepare a reviewable draft PR; do not publish a Nexus release without game validation.

## Review focus

BOM/Unicode paths, per-key overrides, both keys unbound, remapping while paused, invalid/missing DLL metadata, source/published divergence, and failed native commands in PowerShell.

## Execution ledger

- Baseline: sample profile validator passes; no native tests present.
- Ruling: fresh clone on a dedicated branch is sufficient isolation; no existing local edits.
- Ruling: continue implementation under the user's explicit authorization rather than requesting a separate plan approval.
- Task 1: complete locally. Full MSVC Release build and actual x64 PE metadata verification pass; historical main artifact fails the export check as expected.
- Task 2: complete. Settings/input RED on main, GREEN on candidate. MCM upstream schema validates with zero errors.
- Task 3: complete locally. HUD feedback and dialog compile; 9 Python tests, 2 native CTest executables, archive integrity/content checks pass. Draft PR #2 opened; game validation remains the release gate.
- Review ruling: added parent-scope spdlog discovery after independent review found a CMake imported-target scope issue.
- Verification ruling: do not reject ignored runtime-list defaults in Address Library mode; real CommonLib metadata contains 1.0.0 entries. Added a regression test.
- Ruling: keep PR #2 in draft and main/Nexus unchanged until Steam 1.6.1170 game validation; preserve downloaded build tools and local output for repeatable testing.

- Ruling: CTest is the native test entry point; a separate scripts/test.ps1 wrapper was unnecessary.
