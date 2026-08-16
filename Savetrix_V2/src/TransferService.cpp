#include "TransferService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <RE/I/InventoryEntryData.h>
#include <REL/Relocation.h>
#include <spdlog/spdlog.h>

#include "CampaignTransfer.h"
#include "FormRef.h"
#include "Paths.h"
#include "Profile.h"

namespace
{
    std::string TimestampUtc()
    {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        gmtime_s(&tm, &time);
        std::ostringstream out;
        out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return out.str();
    }

    std::string FormKey(const Savetrix::FormRef& a_ref)
    {
        return a_ref.plugin + ":" + std::to_string(a_ref.localFormID) + ":" + a_ref.editorID;
    }

    bool Portable(const RE::TESForm* a_form)
    {
        return a_form && !a_form->IsDynamicForm() && a_form->GetFile(0) != nullptr;
    }

    bool IsNonPlayable(const RE::TESForm* a_form)
    {
        return a_form &&
               (a_form->GetFormFlags() & RE::TESForm::RecordFlags::kNonPlayable) != 0;
    }

    enum class TransferDisposition
    {
        kSafe,
        kSuspicious,
        kSkip
    };

    struct TransferClassification
    {
        TransferDisposition disposition{ TransferDisposition::kSkip };
        std::string reason;
    };

    struct PreflightReport
    {
        bool coreDataValid{ true };
        bool mainQuestReady{ true };

        std::size_t perksSafe{};
        std::size_t perksSuspicious{};
        std::size_t perksUnsafe{};
        std::size_t spellsSafe{};
        std::size_t spellsSuspicious{};
        std::size_t spellsUnsafe{};
        std::size_t shoutsReady{};
        std::size_t inventoryReady{};
        std::size_t inventoryUnsafe{};
        std::size_t questCandidates{};
        std::size_t mainQuestCandidates{};
        std::size_t missingForms{};
        std::set<std::string> missingPlugins;
    };

    std::string Lower(std::string_view a_value)
    {
        std::string out(a_value);
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return out;
    }

