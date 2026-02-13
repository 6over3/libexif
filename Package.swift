// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "swift-exif",
    platforms: [.macOS(.v15)],
    products: [
        .library(name: "Exif", targets: ["Exif"]),
    ],
    targets: [
        .systemLibrary(
            name: "CLibExif",
            path: "wrappers/swift-exif/Sources/CLibExif"
        ),
        .target(
            name: "Exif",
            dependencies: ["CLibExif"],
            path: "wrappers/swift-exif/Sources/Exif",
            linkerSettings: [
                .unsafeFlags(["-Lbuild"]),
            ]
        ),
        .testTarget(
            name: "ExifTests",
            dependencies: ["Exif"],
            path: "wrappers/swift-exif/Tests/ExifTests",
            resources: [.copy("Resources")]
        ),
    ]
)
