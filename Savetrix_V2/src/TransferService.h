#pragma once

#include <cstddef>
#include <string>

namespace Savetrix
{
    struct ImportReport
    {
        std::size_t perksAdded{};
        std::size_t perksAlreadyPresent{};
        std::size_t spellsAdded{};
        std::size_t spellsAlreadyPresent{};
        std::size_t suspiciousPerksSkipped{};
        std::size_t suspiciousSpellsSkipped{};
        std::size_t shoutsAdded{};
        std::size_t shoutsAlreadyPresent{};
        std::size_t wordsUnlocked{};
        std::size_t inventoryStacksAdded{};
        std::size_t inventoryStacksAlreadySatisfied{};
        std::size_t missingForms{};
        std::size_t unsafeFormsSkipped{};
        std::size_t questsRestored{};
        std::size_t questsAlreadyComplete{};
        std::size_t questsAlreadyAhead{};
        std::size_t questsSkippedInProgress{};
        std::size_t questsSkippedUnsafe{};
        std::size_t mainQuestDeferred{};
        std::size_t questsMissing{};
        std::size_t questsFailed{};
        bool statsImported{};
        bool skillsImported{};
        bool perksImported{};
        bool spellsImported{};
        bool shoutsImported{};
        bool inventoryImported{};
        bool questsImported{};
        bool hudRefreshQueued{};
    };

    class TransferService
    {
    public:
        static TransferService& GetSingleton();

        bool ExportCurrentCharacter();
        bool ImportCurrentCharacter();

    private:
        TransferService() = default;
        static void Notify(const std::string& a_text);

        bool importAppliedThisProcess_{ false };
    };
}
