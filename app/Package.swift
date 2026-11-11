// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AMDVirtManager",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "AMDVirtManager",
            path: "Sources/AMDVirtManager",
            swiftSettings: [.swiftLanguageMode(.v5)]
        )
    ]
)
