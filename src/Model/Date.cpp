#include <Pch.hpp>

#include "Date.hpp"

#include <chrono>
#include <format>

namespace TaskManager {

    namespace {

        namespace chr = std::chrono;

        chr::year_month_day toYmd(const Date date) {
            return chr::year_month_day{
                chr::year{getYear(date)},
                chr::month{static_cast<unsigned>(getMonth(date))},
                chr::day{static_cast<unsigned>(getDay(date))}
            };
        }

        Date fromYmd(const chr::year_month_day& ymd) {
            return makeDate(static_cast<int>(ymd.year()), static_cast<int>(static_cast<unsigned>(ymd.month())),
                            static_cast<int>(static_cast<unsigned>(ymd.day())));
        }

        chr::sys_days toDays(const Date date) {
            return chr::sys_days{toYmd(date)};
        }

    }

    Date today() {
        const chr::zoned_time now{chr::current_zone(), chr::system_clock::now()};
        return fromYmd(chr::year_month_day{chr::floor<chr::days>(now.get_local_time())});
    }

    int currentHour() {
        const chr::zoned_time now{chr::current_zone(), chr::system_clock::now()};
        const auto local = now.get_local_time();
        return static_cast<int>(chr::hh_mm_ss{local - chr::floor<chr::days>(local)}.hours().count());
    }

    Date makeDate(const int year, const int month, const int day) {
        return year * 10000 + month * 100 + day;
    }

    int getYear(const Date date) {
        return date / 10000;
    }

    int getMonth(const Date date) {
        return date / 100 % 100;
    }

    int getDay(const Date date) {
        return date % 100;
    }

    int daysInMonth(const int year, const int month) {
        const chr::year_month_day_last last{chr::year{year} / chr::month{static_cast<unsigned>(month)} / chr::last};
        return static_cast<int>(static_cast<unsigned>(last.day()));
    }

    int getWeekday(const Date date) {
        return static_cast<int>(chr::weekday{toDays(date)}.iso_encoding()) - 1;
    }

    Date addDays(const Date date, const int days) {
        return fromYmd(chr::year_month_day{toDays(date) + chr::days{days}});
    }

    Date addMonths(const Date date, const int months) {
        chr::year_month_day ymd = toYmd(date) + chr::months{months};
        if (!ymd.ok()) {
            // 31 January + 1 month ends on the last day of February
            ymd = chr::year_month_day{chr::year_month_day_last{ymd.year() / ymd.month() / chr::last}};
        }
        return fromYmd(ymd);
    }

    int daysBetween(const Date from, const Date to) {
        return static_cast<int>((toDays(to) - toDays(from)).count());
    }

    const char* getMonthName(const int month) {
        static const char* months[] = {
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        };
        return months[(month - 1) % 12];
    }

    std::string formatDate(const Date date) {
        if (date == NO_DATE) {
            return "No date";
        }
        static const char* weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        const std::string month = std::string(getMonthName(getMonth(date))).substr(0, 3);
        if (getYear(date) == getYear(today())) {
            return std::format("{} {} {}", weekdays[getWeekday(date)], getDay(date), month);
        }
        return std::format("{} {} {} {}", weekdays[getWeekday(date)], getDay(date), month, getYear(date));
    }

    std::string formatRelative(const Date date) {
        if (date == NO_DATE) {
            return "";
        }
        const int days = daysBetween(today(), date);
        if (days == 0) return "Today";
        if (days == 1) return "Tomorrow";
        if (days == -1) return "Yesterday";
        if (days > 1 && days < 7) return std::format("In {} days", days);
        if (days < -1 && days > -7) return std::format("{} days ago", -days);
        return formatDate(date);
    }

}