    bool ContainsAny(std::string_view a_text, const std::initializer_list<std::string_view>& a_tokens)
    {
        const auto lower = Lower(a_text);
        for (const auto token : a_tokens) {
            if (lower.find(token) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    template <class T>
    std::string ClassificationText(const T* a_form)
    {
        if (!a_form) {
            return {};
        }

        std::string text;
        if (const auto* name = a_form->GetName(); name && *name) {
            text += name;
        }
        if (const auto* editor = a_form->GetFormEditorID(); editor && *editor) {
            if (!text.empty()) {
                text.push_back(' ');
            }
            text += editor;
        }
        return text;
    }

    bool IsOfficialPlugin(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return false;
        }
        const auto* file = a_form->GetFile(0);
        if (!file) {
            return false;
        }
        const auto filename = file->GetFilename();
        if (filename.empty()) {
            return false;
        }
        const auto plugin = Lower(filename);
        return plugin == "skyrim.esm" ||
               plugin == "update.esm" ||
               plugin == "dawnguard.esm" ||
               plugin == "hearthfires.esm" ||
               plugin == "dragonborn.esm";
    }

    TransferClassification ClassifyPerk(const RE::BGSPerk* a_perk)
    {
        if (!Portable(a_perk)) {
            return { TransferDisposition::kSkip, "non_portable" };
        }
        if (!a_perk->data.playable || a_perk->data.hidden || IsNonPlayable(a_perk)) {
            return { TransferDisposition::kSkip, "non_playable_or_hidden" };
        }

        const auto text = ClassificationText(a_perk);
        if (text.empty()) {
            return { TransferDisposition::kSuspicious, "unnamed_playable_perk" };
        }

        if (!IsOfficialPlugin(a_perk) &&
            ContainsAny(text, {
                "controller",
                "manager",
                "handler",
                "framework",
                "internal",
                "dummy",
                "debug",
                "monitor",
                "tracking",
                "suppression perk",
                "animation perk",
                "interaction icons",
                "event perk",
                "state perk",
                "surrender perk",
                "greeting perk",
                "enchanting mult",
                "multiplier",
                "fake skill",
                "habilidade falsa"
            })) {
            return { TransferDisposition::kSuspicious, "technical_name_pattern" };
        }

        return { TransferDisposition::kSafe, {} };
    }

    TransferClassification ClassifySpell(const RE::SpellItem* a_spell)
    {
        if (!Portable(a_spell)) {
            return { TransferDisposition::kSkip, "non_portable" };
        }
        if (IsNonPlayable(a_spell)) {
            return { TransferDisposition::kSkip, "non_playable" };
        }

        switch (a_spell->GetSpellType()) {
        case RE::MagicSystem::SpellType::kSpell:
        case RE::MagicSystem::SpellType::kPower:
        case RE::MagicSystem::SpellType::kLesserPower:
            break;
        default:
            return { TransferDisposition::kSkip, "unsupported_spell_type" };
        }

        const auto text = ClassificationText(a_spell);
        if (text.empty()) {
            return { TransferDisposition::kSuspicious, "unnamed_spell" };
        }

        if (!IsOfficialPlugin(a_spell) &&
            ContainsAny(text, {
                "apply ",
                "(target)",
                "(self)",
                "controller",
                "manager",
                "handler",
                "framework",
                "internal",
                "dummy",
                "debug",
                "test spell",
                "monitor",
                "tracking",
                "initializer",
                "initialize ",
                "bootstrap",
                "refresh spell",
                "reset spell",
                "switchpower",
                "switch power",
                "options:",
                "preset",
                "configuration",
                "config power",
                "settings power",
                "setup power",
                "utility power"
            })) {
            return { TransferDisposition::kSuspicious, "technical_name_pattern" };
        }

        return { TransferDisposition::kSafe, {} };
    }

    bool IsSafePolicy(std::string_view a_policy)
    {
        return a_policy == "safe";
    }

    bool SafeTransferInventoryItem(
        const RE::TESBoundObject* a_object,
        const RE::InventoryEntryData* a_entry)
    {
        if (!Portable(a_object) || !a_object->IsInventoryObject() || IsNonPlayable(a_object)) {
            return false;
        }

        if (a_object->IsKey() || (a_entry && a_entry->IsQuestObject())) {
            return false;
        }

        const char* name = a_object->GetName();
        return name && *name;
    }

    bool FiniteNonNegative(float a_value)
    {
        return std::isfinite(a_value) && a_value >= 0.0F;
    }

    bool CoreProfileDataValid(const Savetrix::Profile& a_profile)
    {
        if (a_profile.stats.level == 0 ||
            !std::isfinite(a_profile.stats.healthBase) || a_profile.stats.healthBase <= 0.0F ||
            !FiniteNonNegative(a_profile.stats.magickaBase) ||
            !FiniteNonNegative(a_profile.stats.staminaBase) ||
            !FiniteNonNegative(a_profile.stats.dragonSouls) ||
            !FiniteNonNegative(a_profile.stats.playerXp) ||
            !FiniteNonNegative(a_profile.stats.playerLevelThreshold)) {
            return false;
        }

        for (const auto& skill : a_profile.skills) {
            if (!std::isfinite(skill.level) ||
                !FiniteNonNegative(skill.xp) ||
                !FiniteNonNegative(skill.levelThreshold)) {
                return false;
            }
        }
        return true;
    }

    void RecordMissing(PreflightReport& a_report, const Savetrix::FormRef& a_ref)
    {
        ++a_report.missingForms;
        if (!a_ref.plugin.empty()) {
            a_report.missingPlugins.insert(a_ref.plugin);
        }
    }

    bool MainQuestBootstrapReady()
    {
        auto* mq101Form = RE::TESForm::LookupByEditorID("MQ101");
        auto* mq102Form = RE::TESForm::LookupByEditorID("MQ102");
        auto* mq101 = mq101Form ? mq101Form->As<RE::TESQuest>() : nullptr;
        auto* mq102 = mq102Form ? mq102Form->As<RE::TESQuest>() : nullptr;
        if (!mq101 || !mq102) {
            return false;
        }

        const bool helgenResolved = mq101->IsCompleted();
        const bool beforeStormInitialized =
            mq102->IsCompleted() || mq102->IsRunning() || mq102->GetCurrentStageID() > 0;
        return helgenResolved && beforeStormInitialized;
    }

    PreflightReport RunPreflight(const Savetrix::Profile& a_profile)
    {
        PreflightReport report;
        report.coreDataValid = CoreProfileDataValid(a_profile);

        for (const auto& perkState : a_profile.perks) {
            auto* perk = Savetrix::ResolveFormAs<RE::BGSPerk>(perkState.form);
            if (!perk) {
                RecordMissing(report, perkState.form);
                continue;
            }

            const auto classification = ClassifyPerk(perk);
            if (classification.disposition == TransferDisposition::kSkip) {
                ++report.perksUnsafe;
            } else if (!IsSafePolicy(perkState.transferPolicy) ||
                       classification.disposition == TransferDisposition::kSuspicious) {
                ++report.perksSuspicious;
            } else {
                ++report.perksSafe;
            }
        }

        for (const auto& spellState : a_profile.spells) {
            auto* spell = Savetrix::ResolveFormAs<RE::SpellItem>(spellState.form);
            if (!spell) {
                RecordMissing(report, spellState.form);
                continue;
            }

            const auto classification = ClassifySpell(spell);
            if (classification.disposition == TransferDisposition::kSkip) {
                ++report.spellsUnsafe;
            } else if (!IsSafePolicy(spellState.transferPolicy) ||
                       classification.disposition == TransferDisposition::kSuspicious) {
                ++report.spellsSuspicious;
            } else {
                ++report.spellsSafe;
            }
        }

        for (const auto& shoutState : a_profile.shouts) {
            auto* shout = Savetrix::ResolveFormAs<RE::TESShout>(shoutState.form);
            if (!shout) {
                RecordMissing(report, shoutState.form);
                continue;
            }
            ++report.shoutsReady;

            for (const auto& wordState : shoutState.words) {
                if (!wordState.known || wordState.form.empty()) {
                    continue;
                }
                if (!Savetrix::ResolveFormAs<RE::TESWordOfPower>(wordState.form)) {
                    RecordMissing(report, wordState.form);
                }
            }
        }

        for (const auto& itemState : a_profile.inventory) {
            if (itemState.count <= 0) {
                continue;
            }

            auto* object = Savetrix::ResolveFormAs<RE::TESBoundObject>(itemState.form);
            if (!object) {
                RecordMissing(report, itemState.form);
                continue;
            }
            if (!SafeTransferInventoryItem(object, nullptr)) {
                ++report.inventoryUnsafe;
            } else {
                ++report.inventoryReady;
            }
        }

        for (const auto& questState : a_profile.quests) {
            if (!questState.completed || !questState.restorable) {
                continue;
            }

            ++report.questCandidates;
            if (questState.category == "main") {
                ++report.mainQuestCandidates;
            }
            if (!Savetrix::ResolveFormAs<RE::TESQuest>(questState.form)) {
                RecordMissing(report, questState.form);
            }
        }

        if (report.mainQuestCandidates > 0) {
            report.mainQuestReady = MainQuestBootstrapReady();
        }

        return report;
    }

    void LogPreflight(const PreflightReport& a_report)
    {
        spdlog::info(
            "Preflight: coreValid={}, perksSafe={}, perksSuspicious={}, perksUnsafe={}, spellsSafe={}, spellsSuspicious={}, spellsUnsafe={}, shoutsReady={}, inventoryReady={}, inventoryUnsafe={}, questCandidates={}, mainQuestCandidates={}, mainQuestReady={}, missingForms={}, missingPlugins={}",
            a_report.coreDataValid,
            a_report.perksSafe,
            a_report.perksSuspicious,
            a_report.perksUnsafe,
            a_report.spellsSafe,
            a_report.spellsSuspicious,
            a_report.spellsUnsafe,
            a_report.shoutsReady,
            a_report.inventoryReady,
            a_report.inventoryUnsafe,
            a_report.questCandidates,
            a_report.mainQuestCandidates,
            a_report.mainQuestReady,
            a_report.missingForms,
            a_report.missingPlugins.size());
    }

    bool QueueSafeHudRefresh()
    {
        const auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            spdlog::warn("Post-import HUD refresh: SKSE task interface unavailable");
            return false;
        }

        taskInterface->AddUITask([]() {
            auto* queue = RE::UIMessageQueue::GetSingleton();
            if (!queue) {
                spdlog::warn("Post-import HUD refresh: UIMessageQueue unavailable");
                return;
            }

            const RE::BSFixedString hudMenuName{ RE::HUDMenu::MENU_NAME };
            queue->AddMessage(hudMenuName, RE::UI_MESSAGE_TYPE::kUpdate, nullptr);
            spdlog::info("Post-import HUD refresh: queued HUD Menu kUpdate");
        });
        return true;
    }

    void WriteImportReport(
        const Savetrix::Profile& a_profile,
        const PreflightReport& a_preflight,
        const Savetrix::ImportReport& a_report)
    {
        try {
            std::vector<std::string> missingPlugins(
                a_preflight.missingPlugins.begin(),
                a_preflight.missingPlugins.end());

            nlohmann::json json = {
                { "reportVersion", 1 },
                { "savetrixVersion", std::string(Savetrix::kModVersion) },
                { "generatedAtUtc", TimestampUtc() },
                { "sourceCharacter", a_profile.characterName },
                { "sourceRuntime", a_profile.runtimeVersion },
                { "targetRuntime", REL::Module::get().version().string() },
                { "preflight", {
                    { "coreDataValid", a_preflight.coreDataValid },
                    { "perksSafe", a_preflight.perksSafe },
                    { "perksSuspicious", a_preflight.perksSuspicious },
                    { "perksUnsafe", a_preflight.perksUnsafe },
                    { "spellsSafe", a_preflight.spellsSafe },
                    { "spellsSuspicious", a_preflight.spellsSuspicious },
                    { "spellsUnsafe", a_preflight.spellsUnsafe },
                    { "shoutsReady", a_preflight.shoutsReady },
                    { "inventoryReady", a_preflight.inventoryReady },
                    { "inventoryUnsafe", a_preflight.inventoryUnsafe },
                    { "questCandidates", a_preflight.questCandidates },
                    { "mainQuestCandidates", a_preflight.mainQuestCandidates },
                    { "mainQuestReady", a_preflight.mainQuestReady },
                    { "missingForms", a_preflight.missingForms },
                    { "missingPlugins", missingPlugins }
                } },
                { "import", {
                    { "perksAdded", a_report.perksAdded },
                    { "spellsAdded", a_report.spellsAdded },
                    { "suspiciousPerksSkipped", a_report.suspiciousPerksSkipped },
                    { "suspiciousSpellsSkipped", a_report.suspiciousSpellsSkipped },
                    { "shoutsAdded", a_report.shoutsAdded },
                    { "wordsUnlocked", a_report.wordsUnlocked },
                    { "inventoryStacksAdded", a_report.inventoryStacksAdded },
                    { "missingForms", a_report.missingForms },
                    { "unsafeFormsSkipped", a_report.unsafeFormsSkipped },
                    { "questsRestored", a_report.questsRestored },
                    { "questsAlreadyComplete", a_report.questsAlreadyComplete },
                    { "questsAlreadyAhead", a_report.questsAlreadyAhead },
                    { "questsSkippedInProgress", a_report.questsSkippedInProgress },
                    { "questsSkippedUnsafe", a_report.questsSkippedUnsafe },
                    { "mainQuestDeferred", a_report.mainQuestDeferred },
                    { "questsMissing", a_report.questsMissing },
                    { "questsFailed", a_report.questsFailed },
                    { "hudRefreshQueued", a_report.hudRefreshQueued }
                } },
                { "recommendation", "save_and_reload" }
            };

            std::filesystem::create_directories(Savetrix::Paths::ProfileDirectory());
            const auto target = Savetrix::Paths::ProfileDirectory() / "last_import_report.json";
            const auto temp = target.string() + ".tmp";
            {
                std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                if (!out) {
                    throw std::runtime_error("cannot open temporary import report");
                }
                out << json.dump(2) << '\n';
                out.flush();
                if (!out) {
                    throw std::runtime_error("cannot write import report");
                }
            }

            std::error_code ec;
            std::filesystem::remove(target, ec);
            ec.clear();
            std::filesystem::rename(temp, target, ec);
            if (ec) {
                throw std::system_error(ec, "cannot replace import report");
            }

            spdlog::info("Import report written to {}", target.string());
        } catch (const std::exception& e) {
            spdlog::warn("Could not write import report: {}", e.what());
        }
    }

    RE::TESNPC* GetPlayerBase(RE::PlayerCharacter* a_player)
    {
        return a_player ? a_player->GetActorBase() : nullptr;
    }

    RE::ActorValueOwner* GetPlayerActorValueOwner(RE::PlayerCharacter* a_player)
    {
        // ActorValueOwner is a secondary base whose offset changes between Skyrim runtimes.
        // CommonLibSSE-NG's runtime accessor must be used in a cross-runtime plugin.
        return a_player ? a_player->AsActorValueOwner() : nullptr;
    }

    RE::PlayerCharacter::PlayerSkills::Data* GetSkillData(RE::PlayerCharacter* a_player)
    {
        if (!a_player) {
            return nullptr;
        }
        auto& info = a_player->GetInfoRuntimeData();
        return info.skills ? info.skills->data : nullptr;
    }

    std::uint16_t ReadPlayerLevel(RE::PlayerCharacter* a_player, RE::TESNPC* a_base)
    {
        if (!a_player || !a_base) {
            return 1;
        }

        const auto actorLevel = a_player->GetLevel();
        const auto calcLevel = a_player->GetCalcLevel(false);
        const auto baseLevel = a_base->actorData.level;

        auto level = actorLevel;
        if (level <= 1) {
            level = std::max({ actorLevel, calcLevel, baseLevel });
        }

        spdlog::info(
            "Export level sources: Actor::GetLevel()={}, GetCalcLevel(false)={}, actorData.level={}, selected={}",
            actorLevel,
            calcLevel,
            baseLevel,
            level);

        return std::clamp<std::uint16_t>(level, 1, 65535);
    }

    std::optional<std::int8_t> FindPerkRank(RE::PlayerCharacter* a_player, RE::BGSPerk* a_perk)
    {
        if (!a_player || !a_perk) {
            return std::nullopt;
        }
        std::optional<std::int8_t> rank;
        if (auto* base = a_player->GetActorBase()) {
            for (std::uint32_t i = 0; i < base->perkCount; ++i) {
                const auto& entry = base->perks[i];
                if (entry.perk == a_perk) {
                    rank = rank ? std::max(*rank, entry.currentRank) : entry.currentRank;
                }
            }
        }
        for (auto* entry : a_player->GetPlayerRuntimeData().addedPerks) {
            if (entry && entry->perk == a_perk) {
                rank = rank ? std::max(*rank, entry->currentRank) : entry->currentRank;
            }
        }
        return rank;
    }
}

namespace Savetrix
{
    TransferService& TransferService::GetSingleton()
    {
        static TransferService singleton;
        return singleton;
    }

