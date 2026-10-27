#pragma once

#include "../TaskManagerInclude.hpp"
#include "Date.hpp"

#include <VelyraUtils/DevUtils/JsonSerializer.hpp>

#include <string>
#include <vector>

namespace TaskManager {

    using ItemID = U64;

    constexpr ItemID NO_ID = 0;

    enum class Priority : int {
        None = 0,
        Low = 1,
        Medium = 2,
        High = 3,
        Urgent = 4
    };

    constexpr int PRIORITY_COUNT = 5;

    enum class ListKind : int {
        Tasks = 0,    // Tasks with a priority and due date, shown on the home screen
        Checklist = 1 // Plain items to tick off and reset, e.g. a packing list
    };

    enum class SortMode : int {
        Manual = 0,
        DueDate = 1,
        Priority = 2
    };

    struct Folder {
        ItemID id = NO_ID;
        ItemID parentId = NO_ID; // NO_ID for top level folders
        std::string name;
        bool expanded = true;

        VL_GENERATE_JSON_SERIALIZER_RELAXED(Folder, id, parentId, name, expanded)
    };

    struct TaskList {
        ItemID id = NO_ID;
        ItemID folderId = NO_ID; // NO_ID for top level lists
        std::string name;
        int kind = static_cast<int>(ListKind::Tasks); // ListKind, kept as int for the json serializer
        int sortMode = static_cast<int>(SortMode::Manual); // SortMode
        bool hideDone = false;

        bool isChecklist() const { return kind == static_cast<int>(ListKind::Checklist); }

        VL_GENERATE_JSON_SERIALIZER_RELAXED(TaskList, id, folderId, name, kind, sortMode, hideDone)
    };

    // Tasks are kept in their manual order, the order of the store's vector
    struct Task {
        ItemID id = NO_ID;
        ItemID listId = NO_ID;
        std::string title;
        std::string notes;
        int priority = static_cast<int>(Priority::None); // Priority
        Date dueDate = NO_DATE;
        Date createdDate = NO_DATE;
        Date completedDate = NO_DATE;
        bool done = false;

        VL_GENERATE_JSON_SERIALIZER_RELAXED(Task, id, listId, title, notes, priority, dueDate, createdDate, completedDate, done)
    };

    enum class SelectionKind : int {
        Home = 0,
        Folder = 1,
        List = 2
    };

    struct HomeConfig {
        bool showUndated = true;
        bool showCompletedToday = true;
        int minPriority = static_cast<int>(Priority::None);

        VL_GENERATE_JSON_SERIALIZER_RELAXED(HomeConfig, showUndated, showCompletedToday, minPriority)
    };

    // The part of the Store that is persisted in the application settings.
    struct StoreState {
        std::vector<Folder> folders;
        std::vector<TaskList> lists;
        std::vector<Task> tasks;
        ItemID nextId = 1;
        int selectionKind = static_cast<int>(SelectionKind::Home);
        ItemID selectionId = NO_ID;
        HomeConfig home;
        bool initialized = false; // False on the very first launch, which adds some example data

        VL_GENERATE_JSON_SERIALIZER_RELAXED(StoreState, folders, lists, tasks, nextId, selectionKind, selectionId, home, initialized)
    };

    struct FolderProgress {
        Size done = 0;
        Size total = 0;
        Size lists = 0;
    };

    class Store {
    public:
        // Folders
        ItemID createFolder(const std::string& name, ItemID parentId);
        void renameFolder(ItemID id, const std::string& name);
        void deleteFolder(ItemID id); // Also deletes everything inside it
        bool moveFolder(ItemID id, ItemID newParentId); // false if the folder would end up inside itself
        Folder* findFolder(ItemID id);
        const Folder* findFolder(ItemID id) const;
        std::vector<const Folder*> childFolders(ItemID parentId) const; // Sorted by name
        bool isInside(ItemID folderId, ItemID ancestorId) const; // True if folderId is ancestorId or below it

        // Lists
        ItemID createList(const std::string& name, ItemID folderId, ListKind kind);
        void renameList(ItemID id, const std::string& name);
        void deleteList(ItemID id);
        void moveList(ItemID id, ItemID folderId);
        TaskList* findList(ItemID id);
        const TaskList* findList(ItemID id) const;
        std::vector<const TaskList*> listsIn(ItemID folderId) const; // Sorted by name
        std::vector<const TaskList*> allLists() const; // Sorted by path
        std::string listPath(ItemID listId) const; // "AURA / UI tasks"
        std::string folderPath(ItemID folderId) const;

        // Tasks
        ItemID createTask(ItemID listId, const std::string& title, Priority priority = Priority::None, Date dueDate = NO_DATE);
        void deleteTask(ItemID id);
        void setDone(ItemID id, bool done);
        void moveTaskToList(ItemID id, ItemID listId);
        void moveTaskBefore(ItemID id, ItemID beforeId); // Manual reordering, NO_ID moves it to the end
        Task* findTask(ItemID id);
        const Task* findTask(ItemID id) const;
        std::vector<const Task*> tasksIn(ItemID listId) const; // In manual order
        const std::vector<Task>& getTasks() const { return m_Tasks; }
        Size countOpen(ItemID listId) const;
        FolderProgress folderProgress(ItemID folderId) const; // Includes all sub folders

        // Resetting sets every task back to TODO, returns the number of tasks that were reset. The last reset can be undone.
        Size resetList(ItemID listId);
        Size resetFolder(ItemID folderId);
        Size lastResetCount() const { return m_LastReset.size(); }
        void undoReset();
        void forgetReset() { m_LastReset.clear(); }
        Size deleteDone(ItemID listId);

        // Selection
        SelectionKind selectionKind() const { return m_SelectionKind; }
        ItemID selectionId() const { return m_SelectionId; }
        void select(SelectionKind kind, ItemID id = NO_ID);
        ItemID selectedTask() const { return m_SelectedTask; }
        void selectTask(ItemID id) { m_SelectedTask = id; }

        HomeConfig& homeConfig() { return m_Home; }

        // Every change marks the store dirty, so it can be saved shortly after
        void markDirty() { m_Dirty = true; }
        bool isDirty() const { return m_Dirty; }
        void clearDirty() { m_Dirty = false; }

        // Persistence
        StoreState snapshot() const;
        void restore(const StoreState& state);
        void addExampleData();

    private:
        struct ResetTask {
            ItemID id = NO_ID;
            Date completedDate = NO_DATE;
        };

        ItemID nextId() { return m_NextId++; }
        Size resetTasksOf(ItemID listId);
        void validateSelection();

    private:
        std::vector<Folder> m_Folders;
        std::vector<TaskList> m_Lists;
        std::vector<Task> m_Tasks;
        ItemID m_NextId = 1;

        SelectionKind m_SelectionKind = SelectionKind::Home;
        ItemID m_SelectionId = NO_ID;
        ItemID m_SelectedTask = NO_ID;
        HomeConfig m_Home;

        std::vector<ResetTask> m_LastReset;
        bool m_Dirty = false;
    };

    const char* getPriorityName(Priority priority);

}
