#ifndef HR_AGGREGATOR_H
#define HR_AGGREGATOR_H

#include "csv_reader.h"
#include <string>
#include <vector>
#include <map>

struct HrMinute {
    long long minute_epoch;   // epoch at minute boundary (seconds truncated)
    double heart_rate_bpm;    // mean HR for this minute
    int hr_count;             // number of second-level samples
    int quality_flag;         // 0=ok, 1=interpolated, 2=outlier, 3=missing
};

// aggregate second-level HR data → minute-level
// reads from heartrate_seconds_merged.csv
std::vector<HrMinute> aggregate_hr(const CsvTable& table,
                                    const std::string& user_id,
                                    const std::string& date_prefix);

// filter HrMinute records to those within a sleep window
std::vector<HrMinute> filter_hr_window(const std::vector<HrMinute>& hr_data,
                                        long long start_epoch,
                                        long long end_epoch);

#endif
