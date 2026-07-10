#ifndef SLEEP_DETECTOR_H
#define SLEEP_DETECTOR_H

#include "csv_reader.h"
#include <string>
#include <vector>
#include <utility>  // pair

struct SleepWindow {
    long long start_epoch;   // first value=1 timestamp
    long long end_epoch;     // last value=1/2 timestamp
    int awakened;            // count of value=3
    int restless;            // count of value=2
    int total_minutes;       // window duration in minutes
    bool valid;

    SleepWindow() : start_epoch(0), end_epoch(0), awakened(0), restless(0),
                    total_minutes(0), valid(false) {}
};

// sleep record: (epoch, value)
using SleepRecord = std::pair<long long, int>;

// detect the main contiguous sleep window from minuteSleep rows
// returns the longest block of sleep (value=1,2) with gaps <= 60 min
SleepWindow detect_sleep_window(const std::vector<SleepRecord>& records);

// build sorted sleep records from CsvTable (minuteSleep_merged.csv)
std::vector<SleepRecord> build_sleep_records(const CsvTable& table,
                                              const std::string& user_id,
                                              const std::string& date_prefix);

#endif
