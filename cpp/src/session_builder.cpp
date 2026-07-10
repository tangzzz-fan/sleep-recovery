#include "session_builder.h"
#include "time_utils.h"
#include <set>
#include <sstream>
#include <algorithm>

using SessionInfo = std::pair<std::string, SleepWindow>;

std::vector<std::string> get_user_sleep_dates(const CsvTable& sleep_table,
                                               const std::string& user_id) {
    std::set<std::string> dates;
    for (const auto& row : sleep_table) {
        auto uid_it = row.find("Id");
        auto date_it = row.find("date");
        if (uid_it == row.end() || date_it == row.end()) continue;
        if (uid_it->second != user_id) continue;

        // extract date prefix "M/D/YYYY" from "M/D/YYYY H:MM:SS AM/PM"
        const std::string& ts = date_it->second;
        size_t pos = ts.find(' ');
        if (pos == std::string::npos) continue;
        std::string date = ts.substr(0, pos);
        dates.insert(date);
    }
    return std::vector<std::string>(dates.begin(), dates.end());
}

std::vector<SessionInfo> build_user_sessions(const CsvTable& sleep_table,
                                              const std::string& user_id,
                                              const std::vector<std::string>& dates) {
    std::vector<SessionInfo> sessions;

    for (const std::string& date : dates) {
        auto records = build_sleep_records(sleep_table, user_id, date);
        if (records.empty()) continue;

        SleepWindow window = detect_sleep_window(records);
        if (!window.valid || window.total_minutes < 30) continue;  // skip too-short sessions

        // format session ID: userid_M-D-YYYY
        std::string session_id = user_id + "_" + date;
        // replace '/' with '-'
        for (char& c : session_id) {
            if (c == '/') c = '-';
        }

        sessions.emplace_back(session_id, window);
    }

    return sessions;
}

std::vector<ActivityRecord> build_activity_records(
    const CsvTable& intensity_table,
    const CsvTable& steps_table,
    const std::string& user_id,
    const std::string& date_prefix) {

    // First build steps map
    std::map<long long, int> steps_map;
    for (const auto& row : steps_table) {
        auto uid_it = row.find("Id");
        auto time_it = row.find("ActivityMinute");
        auto steps_it = row.find("Steps");
        if (uid_it == row.end() || time_it == row.end() || steps_it == row.end())
            continue;
        if (uid_it->second != user_id) continue;
        if (time_it->second.find(date_prefix) != 0) continue;

        long long epoch = parse_12h_to_epoch(time_it->second);
        if (epoch < 0) continue;
        int steps = std::stoi(steps_it->second);
        steps_map[epoch] = steps;
    }

    // Now build activity records from intensity table
    std::vector<ActivityRecord> result;
    for (const auto& row : intensity_table) {
        auto uid_it = row.find("Id");
        auto time_it = row.find("ActivityMinute");
        auto int_it = row.find("Intensity");
        if (uid_it == row.end() || time_it == row.end() || int_it == row.end())
            continue;
        if (uid_it->second != user_id) continue;
        if (time_it->second.find(date_prefix) != 0) continue;

        long long epoch = parse_12h_to_epoch(time_it->second);
        if (epoch < 0) continue;
        int intensity = std::stoi(int_it->second);
        // intensity is the activity value; steps from separate table
        result.emplace_back(epoch, intensity);
    }

    std::sort(result.begin(), result.end(),
              [](const ActivityRecord& a, const ActivityRecord& b) {
                  return a.first < b.first;
              });

    return result;
}
