#include "FormRef.h"

#include <string_view>

namespace Savetrix
{
    FormRef MakeFormRef(const RE::TESForm* a_form)
    {
        FormRef result;
        if (!a_form || a_form->IsDynamicForm()) {
            return result;
        }

        if (const auto* file = a_form->GetFile(0)) {
            result.plugin = file->GetFilename();
            result.localFormID = a_form->GetLocalFormID();
        }

        if (const char* editorID = a_form->GetFormEditorID(); editorID && *editorID) {
            result.editorID = editorID;
        }
        if (const char* name = a_form->GetName(); name && *name) {
            result.name = name;
        }
        return result;
    }

    RE::TESForm* ResolveForm(const FormRef& a_ref)
    {
        if (!a_ref.plugin.empty()) {
            if (auto* handler = RE::TESDataHandler::GetSingleton()) {
                if (auto* form = handler->LookupForm(a_ref.localFormID, a_ref.plugin)) {
                    return form;
                }
            }
        }
        if (!a_ref.editorID.empty()) {
            return RE::TESForm::LookupByEditorID(a_ref.editorID);
        }
        return nullptr;
    }

    void to_json(nlohmann::json& j, const FormRef& v)
    {
        j = {
            { "plugin", v.plugin },
            { "localFormID", v.localFormID },
            { "editorID", v.editorID },
            { "name", v.name }
        };
    }

    void from_json(const nlohmann::json& j, FormRef& v)
    {
        v.plugin = j.value("plugin", std::string{});
        v.localFormID = j.value("localFormID", 0U);
        v.editorID = j.value("editorID", std::string{});
        v.name = j.value("name", std::string{});
    }
}
