#pragma once

#include <VelyraAppFramework/Widgets/Popup.hpp>

#include "../TaskManagerInclude.hpp"

#include <functional>
#include <string>

namespace TaskManager {

    // Asks the user to confirm a destructive action such as deleting or resetting
    class ConfirmPopup : public App::Widgets::Popup {
    public:
        using ConfirmFunction = std::function<void()>;

        explicit ConfirmPopup(App::AppData& appData);

        ~ConfirmPopup() override = default;

        void open(const std::string& message, const std::string& confirmLabel, ConfirmFunction onConfirm);

    protected:
        void drawContent() override;

        void onConfirm() override;

        void reset() override;

    private:
        std::string m_Message;
        std::string m_ConfirmLabel;
        ConfirmFunction m_OnConfirm;
    };

}
