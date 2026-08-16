#include "TransferService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#include <RE/Skyrim.h>
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
        const auto* filename = file->GetFilename();
        if (!filename || !*filename) {
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
                "state perk"
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
                "reset spell"
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
        // V2.1.1 classifies portable perks as safe or suspicious. Suspicious perks remain
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
            Notify("Savetrix V2.1.1: export concluido. Itens suspeitos ficam no JSON, mas F11 nao os importa.");
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

        ImportReport report;

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


        const auto campaign = ImportCampaignState(profile.quests);
        report.questsRestored = campaign.restored;
        report.questsAlreadyComplete = campaign.alreadyComplete;
        report.questsAlreadyAhead = campaign.alreadyAhead;
        report.questsSkippedInProgress = campaign.skippedInProgress;
        report.questsSkippedUnsafe = campaign.skippedUnsafe;
        report.questsMissing = campaign.missing;
        report.questsFailed = campaign.failed;

        spdlog::info(
            "Import complete: perks={}, spells={}, suspiciousPerksSkipped={}, suspiciousSpellsSkipped={}, shouts={}, words={}, inventoryStacks={}, missingForms={}, unsafeFormsSkipped={}, questsRestored={}, questsUnsafe={}, questsFailed={}",
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
            report.questsSkippedUnsafe,
            report.questsFailed);

        std::ostringstream message;
        message << "Savetrix V2.1.1: import concluido. Suspeitos ignorados: "
                << (report.suspiciousPerksSkipped + report.suspiciousSpellsSkipped)
                << ". Quests: " << report.questsRestored
                << " restauradas, " << report.questsSkippedUnsafe << " protegidas, "
                << report.questsFailed << " falharam. Salve e recarregue.";
        Notify(message.str());
        return true;
    }
}
