import Foundation

// Feature loading service — reads derived_features.csv from C++ output
class FeaturePackageLoader {
    func load(from url: URL) -> [FeaturePackage]? {
        print("[Loader] Starting load from: \(url.path)")
        do {
            let csv = try String(contentsOf: url, encoding: .utf8)
            let lines = csv.components(separatedBy: .newlines).filter { !$0.isEmpty }

            guard lines.count > 1 else {
                print("[Loader] CSV has no data rows")
                return nil
            }
            let header = lines[0].components(separatedBy: ",")
            let rows = lines.dropFirst()

            var packages: [FeaturePackage] = []
            var skipped = 0

            for line in rows {
                let fields = line.components(separatedBy: ",")
                guard fields.count == header.count else { skipped += 1; continue }

                var dict: [String: String] = [:]
                for (i, key) in header.enumerated() {
                    dict[key] = fields[i]
                }

                // Manual parse — CSV fields are all strings, JSONDecoder can't convert
                // String→Int/Double even with keyDecodingStrategy.
                if let pkg = parseFeaturePackage(dict) {
                    packages.append(pkg)
                } else {
                    skipped += 1
                }
            }

            print("[Loader] Decoded \(packages.count) packages, skipped \(skipped)")
            return packages.sorted { a, b in a.sessionId < b.sessionId }

        } catch {
            print("[Loader] Failed to load: \(error)")
            return nil
        }
    }

    // Manual parser — avoids JSONSerialization type mismatch
    private func parseFeaturePackage(_ d: [String: String]) -> FeaturePackage? {
        guard let sid = d["session_id"],
              let meanHr = Double(d["mean_hr"] ?? ""),
              let minHr = Double(d["min_hr"] ?? ""),
              let maxHr = Double(d["max_hr"] ?? ""),
              let hrStd = Double(d["hr_std"] ?? ""),
              let hrDrop = Double(d["hr_drop_first_90m"] ?? ""),
              let actMean = Double(d["activity_mean"] ?? ""),
              let spikes = Int(d["activity_spike_count"] ?? ""),
              let frag = Double(d["sleep_fragmentation_score"] ?? ""),
              let dur = Double(d["total_duration_min"] ?? ""),
              let label = Int(d["recovery_label"] ?? "")
        else { return nil }

        return FeaturePackage(
            sessionId: sid,
            meanHr: meanHr,
            minHr: minHr,
            maxHr: maxHr,
            hrStd: hrStd,
            hrDropFirst90m: hrDrop,
            activityMean: actMean,
            activitySpikeCount: spikes,
            sleepFragmentationScore: frag,
            totalDurationMin: dur,
            recoveryLabel: label
        )
    }

    // .... (time series loaders unchanged)
    func loadHrTimeSeries(for sessionId: String, from baseDir: URL) -> [HrDataPoint] {
        let hrURL = baseDir
            .appendingPathComponent("heart_rate_timeseries")
            .appendingPathComponent("\(sessionId)_hr.csv")
        guard let csv = try? String(contentsOf: hrURL, encoding: .utf8) else {
            return generatePlaceholderHr()
        }
        let lines = csv.components(separatedBy: .newlines).filter { !$0.isEmpty }
        guard lines.count > 1 else { return generatePlaceholderHr() }
        var points: [HrDataPoint] = []
        let df = ISO8601DateFormatter()
        for line in lines.dropFirst() {
            let fields = line.components(separatedBy: ",")
            guard fields.count >= 3 else { continue }
            if let ts = df.date(from: fields[1]),
               let hr = Double(fields[2]) {
                points.append(HrDataPoint(timestamp: ts, heartRate: hr))
            }
        }
        return points.isEmpty ? generatePlaceholderHr() : points
    }

    func loadActivityTimeSeries(for sessionId: String, from baseDir: URL) -> [ActivityDataPoint] {
        let actURL = baseDir
            .appendingPathComponent("activity_timeseries")
            .appendingPathComponent("\(sessionId)_activity.csv")
        guard let csv = try? String(contentsOf: actURL, encoding: .utf8) else {
            return generatePlaceholderActivity()
        }
        let lines = csv.components(separatedBy: .newlines).filter { !$0.isEmpty }
        guard lines.count > 1 else { return generatePlaceholderActivity() }
        var points: [ActivityDataPoint] = []
        let df = ISO8601DateFormatter()
        for line in lines.dropFirst() {
            let fields = line.components(separatedBy: ",")
            guard fields.count >= 4 else { continue }
            if let ts = df.date(from: fields[1]),
               let norm = Double(fields[3]) {
                points.append(ActivityDataPoint(timestamp: ts, intensity: norm))
            }
        }
        return points.isEmpty ? generatePlaceholderActivity() : points
    }

    private func generatePlaceholderHr() -> [HrDataPoint] {
        var points: [HrDataPoint] = []
        let now = Date()
        for i in 0..<400 {
            let ts = now.addingTimeInterval(TimeInterval(-i * 60))
            let base: Double = 75
            let decline = Double(i) * 0.03
            let noise = Double.random(in: -3...3)
            let hr = max(45, min(95, base - decline + noise))
            points.append(HrDataPoint(timestamp: ts, heartRate: hr))
        }
        return points.reversed()
    }

    private func generatePlaceholderActivity() -> [ActivityDataPoint] {
        var points: [ActivityDataPoint] = []
        let now = Date()
        for i in 0..<400 {
            let ts = now.addingTimeInterval(TimeInterval(-i * 60))
            let intensity = Double.random(in: 0...1) < 0.05 ? Double.random(in: 0.1...0.4) : 0.0
            points.append(ActivityDataPoint(timestamp: ts, intensity: intensity))
        }
        return points.reversed()
    }
}
