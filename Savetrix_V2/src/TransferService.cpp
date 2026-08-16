#include "TransferService.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

#include <RE/Skyrim.h>
#include <RE/M/Misc.h>
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

    RE::TESNPC* GetPlayerBase(RE::PlayerCharacter* a_player)
    {
        return a_player ? a_player->GetActorBase() : nullptr;
    }

    RE::PlayerCharacter::PlayerSkills::Data* GetSkillData(RE::PlayerCharacter* a_player)
    {
        if (!a_player) {
            return nullptr;
        }
        auto& info = a_player->GetInfoRuntimeData();
        return info.skills ? info.skills->data : nullptr;
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
        RE::DebugNotification(a_text.c_str(), nullptr, true);
    }

    bool TransferService::ExportCurrentCharacter()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* base = GetPlayerBase(player);
        auto* skillData = GetSkillData(player);
        if (!player || !base || !skillData) {
            spdlog::error("Export failed: player data unavailable");
            Notify("Savetrix: falha ao ler o personagem.");
            return false;
        }

        Profile profile;
        profile.exportedAtUtc = TimestampUtc();
        profile.runtimeVersion = REL::Module::get().version().string();
        profile.characterName = player->GetName();
        profile.stats.level = static_cast<std::uint16_t>(std::clamp<std::uint32_t>(player->GetLevel(), 1, 65535));
        profile.stats.healthBase = player->GetBaseActorValue(RE::ActorValue::kHealth);
        profile.stats.magickaBase = player->GetBaseActorValue(RE::ActorValue::kMagicka);
        profile.stats.staminaBase = player->GetBaseActorValue(RE::ActorValue::kStamina);
        profile.stats.dragonSouls = player->GetActorValue(RE::ActorValue::kDragonSouls);
        profile.stats.playerXp = skillData->xp;
        profile.stats.playerLevelThreshold = skillData->levelThreshold;
        profile.stats.perkPoints = player->GetGameStatsData().perkCount;

        for (std::size_t i = 0; i < kSkillCount; ++i) {
            profile.skills[i].level = skillData->skills[i].level;
            profile.skills[i].xp = skillData->skills[i].xp;
            profile.skills[i].levelThreshold = skillData->skills[i].levelThreshold;
            profile.skills[i].legendaryLevel = skillData->legendaryLevels[i];
        }

        // Perks can live on the player's changed NPC base and/or in the runtime-added list.
        // Merge both sources and preserve the highest observed rank.
        std::map<std::string, PerkState> perkMap;
        const auto mergePerk = [&perkMap](RE::BGSPerk* a_perk, std::int8_t a_rank) {
            if (!Portable(a_perk)) {
                return;
            }
            auto ref = MakeFormRef(a_perk);
            if (ref.empty()) {
                return;
            }
            const auto key = FormKey(ref);
            auto& slot = perkMap[key];
            slot.form = std::move(ref);
            slot.rank = std::max(slot.rank, a_rank);
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
            profile.perks.push_back(std::move(perk));
        }

        if (auto* effects = base->GetSpellList(); effects) {
            for (std::uint32_t i = 0; i < effects->numSpells; ++i) {
                auto* spell = effects->spells[i];
                if (Portable(spell)) {
                    auto ref = MakeFormRef(spell);
                    if (!ref.empty()) {
                        profile.spells.push_back(std::move(ref));
                    }
                }
            }

            for (std::uint32_t i = 0; i < effects->numShouts; ++i) {
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

        profile.quests = ExportCampaignState();

        for (const auto& [object, count] : player->GetInventoryCounts()) {
            if (!object || count <= 0 || !Portable(object)) {
                continue;
            }
            InventoryState item;
            item.form = MakeFormRef(object);
            item.count = count;
            if (!item.form.empty()) {
                profile.inventory.push_back(std::move(item));
            }
        }

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
            Notify("Savetrix V2: personagem + campanha exportados. F11 importa.");
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
        auto* skillData = GetSkillData(player);
        if (!player || !base || !skillData) {
            Notify("Savetrix: falha ao acessar o personagem atual.");
            return false;
        }

        ImportReport report;

        // Character layer: same non-destructive V1 behavior. Campaign milestones are restored afterwards.
        base->actorData.level = std::clamp<std::uint16_t>(profile.stats.level, 1, 65535);
        player->SetBaseActorValue(RE::ActorValue::kHealth, std::max(1.0F, profile.stats.healthBase));
        player->SetBaseActorValue(RE::ActorValue::kMagicka, std::max(0.0F, profile.stats.magickaBase));
        player->SetBaseActorValue(RE::ActorValue::kStamina, std::max(0.0F, profile.stats.staminaBase));
        player->SetActorValue(RE::ActorValue::kDragonSouls, std::max(0.0F, profile.stats.dragonSouls));
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
            const auto exportedRank = std::max<std::int8_t>(perkState.rank, 0);
            const auto currentRank = FindPerkRank(player, perk);
            if (!currentRank || *currentRank < exportedRank) {
                player->AddPerk(perk, static_cast<std::uint32_t>(exportedRank));
                ++report.perksAdded;
            }
        }

        for (const auto& spellRef : profile.spells) {
            auto* spell = ResolveFormAs<RE::SpellItem>(spellRef);
            if (!spell) {
                ++report.missingForms;
                spdlog::warn("Missing spell {} / {}", spellRef.plugin, spellRef.editorID);
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
        // It never removes target-save items/quest items and it does not duplicate on repeated imports.
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
            "Import complete: perks={}, spells={}, shouts={}, words={}, inventoryStacks={}, missingForms={}, questsRestored={}, questsUnsafe={}, questsFailed={}",
            report.perksAdded,
            report.spellsAdded,
            report.shoutsAdded,
            report.wordsUnlocked,
            report.inventoryStacksAdded,
            report.missingForms,
            report.questsRestored,
            report.questsSkippedUnsafe,
            report.questsFailed);

        std::ostringstream message;
        message << "Savetrix V2: import concluido. Quests: " << report.questsRestored
                << " restauradas, " << report.questsSkippedUnsafe << " protegidas, "
                << report.questsFailed << " falharam. Salve e recarregue.";
        Notify(message.str());
        return true;
    }
}
