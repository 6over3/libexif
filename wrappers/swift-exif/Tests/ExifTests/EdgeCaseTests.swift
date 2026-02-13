// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import Testing
import Foundation
@testable import Exif

@Suite("Edge Cases")
struct EdgeCaseTests {
    @Test func multipleReads() throws {
        let url = try testDataURL("test.jpg")
        let jpeg = try Data(contentsOf: url)
        for _ in 0..<5 {
            let json = try sharedExif.read(data: jpeg, url: url)
            #expect(!json.isEmpty)
        }
    }
}
