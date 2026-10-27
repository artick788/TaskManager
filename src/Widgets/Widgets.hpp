#pragma once

#include "../Model/Store.hpp"

#include <VelyraCore/ImGui/ImGui.hpp>

#include <string>

namespace TaskManager::Widgets {

    // Scales a pixel size designed for ImGui's default 13px font to the current font size, so fixed widths keep
    // fitting their text whatever the font or DPI is.
    float Scaled(float pixels);

    ImVec4 PriorityColor(Priority priority);

    // Color of a due date: red when overdue, orange today, yellow this week
    ImVec4 DueDateColor(Date dueDate);

    // Small colored pill with the name of the priority, draws nothing for Priority::None
    void PriorityTag(Priority priority);

    // Combo with colored priority names, returns true when the priority changed
    bool PriorityCombo(const char* label, int& priority, float width);

    // Button showing the date that opens a calendar with quick picks, returns true when the date changed
    bool DatePicker(const char* id, Date& date, float width);

    // Removes leading and trailing spaces of a name typed by the user
    std::string Trimmed(const std::string& text);

    // Large header with a dimmed subtitle below it
    void Header(const std::string& title, const std::string& subtitle);

}
