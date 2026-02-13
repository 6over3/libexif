// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import Foundation

/// Metadata operation failed inside the WASM sandbox.
public enum ExifError: Error, LocalizedError {
    case initializationFailed
    case operationFailed(message: String, exitCode: Int32)

    public var errorDescription: String? {
        switch self {
        case .initializationFailed:
            "Failed to initialize WASM runtime"
        case .operationFailed(let message, _):
            message
        }
    }
}
