// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import Foundation
import Testing
@testable import Exif

let sharedExif: Exif = {
    do {
        return try Exif()
    } catch {
        fatalError("Failed to initialize Exif: \(error)")
    }
}()

func testDataURL(_ name: String) throws -> URL {
    guard let url = Bundle.module.url(forResource: name, withExtension: nil, subdirectory: "Resources") else {
        throw ExifError.operationFailed(message: "test resource not found: \(name)", exitCode: -1)
    }
    return url
}
