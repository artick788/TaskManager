#pragma once

#include <VelyraAppFramework/Widgets/Popup.hpp>

#include "../TaskManagerInclude.hpp"

#include <functional>
#include <string>

namespace TaskManager {

    // Asks for a name, used to create and rename folders and lists
    class NamePopup : public App::Widgets::Popup {
    public:
        using ConfirmFunction = std::function<void(const std::string&)>;

        explicit NamePopup(App::AppData& appData);

        ~NamePopup() override = default;

        void open(const std::string& prompt, const std::string& initialName, const std::string& confirmLabel, ConfirmFunction onConfirm);

    protected:
        void drawContent() override;

        void onConfirm() override;

        void reset() override;

    private:
        std::string m_Prompt;
        std::string m_Name;
        std::string m_ConfirmLabel;
        ConfirmFunction m_OnConfirm;
        bool m_FocusInput = false;
    };

}
