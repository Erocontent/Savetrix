#pragma once

#include <cstddef>
#include <vector>

#include "Profile.h"

namespace Savetrix
{
    struct CampaignImportReport
    {
        std::size_t restored{};
        std::size_t alreadyComplete{};
        std::size_t alreadyAhead{};
        std::size_t skippedInProgress{};
        std::size_t skippedUnsafe{};
        std::size_t missing{};
        std::size_t failed{};
    };

    [[nodiscard]] std::vector<QuestState> ExportCampaignState();
    [[nodiscard]] CampaignImportReport ImportCampaignState(const std::vector<QuestState>& a_quests);
}
