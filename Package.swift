// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "swift-exif",
    platforms: [.macOS(.v15)],
    products: [
        .library(name: "Exif", targets: ["Exif"]),
    ],
    targets: [
        .binaryTarget(
            name: "libexif",
            path: "wrappers/swift-exif/libexif.xcframework"
        ),
        .target(
            name: "Exif",
            dependencies: ["libexif"],
            path: "wrappers/swift-exif/Sources/Exif"
        ),
        .testTarget(
            name: "ExifTests",
            dependencies: ["Exif"],
            path: "wrappers/swift-exif/Tests/ExifTests",
            resources: [.copy("Resources")]
        ),
    ]
)
