#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>

#include <Windows.h>
#include <dinput.h>

#include <spdlog/spdlog.h>

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

        static int ReadIniInt(const std::filesystem::path& a_path, const wchar_t* a_section, const wchar_t* a_key, int a_fallback)
        {
            if (a_path.empty() || !std::filesystem::exists(a_path)) {
                return a_fallback;
            }
            return static_cast<int>(::GetPrivateProfileIntW(a_section, a_key, a_fallback, a_path.c_str()));
        }

        static int ReadMergedInt(const wchar_t* a_section, const wchar_t* a_key, int a_default)
        {
            auto value = ReadIniInt(Paths::McmDefaultsPath(), a_section, a_key, a_default);
            const auto user = ReadIniInt(Paths::McmUserSettingsPath(), a_section, a_key, -1);
            if (user >= 0) {
                value = user;
            }
            return value;
        }

        static bool ReadMergedBool(const wchar_t* a_section, const wchar_t* a_key, bool a_default)
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
            const auto exportKey = ReadMergedInt(L"Controls", L"iExportKey", DIK_F10);
            const auto importKey = ReadMergedInt(L"Controls", L"iImportKey", DIK_F11);

            next.exportKey = (exportKey >= 0 && exportKey <= 255) ? static_cast<std::uint32_t>(exportKey) : DIK_F10;
            next.importKey = (importKey >= 0 && importKey <= 255) ? static_cast<std::uint32_t>(importKey) : DIK_F11;
            if (next.exportKey == next.importKey) {
                spdlog::warn("Savetrix settings: export/import hotkeys collide; restoring F10/F11 defaults");
                next.exportKey = DIK_F10;
                next.importKey = DIK_F11;
            }

            next.import.stats = ReadMergedBool(L"Import", L"bStats", true);
            next.import.skills = ReadMergedBool(L"Import", L"bSkills", true);
            next.import.perks = ReadMergedBool(L"Import", L"bPerks", true);
            next.import.spells = ReadMergedBool(L"Import", L"bSpells", true);
            next.import.shouts = ReadMergedBool(L"Import", L"bShouts", true);
            next.import.inventory = ReadMergedBool(L"Import", L"bInventory", true);
            next.import.quests = ReadMergedBool(L"Import", L"bQuests", true);

            values_ = next;
            loaded_ = true;
        }

        SettingsSnapshot values_{};
        bool loaded_{ false };
        std::chrono::steady_clock::time_point lastCheck_{};
    };
}
