// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import Testing
import Foundation
@testable import Exif

@Suite("Read from URL")
struct ReadURLTests {
    @Test func jpeg() throws {
        let json = try sharedExif.read(from: testDataURL("test.jpg"))
        #expect(json.contains("FileName"))
    }

    @Test func png() throws {
        let json = try sharedExif.read(from: testDataURL("test.png"))
        #expect(json.contains("ImageWidth"))
    }

    @Test func tiff() throws {
        let json = try sharedExif.read(from: testDataURL("test.tiff"))
        #expect(json.contains("FileType"))
    }

    @Test func exr() throws {
        let json = try sharedExif.read(from: testDataURL("test.exr"))
        #expect(json.contains("ImageWidth"))
    }

    @Test func dng() throws {
        let json = try sharedExif.read(from: testDataURL("Mo_Edge20_ColourfulStreet.dng"))
        #expect(json.contains("Make"))
        #expect(json.contains("Model"))
    }
}

@Suite("Read from Buffer")
struct ReadBufferTests {
    @Test func jpeg() throws {
        let url = try testDataURL("test.jpg")
        let data = try Data(contentsOf: url)
        let json = try sharedExif.read(data: data, url: url)
        #expect(json.contains("FileName"))
    }

    @Test func png() throws {
        let url = try testDataURL("test.png")
        let data = try Data(contentsOf: url)
        let json = try sharedExif.read(data: data, url: url)
        #expect(json.contains("ImageWidth"))
    }

    @Test func tiff() throws {
        let url = try testDataURL("test.tiff")
        let data = try Data(contentsOf: url)
        let json = try sharedExif.read(data: data, url: url)
        #expect(json.contains("FileType"))
    }

    @Test func exr() throws {
        let url = try testDataURL("test.exr")
        let data = try Data(contentsOf: url)
        let json = try sharedExif.read(data: data, url: url)
        #expect(json.contains("ImageWidth"))
    }

    @Test func dng() throws {
        let url = try testDataURL("Mo_Edge20_ColourfulStreet.dng")
        let data = try Data(contentsOf: url)
        let json = try sharedExif.read(data: data, url: url)
        #expect(json.contains("Make"))
        #expect(json.contains("Model"))
    }
}
