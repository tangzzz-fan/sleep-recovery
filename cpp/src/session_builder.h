#ifndef SESSION_BUILDER_H
#define SESSION_BUILDER_H

#include "csv_reader.h"
#include "sleep_detector.h"
#include "hr_aggregator.h"
#include "feature_extractor.h"
#include <string>
#include <vector>

struct NightSession {
    std::string session_id;
    int user_id;
    std::string source_type;
    std::string sleep_start;    // ISO 8601
    std::string sleep_end;      // ISO 8601
    double total_duration_min;
    int awaken_count;
    int restless_count;
    int recovery_label;
};

// build all night sessions for a user from sleep data
// returns list of (session_id, SleepWindow) for nights with sleep data
std::vector<std::pair<std::string, SleepWindow>>
build_user_sessions(const CsvTable& sleep_table,
                    const std::string& user_id,
                    const std::vector<std::string>& dates);

// get the list of unique dates for a user from the sleep CSV
std::vector<std::string> get_user_sleep_dates(const CsvTable& sleep_table,
                                               const std::string& user_id);

// build activity records from minuteIntensities + minuteSteps
std::vector<ActivityRecord> build_activity_records(
    const CsvTable& intensity_table,
    const CsvTable& steps_table,
    const std::string& user_id,
    const std::string& date_prefix);

#endif
