#include "feature_extractor.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>

DerivedFeatures extract_features(const std::vector<HrMinute>& hr_all,
                                  const std::vector<ActivityRecord>& activity_all,
                                  const SleepWindow& window,
                                  const std::string& session_id) {
    DerivedFeatures f;
    f.session_id = session_id;
    f.total_duration_min = static_cast<double>(window.total_minutes);

    // Filter HR to sleep window
    std::vector<double> hr_vals;
    for (const auto& m : hr_all) {
        if (m.minute_epoch >= window.start_epoch && m.minute_epoch <= window.end_epoch) {
            hr_vals.push_back(m.heart_rate_bpm);
        }
    }

    if (!hr_vals.empty()) {
        double sum = std::accumulate(hr_vals.begin(), hr_vals.end(), 0.0);
        f.mean_hr = sum / hr_vals.size();
        f.min_hr = *std::min_element(hr_vals.begin(), hr_vals.end());
        f.max_hr = *std::max_element(hr_vals.begin(), hr_vals.end());

        double sq_sum = 0.0;
        for (double v : hr_vals) {
            sq_sum += (v - f.mean_hr) * (v - f.mean_hr);
        }
        f.hr_std = std::sqrt(sq_sum / hr_vals.size());
    }

    // HR drop first 90 min: mean(first 30 min) - mean(min 60–90)
    if (!hr_vals.empty()) {
        long long start = window.start_epoch;
        long long cutoff_30 = start + 30 * 60;
        long long cutoff_60 = start + 60 * 60;
        long long cutoff_90 = start + 90 * 60;

        std::vector<double> first_30, mid_60_90;
        for (const auto& m : hr_all) {
            if (m.minute_epoch >= start && m.minute_epoch <= cutoff_30) {
                first_30.push_back(m.heart_rate_bpm);
            }
            if (m.minute_epoch >= cutoff_60 && m.minute_epoch <= cutoff_90) {
                mid_60_90.push_back(m.heart_rate_bpm);
            }
        }

        double mean_first = first_30.empty() ? 0.0 :
            std::accumulate(first_30.begin(), first_30.end(), 0.0) / first_30.size();
        double mean_mid = mid_60_90.empty() ? 0.0 :
            std::accumulate(mid_60_90.begin(), mid_60_90.end(), 0.0) / mid_60_90.size();
        f.hr_drop_first_90m = mean_first - mean_mid;
    }

    // Activity in window
    std::vector<int> act_in_window;
    for (const auto& a : activity_all) {
        if (a.first >= window.start_epoch && a.first <= window.end_epoch) {
            act_in_window.push_back(a.second);
        }
    }

    if (!act_in_window.empty()) {
        double act_sum = std::accumulate(act_in_window.begin(), act_in_window.end(), 0);
        f.activity_mean = act_sum / act_in_window.size() / 3.0;  // normalize 0-3 → 0-1
        f.activity_spike_count = 0;
        for (int intensity : act_in_window) {
            if (intensity >= 1) f.activity_spike_count++;
        }
    }

    // Sleep fragmentation: (awakened + restless * 0.3) / total_minutes
    if (window.total_minutes > 0) {
        f.sleep_fragmentation_score =
            (window.awakened + window.restless * 0.3) / window.total_minutes;
    }

    // Weak label
    f.recovery_score = compute_weak_label(
        f.total_duration_min, f.mean_hr, f.hr_drop_first_90m,
        f.activity_spike_count, f.sleep_fragmentation_score, window.awakened
    );
    f.recovery_label = score_to_label(f.recovery_score);

    return f;
}

double compute_weak_label(double total_min, double mean_hr, double hr_drop,
                           int spike_count, double fragmentation, int awaken_count) {
    double score = 100.0;

    // Penalty 1: sleep duration
    if (total_min < 420.0) {
        score -= (420.0 - total_min) * 0.08;
    }
    if (total_min >= 480.0) {
        score += std::min((total_min - 480.0) * 0.03, 5.0);
    }

    // Penalty 2: high mean HR
    if (mean_hr > 0.0) {
        if (mean_hr > 80.0) {
            score -= (mean_hr - 80.0) * 0.8;
        } else if (mean_hr > 70.0) {
            score -= (mean_hr - 70.0) * 0.3;
        }
    }

    // Penalty 3: HR drop
    if (hr_drop < 5.0) {
        score -= (5.0 - hr_drop) * 2.0;
    }

    // Penalty 4: activity spikes
    if (total_min > 0.0) {
        double spike_ratio = spike_count / total_min;
        if (spike_ratio > 0.15) {
            score -= (spike_ratio - 0.15) * 200.0;
        }
    }

    // Penalty 5: fragmentation
    if (fragmentation > 0.2) {
        score -= (fragmentation - 0.2) * 80.0;
    }

    // Penalty 6: awakenings
    if (awaken_count > 5) {
        score -= (awaken_count - 5) * 2.0;
    } else if (awaken_count > 3) {
        score -= (awaken_count - 3) * 1.0;
    }

    return std::max(0.0, std::min(100.0, score));
}

int score_to_label(double score) {
    if (score >= 75.0) return 2;
    if (score >= 50.0) return 1;
    return 0;
}
