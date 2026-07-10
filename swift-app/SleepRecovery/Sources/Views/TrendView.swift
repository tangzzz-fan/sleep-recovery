import SwiftUI
import Charts

// Multi-night recovery trend view (Ticket 06)
// Shows recovery scores across multiple nights for the same user,
// or across all users in the loaded dataset.
struct TrendView: View {
    let sessions: [FeaturePackage]
    @State private var selectedUser: String = "all"
    @State private var trendData: [NightResult] = []

    // Unique users in the dataset
    private var users: [String] {
        let all = Set(sessions.map { String($0.sessionId.split(separator: "_").first ?? "") })
        return ["all"] + Array(all).sorted()
    }

    var body: some View {
        VStack(spacing: 16) {
            // User selector
            HStack {
                Text("用户筛选:")
                    .foregroundStyle(.secondary)
                Picker("", selection: $selectedUser) {
                    Text("全部用户 (12)").tag("all")
                    ForEach(users.filter { $0 != "all" }, id: \.self) { uid in
                        Text(uid).tag(uid)
                    }
                }
                .pickerStyle(.menu)
                .onChange(of: selectedUser) { _ in
                    buildTrendData()
                }

                Spacer()

                Text("\(trendData.count) 晚")
                    .foregroundStyle(.secondary)
            }

            // Trend chart
            if !trendData.isEmpty {
                chartView
            } else {
                ContentUnavailableView(
                    "无趋势数据",
                    systemImage: "chart.xyaxis.line",
                    description: Text("加载 derived_features.csv 后显示多夜趋势")
                )
            }

            // Summary stats
            if !trendData.isEmpty {
                statsView
            }
        }
        .onAppear { buildTrendData() }
    }

    // MARK: - Chart

    @ViewBuilder
    private var chartView: some View {
        GroupBox {
            Chart {
                ForEach(trendData) { night in
                    LineMark(
                        x: .value("日期", night.date),
                        y: .value("恢复分数", night.recoveryScore)
                    )
                    .foregroundStyle(by: .value("等级", night.recoveryLabel == 2 ? "良好" : night.recoveryLabel == 1 ? "一般" : "差"))
                }

                // Threshold lines
                RuleMark(y: .value("良好", 75))
                    .lineStyle(StrokeStyle(lineWidth: 1, dash: [5, 5]))
                    .foregroundStyle(.green.opacity(0.5))
                    .annotation(position: .trailing) {
                        Text("良好")
                            .font(.caption2)
                            .foregroundStyle(.green)
                    }

                RuleMark(y: .value("一般", 50))
                    .lineStyle(StrokeStyle(lineWidth: 1, dash: [5, 5]))
                    .foregroundStyle(.orange.opacity(0.5))
                    .annotation(position: .trailing) {
                        Text("一般")
                            .font(.caption2)
                            .foregroundStyle(.orange)
                    }
            }
            .chartYScale(domain: 0...100)
            .chartXAxis {
                AxisMarks(values: .automatic(desiredCount: 8))
            }
            .frame(height: 280)
        } label: {
            Label("恢复趋势", systemImage: "chart.line.uptrend.xyaxis")
        }
    }

    // MARK: - Stats

    @ViewBuilder
    private var statsView: some View {
        HStack(spacing: 20) {
            statCard(
                title: "平均恢复分数",
                value: String(format: "%.0f", trendData.map(\.recoveryScore).reduce(0, +) / Double(trendData.count)),
                color: .blue
            )

            statCard(
                title: "良好比例",
                value: String(format: "%.0f%%", Double(trendData.filter { $0.recoveryLabel == 2 }.count) / Double(trendData.count) * 100),
                color: .green
            )

            statCard(
                title: "最差分",
                value: String(format: "%.0f", trendData.map(\.recoveryScore).min() ?? 0),
                color: .red
            )

            statCard(
                title: "最佳分",
                value: String(format: "%.0f", trendData.map(\.recoveryScore).max() ?? 100),
                color: .green
            )
        }
    }

    @ViewBuilder
    private func statCard(title: String, value: String, color: Color) -> some View {
        VStack(spacing: 4) {
            Text(value)
                .font(.title2)
                .fontWeight(.bold)
                .foregroundStyle(color)
            Text(title)
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 8)
        .background(color.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }

    // MARK: - Data

    private func buildTrendData() {
        var filtered = sessions

        if selectedUser != "all" {
            filtered = sessions.filter {
                ($0.sessionId.split(separator: "_").first ?? "") == selectedUser
            }
        }

        trendData = filtered.compactMap { pkg in
            // Parse date from session_id: userid_M-D-YYYY
            let parts = pkg.sessionId.split(separator: "_")
            guard parts.count >= 2 else { return nil }

            let dateStr = String(parts[1])
            let dateFormatter = DateFormatter()
            dateFormatter.dateFormat = "M-d-yyyy"
            dateFormatter.locale = Locale(identifier: "en_US_POSIX")

            guard let date = dateFormatter.date(from: dateStr) else { return nil }

            return NightResult(
                sessionId: pkg.sessionId,
                date: date,
                recoveryLabel: pkg.recoveryLabel,
                recoveryScore: pkg.recoveryScore,
                confidence: 0.85
            )
        }
        .sorted { $0.date < $1.date }
    }
}

#Preview {
    TrendView(sessions: [])
        .frame(width: 700, height: 500)
}
