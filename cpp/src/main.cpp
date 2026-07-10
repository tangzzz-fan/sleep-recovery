#include "csv_reader.h"
#include "sleep_detector.h"
#include "hr_aggregator.h"
#include "feature_extractor.h"
#include "session_builder.h"
#include "synthetic_generator.h"
#include "time_utils.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>

using SessionInfo = std::pair<std::string, SleepWindow>;

// ============================================================
// CSV output helpers
// ============================================================

void write_night_sessions(const std::string& filepath,
                           const std::vector<std::pair<std::string, SleepWindow>>& sessions,
                           const std::map<std::string, int>& session_labels) {
    std::ofstream f(filepath);
    f << "session_id,user_id,source_type,sleep_start,sleep_end,"
      << "total_duration_min,awaken_count,restless_count,recovery_label\n";

    for (const auto& s : sessions) {
        const std::string& sid = s.first;
        const SleepWindow& w = s.second;

        // extract user_id from session_id (format: userid_date)
        std::string uid = sid.substr(0, sid.find('_'));

        int label = 0;
        auto lit = session_labels.find(sid);
        if (lit != session_labels.end()) label = lit->second;

        f << sid << ","
          << uid << ","
          << "fitbit,"
          << epoch_to_iso(w.start_epoch) << ","
          << epoch_to_iso(w.end_epoch) << ","
          << w.total_minutes << ","
          << w.awakened << ","
          << w.restless << ","
          << label << "\n";
    }
    f.close();
    std::cout << "  wrote " << sessions.size() << " sessions to "
              << filepath << "\n";
}

void write_derived_features(const std::string& filepath,
                              const std::vector<DerivedFeatures>& features) {
    std::ofstream f(filepath);
    f << "session_id,mean_hr,min_hr,max_hr,hr_std,hr_drop_first_90m,"
      << "activity_mean,activity_spike_count,sleep_fragmentation_score,"
      << "total_duration_min,recovery_label\n";

    for (const auto& feat : features) {
        f << feat.session_id << ","
          << std::fixed << std::setprecision(2)
          << feat.mean_hr << ","
          << feat.min_hr << ","
          << feat.max_hr << ","
          << feat.hr_std << ","
          << feat.hr_drop_first_90m << ","
          << std::setprecision(4)
          << feat.activity_mean << ","
          << feat.activity_spike_count << ","
          << feat.sleep_fragmentation_score << ","
          << std::setprecision(1)
          << feat.total_duration_min << ","
          << feat.recovery_label << "\n";
    }
    f.close();
    std::cout << "  wrote " << features.size() << " feature rows to "
              << filepath << "\n";
}

void write_recovery_labels(const std::string& filepath,
                              const std::vector<DerivedFeatures>& features) {
    std::ofstream f(filepath);
    f << "session_id,recovery_score,recovery_label\n";
    for (const auto& feat : features) {
        f << feat.session_id << ","
          << std::fixed << std::setprecision(1) << feat.recovery_score << ","
          << feat.recovery_label << "\n";
    }
    f.close();
    std::cout << "  wrote " << features.size() << " labels to "
              << filepath << "\n";
}

// ============================================================
// main
// ============================================================

struct CliArgs {
    std::string input_dir;      // raw Fitbit CSV directory
    std::string output_dir;     // normalized output directory
    std::string features_dir;   // features output directory
    std::string labels_dir;     // labels output directory
    std::string golden_dir;     // golden sample verification directory
    int synthetic_count = 0;    // number of synthetic sessions (0 = skip)
};

void print_usage() {
    std::cerr << "Usage: sleep-recovery-cli [options]\n"
              << "  --input <dir>      Fitbit raw CSV directory\n"
              << "  --output <dir>     Normalized output directory\n"
              << "  --features <dir>   Features output directory\n"
              << "  --labels <dir>     Labels output directory\n"
              << "  --synthetic <n>    Generate n synthetic sessions\n"
              << "  --golden <dir>     Golden sample verification directory\n";
}

CliArgs parse_args(int argc, char* argv[]) {
    CliArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            args.input_dir = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if (arg == "--features" && i + 1 < argc) {
            args.features_dir = argv[++i];
        } else if (arg == "--labels" && i + 1 < argc) {
            args.labels_dir = argv[++i];
        } else if (arg == "--synthetic" && i + 1 < argc) {
            args.synthetic_count = std::stoi(argv[++i]);
        } else if (arg == "--golden" && i + 1 < argc) {
            args.golden_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        }
    }
    return args;
}

// Forward declaration for use in main
DerivedFeatures extract_features(const std::vector<HrMinute>& hr_all,
                                  const std::vector<ActivityRecord>& activity_all,
                                  const SleepWindow& window,
                                  const std::string& session_id);

