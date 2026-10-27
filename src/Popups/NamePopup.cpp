#include <Pch.hpp>

#include "NamePopup.hpp"
#include "../Widgets/Widgets.hpp"

namespace TaskManager {

    NamePopup::NamePopup(App::AppData& appData) :
    Popup(appData, "Name") {

    }

    void NamePopup::open(const std::string& prompt, const std::string& initialName, const std::string& confirmLabel, ConfirmFunction onConfirm) {
        m_Prompt = prompt;
        m_Name = initialName;
        m_ConfirmLabel = confirmLabel;
        m_OnConfirm = std::move(onConfirm);
        m_FocusInput = true;
        setOpen(true);
    }

    void NamePopup::drawContent() {
        ImGui::TextUnformatted(m_Prompt.c_str());
        if (m_FocusInput) {
            ImGui::SetKeyboardFocusHere();
            m_FocusInput = false;
        }
        ImGui::SetNextItemWidth(Widgets::Scaled(320.0f));
        const bool entered = ImGui::InputText("##Name", &m_Name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        const bool valid = !Widgets::Trimmed(m_Name).empty();
        if (entered && valid) {
            onConfirm();
            reset();
            setOpen(false);
            return;
        }
        drawConfirmationButtons(m_ConfirmLabel, "Cancel", valid);
    }

    void NamePopup::onConfirm() {
        if (m_OnConfirm) {
            m_OnConfirm(Widgets::Trimmed(m_Name));
        }
    }

    void NamePopup::reset() {
        m_OnConfirm = nullptr;
    }

}
