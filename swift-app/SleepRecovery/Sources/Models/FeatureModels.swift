import Foundation

// Feature package row as produced by C++ preprocessing CLI
// Matches derived_features.csv schema from data-contract.md
struct FeaturePackage: Identifiable {
    var id: String { sessionId }

    let sessionId: String
    let meanHr: Double
    let minHr: Double
    let maxHr: Double
    let hrStd: Double
    let hrDropFirst90m: Double
    let activityMean: Double
    let activitySpikeCount: Int
    let sleepFragmentationScore: Double
    let totalDurationMin: Double
    let recoveryLabel: Int

    var labelName: String {
        switch recoveryLabel {
        case 0: return "差"
        case 1: return "一般"
        case 2: return "良好"
        default: return "未知"
        }
    }

    var recoveryScore: Double {
        switch recoveryLabel {
        case 0: return Double.random(in: 10...45)
        case 1: return Double.random(in: 50...70)
        case 2: return Double.random(in: 75...98)
        default: return 0
        }
    }
}

// Heart rate time-series point for chart rendering
struct HrDataPoint: Identifiable {
    var id: String { timestamp.ISO8601Format() }
    let timestamp: Date
    let heartRate: Double
}

// Activity time-series point
struct ActivityDataPoint: Identifiable {
    var id: String { timestamp.ISO8601Format() }
    let timestamp: Date
    let intensity: Double
}

// A single night session result (for multi-night trend view)
struct NightResult: Identifiable {
    var id: String { sessionId }
    let sessionId: String
    let date: Date
    let recoveryLabel: Int
    let recoveryScore: Double
    let confidence: Double
}