int main(int argc, char* argv[]) {
    CliArgs args = parse_args(argc, argv);

    if (args.input_dir.empty() || args.output_dir.empty()) {
        std::cerr << "Error: --input and --output are required\n";
        print_usage();
        return 1;
    }

    // Ensure trailing slash
    if (args.input_dir.back() != '/') args.input_dir += '/';
    if (args.output_dir.back() != '/') args.output_dir += '/';
    if (args.features_dir.empty()) args.features_dir = args.output_dir + "../features/";
    if (args.labels_dir.empty()) args.labels_dir = args.output_dir + "../labels/";

    std::cout << "Sleep Recovery CLI v0.1.0\n";
    std::cout << "  input:    " << args.input_dir << "\n";
    std::cout << "  output:   " << args.output_dir << "\n";
    std::cout << "  features: " << args.features_dir << "\n";
    std::cout << "  labels:   " << args.labels_dir << "\n";

    // ============================================================
    // Step 1: Load raw CSV files
    // ============================================================
    CsvReader sleep_reader(args.input_dir + "minuteSleep_merged.csv");
    CsvReader hr_reader(args.input_dir + "heartrate_seconds_merged.csv");
    CsvReader intensity_reader(args.input_dir + "minuteIntensitiesNarrow_merged.csv");
    CsvReader steps_reader(args.input_dir + "minuteStepsNarrow_merged.csv");

    if (!sleep_reader.ok()) {
        std::cerr << "Error: " << sleep_reader.error() << "\n";
        return 1;
    }
    if (!hr_reader.ok()) {
        std::cerr << "Error: " << hr_reader.error() << "\n";
        return 1;
    }
    if (!intensity_reader.ok()) {
        std::cerr << "Error: " << intensity_reader.error() << "\n";
        return 1;
    }
    if (!steps_reader.ok()) {
        std::cerr << "Error: " << steps_reader.error() << "\n";
        return 1;
    }

    std::cout << "Loaded CSV files:\n";
    std::cout << "  sleep:      " << sleep_reader.size() << " rows\n";
    std::cout << "  HR:         " << hr_reader.size() << " rows\n";
    std::cout << "  intensity:  " << intensity_reader.size() << " rows\n";
    std::cout << "  steps:      " << steps_reader.size() << " rows\n";

    // ============================================================
    // Step 2: Find users with both sleep and HR data
    // ============================================================
    std::set<std::string> sleep_users, hr_users;
    for (const auto& row : sleep_reader.rows()) {
        auto it = row.find("Id");
        if (it != row.end()) sleep_users.insert(it->second);
    }
    for (const auto& row : hr_reader.rows()) {
        auto it = row.find("Id");
        if (it != row.end()) hr_users.insert(it->second);
    }

    std::vector<std::string> valid_users;
    for (const std::string& uid : sleep_users) {
        if (hr_users.count(uid)) {
            valid_users.push_back(uid);
        }
    }

    std::cout << "Users with sleep data: " << sleep_users.size() << "\n";
    std::cout << "Users with HR data:    " << hr_users.size() << "\n";
    std::cout << "Users with BOTH:       " << valid_users.size() << "\n";

    // ============================================================
    // Step 3: Process each user
    // ============================================================
    std::vector<SessionInfo> all_sessions;
    std::vector<DerivedFeatures> all_features;
    std::map<std::string, int> session_labels;

    for (const std::string& uid : valid_users) {
        std::vector<std::string> dates = get_user_sleep_dates(sleep_reader.rows(), uid);
        std::cout << "User " << uid << ": " << dates.size() << " sleep dates\n";

        auto sessions = build_user_sessions(sleep_reader.rows(), uid, dates);

        for (const auto& s : sessions) {
            const std::string& sid = s.first;
            const SleepWindow& w = s.second;

            // extract date from session_id
            std::string date = sid.substr(sid.find('_') + 1);
            // convert M-D-YYYY back to M/D/YYYY for raw data lookup
            std::string raw_date = date;
            for (char& c : raw_date) {
                if (c == '-') c = '/';
            }

            // build activity records for this date
            auto activity = build_activity_records(intensity_reader.rows(), steps_reader.rows(),
                                                    uid, raw_date);

            // aggregate HR for this date
            auto hr_data = aggregate_hr(hr_reader.rows(), uid, raw_date);

            // extract features
            DerivedFeatures feat = extract_features(hr_data, activity, w, sid);

            all_sessions.push_back(s);
            all_features.push_back(feat);
            session_labels[sid] = feat.recovery_label;
        }
    }

    std::cout << "Total sessions processed: " << all_sessions.size() << "\n";

    // ============================================================
    // Step 4: Generate synthetic data (optional)
    // ============================================================
    if (args.synthetic_count > 0) {
        std::cout << "Generating " << args.synthetic_count << " synthetic sessions...\n";
        auto synthetic = generate_synthetic_sessions(args.synthetic_count);

        for (const auto& feat : synthetic) {
            all_features.push_back(feat);
        }
        std::cout << "  total features incl. synthetic: " << all_features.size() << "\n";
    }

    // ============================================================
    // Step 5: Write outputs
    // ============================================================
    write_night_sessions(args.output_dir + "night_sessions.csv", all_sessions, session_labels);
    write_derived_features(args.features_dir + "derived_features.csv", all_features);
    write_recovery_labels(args.labels_dir + "recovery_labels.csv", all_features);

    // ============================================================
    // Step 6: Golden sample verification (optional)
    // ============================================================
    if (!args.golden_dir.empty()) {
        std::cout << "\n=== Golden Sample Verification ===\n";
        std::cout << "Comparing against: " << args.golden_dir << "\n";

        // Read golden derived_features.csv
        std::string golden_path = args.golden_dir;
        if (golden_path.back() != '/') golden_path += '/';
        CsvReader golden_reader(golden_path + "derived_features.csv");

        if (!golden_reader.ok() || golden_reader.size() < 1) {
            std::cerr << "  WARNING: Could not read golden derived_features.csv\n";
        } else {
            // Find our computed features for the golden session
            const std::string golden_sid = "2347167796_4-1-2016";
            bool found = false;

            for (const auto& feat : all_features) {
                if (feat.session_id == golden_sid) {
                    found = true;
                    if (golden_reader.size() < 1) {
                        std::cerr << "  WARNING: golden derived_features.csv is empty\n";
                        break;
                    }
                    const auto& golden_row = golden_reader.rows()[0];

                    auto check = [&](const std::string& name, double actual, double expected, double tolerance) {
                        double diff = std::abs(actual - expected);
                        bool ok = diff <= tolerance;
                        std::cout << "  " << name << ": actual=" << std::fixed << std::setprecision(2) << actual
                                  << " expected=" << expected
                                  << " diff=" << diff
                                  << " " << (ok ? "✓" : "✗ FAIL") << "\n";
                        return ok;
                    };

                    std::cout << "  Golden session: " << golden_sid << "\n";
                    double expected_mean_hr = std::stod(golden_row.at("mean_hr"));
                    double expected_min_hr  = std::stod(golden_row.at("min_hr"));
                    double expected_max_hr  = std::stod(golden_row.at("max_hr"));
                    double expected_hr_std  = std::stod(golden_row.at("hr_std"));
                    double expected_hr_drop = std::stod(golden_row.at("hr_drop_first_90m"));
                    double expected_act_mean = std::stod(golden_row.at("activity_mean"));
                    int expected_spikes    = std::stoi(golden_row.at("activity_spike_count"));
                    double expected_frag   = std::stod(golden_row.at("sleep_fragmentation_score"));
                    double expected_dur    = std::stod(golden_row.at("total_duration_min"));
                    int expected_label     = std::stoi(golden_row.at("recovery_label"));

                    bool all_ok = true;
                    all_ok &= check("mean_hr",                feat.mean_hr, expected_mean_hr, 0.5);
                    all_ok &= check("min_hr",                 feat.min_hr,  expected_min_hr, 1.0);
                    all_ok &= check("max_hr",                 feat.max_hr,  expected_max_hr, 1.0);
                    all_ok &= check("hr_std",                 feat.hr_std,  expected_hr_std, 0.5);
                    all_ok &= check("hr_drop_first_90m",      feat.hr_drop_first_90m, expected_hr_drop, 1.0);
                    all_ok &= check("activity_mean",          feat.activity_mean, expected_act_mean, 0.01);
                    all_ok &= check("activity_spike_count",   feat.activity_spike_count, expected_spikes, 2.0);
                    all_ok &= check("sleep_fragmentation_score", feat.sleep_fragmentation_score, expected_frag, 0.01);
                    all_ok &= check("total_duration_min",     feat.total_duration_min, expected_dur, 1.0);
                    all_ok &= check("recovery_label",         feat.recovery_label, expected_label, 0);

                    if (all_ok) {
                        std::cout << "  ✓ Golden sample verification PASSED\n";
                    } else {
                        std::cerr << "  ✗ Golden sample verification FAILED\n";
                        return 1;
                    }
                    break;
                }
            }

            if (!found) {
                std::cerr << "  WARNING: Golden session " << golden_sid
                          << " not found in processed data\n";
            }
        }
    }

    // ============================================================
    // Step 7: Summary
    // ============================================================
    // Count label distribution
    int count_0 = 0, count_1 = 0, count_2 = 0;
    for (const auto& feat : all_features) {
        if (feat.recovery_label == 0) count_0++;
        else if (feat.recovery_label == 1) count_1++;
        else count_2++;
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Total feature rows: " << all_features.size() << "\n";
    std::cout << "Label distribution:\n";
    std::cout << "  Poor (0):    " << count_0 << "\n";
    std::cout << "  Average (1): " << count_1 << "\n";
    std::cout << "  Good (2):    " << count_2 << "\n";
    std::cout << "Done.\n";

    return 0;
}
