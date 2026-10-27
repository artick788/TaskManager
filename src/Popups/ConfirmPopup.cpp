#include <Pch.hpp>

#include "ConfirmPopup.hpp"
#include "../Widgets/Widgets.hpp"

namespace TaskManager {

    ConfirmPopup::ConfirmPopup(App::AppData& appData) :
    Popup(appData, "Are you sure?") {

    }

    void ConfirmPopup::open(const std::string& message, const std::string& confirmLabel, ConfirmFunction onConfirm) {
        m_Message = message;
        m_ConfirmLabel = confirmLabel;
        m_OnConfirm = std::move(onConfirm);
        setOpen(true);
    }

    void ConfirmPopup::drawContent() {
        ImGui::PushTextWrapPos(Widgets::Scaled(400.0f));
        ImGui::TextUnformatted(m_Message.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        drawConfirmationButtons(m_ConfirmLabel, "Cancel");
    }

    void ConfirmPopup::onConfirm() {
        if (m_OnConfirm) {
            m_OnConfirm();
        }
    }

    void ConfirmPopup::reset() {
        m_OnConfirm = nullptr;
    }

}
