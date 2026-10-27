#include <Pch.hpp>

#include "TaskManagerLayer.hpp"
#include "Platform/Autostart.hpp"

#include <VelyraAppFramework/Settings.hpp>

namespace TaskManager {

    namespace {

        constexpr auto APP_NAME = "TaskManager";
        constexpr auto STORE_SETTING = "TaskStore";
        constexpr double SAVE_DELAY_SECONDS = 2.0;

    }

    TaskManagerLayer::TaskManagerLayer(App::AppData& appData) :
    Layer(appData) {

    }

    void TaskManagerLayer::onAttach() {
        loadStore();
        m_Autostart = isAutostartEnabled(APP_NAME);

        m_MainLayout = createSP<MainLayout>(m_AppData, m_Store);
        m_AppData.registerLayout(m_MainLayout);
        m_AppData.setActiveLayout(m_MainLayout->getID());
    }

    void TaskManagerLayer::onDetach() {
        // The settings are written to disk when the application closes
        m_AppData.getSettings().setSetting(STORE_SETTING, m_Store.snapshot());
    }

    void TaskManagerLayer::onUpdate(const Duration deltaTime) {
        // Saving is delayed a bit, so typing a title does not write the file on every key stroke
        if (!m_Store.isDirty()) {
            m_UnsavedTime = Duration{0};
            return;
        }
        m_UnsavedTime += deltaTime;
        if (m_UnsavedTime.count() >= SAVE_DELAY_SECONDS) {
            saveStore();
        }
    }

    void TaskManagerLayer::mainMenuBar() {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
            saveStore();
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New folder...")) {
                    m_MainLayout->newFolderDialog(NO_ID);
                }
                if (ImGui::MenuItem("New task list...")) {
                    m_MainLayout->newListDialog(NO_ID, ListKind::Tasks);
                }
                if (ImGui::MenuItem("New checklist...")) {
                    m_MainLayout->newListDialog(NO_ID, ListKind::Checklist);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    saveStore();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {
                    m_AppData.getWindow().close();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Home")) {
                    m_Store.select(SelectionKind::Home);
                }
                if (ImGui::MenuItem("Reset layout")) {
                    m_MainLayout->resetResizes();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::MenuItem("Start when I log in", nullptr, m_Autostart)) {
                    if (setAutostartEnabled(APP_NAME, !m_Autostart)) {
                        m_AutostartError.clear();
                    } else {
                        m_AutostartError = "Could not change the autostart entry";
                    }
                    m_Autostart = isAutostartEnabled(APP_NAME);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Starts this executable when you log in,\nenable it from the installed version of the app");
                }
                if (!m_AutostartError.empty()) {
                    ImGui::TextColored(App::Styles::ColorRed, "%s", m_AutostartError.c_str());
                }
                ImGui::EndMenu();
            }
            if (m_Store.isDirty()) {
                ImGui::TextDisabled("(unsaved)");
            }
            ImGui::EndMainMenuBar();
        }
    }

    void TaskManagerLayer::loadStore() {
        m_Store.restore(m_AppData.getSettings().getSetting<StoreState>(STORE_SETTING));
        if (m_Store.isDirty()) {
            saveStore(); // The example data of the first launch
        }
    }

    void TaskManagerLayer::saveStore() {
        App::Settings& settings = m_AppData.getSettings();
        settings.setSetting(STORE_SETTING, m_Store.snapshot());
        settings.save();
        m_Store.clearDirty();
        m_UnsavedTime = Duration{0};
    }

}
