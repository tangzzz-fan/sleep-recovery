#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include "sleep_detector.h"
#include "hr_aggregator.h"
#include <vector>
#include <string>

struct DerivedFeatures {
    std::string session_id;
    double mean_hr = 0.0;
    double min_hr = 0.0;
    double max_hr = 0.0;
    double hr_std = 0.0;
    double hr_drop_first_90m = 0.0;
    double activity_mean = 0.0;
    int activity_spike_count = 0;
    double sleep_fragmentation_score = 0.0;
    double total_duration_min = 0.0;
    int recovery_label = 0;
    double recovery_score = 0.0;
};

// activity record: (epoch, intensity)
using ActivityRecord = std::pair<long long, int>;

// compute 9 MVP features from normalized data
DerivedFeatures extract_features(const std::vector<HrMinute>& hr_all,
                                  const std::vector<ActivityRecord>& activity_all,
                                  const SleepWindow& window,
                                  const std::string& session_id);

// compute weak label score from features
double compute_weak_label(double total_min, double mean_hr, double hr_drop,
                           int spike_count, double fragmentation, int awaken_count);

int score_to_label(double score);

#endif
