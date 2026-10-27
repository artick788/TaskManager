#pragma once

#include "TaskManagerInclude.hpp"
#include <VelyraAppFramework/Layer.hpp>

#include "Layouts/MainLayout.hpp"
#include "Model/Store.hpp"

namespace TaskManager {

    class TaskManagerLayer : public App::Layer {
    public:
        explicit TaskManagerLayer(App::AppData& appData);

        ~TaskManagerLayer() override = default;

        void onAttach() override;

        void onDetach() override;

        void onUpdate(Duration deltaTime) override;

        void mainMenuBar() override;

    private:
        void loadStore();

        // Writes the store to the settings file, so no changes are lost when the PC shuts down without closing the app
        void saveStore();

    private:
        // Declared before the layout, since it keeps a reference to it
        Store m_Store;
        SP<MainLayout> m_MainLayout;

        Duration m_UnsavedTime{0};
        bool m_Autostart = false;
        std::string m_AutostartError;
    };

}
