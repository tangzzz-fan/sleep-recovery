// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "SleepRecovery",
    platforms: [
        .macOS(.v14),
    ],
    products: [
        .executable(name: "SleepRecovery", targets: ["SleepRecovery"]),
    ],
    targets: [
        .executableTarget(
            name: "SleepRecovery",
            path: "SleepRecovery/Sources",
        ),
    ]
)
