import SwiftUI
import Charts
import UniformTypeIdentifiers

struct HomeView: View {
    @State private var featurePackage: FeaturePackage?
    @State private var hrData: [HrDataPoint] = []
    @State private var activityData: [ActivityDataPoint] = []
    @State private var prediction: (label: Int, probabilities: [Int: Double])?
    @State private var factors: [String] = []
    @State private var isShowingFilePicker = false
    @State private var loadedUrl: URL?

    private let loader = FeaturePackageLoader()
    private let predictor = PredictionService()
    @State private var allSessions: [FeaturePackage] = []

    var body: some View {
        NavigationSplitView {
            sidebar
        } detail: {
            TabView {
                mainContent
                    .tabItem {
                        Label("单夜分析", systemImage: "moon.zzz.fill")
                    }

                TrendView(sessions: allSessions)
                    .tabItem {
                        Label("多夜趋势", systemImage: "chart.line.uptrend.xyaxis")
                    }
            }
        }
    }

    // MARK: - Sidebar
    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("睡眠恢复分析")
                .font(.title2)
                .fontWeight(.bold)
                .padding(.bottom, 8)

            Button(action: { isShowingFilePicker = true }) {
                Label("加载数据", systemImage: "folder")
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .buttonStyle(.borderedProminent)
            .fileImporter(
                isPresented: $isShowingFilePicker,
                allowedContentTypes: [.commaSeparatedText],
                allowsMultipleSelection: true
            ) { result in
                if case .success(let urls) = result {
                    loadData(from: urls)
                }
            }

            if let pkg = featurePackage {
                Divider()

                Text("已加载")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text(pkg.sessionId)
                    .font(.headline)
                Text("\(Int(pkg.totalDurationMin)) 分钟 · label=\(pkg.labelName)")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }

            Spacer()
        }
        .padding()
        .frame(minWidth: 200, idealWidth: 250)
    }

    // MARK: - Main Content
    private var mainContent: some View {
        ScrollView {
            VStack(spacing: 24) {
                if featurePackage == nil {
                    emptyState
                } else {
                    recoveryCard
                    trendCharts
                    factorsCard
                }
            }
            .padding()
        }
        .frame(minWidth: 500, idealWidth: 700)
    }

    // MARK: - Empty State
    private var emptyState: some View {
        VStack(spacing: 16) {
            Image(systemName: "moon.zzz.fill")
                .font(.system(size: 64))
                .foregroundStyle(.secondary)

            Text("睡眠恢复分析")
                .font(.title)
                .fontWeight(.medium)

            Text("点击左侧「加载数据」选择一个 derived_features.csv 文件开始分析")
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .frame(maxWidth: 400)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding(.top, 100)
    }

    // MARK: - Recovery Card
    private var recoveryCard: some View {
        GroupBox {
            HStack(spacing: 20) {
                // Recovery label badge
                VStack(spacing: 6) {
                    Text(labelEmoji)
                        .font(.system(size: 36))
                    Text(featurePackage?.labelName ?? "")
                        .font(.title2)
                        .fontWeight(.bold)

                    if let prob = prediction?.probabilities,
                       let maxProb = prob.values.max() {
                        Text("置信度: \(Int(maxProb * 100))%")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
                .padding()
                .background(labelColor.opacity(0.15))
                .clipShape(RoundedRectangle(cornerRadius: 12))

                // Key metrics
                VStack(alignment: .leading, spacing: 8) {
                    metricRow(icon: "timer", label: "睡眠时长", value: "\(Int(featurePackage!.totalDurationMin)) 分钟")
                    metricRow(icon: "heart.fill", label: "平均心率", value: "\(Int(featurePackage!.meanHr)) bpm")
                    metricRow(icon: "figure.walk", label: "活动峰值", value: "\(featurePackage!.activitySpikeCount) 次")
                    metricRow(icon: "bed.double.fill", label: "碎片化分数", value: String(format: "%.2f", featurePackage!.sleepFragmentationScore))
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        } label: {
            Label("恢复评估", systemImage: "heart.text.square")
        }
    }

    // MARK: - Trend Charts
    private var trendCharts: some View {
        VStack(spacing: 16) {
            GroupBox {
                VStack(alignment: .leading, spacing: 8) {
                    Text("夜间心率趋势")
                        .font(.headline)

                    Chart {
                        ForEach(hrData) { point in
                            LineMark(
                                x: .value("时间", point.timestamp),
                                y: .value("心率", point.heartRate)
                            )
                            .foregroundStyle(.red.opacity(0.7))
                        }

                        RuleMark(y: .value("平均", featurePackage?.meanHr ?? 70))
                            .lineStyle(StrokeStyle(lineWidth: 1, dash: [5, 5]))
                            .foregroundStyle(.secondary)
                            .annotation(position: .trailing) {
                                Text("avg")
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                            }
                    }
                    .chartYScale(domain: 40...120)
                    .chartXAxis {
                        AxisMarks(values: .automatic(desiredCount: 6))
                    }
                    .frame(height: 200)
                }
                .padding(.vertical, 8)
            } label: {
                Label("心率", systemImage: "heart")
            }

            GroupBox {
                VStack(alignment: .leading, spacing: 8) {
                    Text("夜间活动强度")
                        .font(.headline)

                    Chart {
                        ForEach(activityData) { point in
                            BarMark(
                                x: .value("时间", point.timestamp),
                                y: .value("强度", point.intensity)
                            )
                            .foregroundStyle(.blue.opacity(0.5))
                        }
                    }
                    .chartYScale(domain: 0...0.5)
                    .chartXAxis {
                        AxisMarks(values: .automatic(desiredCount: 6))
                    }
                    .frame(height: 200)
                }
                .padding(.vertical, 8)
            } label: {
                Label("活动", systemImage: "figure.walk")
            }
        }
    }

    // MARK: - Contributing Factors
    private var factorsCard: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 8) {
                ForEach(factors.indices, id: \.self) { i in
                    HStack(spacing: 8) {
                        Image(systemName: "circle.fill")
                            .font(.system(size: 6))
                            .foregroundStyle(.secondary)
                        Text(factors[i])
                            .font(.body)
                    }
                }

                if factors.isEmpty {
                    Text("正在分析...")
                        .foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        } label: {
            Label("关键影响因素", systemImage: "list.bullet.rectangle")
        }
    }

    // MARK: - Helpers
    private var labelEmoji: String {
        switch featurePackage?.recoveryLabel {
        case 0: return "😫"
        case 1: return "😐"
        case 2: return "😊"
        default: return "❓"
        }
    }

    private var labelColor: Color {
        switch featurePackage?.recoveryLabel {
        case 0: return .red
        case 1: return .orange
        case 2: return .green
        default: return .gray
        }
    }

    private func metricRow(icon: String, label: String, value: String) -> some View {
        HStack(spacing: 6) {
            Image(systemName: icon)
                .frame(width: 20)
                .foregroundStyle(.secondary)
            Text(label)
                .foregroundStyle(.secondary)
            Text(value)
                .fontWeight(.medium)
        }
    }

    // MARK: - Data Loading
    private func loadData(from urls: [URL]) {
        for url in urls {
            // Load derived_features.csv
            if url.lastPathComponent.contains("derived_features") || url.lastPathComponent.contains("features") {
                if let packages = loader.load(from: url) {
                    allSessions = packages
                    if let first = packages.first {
                        featurePackage = first
                        loadedUrl = url.deletingLastPathComponent()

                        // Load time-series data from the same directory
                        hrData = loader.loadHrTimeSeries(for: first.sessionId, from: loadedUrl!)
                        activityData = loader.loadActivityTimeSeries(for: first.sessionId, from: loadedUrl!)

                        // Run prediction
                        prediction = predictor.predict(features: first)
                        factors = predictor.contributingFactors(features: first, prediction: prediction)
                }
                }
            }
        }
    }
}

#Preview {
    HomeView()
}
