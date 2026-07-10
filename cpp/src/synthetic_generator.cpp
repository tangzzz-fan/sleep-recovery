#include "synthetic_generator.h"
#include <random>

// Realistic parameter ranges derived from Fitbit dataset analysis:
// total_duration: 200-540 min, mean_hr: 55-85, hr_std: 2-15, hr_drop: -5 to 15
// activity_mean: 0.0-0.15, spikes: 0-30, fragmentation: 0.0-0.3, awakenings: 0-10

SyntheticParams sample_synthetic_params(std::mt19937& rng) {
    SyntheticParams p;

    std::uniform_int_distribution<int> dur_dist(200, 540);
    std::uniform_real_distribution<double> hr_dist(55.0, 85.0);
    std::uniform_real_distribution<double> hr_std_dist(2.0, 15.0);
    std::uniform_real_distribution<double> drop_dist(-5.0, 15.0);
    std::uniform_real_distribution<double> act_dist(0.0, 0.15);
    std::uniform_int_distribution<int> spike_dist(0, 30);
    std::uniform_real_distribution<double> frag_dist(0.0, 0.3);
    std::uniform_int_distribution<int> awake_dist(0, 10);

    p.total_duration_min = dur_dist(rng);
    p.mean_hr = hr_dist(rng);
    p.hr_std = hr_std_dist(rng);
    p.hr_drop = drop_dist(rng);
    p.activity_mean = act_dist(rng);
    p.spike_count = spike_dist(rng);
    p.fragmentation = frag_dist(rng);
    p.awakenings = awake_dist(rng);

    return p;
}

std::vector<DerivedFeatures> generate_synthetic_sessions(int n_sessions,
                                                          unsigned int seed) {
    std::mt19937 rng(seed);
    std::vector<DerivedFeatures> result;

    for (int i = 0; i < n_sessions; ++i) {
        SyntheticParams p = sample_synthetic_params(rng);

        DerivedFeatures f;
        f.session_id = "synthetic_" + std::to_string(i);
        f.mean_hr = p.mean_hr;
        f.min_hr = p.mean_hr - p.hr_std * 1.5;
        f.max_hr = p.mean_hr + p.hr_std * 1.5;
        f.hr_std = p.hr_std;
        f.hr_drop_first_90m = p.hr_drop;
        f.activity_mean = p.activity_mean;
        f.activity_spike_count = p.spike_count;
        f.sleep_fragmentation_score = p.fragmentation;
        f.total_duration_min = p.total_duration_min;

        f.recovery_score = compute_weak_label(
            f.total_duration_min, f.mean_hr, f.hr_drop_first_90m,
            f.activity_spike_count, f.sleep_fragmentation_score, p.awakenings
        );
        f.recovery_label = score_to_label(f.recovery_score);

        result.push_back(f);
    }

    return result;
}
