#include "InputSink.h"

#include <dinput.h>

#include "Settings.h"
#include "TransferService.h"

namespace Savetrix
{
    InputSink& InputSink::GetSingleton()
    {
        static InputSink singleton;
        return singleton;
    }

    RE::BSEventNotifyControl InputSink::ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>*)
    {
        if (!a_event || !*a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        for (auto* event = *a_event; event; event = event->next) {
            // MCM key remapping receives the same events as gameplay.
            auto* ui = RE::UI::GetSingleton();
            if (!ui || ui->GameIsPaused() || ui->IsApplicationMenuOpen() ||
                ui->IsMenuOpen(RE::Console::MENU_NAME) || ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
                return RE::BSEventNotifyControl::kContinue;
            }
            if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton || event->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
                continue;
            }
            auto* button = event->AsButtonEvent();
            if (!button || !button->IsDown()) {
                continue;
            }

            const auto settings = Settings::GetSingleton().GetSnapshot();
            const auto key = button->GetIDCode();
            if (settings.exportKey != 0 && key == settings.exportKey) {
                TransferService::GetSingleton().ExportCurrentCharacter();
            } else if (settings.importKey != 0 && key == settings.importKey) {
                TransferService::GetSingleton().ImportCurrentCharacter();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
}
