#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "FormRef.h"

namespace Savetrix
{
    inline constexpr std::uint32_t kSchemaVersion = 3;
    inline constexpr std::string_view kModVersion = "2.1.1";
    inline constexpr std::size_t kSkillCount = 18;

    struct SkillState
    {
        float level{ 15.0F };
        float xp{ 0.0F };
        float levelThreshold{ 0.0F };
        std::uint32_t legendaryLevel{ 0 };
    };

    struct PlayerStats
    {
        std::uint16_t level{ 1 };
        float healthBase{ 100.0F };
        float magickaBase{ 100.0F };
        float staminaBase{ 100.0F };
        float dragonSouls{ 0.0F };
        float playerXp{ 0.0F };
        float playerLevelThreshold{ 0.0F };
        std::uint8_t perkPoints{ 0 };
    };

    struct PerkState
    {
        FormRef form;
        std::int8_t rank{ 0 };
        std::string transferPolicy{ "safe" };
        std::string safetyReason;
    };

    struct SpellState
    {
        FormRef form;
        std::string transferPolicy{ "safe" };
        std::string safetyReason;
    };

    struct WordState
    {
        FormRef form;
        bool known{ false };
    };

    struct ShoutState
    {
        FormRef form;
        std::array<WordState, 3> words{};
    };

    struct InventoryState
    {
        FormRef form;
        std::int32_t count{ 0 };
    };

    struct QuestState
    {
        FormRef form;
        std::string category;
        std::uint16_t stage{ 0 };
        bool completed{ false };
        bool active{ false };
        bool running{ false };
        bool restorable{ false };
    };

    struct Profile
    {
        std::uint32_t schemaVersion{ kSchemaVersion };
        std::string modVersion{ kModVersion };
        std::string exportedAtUtc;
        std::string runtimeVersion;
        std::string characterName;

        PlayerStats stats;
        std::array<SkillState, kSkillCount> skills{};
        std::vector<PerkState> perks;
        std::vector<SpellState> spells;
        std::vector<ShoutState> shouts;
        std::vector<InventoryState> inventory;
        std::vector<QuestState> quests;
    };

    void to_json(nlohmann::json& a_json, const SkillState& a_value);
    void from_json(const nlohmann::json& a_json, SkillState& a_value);
    void to_json(nlohmann::json& a_json, const PlayerStats& a_value);
    void from_json(const nlohmann::json& a_json, PlayerStats& a_value);
    void to_json(nlohmann::json& a_json, const PerkState& a_value);
    void from_json(const nlohmann::json& a_json, PerkState& a_value);
    void to_json(nlohmann::json& a_json, const SpellState& a_value);
    void from_json(const nlohmann::json& a_json, SpellState& a_value);
    void to_json(nlohmann::json& a_json, const WordState& a_value);
    void from_json(const nlohmann::json& a_json, WordState& a_value);
    void to_json(nlohmann::json& a_json, const ShoutState& a_value);
    void from_json(const nlohmann::json& a_json, ShoutState& a_value);
    void to_json(nlohmann::json& a_json, const InventoryState& a_value);
    void from_json(const nlohmann::json& a_json, InventoryState& a_value);
    void to_json(nlohmann::json& a_json, const QuestState& a_value);
    void from_json(const nlohmann::json& a_json, QuestState& a_value);
    void to_json(nlohmann::json& a_json, const Profile& a_value);
    void from_json(const nlohmann::json& a_json, Profile& a_value);
}
