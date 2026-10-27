#include <Pch.hpp>

#include "Widgets.hpp"

#include <algorithm>
#include <format>

namespace TaskManager::Widgets {

    namespace {

        bool calendar(Date& date) {
            const Date now = today();
            const Date initial = date != NO_DATE ? date : now;

            // The shown month is kept in the storage of the popup, it starts on the selected date every time it opens
            ImGuiStorage* storage = ImGui::GetStateStorage();
            const ImGuiID monthKey = ImGui::GetID("##ShownMonth");
            int shown = storage->GetInt(monthKey, NO_DATE);
            if (shown == NO_DATE || ImGui::IsWindowAppearing()) {
                shown = makeDate(getYear(initial), getMonth(initial), 1);
            }

            bool changed = false;
            const float arrowWidth = ImGui::GetFrameHeight();
            if (ImGui::ArrowButton("##Previous", ImGuiDir_Left)) {
                shown = addMonths(shown, -1);
            }
            ImGui::SameLine();
            const std::string title = std::format("{} {}", getMonthName(getMonth(shown)), getYear(shown));
            const float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
            const float gridWidth = Scaled(7 * 34.0f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (gridWidth - 2 * arrowWidth - titleWidth) * 0.5f - ImGui::GetStyle().ItemSpacing.x));
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(title.c_str());
            ImGui::SameLine(gridWidth - arrowWidth + ImGui::GetStyle().WindowPadding.x);
            if (ImGui::ArrowButton("##Next", ImGuiDir_Right)) {
                shown = addMonths(shown, 1);
            }
            storage->SetInt(monthKey, shown);

            constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoPadOuterX;
            if (ImGui::BeginTable("##Calendar", 7, flags)) {
                static const char* weekdays[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
                const ImVec4 dim = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
                ImGui::TableNextRow();
                for (int i = 0; i < 7; ++i) {
                    ImGui::TableSetColumnIndex(i);
                    ImGui::TextColored(dim, " %s", weekdays[i]);
                }

                const int firstWeekday = getWeekday(shown);
                const int days = daysInMonth(getYear(shown), getMonth(shown));
                const ImVec2 cellSize(Scaled(30.0f), 0.0f);
                for (int cell = 0; cell < firstWeekday + days; ++cell) {
                    if (cell % 7 == 0) {
                        ImGui::TableNextRow();
                    }
                    ImGui::TableSetColumnIndex(cell % 7);
                    if (cell < firstWeekday) {
                        continue;
                    }
                    const int day = cell - firstWeekday + 1;
                    const Date cellDate = makeDate(getYear(shown), getMonth(shown), day);
                    const bool selected = cellDate == date;
                    const bool isToday = cellDate == now;

                    ImGui::PushID(day);
                    if (selected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    } else if (!isToday) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    }
                    if (ImGui::Button(std::to_string(day).c_str(), cellSize)) {
                        date = cellDate;
                        changed = true;
                    }
                    if (selected || !isToday) {
                        ImGui::PopStyleColor();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            return changed;
        }

    }

    float Scaled(const float pixels) {
        return pixels * ImGui::GetFontSize() / 13.0f;
    }

    ImVec4 PriorityColor(const Priority priority) {
        switch (priority) {
            case Priority::Low:    return {0.45f, 0.70f, 1.00f, 1.0f};
            case Priority::Medium: return {0.95f, 0.80f, 0.25f, 1.0f};
            case Priority::High:   return {1.00f, 0.55f, 0.15f, 1.0f};
            case Priority::Urgent: return {0.95f, 0.25f, 0.25f, 1.0f};
            default:               return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        }
    }

    ImVec4 DueDateColor(const Date dueDate) {
        if (dueDate == NO_DATE) {
            return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        }
        const int days = daysBetween(today(), dueDate);
        if (days < 0) return PriorityColor(Priority::Urgent);
        if (days == 0) return PriorityColor(Priority::High);
        if (days < 7) return PriorityColor(Priority::Medium);
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }

    void PriorityTag(const Priority priority) {
        if (priority == Priority::None) {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
            return;
        }
        const char* label = getPriorityName(priority);
        const ImVec4 color = PriorityColor(priority);
        const ImVec2 padding(Scaled(6.0f), Scaled(1.0f));
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                                ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.18f)), size.y * 0.5f);
        drawList->AddText(ImVec2(pos.x + padding.x, pos.y + padding.y), ImGui::GetColorU32(color), label);
        ImGui::Dummy(size);
    }

    bool PriorityCombo(const char* label, int& priority, const float width) {
        bool changed = false;
        const auto current = static_cast<Priority>(priority);
        ImGui::SetNextItemWidth(width);
        ImGui::PushStyleColor(ImGuiCol_Text, PriorityColor(current));
        const bool open = ImGui::BeginCombo(label, getPriorityName(current));
        ImGui::PopStyleColor();
        if (open) {
            for (int i = 0; i < PRIORITY_COUNT; ++i) {
                const auto option = static_cast<Priority>(i);
                ImGui::PushStyleColor(ImGuiCol_Text, PriorityColor(option));
                if (ImGui::Selectable(getPriorityName(option), i == priority)) {
                    priority = i;
                    changed = true;
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool DatePicker(const char* id, Date& date, const float width) {
        ImGui::PushID(id);
        bool changed = false;

        const std::string label = date != NO_DATE ? formatRelative(date) : "No due date";
        ImGui::PushStyleColor(ImGuiCol_Text, DueDateColor(date));
        if (ImGui::Button(label.c_str(), ImVec2(width, 0.0f))) {
            ImGui::OpenPopup("##Picker");
        }
        ImGui::PopStyleColor();
        if (date != NO_DATE && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", formatDate(date).c_str());
        }

        if (ImGui::BeginPopup("##Picker")) {
            const Date now = today();
            const struct { const char* label; Date value; } quickPicks[] = {
                {"Today", now},
                {"Tomorrow", addDays(now, 1)},
                {"Next week", addDays(now, 7)},
                {"Next month", addMonths(now, 1)},
                {"No date", NO_DATE}
            };
            for (Size i = 0; i < std::size(quickPicks); ++i) {
                if (i > 0) {
                    ImGui::SameLine();
                }
                if (ImGui::SmallButton(quickPicks[i].label)) {
                    date = quickPicks[i].value;
                    changed = true;
                }
            }
            ImGui::Separator();
            if (calendar(date)) {
                changed = true;
            }
            if (changed) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
        return changed;
    }

    std::string Trimmed(const std::string& text) {
        const Size begin = text.find_first_not_of(" \t");
        if (begin == std::string::npos) {
            return {};
        }
        return text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
    }

    void Header(const std::string& title, const std::string& subtitle) {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.6f);
        ImGui::TextUnformatted(title.c_str());
        ImGui::PopFont();
        if (!subtitle.empty()) {
            ImGui::TextDisabled("%s", subtitle.c_str());
        }
    }

}
