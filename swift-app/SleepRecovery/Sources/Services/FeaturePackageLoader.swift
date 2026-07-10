import Foundation
import Combine

// Feature loading service — reads derived_features.csv from C++ output
class FeaturePackageLoader {
    func load(from url: URL) -> [FeaturePackage]? {
        do {
            let csv = try String(contentsOf: url, encoding: .utf8)
            let lines = csv.components(separatedBy: .newlines).filter { !$0.isEmpty }

            guard lines.count > 1 else { return nil }
            let header = lines[0].components(separatedBy: ",")
            let rows = lines.dropFirst()

            var packages: [FeaturePackage] = []
            let decoder = JSONDecoder()

            for line in rows {
                let fields = line.components(separatedBy: ",")
                guard fields.count == header.count else { continue }

                var dict: [String: String] = [:]
                for (i, key) in header.enumerated() {
                    dict[key] = fields[i]
                }

                // Convert string dict to JSON data then decode
                guard let jsonData = try? JSONSerialization.data(withJSONObject: dict, options: []),
                      let package = try? decoder.decode(FeaturePackage.self, from: jsonData)
                else { continue }

                packages.append(package)
            }

            return packages.sorted { a, b in
                // Sort by user then date
                a.sessionId < b.sessionId
            }

        } catch {
            print("[Loader] Failed to load: \(error)")
            return nil
        }
    }

    // Load sample heart rate data for a given session
    func loadHrTimeSeries(for sessionId: String, from baseDir: URL) -> [HrDataPoint] {
        // Try to read the per-session HR CSV
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

    // Load sample activity data
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

    // Placeholder data when real CSVs aren't available
    private func generatePlaceholderHr() -> [HrDataPoint] {
        var points: [HrDataPoint] = []
        let now = Date()
        for i in 0..<400 {
            let ts = now.addingTimeInterval(TimeInterval(-i * 60))
            // Generate a realistic sleep HR pattern: gradual decline + some variation
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
