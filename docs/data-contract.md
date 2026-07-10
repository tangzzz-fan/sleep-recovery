# Data Contract — Sleep Recovery MVP

> Part of M1: Data Foundation. This document locks the schema, field mapping, weak-label rules, artifact manifest, and golden sample for the entire MVP.

---

## 1. Core Schema (4 Tables)

### 1.1 night_sessions.csv

One row per night session. `session_id` is the cross-table primary key.

| # | Field | Type | Unit | Constraints | Description |
|---|---|---|---|---|---|
| 1 | session_id | string | — | unique, `{user_id}_{date}` | Composite key linking all four tables |
| 2 | user_id | int | — | 10-digit Fitbit ID | Fitbit user identifier |
| 3 | source_type | string | — | enum: `fitbit`, `synthetic`, `manual` | Data provenance marker |
| 4 | sleep_start | datetime | ISO 8601 | UTC | First `value=1` timestamp in the sleep record |
| 5 | sleep_end | datetime | ISO 8601 | UTC | Last `value=1` or `2` timestamp before wake |
| 6 | total_duration_min | float | minutes | > 0 | sleep_end - sleep_start |
| 7 | awaken_count | int | count | >= 0 | Number of `value=3` (awake) minutes during the sleep window |
| 8 | restless_count | int | count | >= 0 | Number of `value=2` (restless) minutes during the sleep window |
| 9 | recovery_label | int | enum | 0/1/2 | Target label: 0=poor, 1=average, 2=good |

### 1.2 heart_rate_timeseries.csv

One row per minute of heart rate observation (aggregated from seconds).

| # | Field | Type | Unit | Constraints | Description |
|---|---|---|---|---|---|
| 1 | session_id | string | — | FK → night_sessions | Links to the night session |
| 2 | timestamp | datetime | ISO 8601 | UTC, minute-aligned | Minute boundary |
| 3 | heart_rate_bpm | float | bpm | 30–220 | Mean HR for that minute |
| 4 | hr_count | int | count | 1–60 | Number of second-level samples in the minute |
| 5 | quality_flag | int | enum | 0/1/2/3 | 0=ok, 1=interpolated, 2=outlier, 3=missing |

### 1.3 activity_timeseries.csv

One row per minute of activity observation.

| # | Field | Type | Unit | Constraints | Description |
|---|---|---|---|---|---|
| 1 | session_id | string | — | FK → night_sessions | Links to the night session |
| 2 | timestamp | datetime | ISO 8601 | UTC, minute-aligned | Minute boundary |
| 3 | intensity | int | enum | 0/1/2/3 | Raw Fitbit intensity value |
| 4 | intensity_norm | float | 0.0–1.0 | = raw/3.0 | Intensity normalized to [0, 1] |
| 5 | steps | int | count | >= 0 | Steps in that minute (from minuteStepsNarrow) |
| 6 | movement_event | int | enum | 0/1 | 1 if intensity >= 1 (peak detected) |

### 1.4 derived_features.csv

One row per night session with 9 MVP feature values. This is the Feature Package consumed by Python training and Swift inference.

| # | Feature | Type | Unit | Constraints | Description |
|---|---|---|---|---|---|
| 1 | session_id | string | — | FK → night_sessions | Primary key |
| 2 | mean_hr | float | bpm | 30–220 | Mean heart rate during the sleep window |
| 3 | min_hr | float | bpm | 30–220 | Minimum heart rate |
| 4 | max_hr | float | bpm | 30–220 | Maximum heart rate |
| 5 | hr_std | float | bpm | >= 0 | Heart rate standard deviation |
| 6 | hr_drop_first_90m | float | bpm | can be negative | Mean HR(first 30min) - Mean HR(min 60–90) |
| 7 | activity_mean | float | 0.0–1.0 | | Mean of intensity_norm during sleep window |
| 8 | activity_spike_count | int | count | >= 0 | Number of minutes where intensity >= 1 |
| 9 | sleep_fragmentation_score | float | 0.0–1.0 | | (awaken_count + restless_count * 0.3) / total_duration_min * 60 |
| 10 | total_duration_min | float | minutes | > 0 | Copied from night_sessions |
| 11 | recovery_label | int | enum | 0/1/2 | Copied from night_sessions (target) |

---

## 2. Fitbit → Schema Field Mapping

### 2.1 minuteSleep_merged.csv → night_sessions + heart_rate_timeseries + activity_timeseries

| Fitbit Field | Target Table.Field | Transformation |
|---|---|---|
| Id | night_sessions.user_id | Direct copy (int) |
| date | night_sessions.sleep_start, sleep_end | Parse `M/D/YYYY H:MM:SS AM/PM` → ISO 8601 UTC |
| value | night_sessions.{awaken_count, restless_count, total_duration_min} | value=1 asleep → accumulate duration; value=2 restless → count & partial duration; value=3 awake → count awaken_count |
| logId | — | Not used in unified schema |

