import SwiftUI

@main
struct SleepRecoveryApp: App {
    var body: some Scene {
        Window("睡眠恢复", id: "main") {
            HomeView()
                .frame(minWidth: 800, minHeight: 600)
        }
        .windowResizability(.contentMinSize)
    }
}
