# 2.3.1 hotfix validation

## Provenance

- Main reviewed: `0352a09b51724901be590b4997503b6fbac716aa`.
- PR #1 reviewed: `7859cac2523b66376c4160d1b67521396c436242` (JackOfSpeed-01).
- Hotfix PR: https://github.com/Erocontent/Savetrix/pull/2.
- CommonLibSSE-NG `v6.2.0` resolved locally to `82e137ffb500d5208ea4694f5db881800dd63de5`.
- vcpkg baseline: `ee12231b20c95013c6638d845d04c91559a1d1ff`.
- Local compiler: MSVC 19.44.35229, Windows SDK 10.0.26100.0. Native Release configuration uses static dependencies and the dynamic CRT.

Nexus displays version **2.0**, last uploaded August 22, 2026, file ID **793573**, approximately 365 KB. Main's profile version was **2.3.0**, but CMake and the package manifest said **2.0.0**. There are no GitHub Releases/tags establishing equivalence with that Nexus archive. The historical main Actions build succeeded, but that is not evidence of which DLL was published. The Nexus archive itself has not been binary-compared; release identity remains unverified.

## Reports checked

Live Nexus comments page: https://www.nexusmods.com/skyrimspecialedition/mods/188579?tab=posts (39 comments at inspection).

- August 25 onward: Steam 1.6.1170, no hotkey response or profile folder; multiple confirmations.
- September 7: frij0les reports broken-looking MCM, no generated JSON, remaps/INI changes without effect and missing feedback.
- September 17: PR author reports a successful Steam 1.6.1170 smoke test with their metadata fix. This is contributor evidence, not a test independently performed here.
- September 24/25: another request for a working DLL.
- October 18: Jarrito6969 again requests a 1.6.1170 fix.
- Earlier August report: pressing export at the main menu overwrites a profile. The hotfix blocks transfers in the main menu and during loading/remapping.

Nexus times may be shown in different time zones. The dedicated Bugs tab has no reports; these reports are in Posts.

## Root causes and technical review

1. Main only exported `SKSEPlugin_Load`; it lacked discovery metadata. The PR's `SKSEPluginInfo` macro is supported by the pinned CommonLib and emits `SKSEPlugin_Version` plus `SKSEPlugin_Query`. The hotfix uses the same declaration with a consistent 2.3.1 version and contributor attribution. Address Library/independent structure flags match CommonLib NG's SE/AE support; these flags do not prove every runtime is tested.
2. Reproduced with the actual Windows profile API: a UTF-8 BOM preceding `[Controls]` makes `GetPrivateProfileIntW` return its fallback for both hotkeys. Later `[Import]` values remain readable. SimpleIni reads the same file correctly, including Unicode paths, and matches MCM Helper's encoding.
3. `Notify()` only wrote to the log. The previously removed `DebugNotification` call does not exist in the pinned library. Its supported API is `RE::SendHUDMessage::ShowHUDMessage`; the hotfix uses it and a completion dialog for restart instructions.
4. The MCM config had subpages but no main-page content. MCM Helper draws `MainPage` for initial empty-page selection, leaving the initial view empty. The corrected config also validates against MCM Helper's upstream JSON schema.
5. The original `x64-windows-static-md` preset is absent from the pinned vcpkg triplets. The candidate uses its existing `x64-windows-static-md-release` triplet.

An independent code review identified a parent CMake target visibility problem in the new tests; explicit parent `find_package(spdlog CONFIG REQUIRED)` fixes it.

## Automated checks

- RED: native settings test failed against main at the UTF-8 BOM override assertion.
- RED: native input test failed against main when a journal/MCM keypress exported.
- RED: upstream MCM schema rejected main's missing root content; candidate validates with zero errors.
- GREEN: native settings and input tests pass, including per-key precedence, Unicode paths, missing/malformed values, disabled keys, collisions, reload and menu/input filtering.
- GREEN: nine Python metadata/package contract tests and the legacy sample profile validator pass.
- GREEN: complete local MSVC Release build (507 build steps), then CTest: 2/2 native test executables passed.
- GREEN: real candidate PE inspection confirms x64, all three SKSE exports, 2.3.1 metadata and the expected Address Library/structure flags.
- RED: historical GitHub main artifact `9290466762` exports only `SKSEPlugin_Load` and is rejected for missing `SKSEPlugin_Query`. Its DLL SHA256 is `760807eb7055a58b0ff140e84094751271c5b0fe69f0527e35f7d18e27be9b11`. This artifact is not identified as the Nexus download.
- The first binary inspection also exposed an overly strict verifier assertion about unused runtime-list entries. CommonLib constructs these as 1.0.0; SKSE ignores them in Address Library mode. The verifier now tests the active compatibility flags and minimum SKSE version, with a regression test for these default entries.
- Candidate DLL SHA256: `17b938d7a7b0876591db46a8ad86a86056814fea3afce42d6818b6ee49f8ee1c`.
- Local archive integrity and seven required files checked; DLL and data files in the ZIP match the verified outputs. Static fmt/spdlog/DirectXTK dependencies do not require separate DLLs.

The local package is `Savetrix_V2_Release.zip`. Documentation changes may alter its ZIP hash; verify the DLL hash above when comparing the candidate binary. The GitHub workflow is a separate build and its DLL need not be byte-identical (different toolset/configuration/build metadata).

The profile sample validator covers the supplied legacy schema-2 fixture; it is not a comprehensive test of native export/import or the current schema-3 serializer.

## Required game validation before public release

Not performed here on Steam 1.6.1170. A successful build/PE inspection does not replace these checks:

1. Use SKSE64 and Address Library matching Steam 1.6.1170; verify `Savetrix.log` reports 2.3.1 and correct runtime.
2. Enable `Savetrix.esp`, SkyUI and MCM Helper; confirm initial and Migration pages draw correctly.
3. Change each keyboard binding, verify `Data/MCM/Settings/Savetrix.ini`, close menus and confirm the new key works. Selecting a key in MCM must not migrate anything.
4. Export a disposable source save; confirm start/completion HUD messages and a parseable profile. Under MO2, check Overwrite/output mod.
5. Back up a disposable destination save, disable Inventory, import and inspect `last_import_report.json`; verify inventory was not imported.
6. Confirm the completion dialog and repeat-import guard; save to a new slot, exit to desktop, restart via SKSE and reload.
7. Repeat export/import in additional fresh Skyrim processes; confirm main-menu/loading inputs cannot overwrite the source profile.

Companion quest/relationship state and new MCM export/import buttons are outside this hotfix.
