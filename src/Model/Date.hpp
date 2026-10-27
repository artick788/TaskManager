#pragma once

#include "../TaskManagerInclude.hpp"

#include <string>

namespace TaskManager {

    // Dates are stored as yyyymmdd integers, they serialize trivially and compare in chronological order.
    using Date = int;

    constexpr Date NO_DATE = 0;

    Date today();

    // Hour of the local time, 0 - 23
    int currentHour();

    Date makeDate(int year, int month, int day);

    int getYear(Date date);

    int getMonth(Date date);

    int getDay(Date date);

    int daysInMonth(int year, int month);

    // 0 = Monday, 6 = Sunday
    int getWeekday(Date date);

    Date addDays(Date date, int days);

    Date addMonths(Date date, int months);

    // Positive if to is after from
    int daysBetween(Date from, Date to);

    const char* getMonthName(int month);

    // "Tue 27 Oct 2026", the year is left out for dates in the current year
    std::string formatDate(Date date);

    // "Today", "Tomorrow", "In 3 days", "2 days ago", falls back to formatDate() for dates further away
    std::string formatRelative(Date date);

}
