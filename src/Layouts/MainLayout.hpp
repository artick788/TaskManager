#pragma once

#include <VelyraAppFramework/Layout.hpp>

#include "../Model/Store.hpp"
#include "../Popups/ConfirmPopup.hpp"
#include "../Popups/NamePopup.hpp"

#include <functional>
#include <string>
#include <vector>

namespace TaskManager {

    class MainLayout : public App::Layout {
    public:
        MainLayout(App::AppData& appData, Store& store);

        ~MainLayout() override = default;

        SP<App::Node> getLayout() override;

        // Dialogs, also used by the main menu bar
        void newFolderDialog(ItemID parentId);
        void newListDialog(ItemID folderId, ListKind kind);

    private:
        // Panels
        void drawExplorer();
        void drawContent();
        void drawDetails();

        // Explorer
        void drawFolderTree(ItemID parentId);
        void drawFolderNode(const Folder& folder);
        void drawListNode(const TaskList& list);
        void drawNewMenuItems(ItemID folderId);
        void drawFolderMenu(const Folder& folder);
        void drawListMenu(const TaskList& list);

        // Content views
        void drawHome();
        void drawFolderView(ItemID folderId);
        void drawListView(ItemID listId);
        void drawAddTask(const TaskList& list);
        void drawResetUndo();

        struct TableOptions {
            bool showList = false;      // Show the list a task belongs to, for the home screen
            bool showDetails = true;    // Show priority and due date columns
            bool allowReorder = false;  // Tasks can be dropped on each other to reorder them
        };
        void drawTaskTable(const char* id, const std::vector<const Task*>& tasks, const TableOptions& options);

        // Dialogs
        void renameFolderDialog(const Folder& folder);
        void renameListDialog(const TaskList& list);
        void deleteFolderDialog(const Folder& folder);
        void deleteListDialog(const TaskList& list);
        void deleteTaskDialog(const Task& task);

        void handleShortcuts();

        // Changes that add or remove items invalidate the pointers the panels are iterating over, so they are run
        // after the panel is drawn.
        void defer(std::function<void()> action);
        void runDeferred();

    private:
        App::AppData& m_AppData;
        Store& m_Store;

        SP<NamePopup> m_NamePopup;
        SP<ConfirmPopup> m_ConfirmPopup;

        std::vector<std::function<void()>> m_Deferred;

        // Input row for new tasks
        std::string m_NewTitle;
        int m_NewPriority = static_cast<int>(Priority::None);
        Date m_NewDueDate = NO_DATE;
        bool m_FocusNewTask = false;
    };

}