    void TransferService::Notify(const std::string& a_text)
    {
        spdlog::info("{}", a_text);
    }

    bool TransferService::ExportCurrentCharacter()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* base = GetPlayerBase(player);
        auto* actorValues = GetPlayerActorValueOwner(player);
        auto* skillData = GetSkillData(player);
        if (!player || !base || !actorValues || !skillData) {
            spdlog::error("Export failed: player data unavailable");
            Notify("Savetrix: falha ao ler o personagem.");
            return false;
        }

        Profile profile;
        profile.exportedAtUtc = TimestampUtc();
        profile.runtimeVersion = REL::Module::get().version().string();
        profile.characterName = player->GetName();
        profile.stats.level = ReadPlayerLevel(player, base);
        spdlog::info("Export checkpoint: reading actor values");
        profile.stats.healthBase = actorValues->GetBaseActorValue(RE::ActorValue::kHealth);
        profile.stats.magickaBase = actorValues->GetBaseActorValue(RE::ActorValue::kMagicka);
        profile.stats.staminaBase = actorValues->GetBaseActorValue(RE::ActorValue::kStamina);
        profile.stats.dragonSouls = actorValues->GetActorValue(RE::ActorValue::kDragonSouls);
        profile.stats.playerXp = skillData->xp;
        profile.stats.playerLevelThreshold = skillData->levelThreshold;
        profile.stats.perkPoints = player->GetGameStatsData().perkCount;
        spdlog::info("Export checkpoint: core stats OK");

