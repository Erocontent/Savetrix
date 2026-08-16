#include "Paths.h"

#include <array>
#include <system_error>
#include <Windows.h>

namespace Savetrix::Paths
{
    std::filesystem::path SkyrimRoot()
    {
        std::array<wchar_t, 32768> buffer{};
        const auto size = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size == 0 || size >= buffer.size()) {
            return std::filesystem::current_path();
        }
        return std::filesystem::path(std::wstring_view(buffer.data(), size)).parent_path();
    }

    std::filesystem::path ProfileDirectory()
    {
        return SkyrimRoot() / "Data" / "SKSE" / "Plugins" / "Savetrix";
    }

    std::filesystem::path ProfilePath()
    {
        return ProfileDirectory() / "profile.json";
    }
}
