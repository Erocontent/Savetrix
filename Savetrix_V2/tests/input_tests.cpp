#include "InputSink.h"
#include "Settings.h"
#include "TransferService.h"
#include <iostream>
#include <stdexcept>

namespace {
    RE::UI ui;
    RE::UI* uiPtr = &ui;
    int exports{}, imports{};
    void Check(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
}
RE::UI* RE::UI::GetSingleton() { return uiPtr; }
namespace Savetrix::Paths {
    std::filesystem::path McmDefaultsPath() { return {}; }
    std::filesystem::path McmUserSettingsPath() { return {}; }
}
namespace Savetrix {
    TransferService& TransferService::GetSingleton() { static TransferService service; return service; }
    bool TransferService::ExportCurrentCharacter() { ++exports; return true; }
    bool TransferService::ImportCurrentCharacter() { ++imports; return true; }
}
int main()
{
    auto& sink = Savetrix::InputSink::GetSingleton();
    RE::ButtonEvent button;
    RE::InputEvent* event = &button;
    auto press = [&]() { sink.ProcessEvent(&event, nullptr); };
    try {
        ui.journal = true;
        press();
        Check(exports == 0, "remapping a key in the journal/MCM must not export");
        button.key = 87;
        press();
        Check(imports == 0, "remapping a key in the journal/MCM must not import");
        ui = {};
        ui.paused = true;
        press();
        Check(imports == 0, "paused game must not import");
        ui = {};
        ui.console = true;
        press();
        Check(imports == 0, "console must not import");
        ui = {};
        ui.loading = true;
        press();
        Check(imports == 0, "loading screen must not import");
        ui = {};
        ui.mainMenu = true;
        press();
        Check(imports == 0, "main menu must not import");
        ui = {};
        ui.application = true;
        press();
        Check(imports == 0, "application menu must not import");
        ui = {};
        uiPtr = nullptr;
        press();
        Check(imports == 0, "unavailable UI must not import");
        uiPtr = &ui;
        button.device = RE::INPUT_DEVICE::kMouse;
        press();
        button.device = RE::INPUT_DEVICE::kGamepad;
        press();
        button.device = RE::INPUT_DEVICE::kKeyboard;
        button.down = false;
        press();
        Check(imports == 0, "non-keyboard, key-up and repeat events must not import");
        button.down = true;
        press();
        button.key = 68;
        press();
        Check(imports == 1 && exports == 1, "gameplay hotkeys must work");
        event = nullptr;
        press();
        sink.ProcessEvent(nullptr, nullptr);
        Check(imports == 1 && exports == 1, "null events must be ignored");
        std::cout << "Input regression tests passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
