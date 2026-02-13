// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import Testing
import Foundation
@testable import Exif

@Suite("Unicode")
struct UnicodeTests {
    @Test func korean() throws {
        let url = try testDataURL("test.jpg")
        let jpeg = try Data(contentsOf: url)
        let text = "안녕하세요"
        let modified = try sharedExif.write(data: jpeg, url: url, tags: ["-Artist=\(text)"])
        let json = try sharedExif.read(data: modified, url: url)
        #expect(json.contains(text))
        #expect(!json.contains("\u{FFFD}"))
    }

    @Test func japanese() throws {
        let url = try testDataURL("test.jpg")
        let jpeg = try Data(contentsOf: url)
        let text = "こんにちは"
        let modified = try sharedExif.write(data: jpeg, url: url, tags: ["-Artist=\(text)"])
        let json = try sharedExif.read(data: modified, url: url)
        #expect(json.contains(text))
    }

    @Test func chinese() throws {
        let url = try testDataURL("test.jpg")
        let jpeg = try Data(contentsOf: url)
        let text = "你好世界"
        let modified = try sharedExif.write(data: jpeg, url: url, tags: ["-Artist=\(text)"])
        let json = try sharedExif.read(data: modified, url: url)
        #expect(json.contains(text))
    }

    @Test func mixed() throws {
        let url = try testDataURL("test.jpg")
        let jpeg = try Data(contentsOf: url)
        let text = "Hello 안녕 こんにちは 你好"
        let modified = try sharedExif.write(data: jpeg, url: url, tags: ["-ImageDescription=\(text)"])
        let json = try sharedExif.read(data: modified, url: url)
        #expect(json.contains(text))
    }
}
