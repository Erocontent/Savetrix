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
            if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton || event->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
                continue;
            }
            auto* button = event->AsButtonEvent();
            if (!button || !button->IsDown()) {
                continue;
            }

            const auto settings = Settings::GetSingleton().GetSnapshot();
            const auto key = button->GetIDCode();
            if (key == settings.exportKey) {
                TransferService::GetSingleton().ExportCurrentCharacter();
            } else if (key == settings.importKey) {
                TransferService::GetSingleton().ImportCurrentCharacter();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
}
