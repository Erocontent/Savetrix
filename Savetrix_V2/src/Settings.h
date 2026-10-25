#pragma once

#include <chrono>
#include <algorithm>
#include <charconv>
#include <optional>
#include <string_view>
#include <cstdint>
#include <filesystem>

#include <Windows.h>
#include <dinput.h>

#include <spdlog/spdlog.h>
#include <SimpleIni.h>

#include "Paths.h"

namespace Savetrix
{
    struct ImportOptions
    {
        bool stats{ true };
        bool skills{ true };
        bool perks{ true };
        bool spells{ true };
        bool shouts{ true };
        bool inventory{ true };
        bool quests{ true };

        [[nodiscard]] bool AnyEnabled() const noexcept
        {
            return stats || skills || perks || spells || shouts || inventory || quests;
        }
    };

    struct SettingsSnapshot
    {
        std::uint32_t exportKey{ DIK_F10 };
        std::uint32_t importKey{ DIK_F11 };
        ImportOptions import;
    };

    class Settings
    {
    public:
        static Settings& GetSingleton()
        {
            static Settings singleton;
            return singleton;
        }

        [[nodiscard]] SettingsSnapshot GetSnapshot()
        {
            RefreshIfNeeded();
            return values_;
        }

        void ForceReload()
        {
            loaded_ = false;
            RefreshIfNeeded(true);
        }

    private:
        Settings() = default;

        static std::optional<int> ReadIniInt(const std::filesystem::path& a_path, const char* a_section, const char* a_key)
        {
            CSimpleIniA ini;
            ini.SetUnicode();
            // MCM Helper writes UTF-8, optionally with a BOM. Win32 profile APIs
            // interpret that BOM as part of the first section name.
            if (a_path.empty() || ini.LoadFile(a_path.c_str()) < 0) {
                return std::nullopt;
            }
            const auto* text = ini.GetValue(a_section, a_key, nullptr);
            if (!text) return std::nullopt;
            const std::string_view value(text);
            int result{};
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
            if (error != std::errc{} || end != value.data() + value.size()) return std::nullopt;
            return result;
        }

        static int ReadMergedInt(const char* a_section, const char* a_key, int a_default)
        {
            const auto value = ReadIniInt(Paths::McmDefaultsPath(), a_section, a_key).value_or(a_default);
            return ReadIniInt(Paths::McmUserSettingsPath(), a_section, a_key).value_or(value);
        }

        static bool ReadMergedBool(const char* a_section, const char* a_key, bool a_default)
        {
            return ReadMergedInt(a_section, a_key, a_default ? 1 : 0) != 0;
        }

        void RefreshIfNeeded(bool a_force = false)
        {
            const auto now = std::chrono::steady_clock::now();
            if (!a_force && loaded_ && now - lastCheck_ < std::chrono::milliseconds(500)) {
                return;
            }
            lastCheck_ = now;

            SettingsSnapshot next;
            const auto exportKey = ReadMergedInt("Controls", "iExportKey", DIK_F10);
            const auto importKey = ReadMergedInt("Controls", "iImportKey", DIK_F11);

            next.exportKey = (exportKey >= -1 && exportKey <= 255) ? static_cast<std::uint32_t>(std::max(0, exportKey)) : DIK_F10;
            next.importKey = (importKey >= -1 && importKey <= 255) ? static_cast<std::uint32_t>(std::max(0, importKey)) : DIK_F11;
            if (next.exportKey != 0 && next.exportKey == next.importKey) {
                spdlog::warn("Savetrix settings: export/import hotkeys collide; restoring F10/F11 defaults");
                next.exportKey = DIK_F10;
                next.importKey = DIK_F11;
            }

            next.import.stats = ReadMergedBool("Import", "bStats", true);
            next.import.skills = ReadMergedBool("Import", "bSkills", true);
            next.import.perks = ReadMergedBool("Import", "bPerks", true);
            next.import.spells = ReadMergedBool("Import", "bSpells", true);
            next.import.shouts = ReadMergedBool("Import", "bShouts", true);
            next.import.inventory = ReadMergedBool("Import", "bInventory", true);
            next.import.quests = ReadMergedBool("Import", "bQuests", true);

            values_ = next;
            loaded_ = true;
        }

        SettingsSnapshot values_{};
        bool loaded_{ false };
        std::chrono::steady_clock::time_point lastCheck_{};
    };
}
