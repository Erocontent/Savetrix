#include <filesystem>
#include <memory>
#include <string>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "InputSink.h"
#include "Profile.h"
#include "Settings.h"

// PR #1 (JackOfSpeed-01): export the metadata required for AE discovery.
SKSEPluginInfo(
    .Version = REL::Version{ Savetrix::kVersionMajor, Savetrix::kVersionMinor, Savetrix::kVersionPatch, 0 },
    .Name = std::string_view{ "Savetrix" },
    .Author = std::string_view{ "Erocontent" },
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary)

namespace
{
    void InitializeLog()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }
        *path /= "Savetrix.log";
        auto logger = std::make_shared<spdlog::logger>(
            "global log",
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        if (a_message->type == SKSE::MessagingInterface::kInputLoaded) {
            if (auto* manager = RE::BSInputDeviceManager::GetSingleton()) {
                manager->AddEventSink(&Savetrix::InputSink::GetSingleton());

                const auto settings = Savetrix::Settings::GetSingleton().GetSnapshot();
                spdlog::info(
                    "Input sink registered. exportKey={}, importKey={}",
                    settings.exportKey,
                    settings.importKey);
            }
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    InitializeLog();

    spdlog::info(
        "Savetrix {} loading on runtime {}",
        std::string(Savetrix::kModVersion),
        REL::Module::get().version().string());

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageHandler)) {
        spdlog::error("Savetrix failed to register the SKSE message listener; hotkeys unavailable");
        return false;
    }

    return true;
}
