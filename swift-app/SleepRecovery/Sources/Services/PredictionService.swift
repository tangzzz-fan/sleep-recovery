import Foundation
import CoreML

// Service to load the Core ML model and run inference
class PredictionService {
    private var model: MLModel?

    init() {
        loadModel()
    }

    private func loadModel() {
        // In Xcode, the model is compiled into the app bundle as a .mlmodelc directory.
        // In SPM debug builds (swift run), the model stays as .mlpackage alongside the binary.
        // Search order:
        //   1. Bundle resource (Xcode: compiled .mlmodelc)
        //   2. Executable directory tree (SPM: .mlpackage in .build/debug/)
        let bundleURL = Bundle.main.url(forResource: "lr_classifier", withExtension: "mlmodelc")
            ?? Bundle.main.url(forResource: "lr_classifier", withExtension: "mlpackage")

        if let url = bundleURL {
            model = try? MLModel(contentsOf: url)
            if model != nil {
                print("[PredictionService] Loaded model from bundle: \(url.lastPathComponent)")
                return
            }
        }

        // Fallback: SPM debug build — search executable dir tree
        if let exeDir = Bundle.main.executableURL?.deletingLastPathComponent() {
            let dirs = [exeDir,
                        exeDir.deletingLastPathComponent(),
                        exeDir.deletingLastPathComponent().deletingLastPathComponent()]
            for dir in dirs {
                for name in ["lr_classifier", "rf_classifier", "xgb_classifier"] {
                    let path = dir.appendingPathComponent("\(name).mlpackage")
                    if FileManager.default.fileExists(atPath: path.path) {
                        model = try? MLModel(contentsOf: path)
                        if model != nil {
                            print("[PredictionService] Loaded model from \(path.path)")
                            return
                        }
                    }
                }
            }
        }

        print("[PredictionService] No Core ML model found. Using placeholder predictions.")
    }

    // Predict recovery label from feature package
    func predict(features: FeaturePackage) -> (label: Int, probabilities: [Int: Double])? {
        guard let model = model else {
            return (features.recoveryLabel, [
                0: 0.05,
                1: 0.15,
                2: 0.80,
            ])
        }

        do {
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

            let inputFeatures = try MLDictionaryFeatureProvider(dictionary: input)
            let output = try model.prediction(from: inputFeatures)

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

    func contributingFactors(features: FeaturePackage, prediction: (label: Int, probabilities: [Int: Double])?) -> [String] {
        var factors: [String] = []

        if features.totalDurationMin < 360 {
            factors.append("睡眠时长不足 (\(Int(features.totalDurationMin)) 分钟)")
        } else if features.totalDurationMin >= 480 {
            factors.append("睡眠时长充足 (\(Int(features.totalDurationMin)) 分钟)")
        }

        if features.meanHr > 80 {
            factors.append("平均心率偏高 (\(Int(features.meanHr)) bpm)")
        } else if features.meanHr < 70 {
            factors.append("平均心率较低 (\(Int(features.meanHr)) bpm)")
        }

        if features.hrDropFirst90m < 3 {
            factors.append("入睡后心率下降不足")
        } else if features.hrDropFirst90m > 8 {
            factors.append("入睡后心率下降充分")
        }

        if features.activitySpikeCount > 10 {
            factors.append("夜间活动峰值较多 (\(features.activitySpikeCount) 次)")
        }

        if features.sleepFragmentationScore > 0.2 {
            factors.append("睡眠碎片化程度较高")
        }

        return factors.isEmpty ? ["各项指标均在正常范围"] : factors
    }
}