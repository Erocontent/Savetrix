#include "Settings.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    std::filesystem::path testRoot;
    void Check(bool condition, const char* message)
    {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }
    void Write(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
    }
}

namespace Savetrix::Paths {
    std::filesystem::path McmDefaultsPath() { return testRoot / "Config" / "settings.ini"; }
    std::filesystem::path McmUserSettingsPath() { return testRoot / "Settings" / "Savetrix.ini"; }
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    testRoot = std::filesystem::absolute(argv[1]) / L"configura\u00e7\u00f5es" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    auto& settings = Savetrix::Settings::GetSingleton();
    auto reload = [&]() { settings.ForceReload(); return settings.GetSnapshot(); };
    try {
        auto s = reload();
        Check(s.exportKey == 68 && s.importKey == 87 && s.import.AnyEnabled(), "missing files must use defaults");
        Write(Savetrix::Paths::McmDefaultsPath(), "[Controls]\niExportKey=61\niImportKey=62\n[Import]\nbStats=0\nbSkills=1\n");
        Write(Savetrix::Paths::McmUserSettingsPath(), "\xEF\xBB\xBF[Controls]\niExportKey=65\niImportKey=66\n[Import]\nbSkills=0\n");
        s = reload();
        Check(s.exportKey == 65 && s.importKey == 66, "UTF-8 BOM first-section user keys must override defaults");
        Check(!s.import.stats && !s.import.skills && s.import.inventory, "merge each setting independently");
        Write(Savetrix::Paths::McmUserSettingsPath(), "[Controls]\niExportKey=-1\niImportKey=0\n");
        s = reload();
        Check(s.exportKey == 0 && s.importKey == 0, "both hotkeys may be explicitly unbound");
        Write(Savetrix::Paths::McmUserSettingsPath(), "[Controls]\niExportKey=70\niImportKey=70\n");
        s = reload();
        Check(s.exportKey == 68 && s.importKey == 87, "active key collision must restore F10/F11");
        Write(Savetrix::Paths::McmUserSettingsPath(), "[Controls]\niExportKey=invalid\niImportKey=256\n[Import]\nbStats=invalid\n");
        s = reload();
        Check(s.exportKey == 61 && s.importKey == 87 && !s.import.stats, "malformed user values inherit defaults; non-keyboard codes use safe keys");
        Write(Savetrix::Paths::McmUserSettingsPath(), "\xEF\xBB\xBF[Controls]\niExportKey=67\niImportKey=69\n");
        s = reload();
        Check(s.exportKey == 67 && s.importKey == 69, "reload must observe newly written settings");
        std::cout << "Settings regression tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
