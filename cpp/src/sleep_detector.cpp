#include "sleep_detector.h"
#include "time_utils.h"
#include <algorithm>
#include <string>
#include <vector>
#include <utility>
#include <cassert>

std::vector<SleepRecord> build_sleep_records(const CsvTable& table,
                                              const std::string& user_id,
                                              const std::string& date_prefix) {
    std::vector<SleepRecord> records;
    for (const auto& row : table) {
        auto uid_it = row.find("Id");
        auto date_it = row.find("date");
        auto val_it = row.find("value");
        if (uid_it == row.end() || date_it == row.end() || val_it == row.end())
            continue;
        if (uid_it->second != user_id) continue;
        if (date_it->second.find(date_prefix) != 0) continue;

        long long epoch = parse_12h_to_epoch(date_it->second);
        if (epoch < 0) continue;

        int val = std::stoi(val_it->second);
        records.emplace_back(epoch, val);
    }

    // sort by epoch
    std::sort(records.begin(), records.end(),
              [](const SleepRecord& a, const SleepRecord& b) {
                  return a.first < b.first;
              });

    return records;
}

SleepWindow detect_sleep_window(const std::vector<SleepRecord>& records) {
    SleepWindow best;
    if (records.empty()) return best;

    // Find longest contiguous block of sleep (value=1,2)
    // Break block if gap > 60 minutes between samples
    long long block_start = 0;
    long long block_end = 0;
    int block_mins = 0;
    int block_awake = 0;
    int block_rest = 0;
    bool in_block = false;
    long long prev_epoch = 0;

    // collect all blocks, then pick longest
    std::vector<SleepWindow> blocks;

    for (size_t i = 0; i < records.size(); ++i) {
        long long epoch = records[i].first;
        int val = records[i].second;

        // check continuity: gap > 3600 seconds breaks the block
        if (i > 0 && (epoch - prev_epoch) > 3600) {
            if (in_block) {
                SleepWindow w;
                w.start_epoch = block_start;
                w.end_epoch = block_end;
                w.total_minutes = block_mins;
                w.awakened = block_awake;
                w.restless = block_rest;
                w.valid = true;
                blocks.push_back(w);
            }
            in_block = false;
            block_mins = 0;
            block_awake = 0;
            block_rest = 0;
        }

        if (!in_block && val != 3) {
            // start a new block (exclude value=3 at start)
            in_block = true;
            block_start = epoch;
            block_end = epoch;
            block_mins = 1;
            block_awake = 0;
            block_rest = 0;
            if (val == 2) block_rest = 1;
        } else if (in_block) {
            block_end = epoch;
            block_mins++;
            if (val == 3) block_awake++;
            if (val == 2) block_rest++;
        }

        prev_epoch = epoch;
    }

    // final block
    if (in_block) {
        SleepWindow w;
        w.start_epoch = block_start;
        w.end_epoch = block_end;
        w.total_minutes = block_mins;
        w.awakened = block_awake;
        w.restless = block_rest;
        w.valid = true;
        blocks.push_back(w);
    }

    if (blocks.empty()) {
        // fallback: use the whole range
        best.start_epoch = records.front().first;
        best.end_epoch = records.back().first;
        best.total_minutes = static_cast<int>(records.size());
        for (const auto& r : records) {
            if (r.second == 3) best.awakened++;
            if (r.second == 2) best.restless++;
        }
        best.valid = true;
        return best;
    }

    // pick the longest block
    best = blocks[0];
    for (size_t i = 1; i < blocks.size(); ++i) {
        if (blocks[i].total_minutes > best.total_minutes) {
            best = blocks[i];
        }
    }

    return best;
}
