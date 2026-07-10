import Foundation
import CoreML
import Charts

// Service to load the Core ML model and run inference
class PredictionService {
    private var model: MLModel?

    init() {
        loadModel()
    }

    private func loadModel() {
        // Search paths: bundle first, then executable directory (for SPM debug builds)
        let candidateURLs: [URL] = [
            Bundle.main.url(forResource: "lr_classifier", withExtension: "mlpackage"),
            Bundle.main.url(forResource: "rf_classifier", withExtension: "mlpackage"),
            Bundle.main.url(forResource: "xgb_classifier", withExtension: "mlpackage"),
            // SPM .build/debug directory
            Bundle.main.bundleURL.deletingLastPathComponent()
                .appendingPathComponent("lr_classifier.mlpackage"),
            Bundle.main.bundleURL.deletingLastPathComponent()
                .appendingPathComponent("rf_classifier.mlpackage"),
        ].compactMap { $0 }

        for url in candidateURLs {
            if FileManager.default.fileExists(atPath: url.path) {
                do {
                    model = try MLModel(contentsOf: url)
                    print("[PredictionService] Loaded model: \(url.lastPathComponent) from \(url.path)")
                    return
                } catch {
                    print("[PredictionService] Failed to load \(url.lastPathComponent): \(error)")
                }
            }
        }

        print("[PredictionService] No Core ML model found. Using placeholder predictions.")
    }

    // Predict recovery label from feature package
    func predict(features: FeaturePackage) -> (label: Int, probabilities: [Int: Double])? {
        guard let model = model else {
            // Placeholder: return the weak label
            return (features.recoveryLabel, [
                0: 0.05,
                1: 0.15,
                2: 0.80,
            ])
        }

        do {
            // Build input dictionary matching the model's expected features
            let input: [String: Any] = [
                "mean_hr": features.meanHr,
                "min_hr": features.minHr,
                "max_hr": features.maxHr,
                "hr_std": features.hrStd,
                "hr_drop_first_90m": features.hrDropFirst90m,
                "activity_mean": features.activityMean,
                "activity_spike_count": Double(features.activitySpikeCount),
                "sleep_fragmentation_score": features.sleepFragmentationScore,
                "total_duration_min": features.totalDurationMin,
            ]

            // Create MLFeatureProvider from dictionary
            let inputFeatures = try MLDictionaryFeatureProvider(dictionary: input)
            let output = try model.prediction(from: inputFeatures)

            // Extract class label and probabilities
            if let classLabel = output.featureValue(for: "classLabel")?.int64Value {
                let label = Int(classLabel)

                var probs: [Int: Double] = [:]
                if let probDict = output.featureValue(for: "classProbability")?.dictionaryValue {
                    for (key, value) in probDict {
                        if let classIdx = Int("\(key)"), let prob = value as? Double {
                            probs[classIdx] = prob
                        }
                    }
                }

                return (label, probs)
            }

            // Fallback: try xgboost output format
            if let recoveryLabel = output.featureValue(for: "recovery_label")?.int64Value {
                let label = Int(recoveryLabel)

                var probs: [Int: Double] = [:]
                if let probDict = output.featureValue(for: "classProbability")?.dictionaryValue {
                    for (key, value) in probDict {
                        if let classIdx = Int("\(key)"), let prob = value as? Double {
                            probs[classIdx] = prob
                        }
                    }
                }

                return (label, probs)
            }

        } catch {
            print("[PredictionService] Prediction failed: \(error)")
        }

        return nil
    }

    // Get top N contributing factors (simplified for MVP)
    func contributingFactors(features: FeaturePackage, prediction: (label: Int, probabilities: [Int: Double])?) -> [String] {
        var factors: [String] = []

        // Duration-based
        if features.totalDurationMin < 360 {
            factors.append("睡眠时长不足 (\(Int(features.totalDurationMin)) 分钟)")
        } else if features.totalDurationMin >= 480 {
            factors.append("睡眠时长充足 (\(Int(features.totalDurationMin)) 分钟)")
        }

        // HR-based
        if features.meanHr > 80 {
            factors.append("平均心率偏高 (\(Int(features.meanHr)) bpm)")
        } else if features.meanHr < 70 {
            factors.append("平均心率较低 (\(Int(features.meanHr)) bpm)")
        }

        // HR drop
        if features.hrDropFirst90m < 3 {
            factors.append("入睡后心率下降不足")
        } else if features.hrDropFirst90m > 8 {
            factors.append("入睡后心率下降充分")
        }

        // Activity
        if features.activitySpikeCount > 10 {
            factors.append("夜间活动峰值较多 (\(features.activitySpikeCount) 次)")
        }

        // Fragmentation
        if features.sleepFragmentationScore > 0.2 {
            factors.append("睡眠碎片化程度较高")
        }

        return factors.isEmpty ? ["各项指标均在正常范围"] : factors
    }
}
