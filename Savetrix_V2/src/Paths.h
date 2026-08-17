#pragma once

#include <filesystem>

namespace Savetrix::Paths
{
    [[nodiscard]] std::filesystem::path SkyrimRoot();
    [[nodiscard]] std::filesystem::path ProfileDirectory();
    [[nodiscard]] std::filesystem::path ProfilePath();
    [[nodiscard]] std::filesystem::path McmDefaultsPath();
    [[nodiscard]] std::filesystem::path McmUserSettingsPath();
}
