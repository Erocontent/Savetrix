#pragma once

#include <RE/Skyrim.h>

namespace Savetrix
{
    class InputSink final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static InputSink& GetSingleton();
        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_event,
            RE::BSTEventSource<RE::InputEvent*>* a_source) override;

    private:
        InputSink() = default;
    };
}
