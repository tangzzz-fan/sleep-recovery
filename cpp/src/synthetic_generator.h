#ifndef SYNTHETIC_GENERATOR_H
#define SYNTHETIC_GENERATOR_H

#include "sleep_detector.h"
#include "hr_aggregator.h"
#include "feature_extractor.h"
#include <vector>
#include <string>
#include <random>

// generate synthetic sleep data for augmentation/fallback
// n_sessions: how many nights to generate (default 200)
// seed: random seed for reproducibility
std::vector<DerivedFeatures> generate_synthetic_sessions(
    int n_sessions = 200,
    unsigned int seed = 42);

// parameters sampled from real Fitbit distributions (12 users)
struct SyntheticParams {
    int total_duration_min;
    double mean_hr;
    double hr_std;
    double hr_drop;
    double activity_mean;
    int spike_count;
    double fragmentation;
    int awakenings;
};

SyntheticParams sample_synthetic_params(std::mt19937& rng);

#endif
