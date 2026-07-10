"""
Extract the golden sample from Fitbit raw data.

Golden sample: user 2347167796, night of 2016-04-01
Outputs: normalized CSVs + derived_features.csv under data/samples/golden/

Key: Fitbit timestamps use 12-hour AM/PM format. Sleep data for this user on
4/1/2016 spans from 1:00:00 AM to 12:59:00 AM (the next midnight), which is
actually a single night session. Must parse with proper AM/PM handling.
"""

import csv
import json
import math
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FITBIT_DIR = ROOT / "Fitabase Data 3.12.16-4.11.16"
OUT_DIR = ROOT / "data/samples/golden"

GOLDEN_USER = "2347167796"
GOLDEN_DATE = "4/1/2016"
SESSION_ID = f"{GOLDEN_USER}_{GOLDEN_DATE.replace('/', '-')}"


def parse_fitbit_12h(s: str) -> datetime:
    """Parse 'M/D/YYYY H:MM:SS AM/PM' (12-hour) → naive datetime."""
    return datetime.strptime(s, "%m/%d/%Y %I:%M:%S %p")


def to_iso(dt: datetime) -> str:
    """Convert datetime to ISO 8601 UTC string."""
    return dt.strftime("%Y-%m-%dT%H:%M:%SZ")


def load_sleep_data():
    """Load minuteSleep for golden user+date, return sorted list of (datetime, value)."""
    rows = []
    with open(FITBIT_DIR / "minuteSleep_merged.csv", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["Id"] == GOLDEN_USER and row["date"].startswith(GOLDEN_DATE):
                dt = parse_fitbit_12h(row["date"])
                rows.append((dt, int(row["value"])))
    rows.sort(key=lambda x: x[0])
    return rows


def load_hr_data():
    """Load heartrate_seconds for golden user+date, return sorted list of (datetime, value)."""
    rows = []
    with open(FITBIT_DIR / "heartrate_seconds_merged.csv", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["Id"] == GOLDEN_USER:
                time_str = row["Time"]
                if time_str.startswith(GOLDEN_DATE):
                    dt = parse_fitbit_12h(time_str)
                    rows.append((dt, float(row["Value"])))
    rows.sort(key=lambda x: x[0])
    return rows


def load_activity_data():
    """Load minuteIntensities + minuteSteps for golden user+date."""
    intensity_rows = []
    with open(FITBIT_DIR / "minuteIntensitiesNarrow_merged.csv", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["Id"] == GOLDEN_USER and row["ActivityMinute"].startswith(GOLDEN_DATE):
                dt = parse_fitbit_12h(row["ActivityMinute"])
                intensity_rows.append((dt, int(row["Intensity"])))

    steps_map = {}
    with open(FITBIT_DIR / "minuteStepsNarrow_merged.csv", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["Id"] == GOLDEN_USER and row["ActivityMinute"].startswith(GOLDEN_DATE):
                dt = parse_fitbit_12h(row["ActivityMinute"])
                steps_map[dt] = int(row["Steps"])

    intensity_rows.sort(key=lambda x: x[0])
    return intensity_rows, steps_map


def detect_sleep_window(sleep_rows):
    """
    Find the main contiguous sleep block starting from the evening.
    For 4/1 data, sleep starts at 1:00 AM (early morning) and continues
    through the night. The records span 1:00 AM → 12:59 AM.

    Strategy: find the longest block of sleep (val=1,2) with gaps ≤ 60 min.
    """
    # Build contiguous blocks
    blocks = []
    block_start = None
    block_end = None
    block_minutes = 0
    block_awakened = 0
    block_restless = 0
    prev_dt = None

    in_block = False
    block_start_dt = None
    block_end_dt = None
    block_mins = 0
    block_awake = 0
    block_rest = 0

    for dt, val in sleep_rows:
        # Check continuity: gap > 60 min breaks the block
        if prev_dt is not None and (dt - prev_dt).total_seconds() > 3600:
            if in_block:
                blocks.append((block_start_dt, block_end_dt, block_mins, block_awake, block_rest))
            in_block = False
            block_mins = 0
            block_awake = 0
            block_rest = 0

        if not in_block and val in (1, 2):
            in_block = True
            block_start_dt = dt
            block_end_dt = dt
            block_mins = 1
            block_awake = 0
            block_rest = 0
            if val == 2:
                block_rest += 1
        elif in_block:
            if val in (1, 2):
                block_end_dt = dt
                block_mins += 1
                if val == 2:
                    block_rest += 1
            elif val == 3:
                block_end_dt = dt
                block_mins += 1
                block_awake += 1

        prev_dt = dt

    if in_block:
        blocks.append((block_start_dt, block_end_dt, block_mins, block_awake, block_rest))

    # Select the longest block
    if not blocks:
        blocks = [(sleep_rows[0][0], sleep_rows[-1][0], len(sleep_rows), 0, 0)]

    best = max(blocks, key=lambda b: b[2])
    start_dt, end_dt, total_min, awakened, restless = best

    return start_dt, end_dt, awakened, restless, total_min


def aggregate_hr_to_minute(hr_rows):
    """Aggregate second-level HR to minute-level."""
    minute_data = {}
    for dt, val in hr_rows:
        minute_key = dt.replace(second=0)
        if minute_key not in minute_data:
            minute_data[minute_key] = []
        minute_data[minute_key].append(val)

    result = []
    for minute_key in sorted(minute_data.keys()):
        vals = minute_data[minute_key]
        mean_hr = sum(vals) / len(vals)
        result.append({
            "timestamp": to_iso(minute_key),
            "heart_rate_bpm": round(mean_hr, 2),
            "hr_count": len(vals),
            "quality_flag": 0,
            "_dt": minute_key,  # internal use only
        })
    return result


def filter_data(data_rows, start_dt, end_dt, accessor):
    """Filter data to rows within [start_dt, end_dt]."""
    result = []
    for row in data_rows:
        ts = accessor(row)
        if start_dt <= ts <= end_dt:
            result.append(row)
    return result


def extract_features(hr_minute, activity_rows, sleep_window):
    """Compute 9 MVP features from normalized data."""
    start_dt, end_dt, awakened, restless, total_min = sleep_window

    # HR features — filter by datetime
    hr_in_window = []
    for r in hr_minute:
        ts = r["_dt"]
        if start_dt <= ts <= end_dt:
            hr_in_window.append(r)

    hr_vals = [r["heart_rate_bpm"] for r in hr_in_window] if hr_in_window else []

    if hr_vals:
        mean_hr = sum(hr_vals) / len(hr_vals)
        min_hr = min(hr_vals)
        max_hr = max(hr_vals)
        hr_std = math.sqrt(sum((x - mean_hr) ** 2 for x in hr_vals) / len(hr_vals))
    else:
        mean_hr = min_hr = max_hr = hr_std = 0.0

    # HR drop first 90 min
    hr_first_30 = []
    hr_60_90 = []
    if start_dt and hr_in_window:
        from datetime import timedelta
        cutoff_30 = start_dt + timedelta(minutes=30)
        cutoff_60 = start_dt + timedelta(minutes=60)
        cutoff_90 = start_dt + timedelta(minutes=90)

        for r in hr_in_window:
            ts = r["_dt"]
            if start_dt <= ts <= cutoff_30:
                hr_first_30.append(r["heart_rate_bpm"])
            if cutoff_60 <= ts <= cutoff_90:
                hr_60_90.append(r["heart_rate_bpm"])

        mean_first_30 = sum(hr_first_30) / len(hr_first_30) if hr_first_30 else 0
        mean_60_90 = sum(hr_60_90) / len(hr_60_90) if hr_60_90 else 0
        hr_drop = mean_first_30 - mean_60_90
    else:
        hr_drop = 0.0

    # Activity features
    act_in_window = []
    for dt, intensity in activity_rows:
        if start_dt <= dt <= end_dt:
            act_in_window.append(intensity)

    if act_in_window:
        activity_mean = sum(act_in_window) / len(act_in_window) / 3.0
        activity_spike_count = sum(1 for x in act_in_window if x >= 1)
    else:
        activity_mean = 0.0
        activity_spike_count = 0

    # Sleep fragmentation: (awakened + restless*0.3) / total_min * 60
    # This is a per-minute score; scale to 0-1 range
    if total_min > 0:
        fragmentation = (awakened + restless * 0.3) / total_min
    else:
        fragmentation = 0.0

    # Weak label
    recovery_score = compute_weak_label(
        total_min, mean_hr, hr_drop, activity_spike_count, fragmentation, awakened
    )
    label = score_to_label(recovery_score)

    return {
        "session_id": SESSION_ID,
        "mean_hr": round(mean_hr, 2),
        "min_hr": round(min_hr, 2),
        "max_hr": round(max_hr, 2),
        "hr_std": round(hr_std, 2),
        "hr_drop_first_90m": round(hr_drop, 2),
        "activity_mean": round(activity_mean, 4),
        "activity_spike_count": activity_spike_count,
        "sleep_fragmentation_score": round(fragmentation, 4),
        "total_duration_min": float(total_min),
        "recovery_label": label,
        "recovery_score": round(recovery_score, 1),
    }


def compute_weak_label(total_min, mean_hr, hr_drop, spike_count, fragmentation, awaken_count):
    """Apply the weak-label scoring formula from data-contract.md."""
    score = 100.0

    # Penalty 1: sleep duration (target: >= 420 min)
    if total_min < 420:
        score -= (420 - total_min) * 0.08
    if total_min >= 480:
        score += min((total_min - 480) * 0.03, 5)

    # Penalty 2: high mean HR (target: < 70 bpm)
    if mean_hr > 0:
        if mean_hr > 80:
            score -= (mean_hr - 80) * 0.8
        elif mean_hr > 70:
            score -= (mean_hr - 70) * 0.3

    # Penalty 3: HR not dropping (target: drop > 5 bpm)
    if hr_drop < 5:
        score -= (5 - hr_drop) * 2.0

    # Penalty 4: activity spikes (target: < 10%)
    if total_min > 0:
        spike_ratio = spike_count / total_min
        if spike_ratio > 0.15:
            score -= (spike_ratio - 0.15) * 200

    # Penalty 5: fragmentation (target: < 0.2)
    if fragmentation > 0.2:
        score -= (fragmentation - 0.2) * 80

    # Penalty 6: awakenings (target: < 3)
    if awaken_count > 5:
        score -= (awaken_count - 5) * 2.0
    elif awaken_count > 3:
        score -= (awaken_count - 3) * 1.0

    return max(0.0, min(100.0, score))


def score_to_label(score):
    if score >= 75:
        return 2
    elif score >= 50:
        return 1
    else:
        return 0


def write_csv(path, fieldnames, rows):
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            # Remove internal fields
            clean = {k: v for k, v in row.items() if not k.startswith("_")}
            writer.writerow(clean)


def main():
    print(f"Extracting golden sample: user={GOLDEN_USER}, date={GOLDEN_DATE}")
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # 1. Load raw data with correct 12-hour parsing
    sleep_rows = load_sleep_data()
    hr_rows = load_hr_data()
    act_rows, steps_map = load_activity_data()

    print(f"  sleep records: {len(sleep_rows)} ({to_iso(sleep_rows[0][0])} → {to_iso(sleep_rows[-1][0])})")
    print(f"  HR seconds: {len(hr_rows)} ({to_iso(hr_rows[0][0])} → {to_iso(hr_rows[-1][0])})")
    print(f"  activity minutes: {len(act_rows)}")

    # 2. Detect sleep window
    start_dt, end_dt, awakened, restless, total_min = detect_sleep_window(sleep_rows)
    print(f"  sleep window: {to_iso(start_dt)} → {to_iso(end_dt)} ({total_min} min)")
    print(f"  awakened={awakened}, restless={restless}")

    # 3. Aggregate HR to minute level
    hr_minute = aggregate_hr_to_minute(hr_rows)
    hr_in_window = [r for r in hr_minute if start_dt <= r["_dt"] <= end_dt]
    print(f"  HR minutes in window: {len(hr_in_window)}")

    # 4. Write night_sessions.csv row
    session_row = {
        "session_id": SESSION_ID,
        "user_id": int(GOLDEN_USER),
        "source_type": "fitbit",
        "sleep_start": to_iso(start_dt),
        "sleep_end": to_iso(end_dt),
        "total_duration_min": float(total_min),
        "awaken_count": awakened,
        "restless_count": restless,
        "recovery_label": 2,  # placeholder
    }
    write_csv(OUT_DIR / "night_session.csv", session_row.keys(), [session_row])

    # 5. Write heart_rate_timeseries.csv
    if hr_in_window:
        hr_out = []
        for r in hr_in_window:
            hr_out.append({
                "session_id": SESSION_ID,
                "timestamp": r["timestamp"],
                "heart_rate_bpm": r["heart_rate_bpm"],
                "hr_count": r["hr_count"],
                "quality_flag": r["quality_flag"],
            })
        write_csv(
            OUT_DIR / "heart_rate_timeseries.csv",
            ["session_id", "timestamp", "heart_rate_bpm", "hr_count", "quality_flag"],
            hr_out,
        )

    # 6. Write activity_timeseries.csv
    act_out = []
    for dt, intensity in act_rows:
        if start_dt <= dt <= end_dt:
            act_out.append({
                "session_id": SESSION_ID,
                "timestamp": to_iso(dt),
                "intensity": intensity,
                "intensity_norm": round(intensity / 3.0, 4),
                "steps": steps_map.get(dt, 0),
                "movement_event": 1 if intensity >= 1 else 0,
            })
    if act_out:
        write_csv(
            OUT_DIR / "activity_timeseries.csv",
            ["session_id", "timestamp", "intensity", "intensity_norm", "steps", "movement_event"],
            act_out,
        )

    # 7. Compute features
    features = extract_features(hr_minute, act_rows, (start_dt, end_dt, awakened, restless, total_min))
    session_row["recovery_label"] = features["recovery_label"]
    write_csv(OUT_DIR / "night_session.csv", session_row.keys(), [session_row])

    # 8. Write derived_features.csv
    feature_fields = [
        "session_id", "mean_hr", "min_hr", "max_hr", "hr_std",
        "hr_drop_first_90m", "activity_mean", "activity_spike_count",
        "sleep_fragmentation_score", "total_duration_min", "recovery_label",
    ]
    feature_row = {k: features[k] for k in feature_fields}
    write_csv(OUT_DIR / "derived_features.csv", feature_fields, [feature_row])

    # 9. Write manifest
    manifest = {
        "schema_version": "1.0.0",
        "golden_sample": {
            "session_id": SESSION_ID,
            "user_id": int(GOLDEN_USER),
            "date": GOLDEN_DATE,
            "sleep_window": {
                "start": to_iso(start_dt),
                "end": to_iso(end_dt),
                "duration_min": total_min,
            },
        },
        "features": features,
        "weak_label": {
            "recovery_score": features["recovery_score"],
            "recovery_label": features["recovery_label"],
            "label_name": {0: "poor", 1: "average", 2: "good"}[features["recovery_label"]],
        },
    }
    with open(OUT_DIR / "manifest.json", "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"\n=== Golden Sample Summary ===")
    print(f"  Session: {SESSION_ID}")
    print(f"  Sleep: {to_iso(start_dt)} → {to_iso(end_dt)} ({total_min} min)")
    print(f"  HR: mean={features['mean_hr']}, min={features['min_hr']}, max={features['max_hr']}, "
          f"std={features['hr_std']}, drop_90m={features['hr_drop_first_90m']}")
    print(f"  Activity: mean={features['activity_mean']}, spikes={features['activity_spike_count']}")
    print(f"  Fragmentation: {features['sleep_fragmentation_score']}")
    print(f"  Label: recovery_score={features['recovery_score']:.1f}, label={features['recovery_label']} "
          f"({['poor', 'average', 'good'][features['recovery_label']]})")
    print(f"\n  Golden sample written to: {OUT_DIR}")


if __name__ == "__main__":
    main()
