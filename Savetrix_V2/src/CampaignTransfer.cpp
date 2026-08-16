#include "CampaignTransfer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>

#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

#include "FormRef.h"

namespace
{
    using QuestType = RE::QUEST_DATA::Type;

    std::string Lower(std::string_view a_value)
    {
        std::string out(a_value);
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return out;
    }

    bool IsOfficialPlugin(const RE::TESQuest* a_quest)
    {
        if (!a_quest) {
            return false;
        }
        const auto* file = a_quest->GetFile(0);
        if (!file) {
            return false;
        }
        const auto name = Lower(file->GetFilename());
        return name == "skyrim.esm" || name == "dawnguard.esm" || name == "dragonborn.esm";
    }

    bool IsCampaignType(QuestType a_type)
    {
        switch (a_type) {
        case QuestType::kMainQuest:
        case QuestType::kMagesGuild:
        case QuestType::kThievesGuild:
        case QuestType::kDarkBrotherhood:
        case QuestType::kCompanionsQuest:
        case QuestType::kCivilWar:
        case QuestType::kDLC01_Vampire:
        case QuestType::kDLC02_Dragonborn:
            return true;
        default:
            return false;
        }
    }

    std::string CategoryFor(QuestType a_type)
    {
        switch (a_type) {
        case QuestType::kMainQuest: return "main";
        case QuestType::kMagesGuild: return "college";
        case QuestType::kThievesGuild: return "thieves";
        case QuestType::kDarkBrotherhood: return "dark_brotherhood";
        case QuestType::kCompanionsQuest: return "companions";
        case QuestType::kCivilWar: return "civil_war";
        case QuestType::kDLC01_Vampire: return "dawnguard";
        case QuestType::kDLC02_Dragonborn: return "dragonborn";
        default: return "other";
        }
    }

    int CategoryOrder(std::string_view a_category)
    {
        static constexpr std::array<std::string_view, 8> kOrder{
            "main", "companions", "college", "thieves", "dark_brotherhood", "dawnguard", "dragonborn", "civil_war"
        };
        const auto it = std::find(kOrder.begin(), kOrder.end(), a_category);
        return it == kOrder.end() ? 999 : static_cast<int>(std::distance(kOrder.begin(), it));
    }

    bool BlacklistedForAutomaticRestore(const RE::TESQuest* a_quest)
    {
        if (!a_quest) {
            return true;
        }
        const auto* editor = a_quest->GetFormEditorID();
        if (!editor || !*editor) {
            return true;
        }
        const auto id = Lower(editor);

        // These quests directly control the tutorial/civil-war world state or contain
        // mutually-exclusive decisions that V2 does not attempt to reconstruct.
        return id == "mq101" || id == "mq102" || id == "mq302" || id == "mqpaarthurnax";
    }


    bool MatchesStorylineWhitelist(const RE::TESQuest* a_quest)
    {
        if (!a_quest) {
            return false;
        }
        const auto* editor = a_quest->GetFormEditorID();
        if (!editor || !*editor) {
            return false;
        }
        const auto id = Lower(editor);

        const auto equalsAny = [&id](std::initializer_list<std::string_view> values) {
            return std::ranges::any_of(values, [&id](std::string_view value) { return id == value; });
        };

        switch (a_quest->GetType()) {
        case QuestType::kMainQuest:
            return equalsAny({
                "mq103", "mq104", "mq105", "mq105ustengrav", "mq106",
                "mq201", "mq202", "mq203", "mq204", "mq205", "mq206",
                "mq301", "mq303", "mq304", "mq305"
            });
        case QuestType::kMagesGuild:
            return equalsAny({ "mg01", "mg02", "mg03", "mg04", "mg05", "mg06", "mg07", "mg08" });
        case QuestType::kThievesGuild:
            return equalsAny({
                "tg00", "tg01", "tg02", "tg03", "tg04", "tg05", "tg06", "tg07",
                "tg08a", "tg08b", "tg09", "tgleadership"
            });
        case QuestType::kDarkBrotherhood:
            return equalsAny({
                "db01", "db02", "db03", "db04", "db05", "db06", "db07", "db08",
                "db09", "db10", "db11", "dbdestroy"
            });
        case QuestType::kCompanionsQuest:
            return equalsAny({ "c00", "c01", "c03", "c04", "c05", "c06" });
        case QuestType::kDLC01_Vampire:
            return id.starts_with("dlc1vq");
        case QuestType::kDLC02_Dragonborn:
            return id.starts_with("dlc2mq");
        default:
            return false;
        }
    }

    bool IsAutomaticallyRestorable(const RE::TESQuest* a_quest)
    {
        if (!a_quest || !IsOfficialPlugin(a_quest) || !IsCampaignType(a_quest->GetType())) {
            return false;
        }
        if (a_quest->GetType() == QuestType::kCivilWar || BlacklistedForAutomaticRestore(a_quest)) {
            return false;
        }
        if (!MatchesStorylineWhitelist(a_quest)) {
            return false;
        }
        if (a_quest->data.flags.all(RE::QuestFlag::kAllowRepeatStages)) {
            return false;
        }
        return true;
    }