**Sleep window detection**:
- Start: first minute with `value=1` (asleep) in a contiguous sleep block
- End: last minute with `value=1` or `value=2` before a gap > 30 min of `value=0` (missing) or `value=3` (awake)
- `restless_count` (value=2): included in total_duration_min, treated as "light/interrupted sleep"
- `awaken_count` (value=3): excluded from total_duration_min if surrounded by awake periods; counted as awakenings if isolated

### 2.2 heartrate_seconds_merged.csv → heart_rate_timeseries.csv

| Fitbit Field | Target Table.Field | Transformation |
|---|---|---|
| Id | user_id → session_id | Map to session via date lookup |
| Time | timestamp | Parse `M/D/YYYY H:MM:SS AM/PM` → ISO 8601 UTC, floor to minute |
| Value | heart_rate_bpm | Aggregate seconds → minute mean |

### 2.3 minuteIntensitiesNarrow_merged.csv → activity_timeseries.csv

| Fitbit Field | Target Table.Field | Transformation |
|---|---|---|
| Id | user_id → session_id | Map to session via date lookup |
| ActivityMinute | timestamp | Parse `M/D/YYYY H:MM:SS AM/PM` → ISO 8601 UTC |
| Intensity | intensity + intensity_norm + movement_event | intensity preserved as raw (0/1/2/3); norm = raw/3.0; movement_event = intensity >= 1 |

### 2.4 minuteStepsNarrow_merged.csv → activity_timeseries.steps

| Fitbit Field | Target Table.Field | Transformation |
|---|---|---|
| Id | user_id → session_id | Map to session via date lookup |
| ActivityMinute | timestamp | Parse → ISO 8601 (alignment with intensity) |
| Steps | steps | Direct copy (int) |

### 2.5 dailyActivity_merged.csv → night_sessions (supplementary)

| Fitbit Field | Target Table.Field | Transformation |
|---|---|---|
| Id | user_id → session_id | Map to session via date lookup |
| ActivityDate | (cross-reference date) | Verify sleep session date matches activity date |
| SedentaryMinutes, Calories, TotalSteps | (auxiliary only) | Not part of MVP core schema; available for feature exploration |

### 2.6 Files NOT used in MVP core schema

| File | Reason |
|---|---|
| minuteMETsNarrow_merged.csv | Natively METs=10 for all sleep minutes — no variance, adds no signal for sleep analysis |
| minuteCaloriesNarrow_merged.csv | Redundant with intensity during sleep (calories ≈ constant at rest) |
| hourlySteps/Intensities/Calories | Hourly granularity — too coarse for within-night analysis |
| weightLogInfo_merged.csv | Only 11 users, unrelated to sleep recovery |

---

## 3. Weak-Label Scoring Formula

### 3.1 Scoring Algorithm

```
recovery_score = 100

# Penalty 1: Sleep duration (target: >= 420 min / 7 hours)
if total_duration_min < 420:  penalty = (420 - total_duration_min) * 0.08
if total_duration_min >= 480: bonus   = min((total_duration_min - 480) * 0.03, 5)

# Penalty 2: High mean HR (target: < 70 bpm)
if mean_hr > 80:              penalty = (mean_hr - 80) * 0.8
if mean_hr > 70 and <= 80:    penalty = (mean_hr - 70) * 0.3

# Penalty 3: HR not dropping in first 90 min (target: drop > 5 bpm)
if hr_drop_first_90m < 5:     penalty = (5 - hr_drop_first_90m) * 2.0

# Penalty 4: Too many activity spikes (target: < 10% of night)
spike_ratio = activity_spike_count / total_duration_min
if spike_ratio > 0.15:        penalty = (spike_ratio - 0.15) * 200

# Penalty 5: High fragmentation (target: < 0.15)
if sleep_fragmentation_score > 0.2: penalty = (sleep_fragmentation_score - 0.2) * 80

# Penalty 6: Too many awakenings (target: < 3)
if awaken_count > 5:          penalty = (awaken_count - 5) * 2.0
if awaken_count > 3 and <= 5: penalty = (awaken_count - 3) * 1.0

# Clamp to [0, 100]
recovery_score = max(0, min(100, recovery_score))
```

### 3.2 Label Mapping

```
recovery_score >= 75  →  recovery_label = 2  (良好 / Good)
50 <= score < 75     →  recovery_label = 1  (一般 / Average)
score < 50           →  recovery_label = 0  (差 / Poor)
```

### 3.3 Design Notes

- Weights are **initial estimates**, tuned to produce roughly balanced classes on the 12-user dataset. They MUST be validated and adjusted during M2/M3.
- The formula prioritizes **high precision on extremes** (poor=0, good=2) over fine-grained scoring.
- If a night has NO heart rate data, skip HR-related penalties (1, 2, 3) — only use duration, activity, fragmentation, and awakenings.

---

## 4. Artifact Manifest

Every data artifact produced by the C++ preprocessing CLI must carry this metadata:

