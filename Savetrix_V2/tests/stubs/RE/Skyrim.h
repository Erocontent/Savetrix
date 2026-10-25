// Minimal engine boundary for exercising the production InputSink without Skyrim.
#pragma once
#include <cstdint>
#include <string_view>
namespace RE {
    enum class BSEventNotifyControl { kContinue };
    template <class T> class BSTEventSource {};
    template <class T> class BSTEventSink {
    public:
        virtual ~BSTEventSink() = default;
        virtual BSEventNotifyControl ProcessEvent(const T*, BSTEventSource<T>*) = 0;
    };
    enum class INPUT_EVENT_TYPE { kButton, kOther };
    enum class INPUT_DEVICE { kKeyboard, kMouse, kGamepad };
    struct ButtonEvent;
    struct InputEvent {
        InputEvent* next{};
        INPUT_EVENT_TYPE type{INPUT_EVENT_TYPE::kButton};
        INPUT_DEVICE device{INPUT_DEVICE::kKeyboard};
        INPUT_EVENT_TYPE GetEventType() { return type; }
        INPUT_DEVICE GetDevice() { return device; }
        ButtonEvent* AsButtonEvent();
    };
    struct ButtonEvent : InputEvent {
        bool down{true};
        std::uint32_t key{68};
        bool IsDown() { return down; }
        std::uint32_t GetIDCode() { return key; }
    };
    inline ButtonEvent* InputEvent::AsButtonEvent() { return static_cast<ButtonEvent*>(this); }
    struct UI {
        bool paused{}, application{}, console{}, journal{}, mainMenu{}, loading{};
        static UI* GetSingleton();
        bool GameIsPaused() { return paused; }
        bool IsApplicationMenuOpen() const { return application; }
        bool IsMenuOpen(std::string_view name) {
            return (name == "Console" && console) || (name == "Journal Menu" && journal) ||
                (name == "Main Menu" && mainMenu) || (name == "Loading Menu" && loading);
        }
    };
    struct Console { static constexpr auto MENU_NAME = "Console"; };
    struct JournalMenu { static constexpr auto MENU_NAME = "Journal Menu"; };
    struct MainMenu { static constexpr auto MENU_NAME = "Main Menu"; };
    struct LoadingMenu { static constexpr auto MENU_NAME = "Loading Menu"; };
}