        for (std::size_t i = 0; i < kSkillCount; ++i) {
            profile.skills[i].level = skillData->skills[i].level;
            profile.skills[i].xp = skillData->skills[i].xp;
            profile.skills[i].levelThreshold = skillData->skills[i].levelThreshold;
            profile.skills[i].legendaryLevel = skillData->legendaryLevels[i];
        }

        spdlog::info("Export checkpoint: skills OK");

        // Perks can live on the player's changed NPC base and/or in the runtime-added list.
        // V2.2 classifies portable perks as safe or suspicious. Suspicious perks remain
        // visible in profile.json for auditing, but F11 will not restore them automatically.
        std::map<std::string, PerkState> perkMap;
        std::size_t perkSafe = 0;
        std::size_t perkSuspicious = 0;
        std::size_t perkSkipped = 0;
        const auto mergePerk = [&perkMap, &perkSkipped](RE::BGSPerk* a_perk, std::int8_t a_rank) {
            const auto classification = ClassifyPerk(a_perk);
            if (classification.disposition == TransferDisposition::kSkip) {
                ++perkSkipped;
                return;
            }

            auto ref = MakeFormRef(a_perk);
            if (ref.empty()) {
                ++perkSkipped;
                return;
            }

            const auto key = FormKey(ref);
            auto& slot = perkMap[key];
            slot.form = std::move(ref);
            slot.rank = std::max(slot.rank, a_rank);

            if (classification.disposition == TransferDisposition::kSuspicious) {
                slot.transferPolicy = "suspicious";
                slot.safetyReason = classification.reason;
            }
        };

