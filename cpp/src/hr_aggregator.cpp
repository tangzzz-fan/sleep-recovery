#include "hr_aggregator.h"
#include "time_utils.h"
#include <map>
#include <vector>
#include <string>
#include <cassert>
#include <numeric>

std::vector<HrMinute> aggregate_hr(const CsvTable& table,
                                    const std::string& user_id,
                                    const std::string& date_prefix) {
    // map from minute_epoch → vector of HR values
    std::map<long long, std::vector<double>> minute_buckets;

    for (const auto& row : table) {
        auto uid_it = row.find("Id");
        auto time_it = row.find("Time");
        auto val_it = row.find("Value");
        if (uid_it == row.end() || time_it == row.end() || val_it == row.end())
            continue;
        if (uid_it->second != user_id) continue;
        if (time_it->second.find(date_prefix) != 0) continue;

        long long epoch = parse_12h_to_epoch(time_it->second);
        if (epoch < 0) continue;

        // floor to minute boundary
        long long minute_epoch = (epoch / 60) * 60;

        double hr_val = std::stod(val_it->second);
        minute_buckets[minute_epoch].push_back(hr_val);
    }

    std::vector<HrMinute> result;
    for (const auto& kv : minute_buckets) {
        HrMinute m;
        m.minute_epoch = kv.first;
        m.hr_count = static_cast<int>(kv.second.size());

        double sum = std::accumulate(kv.second.begin(), kv.second.end(), 0.0);
        m.heart_rate_bpm = sum / kv.second.size();
        m.quality_flag = 0;  // ok by default
        result.push_back(m);
    }

    return result;
}

std::vector<HrMinute> filter_hr_window(const std::vector<HrMinute>& hr_data,
                                        long long start_epoch,
                                        long long end_epoch) {
    std::vector<HrMinute> result;
    for (const auto& m : hr_data) {
        if (m.minute_epoch >= start_epoch && m.minute_epoch <= end_epoch) {
            result.push_back(m);
        }
    }
    return result;
}
