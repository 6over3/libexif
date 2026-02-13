// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import Testing
import Foundation
@testable import Exif

@Suite("Write")
struct WriteTests {
    @Test func roundtrip() throws {
        let url = try testDataURL("test.jpg")
        let jpeg = try Data(contentsOf: url)
        let modified = try sharedExif.write(data: jpeg, url: url, tags: ["-Artist=swift test"])
        let json = try sharedExif.read(data: modified, url: url)
        #expect(json.contains("\"swift test\""))
    }

    @Test func multipleTags() throws {
        let url = try testDataURL("test.jpg")
        let jpeg = try Data(contentsOf: url)
        let modified = try sharedExif.write(
            data: jpeg,
            url: url,
            tags: ["-Artist=Alice", "-Comment=hello"]
        )
        let json = try sharedExif.read(data: modified, url: url)
        #expect(json.contains("\"Alice\""))
    }
}

@Suite("Async Write")
struct AsyncWriteTests {
    @Test func roundtrip() async throws {
        let url = try testDataURL("test.jpg")
        let jpeg = try Data(contentsOf: url)
        let modified = try await sharedExif.write(data: jpeg, url: url, tags: ["-Artist=async test"])
        let json = try await sharedExif.read(data: modified, url: url)
        #expect(json.contains("\"async test\""))
    }
}