        for (std::uint32_t i = 0; i < base->perkCount; ++i) {
            mergePerk(base->perks[i].perk, base->perks[i].currentRank);
        }
        for (auto* rankData : player->GetPlayerRuntimeData().addedPerks) {
            if (rankData) {
                mergePerk(rankData->perk, rankData->currentRank);
            }
        }
        for (auto& [_, perk] : perkMap) {
            if (perk.transferPolicy == "suspicious") {
                ++perkSuspicious;
            } else {
                ++perkSafe;
            }
            profile.perks.push_back(std::move(perk));
        }
        spdlog::info(
            "Export checkpoint: perks OK (safe={}, suspicious={}, skipped={})",
            perkSafe,
            perkSuspicious,
            perkSkipped);

        std::map<std::string, SpellState> spellMap;
        std::size_t spellSkipped = 0;
        const auto mergeSpell = [&spellMap, &spellSkipped](RE::SpellItem* a_spell) {
            const auto classification = ClassifySpell(a_spell);
            if (classification.disposition == TransferDisposition::kSkip) {
                ++spellSkipped;
                return;
            }

            auto ref = MakeFormRef(a_spell);
            if (ref.empty()) {
                ++spellSkipped;
                return;
            }

            const auto key = FormKey(ref);
            auto& slot = spellMap[key];
            slot.form = std::move(ref);
            if (classification.disposition == TransferDisposition::kSuspicious) {
                slot.transferPolicy = "suspicious";
                slot.safetyReason = classification.reason;
            }
        };

