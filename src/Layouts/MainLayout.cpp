#include <Pch.hpp>

#include "MainLayout.hpp"
#include "../Widgets/Widgets.hpp"

#include <algorithm>
#include <cfloat>
#include <format>

namespace TaskManager {

    namespace {

        constexpr const char* PAYLOAD_FOLDER = "TM_FOLDER";
        constexpr const char* PAYLOAD_LIST = "TM_LIST";
        constexpr const char* PAYLOAD_TASK = "TM_TASK";

        void setDragPayload(const char* type, const ItemID id, const std::string& label) {
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(type, &id, sizeof(id));
                ImGui::TextUnformatted(label.c_str());
                ImGui::EndDragDropSource();
            }
        }

        std::optional<ItemID> acceptPayload(const char* type) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type)) {
                ItemID id = NO_ID;
                std::memcpy(&id, payload->Data, sizeof(id));
                return id;
            }
            return std::nullopt;
        }

        // Draws text against the right edge of the last item, without affecting the layout. Nothing is drawn when it
        // would overlap the label, which ends at labelEndX.
        void drawRightAligned(const float rightX, const float labelEndX, const std::string& text, const ImVec4& color) {
            const ImVec2 size = ImGui::CalcTextSize(text.c_str());
            const float x = rightX - size.x - ImGui::GetStyle().FramePadding.x;
            if (x < labelEndX + ImGui::GetStyle().ItemSpacing.x) {
                return;
            }
            const float y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y - size.y) * 0.5f;
            ImGui::GetWindowDrawList()->AddText(ImVec2(x, y), ImGui::GetColorU32(color), text.c_str());
        }

        // End of the label of a tree node that started at startX, the label comes after the arrow
        float treeLabelEnd(const float startX, const std::string& label) {
            return startX + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::CalcTextSize(label.c_str()).x;
        }

        // Continues on the same line when an item of the given width still fits before rightX, otherwise wraps
        void sameLineIfFits(const float rightX, const float width) {
            if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= rightX) {
                ImGui::SameLine();
            }
        }

        float buttonWidth(const char* label) {
            return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        }

        bool coloredButton(const char* label, const ImVec4& color) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x, color.y, color.z, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 0.65f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x, color.y, color.z, 0.85f));
            const bool pressed = ImGui::Button(label);
            ImGui::PopStyleColor(3);
            return pressed;
        }

        void progressBar(const Size done, const Size total, const char* emptyLabel, const char* completeLabel, const float width) {
            const float fraction = total > 0 ? static_cast<float>(done) / static_cast<float>(total) : 0.0f;
            const bool complete = total > 0 && done == total;
            const std::string overlay = total == 0 ? emptyLabel
                : complete ? completeLabel
                : std::format("{} of {} done", done, total);
            if (complete) {
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.20f, 0.65f, 0.30f, 1.0f));
            }
            ImGui::ProgressBar(fraction, ImVec2(width, 0.0f), overlay.c_str());
            if (complete) {
                ImGui::PopStyleColor();
            }
        }

        bool byDueDate(const Task* a, const Task* b) {
            // Tasks without a due date go last
            const Date dueA = a->dueDate == NO_DATE ? 99999999 : a->dueDate;
            const Date dueB = b->dueDate == NO_DATE ? 99999999 : b->dueDate;
            if (dueA != dueB) return dueA < dueB;
            return a->priority > b->priority;
        }

        bool byPriority(const Task* a, const Task* b) {
            if (a->priority != b->priority) return a->priority > b->priority;
            const Date dueA = a->dueDate == NO_DATE ? 99999999 : a->dueDate;
            const Date dueB = b->dueDate == NO_DATE ? 99999999 : b->dueDate;
            return dueA < dueB;
        }

    }

    MainLayout::MainLayout(App::AppData& appData, Store& store) :
    Layout(),
    m_AppData(appData),
    m_Store(store),
    m_NamePopup(createSP<NamePopup>(appData)),
    m_ConfirmPopup(createSP<ConfirmPopup>(appData)) {
        m_AppData.addPopup(m_NamePopup);
        m_AppData.addPopup(m_ConfirmPopup);
    }

    SP<App::Node> MainLayout::getLayout() {
        using namespace Velyra::App;

        const bool showDetails = m_Store.findTask(m_Store.selectedTask()) != nullptr;
        return createLayout(
            horizontalSplit(
                createPanel({
                    .name = "Explorer",
                    .drawFunction = bindDraw(this, &MainLayout::drawExplorer),
                    .sizeRatio = 0.22f
                }),
                createPanel({
                    .name = "Tasks",
                    .drawFunction = bindDraw(this, &MainLayout::drawContent)
                }),
                showDetails ? createPanel({
                    .name = "Details",
                    .drawFunction = bindDraw(this, &MainLayout::drawDetails),
                    .sizeRatio = 0.28f
                }) : nullptr
            )
        );
    }

    void MainLayout::newFolderDialog(const ItemID parentId) {
        const Folder* parent = m_Store.findFolder(parentId);
        const std::string prompt = parent != nullptr ? "New folder in " + parent->name : "New folder";
        m_NamePopup->open(prompt, "", "Create", [this, parentId](const std::string& name) {
            const ItemID id = m_Store.createFolder(name, parentId);
            if (Folder* parentFolder = m_Store.findFolder(parentId)) {
                parentFolder->expanded = true;
            }
            m_Store.select(SelectionKind::Folder, id);
        });
    }

    void MainLayout::newListDialog(const ItemID folderId, const ListKind kind) {
        const Folder* folder = m_Store.findFolder(folderId);
        std::string prompt = kind == ListKind::Checklist ? "New checklist" : "New task list";
        if (folder != nullptr) {
            prompt += " in " + folder->name;
        }
        m_NamePopup->open(prompt, "", "Create", [this, folderId, kind](const std::string& name) {
            const ItemID id = m_Store.createList(name, folderId, kind);
            if (Folder* parent = m_Store.findFolder(folderId)) {
                parent->expanded = true;
            }
            m_Store.select(SelectionKind::List, id);
            m_FocusNewTask = true;
        });
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Explorer
    // ---------------------------------------------------------------------------------------------------------------

    void MainLayout::drawExplorer() {
        handleShortcuts();

        // Home, with the number of tasks that need attention today
        const Date now = today();
        Size urgent = 0;
        for (const Task& task : m_Store.getTasks()) {
            const TaskList* list = m_Store.findList(task.listId);
            if (!task.done && task.dueDate != NO_DATE && task.dueDate <= now && list != nullptr && !list->isChecklist()) {
                ++urgent;
            }
        }
        const float startX = ImGui::GetCursorScreenPos().x;
        const float rightX = startX + ImGui::GetContentRegionAvail().x;
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.15f);
        if (ImGui::Selectable("Home", m_Store.selectionKind() == SelectionKind::Home, 0, ImVec2(0.0f, ImGui::GetFrameHeight()))) {
            m_Store.select(SelectionKind::Home);
        }
        const float homeEnd = startX + ImGui::CalcTextSize("Home").x;
        ImGui::PopFont();
        if (urgent > 0) {
            drawRightAligned(rightX, homeEnd, std::to_string(urgent), Widgets::PriorityColor(Priority::Urgent));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%zu task(s) due today or overdue", urgent);
            }
        }

        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("FOLDERS & LISTS");
        ImGui::SameLine();
        const float addWidth = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - addWidth);
        if (ImGui::Button("+")) {
            ImGui::OpenPopup("##NewTopLevel");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create a top level folder or list");
        }
        if (ImGui::BeginPopup("##NewTopLevel")) {
            drawNewMenuItems(NO_ID);
            ImGui::EndPopup();
        }
        ImGui::Separator();

        if (ImGui::BeginChild("##Tree")) {
            drawFolderTree(NO_ID);

            // The empty space below the tree accepts drops to move items to the top level
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::InvisibleButton("##TopLevel", ImVec2(std::max(1.0f, avail.x), std::max(avail.y, ImGui::GetFrameHeight())));
            if (ImGui::BeginDragDropTarget()) {
                if (const auto folder = acceptPayload(PAYLOAD_FOLDER)) {
                    defer([this, id = *folder] { m_Store.moveFolder(id, NO_ID); });
                }
                if (const auto list = acceptPayload(PAYLOAD_LIST)) {
                    defer([this, id = *list] { m_Store.moveList(id, NO_ID); });
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::BeginPopupContextItem("##TopLevelMenu")) {
                drawNewMenuItems(NO_ID);
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();

        runDeferred();
    }

    void MainLayout::drawFolderTree(const ItemID parentId) {
        for (const Folder* folder : m_Store.childFolders(parentId)) {
            drawFolderNode(*folder);
        }
        for (const TaskList* list : m_Store.listsIn(parentId)) {
            drawListNode(*list);
        }
    }

    void MainLayout::drawFolderNode(const Folder& folder) {
        ImGui::PushID(static_cast<int>(folder.id));

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        if (m_Store.selectionKind() == SelectionKind::Folder && m_Store.selectionId() == folder.id) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        const float startX = ImGui::GetCursorScreenPos().x;
        const float rightX = startX + ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemOpen(folder.expanded, ImGuiCond_Once);
        const bool open = ImGui::TreeNodeEx("##Folder", flags, "%s", folder.name.c_str());
        if (ImGui::IsItemToggledOpen()) {
            m_Store.findFolder(folder.id)->expanded = open;
        } else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_Store.select(SelectionKind::Folder, folder.id);
        }

        setDragPayload(PAYLOAD_FOLDER, folder.id, folder.name);
        if (ImGui::BeginDragDropTarget()) {
            if (const auto moved = acceptPayload(PAYLOAD_FOLDER)) {
                defer([this, id = *moved, target = folder.id] { m_Store.moveFolder(id, target); });
            }
            if (const auto moved = acceptPayload(PAYLOAD_LIST)) {
                defer([this, id = *moved, target = folder.id] { m_Store.moveList(id, target); });
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##FolderMenu")) {
            drawFolderMenu(folder);
            ImGui::EndPopup();
        }

        const FolderProgress progress = m_Store.folderProgress(folder.id);
        if (progress.total > progress.done) {
            drawRightAligned(rightX, treeLabelEnd(startX, folder.name), std::to_string(progress.total - progress.done),
                             ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        }

        if (open) {
            drawFolderTree(folder.id);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void MainLayout::drawListNode(const TaskList& list) {
        ImGui::PushID(static_cast<int>(list.id));

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                   ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        if (m_Store.selectionKind() == SelectionKind::List && m_Store.selectionId() == list.id) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float rightX = pos.x + ImGui::GetContentRegionAvail().x;
        ImGui::TreeNodeEx("##List", flags, "%s", list.name.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_Store.select(SelectionKind::List, list.id);
        }

        // Leaf nodes leave the space of the arrow empty, the icon of the list kind goes there
        const float fontSize = ImGui::GetFontSize();
        const ImVec2 center(pos.x + ImGui::GetStyle().FramePadding.x + fontSize * 0.5f, pos.y + ImGui::GetFrameHeight() * 0.5f);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (list.isChecklist()) {
            const float half = fontSize * 0.28f;
            drawList->AddRect(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half),
                              ImGui::GetColorU32(ImVec4(0.35f, 0.80f, 0.70f, 1.0f)), 2.0f, 0, std::max(1.0f, Widgets::Scaled(1.5f)));
        } else {
            drawList->AddCircleFilled(center, fontSize * 0.2f, ImGui::GetColorU32(Widgets::PriorityColor(Priority::Low)));
        }

        setDragPayload(PAYLOAD_LIST, list.id, list.name);
        if (ImGui::BeginDragDropTarget()) {
            if (const auto task = acceptPayload(PAYLOAD_TASK)) {
                defer([this, id = *task, target = list.id] { m_Store.moveTaskToList(id, target); });
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##ListMenu")) {
            drawListMenu(list);
            ImGui::EndPopup();
        }

        const auto tasks = m_Store.tasksIn(list.id);
        const Size open = m_Store.countOpen(list.id);
        const ImVec4 dim = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        const float labelEnd = treeLabelEnd(pos.x, list.name);
        if (list.isChecklist() && !tasks.empty()) {
            drawRightAligned(rightX, labelEnd, std::format("{}/{}", tasks.size() - open, tasks.size()), dim);
        } else if (!list.isChecklist() && open > 0) {
            drawRightAligned(rightX, labelEnd, std::to_string(open), dim);
        }

        ImGui::PopID();
    }

    void MainLayout::drawNewMenuItems(const ItemID folderId) {
        if (ImGui::MenuItem("New folder...")) {
            newFolderDialog(folderId);
        }
        if (ImGui::MenuItem("New task list...")) {
            newListDialog(folderId, ListKind::Tasks);
        }
        if (ImGui::MenuItem("New checklist...")) {
            newListDialog(folderId, ListKind::Checklist);
        }
    }

    void MainLayout::drawFolderMenu(const Folder& folder) {
        drawNewMenuItems(folder.id);
        ImGui::Separator();
        if (ImGui::MenuItem("Rename...")) {
            renameFolderDialog(folder);
        }
        const FolderProgress progress = m_Store.folderProgress(folder.id);
        if (ImGui::MenuItem("Reset all tasks", nullptr, false, progress.done > 0)) {
            m_Store.select(SelectionKind::Folder, folder.id);
            m_Store.resetFolder(folder.id);
        }
        if (folder.parentId != NO_ID && ImGui::MenuItem("Move to top level")) {
            defer([this, id = folder.id] { m_Store.moveFolder(id, NO_ID); });
        }
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, Widgets::PriorityColor(Priority::Urgent));
        if (ImGui::MenuItem("Delete...")) {
            deleteFolderDialog(folder);
        }
        ImGui::PopStyleColor();
    }

    void MainLayout::drawListMenu(const TaskList& list) {
        if (ImGui::MenuItem("Rename...")) {
            renameListDialog(list);
        }
        const Size open = m_Store.countOpen(list.id);
        const Size done = m_Store.tasksIn(list.id).size() - open;
        if (ImGui::MenuItem("Reset all tasks", nullptr, false, done > 0)) {
            m_Store.select(SelectionKind::List, list.id);
            m_Store.resetList(list.id);
        }
        if (ImGui::MenuItem(list.isChecklist() ? "Convert to task list" : "Convert to checklist")) {
            TaskList* mutableList = m_Store.findList(list.id);
            mutableList->kind = static_cast<int>(list.isChecklist() ? ListKind::Tasks : ListKind::Checklist);
            m_Store.markDirty();
        }
        if (list.folderId != NO_ID && ImGui::MenuItem("Move to top level")) {
            defer([this, id = list.id] { m_Store.moveList(id, NO_ID); });
        }
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, Widgets::PriorityColor(Priority::Urgent));
        if (ImGui::MenuItem("Delete...")) {
            deleteListDialog(list);
        }
        ImGui::PopStyleColor();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Content
    // ---------------------------------------------------------------------------------------------------------------

    void MainLayout::drawContent() {
        switch (m_Store.selectionKind()) {
            case SelectionKind::Home:   drawHome(); break;
            case SelectionKind::Folder: drawFolderView(m_Store.selectionId()); break;
            case SelectionKind::List:   drawListView(m_Store.selectionId()); break;
        }
        runDeferred();
    }

    void MainLayout::drawHome() {
        const Date now = today();
        const int hour = currentHour();
        const char* greeting = hour < 12 ? "Good morning" : hour < 18 ? "Good afternoon" : "Good evening";
        static const char* weekdays[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        Widgets::Header(greeting, std::format("{} {} {} {}", weekdays[getWeekday(now)], getDay(now),
                                              getMonthName(getMonth(now)), getYear(now)));

        enum BucketIndex { Overdue, Today, Tomorrow, ThisWeek, Later, Undated, CompletedToday, BucketCount };
        struct Bucket {
            const char* name;
            ImVec4 color;
            std::vector<const Task*> tasks;
        };
        const ImVec4 text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        const ImVec4 dim = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        Bucket buckets[BucketCount] = {
            {"Overdue", Widgets::PriorityColor(Priority::Urgent), {}},
            {"Today", Widgets::PriorityColor(Priority::High), {}},
            {"Tomorrow", Widgets::PriorityColor(Priority::Medium), {}},
            {"This week", text, {}},
            {"Later", text, {}},
            {"No due date", dim, {}},
            {"Completed today", dim, {}}
        };

        HomeConfig& config = m_Store.homeConfig();
        for (const Task& task : m_Store.getTasks()) {
            const TaskList* list = m_Store.findList(task.listId);
            if (list == nullptr || list->isChecklist()) {
                continue; // Checklist items have no due date or priority, they stay in their list
            }
            if (task.done) {
                if (config.showCompletedToday && task.completedDate == now) {
                    buckets[CompletedToday].tasks.push_back(&task);
                }
                continue;
            }
            if (task.priority < config.minPriority) {
                continue;
            }
            if (task.dueDate == NO_DATE) {
                if (config.showUndated) {
                    buckets[Undated].tasks.push_back(&task);
                }
                continue;
            }
            const int days = daysBetween(now, task.dueDate);
            const BucketIndex index = days < 0 ? Overdue : days == 0 ? Today : days == 1 ? Tomorrow : days < 7 ? ThisWeek : Later;
            buckets[index].tasks.push_back(&task);
        }
        for (int i = 0; i < Undated; ++i) {
            std::ranges::stable_sort(buckets[i].tasks, byDueDate);
        }
        std::ranges::stable_sort(buckets[Undated].tasks, byPriority);

        // Summary, with the filters on the right
        const Size overdue = buckets[Overdue].tasks.size();
        const Size dueToday = buckets[Today].tasks.size();
        const Size thisWeek = buckets[Tomorrow].tasks.size() + buckets[ThisWeek].tasks.size();
        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        if (overdue + dueToday + thisWeek == 0) {
            ImGui::TextUnformatted("Nothing due this week.");
        } else {
            bool first = true;
            const auto chip = [&](const Size count, const char* label, const ImVec4& color) {
                if (count == 0) {
                    return;
                }
                if (!first) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("-");
                    ImGui::SameLine();
                }
                ImGui::TextColored(color, "%zu %s", count, label);
                first = false;
            };
            chip(overdue, "overdue", buckets[Overdue].color);
            chip(dueToday, "due today", buckets[Today].color);
            chip(thisWeek, "due later this week", buckets[Tomorrow].color);
        }

        ImGui::SameLine();
        const float filterWidth = ImGui::CalcTextSize("Filters").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - filterWidth));
        if (ImGui::Button("Filters")) {
            ImGui::OpenPopup("##Filters");
        }
        if (ImGui::BeginPopup("##Filters")) {
            if (ImGui::Checkbox("Show tasks without due date", &config.showUndated)) {
                m_Store.markDirty();
            }
            if (ImGui::Checkbox("Show tasks completed today", &config.showCompletedToday)) {
                m_Store.markDirty();
            }
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Minimum priority");
            ImGui::SameLine();
            if (Widgets::PriorityCombo("##MinPriority", config.minPriority, Widgets::Scaled(80.0f))) {
                m_Store.markDirty();
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();

        if (ImGui::BeginChild("##Agenda")) {
            bool empty = true;
            for (const Bucket& bucket : buckets) {
                if (bucket.tasks.empty()) {
                    continue;
                }
                empty = false;
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, bucket.color);
                ImGui::SeparatorText(std::format("{} ({})", bucket.name, bucket.tasks.size()).c_str());
                ImGui::PopStyleColor();
                drawTaskTable(bucket.name, bucket.tasks, {.showList = true, .showDetails = true, .allowReorder = false});
            }
            if (empty) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y * 0.3f);
                const char* message = "Nothing on your plate. Enjoy your day!";
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(message).x) * 0.5f));
                ImGui::TextDisabled("%s", message);
            }
        }
        ImGui::EndChild();
    }

    void MainLayout::drawFolderView(const ItemID folderId) {
        const Folder* folder = m_Store.findFolder(folderId);
        if (folder == nullptr) {
            return;
        }
        const std::string parentPath = m_Store.folderPath(folder->parentId);
        Widgets::Header(folder->name, parentPath.empty() ? "Folder" : "Folder in " + parentPath);
        ImGui::Spacing();

        const FolderProgress progress = m_Store.folderProgress(folderId);
        if (ImGui::Button("New task list")) {
            newListDialog(folderId, ListKind::Tasks);
        }
        ImGui::SameLine();
        if (ImGui::Button("New checklist")) {
            newListDialog(folderId, ListKind::Checklist);
        }
        ImGui::SameLine();
        if (ImGui::Button("New subfolder")) {
            newFolderDialog(folderId);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(progress.done == 0);
        if (coloredButton("Reset all tasks", Widgets::PriorityColor(Priority::High))) {
            m_Store.resetFolder(folderId);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Set every task in every list of this folder back to TODO");
        }
        ImGui::SameLine();
        if (ImGui::Button("Rename")) {
            renameFolderDialog(*folder);
        }
        ImGui::SameLine();
        if (coloredButton("Delete", Widgets::PriorityColor(Priority::Urgent))) {
            deleteFolderDialog(*folder);
        }

        drawResetUndo();
        ImGui::Spacing();
        progressBar(progress.done, progress.total, "No tasks yet", "All done!", -FLT_MIN);
        ImGui::Spacing();

        const auto folders = m_Store.childFolders(folderId);
        const auto lists = m_Store.listsIn(folderId);
        if (folders.empty() && lists.empty()) {
            ImGui::TextDisabled("This folder is empty. Create a list or a subfolder to get started.");
            return;
        }

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
        if (ImGui::BeginTable("##Contents", 4, flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Widgets::Scaled(80.0f));
            ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("##Reset", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Reset").x + ImGui::GetStyle().FramePadding.x * 2.0f);
            ImGui::TableHeadersRow();

            const auto row = [&](const ItemID id, const std::string& name, const char* type, const Size done, const Size total,
                                 const SelectionKind kind, const std::function<void()>& reset) {
                ImGui::PushID(static_cast<int>(id));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                    m_Store.select(kind, id);
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s", type);
                ImGui::TableSetColumnIndex(2);
                progressBar(done, total, "Empty", "Done", -FLT_MIN);
                ImGui::TableSetColumnIndex(3);
                ImGui::BeginDisabled(done == 0);
                if (ImGui::Button("Reset")) {
                    reset();
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            };

            for (const Folder* sub : folders) {
                const FolderProgress subProgress = m_Store.folderProgress(sub->id);
                row(sub->id, sub->name, "Folder", subProgress.done, subProgress.total, SelectionKind::Folder,
                    [this, id = sub->id] { m_Store.resetFolder(id); });
            }
            for (const TaskList* list : lists) {
                const Size total = m_Store.tasksIn(list->id).size();
                const Size done = total - m_Store.countOpen(list->id);
                row(list->id, list->name, list->isChecklist() ? "Checklist" : "Task list", done, total, SelectionKind::List,
                    [this, id = list->id] { m_Store.resetList(id); });
            }
            ImGui::EndTable();
        }
    }

    void MainLayout::drawListView(const ItemID listId) {
        TaskList* list = m_Store.findList(listId);
        if (list == nullptr) {
            return;
        }
        const bool checklist = list->isChecklist();
        const std::vector<const Task*> tasks = m_Store.tasksIn(listId);
        const Size total = tasks.size();
        const Size done = total - m_Store.countOpen(listId);

        const std::string folderPath = m_Store.folderPath(list->folderId);
        const char* kindName = checklist ? "Checklist" : "Task list";
        Widgets::Header(list->name, folderPath.empty() ? kindName : std::format("{} in {}", kindName, folderPath));
        ImGui::Spacing();

        // Toolbar, wraps when the panel is narrow
        const float rightX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        ImGui::BeginDisabled(done == 0);
        if (coloredButton("Reset all", Widgets::PriorityColor(Priority::High))) {
            m_Store.resetList(listId);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Set every %s back to TODO", checklist ? "item" : "task");
        }
        sameLineIfFits(rightX, buttonWidth("Delete completed"));
        ImGui::BeginDisabled(done == 0);
        if (ImGui::Button("Delete completed")) {
            m_ConfirmPopup->open(std::format("Delete the {} completed item(s) of \"{}\"? This cannot be undone.", done, list->name),
                                 "Delete", [this, listId] { m_Store.deleteDone(listId); });
        }
        ImGui::EndDisabled();
        sameLineIfFits(rightX, ImGui::GetFrameHeight() + buttonWidth("Hide completed"));
        if (ImGui::Checkbox("Hide completed", &list->hideDone)) {
            m_Store.markDirty();
        }
        if (!checklist) {
            sameLineIfFits(rightX, buttonWidth("Sort by") + Widgets::Scaled(110.0f) + ImGui::GetStyle().ItemSpacing.x);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Sort by");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(Widgets::Scaled(110.0f));
            if (ImGui::Combo("##Sort", &list->sortMode, "Manual\0Due date\0Priority\0")) {
                m_Store.markDirty();
            }
        }

        drawResetUndo();
        ImGui::Spacing();
        progressBar(done, total, "Empty", checklist ? "All checked!" : "All done!", -FLT_MIN);
        ImGui::Spacing();
        drawAddTask(*list);
        ImGui::Spacing();

        if (ImGui::BeginChild("##Tasks")) {
            if (tasks.empty()) {
                ImGui::TextDisabled(checklist ? "No items yet, add them above." : "No tasks yet, add them above.");
            }

            if (checklist) {
                // Checklist items keep their place when checked, so a packing list does not shuffle around
                std::vector<const Task*> visible;
                std::ranges::copy_if(tasks, std::back_inserter(visible), [&](const Task* t) { return !list->hideDone || !t->done; });
                drawTaskTable("##Items", visible, {.showList = false, .showDetails = false, .allowReorder = true});
            } else {
                std::vector<const Task*> open;
                std::vector<const Task*> completed;
                for (const Task* task : tasks) {
                    (task->done ? completed : open).push_back(task);
                }
                const auto sortMode = static_cast<SortMode>(list->sortMode);
                if (sortMode == SortMode::DueDate) {
                    std::ranges::stable_sort(open, byDueDate);
                } else if (sortMode == SortMode::Priority) {
                    std::ranges::stable_sort(open, byPriority);
                }
                drawTaskTable("##Open", open, {.showList = false, .showDetails = true, .allowReorder = sortMode == SortMode::Manual});

                if (!list->hideDone && !completed.empty()) {
                    ImGui::Spacing();
                    if (ImGui::CollapsingHeader(std::format("Completed ({})###Completed", completed.size()).c_str())) {
                        drawTaskTable("##Done", completed, {.showList = false, .showDetails = true, .allowReorder = false});
                    }
                }
            }
        }
        ImGui::EndChild();
    }

    void MainLayout::drawAddTask(const TaskList& list) {
        const ImGuiStyle& style = ImGui::GetStyle();
        const bool checklist = list.isChecklist();
        const float addWidth = buttonWidth("Add");
        const float priorityWidth = Widgets::Scaled(80.0f);
        const float dateWidth = Widgets::Scaled(100.0f);
        const float detailsWidth = checklist ? 0.0f : priorityWidth + dateWidth + 2.0f * style.ItemSpacing.x;
        float inputWidth = ImGui::GetContentRegionAvail().x - addWidth - style.ItemSpacing.x;
        // The priority and due date go on their own line when they leave too little room to type
        const bool wrapDetails = inputWidth - detailsWidth < Widgets::Scaled(200.0f);
        if (!wrapDetails) {
            inputWidth -= detailsWidth;
        }

        if (m_FocusNewTask) {
            ImGui::SetKeyboardFocusHere();
            m_FocusNewTask = false;
        }
        ImGui::SetNextItemWidth(inputWidth);
        bool submit = ImGui::InputTextWithHint("##NewTask", checklist ? "Add an item and press Enter" : "Add a task and press Enter",
                                               &m_NewTitle, ImGuiInputTextFlags_EnterReturnsTrue);
        const auto drawDetails = [&] {
            Widgets::PriorityCombo("##NewPriority", m_NewPriority, priorityWidth);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Priority of the new task");
            }
            ImGui::SameLine();
            Widgets::DatePicker("##NewDueDate", m_NewDueDate, dateWidth);
        };
        if (!checklist && !wrapDetails) {
            ImGui::SameLine();
            drawDetails();
        }
        ImGui::SameLine();
        const std::string title = Widgets::Trimmed(m_NewTitle);
        ImGui::BeginDisabled(title.empty());
        submit |= ImGui::Button("Add");
        ImGui::EndDisabled();
        if (!checklist && wrapDetails) {
            drawDetails();
        }

        if (submit && !title.empty()) {
            defer([this, listId = list.id, title, priority = m_NewPriority, dueDate = checklist ? NO_DATE : m_NewDueDate] {
                m_Store.createTask(listId, title, static_cast<Priority>(priority), dueDate);
            });
            m_NewTitle.clear();
            m_NewPriority = static_cast<int>(Priority::None);
            m_NewDueDate = NO_DATE;
            m_FocusNewTask = true; // Enter deactivates the input, keep typing the next task
        }
    }

    void MainLayout::drawResetUndo() {
        const Size count = m_Store.lastResetCount();
        if (count == 0) {
            return;
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Widgets::PriorityColor(Priority::Medium), "%zu item(s) were set back to TODO.", count);
        ImGui::SameLine();
        if (ImGui::SmallButton("Undo")) {
            m_Store.undoReset();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss")) {
            m_Store.forgetReset();
        }
    }

    void MainLayout::drawTaskTable(const char* id, const std::vector<const Task*>& tasks, const TableOptions& options) {
        if (tasks.empty()) {
            return;
        }
        const int columns = 2 + (options.showList ? 1 : 0) + (options.showDetails ? 2 : 0);
        constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
        if (!ImGui::BeginTable(id, columns, flags)) {
            return;
        }
        ImGui::TableSetupColumn("##Done", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        ImGui::TableSetupColumn("Task", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        if (options.showList) {
            ImGui::TableSetupColumn("List", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        }
        if (options.showDetails) {
            const float padding = ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Medium").x + Widgets::Scaled(12.0f) + padding);
            ImGui::TableSetupColumn("Due", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Wed 30 Sep 2026").x + padding);
        }

        const ImVec4 dim = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        for (const Task* task : tasks) {
            ImGui::PushID(static_cast<int>(task->id));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            bool done = task->done;
            if (ImGui::Checkbox("##Done", &done)) {
                m_Store.setDone(task->id, done);
            }

            // The selectable spans the whole row, the checkbox before it keeps receiving its clicks
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            const ImVec2 textPos = ImGui::GetCursorScreenPos();
            const float columnWidth = ImGui::GetContentRegionAvail().x;
            const bool selected = m_Store.selectedTask() == task->id;
            if (task->done) {
                ImGui::PushStyleColor(ImGuiCol_Text, dim);
            }
            const std::string label = task->title.empty() ? "(untitled)" : task->title;
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                m_Store.selectTask(selected ? NO_ID : task->id);
            }
            if (task->done) {
                ImGui::PopStyleColor();
            }
            const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
            if (task->done) {
                const float y = textPos.y + labelSize.y * 0.55f;
                ImGui::GetWindowDrawList()->AddLine(ImVec2(textPos.x, y), ImVec2(textPos.x + labelSize.x, y),
                                                    ImGui::GetColorU32(dim), std::max(1.0f, Widgets::Scaled(1.0f)));
            }
            if (!task->notes.empty()) {
                ImGui::GetWindowDrawList()->AddText(ImVec2(textPos.x + labelSize.x + ImGui::GetStyle().ItemSpacing.x, textPos.y),
                                                    ImGui::GetColorU32(dim), "[notes]");
            }
            const bool truncated = labelSize.x > columnWidth;
            if ((truncated || !task->notes.empty()) && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::BeginTooltip();
                if (truncated) {
                    ImGui::TextUnformatted(label.c_str());
                }
                if (!task->notes.empty()) {
                    ImGui::TextDisabled("%s", task->notes.c_str());
                }
                ImGui::EndTooltip();
            }

            setDragPayload(PAYLOAD_TASK, task->id, label);
            if (options.allowReorder && ImGui::BeginDragDropTarget()) {
                if (const auto moved = acceptPayload(PAYLOAD_TASK)) {
                    defer([this, movedId = *moved, before = task->id] { m_Store.moveTaskBefore(movedId, before); });
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::BeginPopupContextItem("##TaskMenu")) {
                if (ImGui::MenuItem(task->done ? "Mark as TODO" : "Mark as done")) {
                    m_Store.setDone(task->id, !task->done);
                }
                if (ImGui::MenuItem("Edit...")) {
                    m_Store.selectTask(task->id);
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, Widgets::PriorityColor(Priority::Urgent));
                if (ImGui::MenuItem("Delete...")) {
                    deleteTaskDialog(*task);
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            int column = 2;
            if (options.showList) {
                ImGui::TableSetColumnIndex(column++);
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s", m_Store.listPath(task->listId).c_str());
            }
            if (options.showDetails) {
                ImGui::TableSetColumnIndex(column++);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y - Widgets::Scaled(1.0f));
                Widgets::PriorityTag(static_cast<Priority>(task->priority));

                ImGui::TableSetColumnIndex(column);
                if (task->dueDate != NO_DATE) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(task->done ? dim : Widgets::DueDateColor(task->dueDate), "%s", formatRelative(task->dueDate).c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", formatDate(task->dueDate).c_str());
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Details
    // ---------------------------------------------------------------------------------------------------------------

    void MainLayout::drawDetails() {
        Task* task = m_Store.findTask(m_Store.selectedTask());
        if (task == nullptr) {
            return;
        }
        const ItemID taskId = task->id;
        const TaskList* list = m_Store.findList(task->listId);
        const bool checklist = list != nullptr && list->isChecklist();
        const ImGuiStyle& style = ImGui::GetStyle();

        bool done = task->done;
        if (ImGui::Checkbox(checklist ? "Checked" : "Done", &done)) {
            m_Store.setDone(taskId, done);
        }
        ImGui::SameLine();
        const float closeWidth = ImGui::CalcTextSize("Close").x + style.FramePadding.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - closeWidth);
        if (ImGui::Button("Close")) {
            m_Store.selectTask(NO_ID);
            return;
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Title");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##Title", &task->title)) {
            m_Store.markDirty();
        }

        if (!checklist) {
            ImGui::TextDisabled("Priority");
            if (Widgets::PriorityCombo("##Priority", task->priority, -FLT_MIN)) {
                m_Store.markDirty();
            }
            ImGui::TextDisabled("Due date");
            if (Widgets::DatePicker("##DueDate", task->dueDate, -FLT_MIN)) {
                m_Store.markDirty();
            }
        }

        ImGui::TextDisabled("List");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##List", m_Store.listPath(task->listId).c_str())) {
            for (const TaskList* option : m_Store.allLists()) {
                if (ImGui::Selectable(m_Store.listPath(option->id).c_str(), option->id == task->listId)) {
                    defer([this, taskId, target = option->id] { m_Store.moveTaskToList(taskId, target); });
                }
            }
            ImGui::EndCombo();
        }
        if (!(m_Store.selectionKind() == SelectionKind::List && m_Store.selectionId() == task->listId)) {
            if (ImGui::SmallButton("Go to list")) {
                m_Store.select(SelectionKind::List, task->listId);
                m_Store.selectTask(taskId);
            }
        }

        ImGui::TextDisabled("Notes");
        const float footerHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f + ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y;
        const float notesHeight = std::max(Widgets::Scaled(80.0f), ImGui::GetContentRegionAvail().y - footerHeight);
        if (ImGui::InputTextMultiline("##Notes", &task->notes, ImVec2(-FLT_MIN, notesHeight))) {
            m_Store.markDirty();
        }

        if (task->createdDate != NO_DATE) {
            ImGui::TextDisabled("Created %s", formatDate(task->createdDate).c_str());
        }
        if (task->done && task->completedDate != NO_DATE) {
            ImGui::TextDisabled("Completed %s", formatDate(task->completedDate).c_str());
        }
        if (coloredButton(checklist ? "Delete item" : "Delete task", Widgets::PriorityColor(Priority::Urgent))) {
            deleteTaskDialog(*task);
        }

        runDeferred();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Dialogs
    // ---------------------------------------------------------------------------------------------------------------

    void MainLayout::renameFolderDialog(const Folder& folder) {
        m_NamePopup->open("Rename folder", folder.name, "Rename", [this, id = folder.id](const std::string& name) {
            m_Store.renameFolder(id, name);
        });
    }

    void MainLayout::renameListDialog(const TaskList& list) {
        m_NamePopup->open("Rename list", list.name, "Rename", [this, id = list.id](const std::string& name) {
            m_Store.renameList(id, name);
        });
    }

    void MainLayout::deleteFolderDialog(const Folder& folder) {
        const FolderProgress progress = m_Store.folderProgress(folder.id);
        const std::string message = progress.lists == 0
            ? std::format("Delete the folder \"{}\"?", folder.name)
            : std::format("Delete the folder \"{}\" with its {} list(s) and {} task(s)? This cannot be undone.",
                          folder.name, progress.lists, progress.total);
        m_ConfirmPopup->open(message, "Delete", [this, id = folder.id] { m_Store.deleteFolder(id); });
    }

    void MainLayout::deleteListDialog(const TaskList& list) {
        const Size count = m_Store.tasksIn(list.id).size();
        const std::string message = count == 0
            ? std::format("Delete the list \"{}\"?", list.name)
            : std::format("Delete the list \"{}\" with its {} task(s)? This cannot be undone.", list.name, count);
        m_ConfirmPopup->open(message, "Delete", [this, id = list.id] { m_Store.deleteList(id); });
    }

    void MainLayout::deleteTaskDialog(const Task& task) {
        m_ConfirmPopup->open(std::format("Delete \"{}\"?", task.title), "Delete", [this, id = task.id] {
            m_Store.deleteTask(id);
        });
    }

    void MainLayout::handleShortcuts() {
        if (ImGui::GetIO().WantTextInput || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            m_Store.selectTask(NO_ID);
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N) && m_Store.selectionKind() == SelectionKind::List) {
            m_FocusNewTask = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            if (const Task* task = m_Store.findTask(m_Store.selectedTask())) {
                deleteTaskDialog(*task);
            }
        }
    }

    void MainLayout::defer(std::function<void()> action) {
        m_Deferred.push_back(std::move(action));
    }

    void MainLayout::runDeferred() {
        std::vector<std::function<void()>> actions = std::move(m_Deferred);
        m_Deferred.clear();
        for (const auto& action : actions) {
            action();
        }
    }

}