    std::string QuestSortKey(const Savetrix::QuestState& a_state)
    {
        std::ostringstream out;
        out << std::setw(3) << std::setfill('0') << CategoryOrder(a_state.category) << ':'
            << Lower(a_state.form.editorID) << ':' << std::hex << a_state.form.localFormID;
        return out.str();
    }

    std::string SetStageCommand(RE::TESQuest* a_quest, std::uint16_t a_stage)
    {
        std::ostringstream out;
        out << "setstage " << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
            << a_quest->GetFormID() << std::dec << ' ' << a_stage;
        return out.str();
    }

    bool ExecuteCommand(RE::Script* a_script, RE::TESObjectREFR* a_target, std::string_view a_command)
    {
        if (!a_script) {
            return false;
        }
        try {
            a_script->SetCommand(a_command);
            a_script->CompileAndRun(a_target, RE::COMPILER_NAME::kSystemWindowCompiler);
            a_script->ClearCommand();
            return true;
        } catch (...) {
            a_script->ClearCommand();
            return false;
        }
    }
}

namespace Savetrix
{
    std::vector<QuestState> ExportCampaignState()
    {
        std::vector<QuestState> result;
        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) {
            spdlog::warn("Campaign export: TESDataHandler unavailable");
            return result;
        }

        for (auto* quest : handler->GetFormArray<RE::TESQuest>()) {
            if (!quest || !IsOfficialPlugin(quest) || !IsCampaignType(quest->GetType())) {
                continue;
            }

            const auto stage = quest->GetCurrentStageID();
            const bool completed = quest->IsCompleted();
            const bool active = quest->IsActive();
            const bool running = quest->IsRunning();
            if (stage == 0 && !completed && !active) {
                continue;
            }

            QuestState state;
            state.form = MakeFormRef(quest);
            if (state.form.empty()) {
                continue;
            }
            state.category = CategoryFor(quest->GetType());
            state.stage = stage;
            state.completed = completed;
            state.active = active;
            state.running = running;
            state.restorable = IsAutomaticallyRestorable(quest);
            result.push_back(std::move(state));
        }

        std::ranges::sort(result, [](const QuestState& a, const QuestState& b) {
            return QuestSortKey(a) < QuestSortKey(b);
        });

        spdlog::info("Campaign export: {} quest snapshots", result.size());
        return result;
    }

    CampaignImportReport ImportCampaignState(const std::vector<QuestState>& a_quests)
    {
        CampaignImportReport report;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            report.failed = a_quests.size();
            return report;
        }

        // A valid runtime-created Script form is used to run the game's own SetStage console
        // command. That path executes the quest stage machinery/fragments instead of merely
        // overwriting TESQuest::currentStage.
        auto* commandScript = RE::IFormFactory::Create<RE::Script>();
        if (!commandScript) {
            spdlog::error("Campaign import: could not create command Script form");
            report.failed = a_quests.size();
            return report;
        }

        auto ordered = a_quests;
        std::ranges::sort(ordered, [](const QuestState& a, const QuestState& b) {
            return QuestSortKey(a) < QuestSortKey(b);
        });

        for (const auto& state : ordered) {
            // V2 only replays completed quest milestones. In-progress quest restoration is
            // deliberately deferred because aliases/objective state are not portable enough.
            if (!state.completed) {
                ++report.skippedInProgress;
                continue;
            }
            if (!state.restorable || state.category == "civil_war") {
                ++report.skippedUnsafe;
                continue;
            }

            auto* quest = ResolveFormAs<RE::TESQuest>(state.form);
            if (!quest) {
                ++report.missing;
                spdlog::warn("Campaign import missing quest {} / {}", state.form.plugin, state.form.editorID);
                continue;
            }
            if (!IsAutomaticallyRestorable(quest)) {
                ++report.skippedUnsafe;
                continue;
            }
            if (quest->IsCompleted()) {
                ++report.alreadyComplete;
                continue;
            }
            if (quest->GetCurrentStageID() > state.stage) {
                ++report.alreadyAhead;
                continue;
            }
            if (state.stage == 0) {
                ++report.skippedUnsafe;
                continue;
            }

            if (quest->IsStopped() && quest->eventID == RE::QuestEvent::kNone) {
                quest->Start();
            }

            const auto command = SetStageCommand(quest, state.stage);
            spdlog::info("Campaign import: {}", command);
            if (!ExecuteCommand(commandScript, player, command)) {
                ++report.failed;
                continue;
            }

            if (quest->IsCompleted() || quest->GetCurrentStageID() >= state.stage) {
                ++report.restored;
            } else {
                ++report.failed;
                spdlog::warn(
                    "Campaign import verification failed for {} targetStage={} currentStage={} completed={}",
                    state.form.editorID,
                    state.stage,
                    quest->GetCurrentStageID(),
                    quest->IsCompleted());
            }
        }

        commandScript->ClearCommand();
        return report;
    }
}