        if (auto* effects = base->GetSpellList(); effects) {
            if (effects->numSpells > 0 && !effects->spells) {
                spdlog::warn("Export: spell count is non-zero but spell array is null; skipping base spell array");
            }
            for (std::uint32_t i = 0; effects->spells && i < effects->numSpells; ++i) {
                mergeSpell(effects->spells[i]);
            }

            if (effects->numShouts > 0 && !effects->shouts) {
                spdlog::warn("Export: shout count is non-zero but shout array is null; skipping shouts");
            }
            for (std::uint32_t i = 0; effects->shouts && i < effects->numShouts; ++i) {
                auto* shout = effects->shouts[i];
                if (!Portable(shout)) {
                    continue;
                }
                ShoutState shoutState;
                shoutState.form = MakeFormRef(shout);
                if (shoutState.form.empty()) {
                    continue;
                }
                for (std::size_t w = 0; w < shoutState.words.size(); ++w) {
                    auto* word = shout->variations[w].word;
                    if (Portable(word)) {
                        shoutState.words[w].form = MakeFormRef(word);
                        shoutState.words[w].known = word->GetKnown();
                    }
                }
                profile.shouts.push_back(std::move(shoutState));
            }
        }

        // Learned/script-added spells are stored on the actor runtime data rather than
        // necessarily on the TESNPC base spell list. Merge both sources and filter out
        // abilities/technical effects.
        for (auto* spell : player->GetActorRuntimeData().addedSpells) {
            mergeSpell(spell);
        }
        std::size_t spellSafe = 0;
        std::size_t spellSuspicious = 0;
        for (auto& [_, spell] : spellMap) {
            if (spell.transferPolicy == "suspicious") {
                ++spellSuspicious;
            } else {
                ++spellSafe;
            }
            profile.spells.push_back(std::move(spell));
        }

        spdlog::info(
            "Export checkpoint: spells/shouts OK (spellSafe={}, spellSuspicious={}, spellSkipped={}, shouts={})",
            spellSafe,
            spellSuspicious,
            spellSkipped,
            profile.shouts.size());

        profile.quests = ExportCampaignState();
        spdlog::info("Export checkpoint: campaign OK ({})", profile.quests.size());

        std::size_t inventorySkippedUnsafe = 0;
        for (const auto& [object, data] : player->GetInventory()) {
            const auto count = data.first;
            const auto* entry = data.second.get();
            if (!object || count <= 0) {
                continue;
            }
            if (!SafeTransferInventoryItem(object, entry)) {
                ++inventorySkippedUnsafe;
                continue;
            }

            InventoryState item;
            item.form = MakeFormRef(object);
            item.count = count;
            if (!item.form.empty()) {
                profile.inventory.push_back(std::move(item));
            }
        }

        spdlog::info(
            "Export checkpoint: inventory OK (portable={}, skippedUnsafe={})",
            profile.inventory.size(),
            inventorySkippedUnsafe);

        try {
            std::filesystem::create_directories(Paths::ProfileDirectory());
            const auto target = Paths::ProfilePath();
            const auto temp = target.string() + ".tmp";
            {
                std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                if (!out) {
                    throw std::runtime_error("cannot open temporary profile file");
                }
                const nlohmann::json json = profile;
                out << json.dump(2) << '\n';
                out.flush();
                if (!out) {
                    throw std::runtime_error("cannot write profile file");
                }
            }
            std::error_code ec;
            std::filesystem::remove(target, ec);
            ec.clear();
            std::filesystem::rename(temp, target, ec);
            if (ec) {
                throw std::system_error(ec, "cannot replace profile file");
            }

            spdlog::info("Exported '{}' level {} with {} quest snapshots to {}", profile.characterName, profile.stats.level, profile.quests.size(), target.string());
            Notify("Savetrix V2.2: export concluido. Itens suspeitos ficam no JSON, mas F11 nao os importa.");
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Export failed: {}", e.what());
            Notify("Savetrix: erro ao gravar profile.json.");
            return false;
        }
    }

