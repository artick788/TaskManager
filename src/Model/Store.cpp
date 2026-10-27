#include <Pch.hpp>

#include "Store.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace TaskManager {

    namespace {

        bool isDigit(const char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        }

        // Compares digit runs numerically so that "Day 2" sorts before "Day 10", case-insensitive otherwise.
        bool naturalLess(const std::string& a, const std::string& b) {
            Size i = 0;
            Size j = 0;
            while (i < a.size() && j < b.size()) {
                if (isDigit(a[i]) && isDigit(b[j])) {
                    Size iEnd = i;
                    Size jEnd = j;
                    while (iEnd < a.size() && isDigit(a[iEnd])) ++iEnd;
                    while (jEnd < b.size() && isDigit(b[jEnd])) ++jEnd;
                    // Skip leading zeros but keep at least one digit.
                    while (i + 1 < iEnd && a[i] == '0') ++i;
                    while (j + 1 < jEnd && b[j] == '0') ++j;
                    const Size lenA = iEnd - i;
                    const Size lenB = jEnd - j;
                    if (lenA != lenB) {
                        return lenA < lenB;
                    }
                    const int cmp = a.compare(i, lenA, b, j, lenB);
                    if (cmp != 0) {
                        return cmp < 0;
                    }
                    i = iEnd;
                    j = jEnd;
                } else {
                    const int ca = std::tolower(static_cast<unsigned char>(a[i]));
                    const int cb = std::tolower(static_cast<unsigned char>(b[j]));
                    if (ca != cb) {
                        return ca < cb;
                    }
                    ++i;
                    ++j;
                }
            }
            return (a.size() - i) < (b.size() - j);
        }

        template<typename T>
        T* findById(std::vector<T>& items, const ItemID id) {
            const auto it = std::ranges::find_if(items, [id](const T& item) { return item.id == id; });
            return it != items.end() ? &*it : nullptr;
        }

        template<typename T>
        const T* findById(const std::vector<T>& items, const ItemID id) {
            const auto it = std::ranges::find_if(items, [id](const T& item) { return item.id == id; });
            return it != items.end() ? &*it : nullptr;
        }

    }

    const char* getPriorityName(const Priority priority) {
        switch (priority) {
            case Priority::Low:    return "Low";
            case Priority::Medium: return "Medium";
            case Priority::High:   return "High";
            case Priority::Urgent: return "Urgent";
            default:               return "None";
        }
    }

    ItemID Store::createFolder(const std::string& name, const ItemID parentId) {
        Folder folder;
        folder.id = nextId();
        folder.parentId = findFolder(parentId) != nullptr ? parentId : NO_ID;
        folder.name = name;
        m_Folders.push_back(std::move(folder));
        markDirty();
        return m_Folders.back().id;
    }

    void Store::renameFolder(const ItemID id, const std::string& name) {
        if (Folder* folder = findFolder(id)) {
            folder->name = name;
            markDirty();
        }
    }

    void Store::deleteFolder(const ItemID id) {
        std::unordered_set<ItemID> folders;
        for (const Folder& folder : m_Folders) {
            if (isInside(folder.id, id)) {
                folders.insert(folder.id);
            }
        }
        std::unordered_set<ItemID> lists;
        for (const TaskList& list : m_Lists) {
            if (folders.contains(list.folderId)) {
                lists.insert(list.id);
            }
        }
        std::erase_if(m_Tasks, [&](const Task& t) { return lists.contains(t.listId); });
        std::erase_if(m_Lists, [&](const TaskList& l) { return lists.contains(l.id); });
        std::erase_if(m_Folders, [&](const Folder& f) { return folders.contains(f.id); });
        validateSelection();
        markDirty();
    }

    bool Store::moveFolder(const ItemID id, const ItemID newParentId) {
        Folder* folder = findFolder(id);
        if (folder == nullptr || (newParentId != NO_ID && isInside(newParentId, id))) {
            return false;
        }
        folder->parentId = newParentId;
        markDirty();
        return true;
    }

    Folder* Store::findFolder(const ItemID id) {
        return findById(m_Folders, id);
    }

    const Folder* Store::findFolder(const ItemID id) const {
        return findById(m_Folders, id);
    }

    std::vector<const Folder*> Store::childFolders(const ItemID parentId) const {
        std::vector<const Folder*> result;
        for (const Folder& folder : m_Folders) {
            if (folder.parentId == parentId) {
                result.push_back(&folder);
            }
        }
        std::ranges::sort(result, [](const Folder* a, const Folder* b) { return naturalLess(a->name, b->name); });
        return result;
    }

    bool Store::isInside(ItemID folderId, const ItemID ancestorId) const {
        // The depth guard protects against cycles in a hand-edited settings file
        for (Size depth = 0; folderId != NO_ID && depth <= m_Folders.size(); ++depth) {
            if (folderId == ancestorId) {
                return true;
            }
            const Folder* folder = findFolder(folderId);
            folderId = folder != nullptr ? folder->parentId : NO_ID;
        }
        return false;
    }

    ItemID Store::createList(const std::string& name, const ItemID folderId, const ListKind kind) {
        TaskList list;
        list.id = nextId();
        list.folderId = findFolder(folderId) != nullptr ? folderId : NO_ID;
        list.name = name;
        list.kind = static_cast<int>(kind);
        m_Lists.push_back(std::move(list));
        markDirty();
        return m_Lists.back().id;
    }

    void Store::renameList(const ItemID id, const std::string& name) {
        if (TaskList* list = findList(id)) {
            list->name = name;
            markDirty();
        }
    }

    void Store::deleteList(const ItemID id) {
        std::erase_if(m_Tasks, [id](const Task& t) { return t.listId == id; });
        std::erase_if(m_Lists, [id](const TaskList& l) { return l.id == id; });
        validateSelection();
        markDirty();
    }

    void Store::moveList(const ItemID id, const ItemID folderId) {
        if (TaskList* list = findList(id)) {
            list->folderId = findFolder(folderId) != nullptr ? folderId : NO_ID;
            markDirty();
        }
    }

    TaskList* Store::findList(const ItemID id) {
        return findById(m_Lists, id);
    }

    const TaskList* Store::findList(const ItemID id) const {
        return findById(m_Lists, id);
    }

    std::vector<const TaskList*> Store::listsIn(const ItemID folderId) const {
        std::vector<const TaskList*> result;
        for (const TaskList& list : m_Lists) {
            if (list.folderId == folderId) {
                result.push_back(&list);
            }
        }
        std::ranges::sort(result, [](const TaskList* a, const TaskList* b) { return naturalLess(a->name, b->name); });
        return result;
    }

    std::vector<const TaskList*> Store::allLists() const {
        std::vector<const TaskList*> result;
        for (const TaskList& list : m_Lists) {
            result.push_back(&list);
        }
        std::ranges::sort(result, [this](const TaskList* a, const TaskList* b) {
            return naturalLess(listPath(a->id), listPath(b->id));
        });
        return result;
    }

    std::string Store::listPath(const ItemID listId) const {
        const TaskList* list = findList(listId);
        if (list == nullptr) {
            return {};
        }
        const std::string folder = folderPath(list->folderId);
        return folder.empty() ? list->name : folder + " / " + list->name;
    }

    std::string Store::folderPath(ItemID folderId) const {
        std::string path;
        for (Size depth = 0; folderId != NO_ID && depth <= m_Folders.size(); ++depth) {
            const Folder* folder = findFolder(folderId);
            if (folder == nullptr) {
                break;
            }
            path = path.empty() ? folder->name : folder->name + " / " + path;
            folderId = folder->parentId;
        }
        return path;
    }

    ItemID Store::createTask(const ItemID listId, const std::string& title, const Priority priority, const Date dueDate) {
        Task task;
        task.id = nextId();
        task.listId = listId;
        task.title = title;
        task.priority = static_cast<int>(priority);
        task.dueDate = dueDate;
        task.createdDate = today();
        m_Tasks.push_back(std::move(task));
        markDirty();
        return m_Tasks.back().id;
    }

    void Store::deleteTask(const ItemID id) {
        std::erase_if(m_Tasks, [id](const Task& t) { return t.id == id; });
        if (m_SelectedTask == id) {
            m_SelectedTask = NO_ID;
        }
        markDirty();
    }

    void Store::setDone(const ItemID id, const bool done) {
        if (Task* task = findTask(id); task != nullptr && task->done != done) {
            task->done = done;
            task->completedDate = done ? today() : NO_DATE;
            markDirty();
        }
    }

    void Store::moveTaskToList(const ItemID id, const ItemID listId) {
        Task* task = findTask(id);
        if (task == nullptr || findList(listId) == nullptr || task->listId == listId) {
            return;
        }
        // Moved tasks go to the end of their new list
        Task moved = std::move(*task);
        moved.listId = listId;
        std::erase_if(m_Tasks, [id](const Task& t) { return t.id == id; });
        m_Tasks.push_back(std::move(moved));
        markDirty();
    }

    void Store::moveTaskBefore(const ItemID id, const ItemID beforeId) {
        if (id == beforeId) {
            return;
        }
        const auto it = std::ranges::find_if(m_Tasks, [id](const Task& t) { return t.id == id; });
        if (it == m_Tasks.end()) {
            return;
        }
        Task moved = std::move(*it);
        m_Tasks.erase(it);
        const auto target = std::ranges::find_if(m_Tasks, [beforeId](const Task& t) { return t.id == beforeId; });
        if (target != m_Tasks.end()) {
            moved.listId = target->listId;
        }
        m_Tasks.insert(target, std::move(moved));
        markDirty();
    }

    Task* Store::findTask(const ItemID id) {
        return findById(m_Tasks, id);
    }

    const Task* Store::findTask(const ItemID id) const {
        return findById(m_Tasks, id);
    }

    std::vector<const Task*> Store::tasksIn(const ItemID listId) const {
        std::vector<const Task*> result;
        for (const Task& task : m_Tasks) {
            if (task.listId == listId) {
                result.push_back(&task);
            }
        }
        return result;
    }

    Size Store::countOpen(const ItemID listId) const {
        return static_cast<Size>(std::ranges::count_if(m_Tasks, [listId](const Task& t) {
            return t.listId == listId && !t.done;
        }));
    }

    FolderProgress Store::folderProgress(const ItemID folderId) const {
        FolderProgress progress;
        std::unordered_set<ItemID> lists;
        for (const TaskList& list : m_Lists) {
            if (list.folderId != NO_ID && isInside(list.folderId, folderId)) {
                lists.insert(list.id);
            }
        }
        progress.lists = lists.size();
        for (const Task& task : m_Tasks) {
            if (lists.contains(task.listId)) {
                ++progress.total;
                progress.done += task.done ? 1 : 0;
            }
        }
        return progress;
    }

    Size Store::resetList(const ItemID listId) {
        m_LastReset.clear();
        return resetTasksOf(listId);
    }

    Size Store::resetFolder(const ItemID folderId) {
        m_LastReset.clear();
        Size count = 0;
        for (const TaskList& list : m_Lists) {
            if (list.folderId != NO_ID && isInside(list.folderId, folderId)) {
                count += resetTasksOf(list.id);
            }
        }
        return count;
    }

    void Store::undoReset() {
        for (const ResetTask& reset : m_LastReset) {
            if (Task* task = findTask(reset.id)) {
                task->done = true;
                task->completedDate = reset.completedDate;
            }
        }
        if (!m_LastReset.empty()) {
            markDirty();
        }
        m_LastReset.clear();
    }

    Size Store::resetTasksOf(const ItemID listId) {
        Size count = 0;
        for (Task& task : m_Tasks) {
            if (task.listId == listId && task.done) {
                m_LastReset.push_back({task.id, task.completedDate});
                task.done = false;
                task.completedDate = NO_DATE;
                ++count;
            }
        }
        if (count > 0) {
            markDirty();
        }
        return count;
    }

    Size Store::deleteDone(const ItemID listId) {
        const Size count = std::erase_if(m_Tasks, [listId](const Task& t) { return t.listId == listId && t.done; });
        if (count > 0) {
            if (findTask(m_SelectedTask) == nullptr) {
                m_SelectedTask = NO_ID;
            }
            markDirty();
        }
        return count;
    }

    void Store::select(const SelectionKind kind, const ItemID id) {
        m_SelectionKind = kind;
        m_SelectionId = id;
        m_SelectedTask = NO_ID;
        m_LastReset.clear();
        validateSelection();
    }

    StoreState Store::snapshot() const {
        StoreState state;
        state.folders = m_Folders;
        state.lists = m_Lists;
        state.tasks = m_Tasks;
        state.nextId = m_NextId;
        state.selectionKind = static_cast<int>(m_SelectionKind);
        state.selectionId = m_SelectionId;
        state.home = m_Home;
        state.initialized = true;
        return state;
    }

    void Store::restore(const StoreState& state) {
        m_Folders = state.folders;
        m_Lists = state.lists;
        m_Tasks = state.tasks;
        m_Home = state.home;
        m_Home.minPriority = std::clamp(m_Home.minPriority, 0, PRIORITY_COUNT - 1);

        // Guard against hand-edited settings, ids must stay unique
        ItemID highest = NO_ID;
        for (const Folder& f : m_Folders) highest = std::max(highest, f.id);
        for (const TaskList& l : m_Lists) highest = std::max(highest, l.id);
        for (const Task& t : m_Tasks) highest = std::max(highest, t.id);
        m_NextId = std::max(state.nextId, highest + 1);

        for (TaskList& list : m_Lists) {
            list.kind = std::clamp(list.kind, 0, static_cast<int>(ListKind::Checklist));
            list.sortMode = std::clamp(list.sortMode, 0, static_cast<int>(SortMode::Priority));
        }
        for (Task& task : m_Tasks) {
            task.priority = std::clamp(task.priority, 0, PRIORITY_COUNT - 1);
        }

        const int kind = std::clamp(state.selectionKind, 0, static_cast<int>(SelectionKind::List));
        m_SelectionKind = static_cast<SelectionKind>(kind);
        m_SelectionId = state.selectionId;
        m_SelectedTask = NO_ID;
        validateSelection();

        m_Dirty = false;
        if (!state.initialized) {
            addExampleData(); // Marks the store dirty, so the example data is saved right away
        }
    }

    void Store::addExampleData() {
        const Date now = today();

        const ItemID aura = createFolder("AURA", NO_ID);
        const ItemID ui = createList("UI tasks", aura, ListKind::Tasks);
        createTask(ui, "Design the settings page", Priority::Medium, addDays(now, 3));
        createTask(ui, "Fix scaling on high DPI screens", Priority::High, now);
        const ItemID backend = createList("Backend tasks", aura, ListKind::Tasks);
        createTask(backend, "Write migration for the new schema", Priority::Urgent, addDays(now, -1));
        createTask(backend, "Add logging to the sync service", Priority::Low);

        const ItemID packlists = createFolder("Packlists", NO_ID);
        const ItemID weekend = createList("Weekend trip", packlists, ListKind::Checklist);
        for (const char* item : {"Passport", "Phone charger", "Toothbrush", "Rain jacket", "Headphones"}) {
            createTask(weekend, item);
        }
    }

    void Store::validateSelection() {
        if (m_SelectionKind == SelectionKind::Folder && findFolder(m_SelectionId) == nullptr) {
            m_SelectionKind = SelectionKind::Home;
        }
        if (m_SelectionKind == SelectionKind::List && findList(m_SelectionId) == nullptr) {
            m_SelectionKind = SelectionKind::Home;
        }
        if (m_SelectionKind == SelectionKind::Home) {
            m_SelectionId = NO_ID;
        }
        if (findTask(m_SelectedTask) == nullptr) {
            m_SelectedTask = NO_ID;
        }
    }

}
