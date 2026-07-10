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
            resources: [
                // Keep Core ML packages as bundled resources instead of
                // source inputs, so SwiftPM does not try to codegen wrappers.
                .copy("Resources/Models"),
            ]
        ),
    ]
)