    bool TransferService::ImportCurrentCharacter()
    {
        Profile profile;
        try {
            std::ifstream in(Paths::ProfilePath(), std::ios::binary);
            if (!in) {
                Notify("Savetrix: profile.json nao encontrado. Use F10 primeiro.");
                return false;
            }
            nlohmann::json json;
            in >> json;
            profile = json.get<Profile>();
        } catch (const std::exception& e) {
            spdlog::error("Import parse failed: {}", e.what());
            Notify("Savetrix: profile.json invalido ou incompatível.");
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* base = GetPlayerBase(player);
        auto* actorValues = GetPlayerActorValueOwner(player);
        auto* skillData = GetSkillData(player);
        if (!player || !base || !actorValues || !skillData) {
            Notify("Savetrix: falha ao acessar o personagem atual.");
            return false;
        }

        if (importAppliedThisProcess_) {
            spdlog::warn("Import blocked: F11 was already used successfully in this Skyrim process");
            Notify("Savetrix V2.2: F11 ja foi usado nesta execucao. Salve, feche o Skyrim e abra novamente para outro import.");
            return false;
        }

        const auto preflight = RunPreflight(profile);
        LogPreflight(preflight);
        if (!preflight.coreDataValid) {
            spdlog::error("Import blocked by preflight: invalid core numeric data");
            Notify("Savetrix V2.2: preflight bloqueou o import por dados centrais invalidos.");
            return false;
        }

        if (!preflight.mainQuestReady && preflight.mainQuestCandidates > 0) {
            spdlog::warn(
                "Preflight: main quest bootstrap is not ready; {} main-quest milestones will be deferred",
                preflight.mainQuestCandidates);
        }

        ImportReport report;
        importAppliedThisProcess_ = true;

        // Character layer: same non-destructive V1 behavior. Campaign milestones are restored afterwards.
        base->actorData.level = std::clamp<std::uint16_t>(profile.stats.level, 1, 65535);
        actorValues->SetBaseActorValue(RE::ActorValue::kHealth, std::max(1.0F, profile.stats.healthBase));
        actorValues->SetBaseActorValue(RE::ActorValue::kMagicka, std::max(0.0F, profile.stats.magickaBase));
        actorValues->SetBaseActorValue(RE::ActorValue::kStamina, std::max(0.0F, profile.stats.staminaBase));
        actorValues->SetActorValue(RE::ActorValue::kDragonSouls, std::max(0.0F, profile.stats.dragonSouls));
        player->GetGameStatsData().perkCount = profile.stats.perkPoints;

        skillData->xp = std::max(0.0F, profile.stats.playerXp);
        skillData->levelThreshold = std::max(0.0F, profile.stats.playerLevelThreshold);
        for (std::size_t i = 0; i < kSkillCount; ++i) {
            skillData->skills[i].level = std::clamp(profile.skills[i].level, 0.0F, 100.0F);
            skillData->skills[i].xp = std::max(0.0F, profile.skills[i].xp);
            skillData->skills[i].levelThreshold = std::max(0.0F, profile.skills[i].levelThreshold);
            skillData->legendaryLevels[i] = profile.skills[i].legendaryLevel;
        }
        base->AddChange(static_cast<std::uint32_t>(RE::TESNPC::ChangeFlags::kBaseData));
        base->AddChange(static_cast<std::uint32_t>(RE::TESNPC::ChangeFlags::kAttributes));
        base->AddChange(static_cast<std::uint32_t>(RE::TESNPC::ChangeFlags::kNPCSkills));

        for (const auto& perkState : profile.perks) {
            auto* perk = ResolveFormAs<RE::BGSPerk>(perkState.form);
            if (!perk) {
                ++report.missingForms;
                spdlog::warn("Missing perk {} / {}", perkState.form.plugin, perkState.form.editorID);
                continue;
            }

            const auto runtimeClassification = ClassifyPerk(perk);
            if (runtimeClassification.disposition == TransferDisposition::kSkip) {
                ++report.unsafeFormsSkipped;
                spdlog::info(
                    "Skipping unsafe perk {} / {} ({})",
                    perkState.form.plugin,
                    perkState.form.name,
                    runtimeClassification.reason);
                continue;
            }
            if (!IsSafePolicy(perkState.transferPolicy) ||
                runtimeClassification.disposition == TransferDisposition::kSuspicious) {
                ++report.suspiciousPerksSkipped;
                spdlog::info(
                    "Skipping suspicious perk {} / {} (profileReason='{}', runtimeReason='{}')",
                    perkState.form.plugin,
                    perkState.form.name,
                    perkState.safetyReason,
                    runtimeClassification.reason);
                continue;
            }

            const auto exportedRank = std::max<std::int8_t>(perkState.rank, 0);
            const auto currentRank = FindPerkRank(player, perk);
            if (!currentRank || *currentRank < exportedRank) {
                player->AddPerk(perk, static_cast<std::uint32_t>(exportedRank));
                ++report.perksAdded;
            }
        }

        for (const auto& spellState : profile.spells) {
            auto* spell = ResolveFormAs<RE::SpellItem>(spellState.form);
            if (!spell) {
                ++report.missingForms;
                spdlog::warn("Missing spell {} / {}", spellState.form.plugin, spellState.form.editorID);
                continue;
            }

            const auto runtimeClassification = ClassifySpell(spell);
            if (runtimeClassification.disposition == TransferDisposition::kSkip) {
                ++report.unsafeFormsSkipped;
                spdlog::info(
                    "Skipping unsafe spell {} / {} ({})",
                    spellState.form.plugin,
                    spellState.form.name,
                    runtimeClassification.reason);
                continue;
            }
            if (!IsSafePolicy(spellState.transferPolicy) ||
                runtimeClassification.disposition == TransferDisposition::kSuspicious) {
                ++report.suspiciousSpellsSkipped;
                spdlog::info(
                    "Skipping suspicious spell {} / {} (profileReason='{}', runtimeReason='{}')",
                    spellState.form.plugin,
                    spellState.form.name,
                    spellState.safetyReason,
                    runtimeClassification.reason);
                continue;
            }

            if (!player->HasSpell(spell)) {
                if (player->AddSpell(spell)) {
                    ++report.spellsAdded;
                }
            }
        }

        for (const auto& shoutState : profile.shouts) {
            auto* shout = ResolveFormAs<RE::TESShout>(shoutState.form);
            if (!shout) {
                ++report.missingForms;
                spdlog::warn("Missing shout {} / {}", shoutState.form.plugin, shoutState.form.editorID);
                continue;
            }
            if (!player->HasShout(shout)) {
                if (player->AddShout(shout)) {
                    ++report.shoutsAdded;
                }
            }
            for (const auto& wordState : shoutState.words) {
                if (!wordState.known || wordState.form.empty()) {
                    continue;
                }
                auto* word = ResolveFormAs<RE::TESWordOfPower>(wordState.form);
                if (!word) {
                    ++report.missingForms;
                    continue;
                }
                if (!word->GetKnown()) {
                    player->UnlockWord(word);
                    ++report.wordsUnlocked;
                }
            }
        }

        // Non-destructive inventory import: only tops current stacks up to exported counts.
        // Old profiles are re-filtered too, so quest keys/non-playable technical objects from
        // earlier Savetrix builds are not injected into the destination save.
        const auto currentCounts = player->GetInventoryCounts();
        for (const auto& itemState : profile.inventory) {
            if (itemState.count <= 0) {
                continue;
            }
            auto* object = ResolveFormAs<RE::TESBoundObject>(itemState.form);
            if (!object) {
                ++report.missingForms;
                spdlog::warn("Missing inventory form {} / {}", itemState.form.plugin, itemState.form.editorID);
                continue;
            }
            if (!SafeTransferInventoryItem(object, nullptr)) {
                ++report.unsafeFormsSkipped;
                spdlog::info("Skipping unsafe inventory form {} / {}", itemState.form.plugin, itemState.form.name);
                continue;
            }

            std::int32_t current = 0;
            if (const auto it = currentCounts.find(object); it != currentCounts.end()) {
                current = it->second;
            }
            const auto delta = itemState.count - current;
            if (delta > 0) {
                player->AddObjectToContainer(object, nullptr, delta, nullptr);
                ++report.inventoryStacksAdded;
            }
        }


        auto questsForImport = profile.quests;
        if (!preflight.mainQuestReady && preflight.mainQuestCandidates > 0) {
            for (auto& questState : questsForImport) {
                if (questState.category == "main" && questState.completed && questState.restorable) {
                    questState.restorable = false;
                    ++report.mainQuestDeferred;
                }
            }
        }

        const auto campaign = ImportCampaignState(questsForImport);
        report.questsRestored = campaign.restored;
        report.questsAlreadyComplete = campaign.alreadyComplete;
        report.questsAlreadyAhead = campaign.alreadyAhead;
        report.questsSkippedInProgress = campaign.skippedInProgress;
        report.questsSkippedUnsafe = campaign.skippedUnsafe;
        report.questsMissing = campaign.missing;
        report.questsFailed = campaign.failed;

        report.hudRefreshQueued = QueueSafeHudRefresh();
        WriteImportReport(profile, preflight, report);

        spdlog::info(
            "Import complete: perks={}, spells={}, suspiciousPerksSkipped={}, suspiciousSpellsSkipped={}, shouts={}, words={}, inventoryStacks={}, missingForms={}, unsafeFormsSkipped={}, questsRestored={}, mainQuestDeferred={}, questsUnsafe={}, questsFailed={}, hudRefreshQueued={}",
            report.perksAdded,
            report.spellsAdded,
            report.suspiciousPerksSkipped,
            report.suspiciousSpellsSkipped,
            report.shoutsAdded,
            report.wordsUnlocked,
            report.inventoryStacksAdded,
            report.missingForms,
            report.unsafeFormsSkipped,
            report.questsRestored,
            report.mainQuestDeferred,
            report.questsSkippedUnsafe,
            report.questsFailed,
            report.hudRefreshQueued);

        std::ostringstream message;
        message << "Savetrix V2.2: import concluido. Suspeitos ignorados: "
                << (report.suspiciousPerksSkipped + report.suspiciousSpellsSkipped)
                << ". Quests: " << report.questsRestored
                << " restauradas, " << report.mainQuestDeferred << " main quest adiadas, "
                << report.questsSkippedUnsafe << " protegidas, "
                << report.questsFailed << " falharam. Relatorio: last_import_report.json. Salve e recarregue.";
        Notify(message.str());
        return true;
    }
}
