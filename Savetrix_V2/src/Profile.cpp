#include "Profile.h"

namespace Savetrix
{
    void to_json(nlohmann::json& j, const SkillState& v)
    {
        j = { { "level", v.level }, { "xp", v.xp }, { "levelThreshold", v.levelThreshold }, { "legendaryLevel", v.legendaryLevel } };
    }
    void from_json(const nlohmann::json& j, SkillState& v)
    {
        j.at("level").get_to(v.level);
        j.at("xp").get_to(v.xp);
        j.at("levelThreshold").get_to(v.levelThreshold);
        v.legendaryLevel = j.value("legendaryLevel", 0U);
    }

    void to_json(nlohmann::json& j, const PlayerStats& v)
    {
        j = {
            { "level", v.level },
            { "healthBase", v.healthBase },
            { "magickaBase", v.magickaBase },
            { "staminaBase", v.staminaBase },
            { "dragonSouls", v.dragonSouls },
            { "playerXp", v.playerXp },
            { "playerLevelThreshold", v.playerLevelThreshold },
            { "perkPoints", v.perkPoints }
        };
    }
    void from_json(const nlohmann::json& j, PlayerStats& v)
    {
        j.at("level").get_to(v.level);
        j.at("healthBase").get_to(v.healthBase);
        j.at("magickaBase").get_to(v.magickaBase);
        j.at("staminaBase").get_to(v.staminaBase);
        j.at("dragonSouls").get_to(v.dragonSouls);
        j.at("playerXp").get_to(v.playerXp);
        j.at("playerLevelThreshold").get_to(v.playerLevelThreshold);
        j.at("perkPoints").get_to(v.perkPoints);
    }

    void to_json(nlohmann::json& j, const PerkState& v)
    {
        j = {
            { "form", v.form },
            { "rank", v.rank },
            { "transferPolicy", v.transferPolicy },
            { "safetyReason", v.safetyReason }
        };
    }
    void from_json(const nlohmann::json& j, PerkState& v)
    {
        j.at("form").get_to(v.form);
        j.at("rank").get_to(v.rank);
        v.transferPolicy = j.value("transferPolicy", std::string{ "safe" });
        v.safetyReason = j.value("safetyReason", std::string{});
    }

    void to_json(nlohmann::json& j, const SpellState& v)
    {
        j = {
            { "form", v.form },
            { "transferPolicy", v.transferPolicy },
            { "safetyReason", v.safetyReason }
        };
    }
    void from_json(const nlohmann::json& j, SpellState& v)
    {
        j.at("form").get_to(v.form);
        v.transferPolicy = j.value("transferPolicy", std::string{ "safe" });
        v.safetyReason = j.value("safetyReason", std::string{});
    }

    void to_json(nlohmann::json& j, const WordState& v) { j = { { "form", v.form }, { "known", v.known } }; }
    void from_json(const nlohmann::json& j, WordState& v) { j.at("form").get_to(v.form); v.known = j.value("known", false); }

    void to_json(nlohmann::json& j, const ShoutState& v) { j = { { "form", v.form }, { "words", v.words } }; }
    void from_json(const nlohmann::json& j, ShoutState& v) { j.at("form").get_to(v.form); j.at("words").get_to(v.words); }

    void to_json(nlohmann::json& j, const InventoryState& v) { j = { { "form", v.form }, { "count", v.count } }; }
    void from_json(const nlohmann::json& j, InventoryState& v) { j.at("form").get_to(v.form); j.at("count").get_to(v.count); }

    void to_json(nlohmann::json& j, const QuestState& v)
    {
        j = {
            { "form", v.form },
            { "category", v.category },
            { "stage", v.stage },
            { "completed", v.completed },
            { "active", v.active },
            { "running", v.running },
            { "restorable", v.restorable }
        };
    }
    void from_json(const nlohmann::json& j, QuestState& v)
    {
        j.at("form").get_to(v.form);
        v.category = j.value("category", std::string{});
        v.stage = j.value("stage", static_cast<std::uint16_t>(0));
        v.completed = j.value("completed", false);
        v.active = j.value("active", false);
        v.running = j.value("running", false);
        v.restorable = j.value("restorable", false);
    }

    void to_json(nlohmann::json& j, const Profile& v)
    {
        j = {
            { "schemaVersion", v.schemaVersion },
            { "modVersion", v.modVersion },
            { "exportedAtUtc", v.exportedAtUtc },
            { "runtimeVersion", v.runtimeVersion },
            { "characterName", v.characterName },
            { "stats", v.stats },
            { "skills", v.skills },
            { "perks", v.perks },
            { "spells", v.spells },
            { "shouts", v.shouts },
            { "inventory", v.inventory },
            { "quests", v.quests }
        };
    }

    void from_json(const nlohmann::json& j, Profile& v)
    {
        j.at("schemaVersion").get_to(v.schemaVersion);
        if (v.schemaVersion != kSchemaVersion) {
            throw nlohmann::json::other_error::create(501, "unsupported Savetrix profile schema", &j);
        }
        v.modVersion = j.value("modVersion", std::string{});
        v.exportedAtUtc = j.value("exportedAtUtc", std::string{});
        v.runtimeVersion = j.value("runtimeVersion", std::string{});
        v.characterName = j.value("characterName", std::string{});
        j.at("stats").get_to(v.stats);
        j.at("skills").get_to(v.skills);
        v.perks = j.value("perks", std::vector<PerkState>{});
        v.spells = j.value("spells", std::vector<SpellState>{});
        v.shouts = j.value("shouts", std::vector<ShoutState>{});
        v.inventory = j.value("inventory", std::vector<InventoryState>{});
        v.quests = j.value("quests", std::vector<QuestState>{});
    }
}