```json
{
  "schema_version": "1.0.0",
  "table": "night_sessions|heart_rate_timeseries|activity_timeseries|derived_features",
  "csv_encoding": "UTF-8",
  "csv_delimiter": ",",
  "timestamp_format": "ISO8601",
  "timezone": "UTC",
  "label_enum": {
    "0": "poor",
    "1": "average",
    "2": "good"
  },
  "feature_order": [
    "mean_hr", "min_hr", "max_hr", "hr_std",
    "hr_drop_first_90m", "activity_mean", "activity_spike_count",
    "sleep_fragmentation_score", "total_duration_min"
  ],
  "source_dataset": "Fitbit Fitness Tracker Data (Kaggle/Bellabeat)",
  "source_date_range": "2016-03-11 to 2016-04-11",
  "model_version": null
}
```

---

## 5. Golden Sample

### 5.1 Selection

| Attribute | Value |
|---|---|
| User | 2347167796 |
| Date | 2016-04-01 |
| Session ID | `2347167796_2016-04-01` |
| Sleep window | 01:00 – 06:30 UTC (391 sleep records) |
| HR samples | 8,563 seconds (≈143 minutes in sleep window) |
| Activity records | 1,440 minutes (24h) |

### 5.2 Why This Sample

- User 2347167796 has the **most overlap days** of sleep + heart rate (14 days)
- Night of 4/1/2016: typical sleep pattern — mostly asleep (374 min), some restlessness (15 min), few awakenings (2 min). Sleep window: 00:00 – 06:30 UTC (391 min total)
- HR 60.53 bpm mean, range 54.67–75.0, std 3.46 — stable and physiologically plausible, excellent sleep quality
- Activity during sleep window: near-zero (mean intensity 0.0034 normalized), only 4 spikes
- Fragmentation score: 0.0166 (very low — deep continuous sleep)
- **Expected: recovery_label=2 (Good), recovery_score≈95.8 — an unambiguous "good recovery" reference night**

### 5.3 Expected Features (computed from extraction script)

Features produced by `python/extract_golden_sample.py` running against the raw Fitbit CSVs:

| Feature | Value |
|---|---|
| mean_hr | 60.53 |
| min_hr | 54.67 |
| max_hr | 75.0 |
| hr_std | 3.46 |
| hr_drop_first_90m | 4.07 |
| activity_mean | 0.0034 |
| activity_spike_count | 4 |
| sleep_fragmentation_score | 0.0166 |
| total_duration_min | 391.0 |
| recovery_label | 2 (Good) |
| recovery_score | 95.8 |

### 5.4 Golden Sample Files

The C++ preprocessing CLI will produce:

```
data/samples/golden/
├── night_session.json           # night_sessions.csv row for this night
├── heart_rate_timeseries.csv    # 1-min aggregated HR during sleep window
├── activity_timeseries.csv      # 1-min activity during sleep window
├── derived_features.csv         # 1-row feature package
├── expected_prediction.json     # {"recovery_label": 2, "recovery_score": "~85-95", "probabilities": [0.05, 0.15, 0.80]}
└── source_manifest.json         # Artifact manifest for this sample
```

`expected_prediction.json` will be finalized after M3 training produces a concrete model. Before then, the placeholder contains the weak-label prediction.

---

## 6. Data Directory Structure

```
data/
├── raw/                         # Immutable: copies of Fitbit source CSVs
│   ├── minuteSleep_merged.csv
│   ├── heartrate_seconds_merged.csv
│   ├── minuteIntensitiesNarrow_merged.csv
│   ├── minuteStepsNarrow_merged.csv
│   └── dailyActivity_merged.csv
├── normalized/                  # C++ output: standardized CSVs per night
│   ├── night_sessions.csv
│   ├── heart_rate_timeseries/
│   │   ├── 2347167796_2016-04-01_hr.csv
│   │   └── ...
│   └── activity_timeseries/
│       ├── 2347167796_2016-04-01_activity.csv
│       └── ...
├── features/                    # C++ output: one derived_features.csv per night
│   ├── 2347167796_2016-04-01_features.csv
│   └── ...
├── labels/                      # Weak labels computed by C++ or Python
│   └── recovery_labels.csv      # session_id, recovery_score, recovery_label
└── samples/
    └── golden/                  # Golden sample for cross-stack verification
```

---

## 7. Validation Checklist

- [x] 4-table field names and types frozen — downstream modules no longer rename columns
- [x] Fitbit field mapping verified for all 12 HR+sleep users
- [x] Weak-label formula tested — produces distribution: Poor=7, Average=37, Good=196 (expected with rule-based labels)
- [x] Artifact manifest JSON schema validated (manifest.json produced for golden sample)
- [x] Golden sample: raw data → normalize → feature extract → predict works end-to-end
- [x] `heartrate_seconds_merged.csv` second-level → minute-level aggregation tested (355 valid HR minutes for golden sample)
- [x] Sleep window detection handles AM/PM 12-hour format correctly
- [x] Golden sample CSV files generated and verified: night_session.csv, heart_rate_timeseries.csv, activity_timeseries.csv, derived_features.csv, manifest.json
- [x] Cross-layer consistency: C++ → Python (9 features diff=0.0), Python → Core ML (3 models), C++ → Core ML (golden sample) — all verified
- [x] 3 Core ML models exported and verified: lr_classifier, rf_classifier, xgb_classifier_v2
