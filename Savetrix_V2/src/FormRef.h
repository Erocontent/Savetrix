#pragma once

#include <cstdint>
#include <string>

#include <RE/Skyrim.h>
#include <nlohmann/json.hpp>

namespace Savetrix
{
    struct FormRef
    {
        std::string plugin;
        std::uint32_t localFormID{ 0 };
        std::string editorID;
        std::string name;

        [[nodiscard]] bool empty() const noexcept { return plugin.empty() && editorID.empty(); }
    };

    [[nodiscard]] FormRef MakeFormRef(const RE::TESForm* a_form);
    [[nodiscard]] RE::TESForm* ResolveForm(const FormRef& a_ref);

    template <class T>
    [[nodiscard]] T* ResolveFormAs(const FormRef& a_ref)
    {
        if (auto* form = ResolveForm(a_ref)) {
            return form->As<T>();
        }
        return nullptr;
    }

    void to_json(nlohmann::json& a_json, const FormRef& a_value);
    void from_json(const nlohmann::json& a_json, FormRef& a_value);
}
