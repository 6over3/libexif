// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import CLibExif
import Foundation

extension Exif {
    /// Extract metadata from a file on disk as structured JSON.
    public func read(from url: URL, args: [String] = []) throws(ExifError) -> String {
        try ctx.withLock { (ptr: inout OpaquePointer) throws(ExifError) -> String in
            try string(ctx: ptr, from: withOptions(args: args) { exif_read(ptr, url.path, &$0) })
        }
    }

    /// Extract metadata from a file on disk as structured JSON.
    public func read(from url: URL, args: [String] = []) async throws(ExifError) -> String {
        try await runBlocking { try self.read(from: url, args: args) }
    }

    /// Extract metadata from an in-memory buffer as structured JSON.
    /// - Parameter url: Used for extension-based format detection (e.g. "photo.dng").
    public func read(data: Data, url: URL, args: [String] = []) throws(ExifError) -> String {
        try ctx.withLock { (ptr: inout OpaquePointer) throws(ExifError) -> String in
            let filename = url.lastPathComponent
            let result = data.withUnsafeBytes { bytes in
                filename.withCString { fname in
                    let buf = exif_buf_t(data: bytes.baseAddress, len: bytes.count, filename: fname)
                    return withOptions(args: args) { exif_read_buf(ptr, buf, &$0) }
                }
            }
            return try string(ctx: ptr, from: result)
        }
    }

    /// Extract metadata from an in-memory buffer as structured JSON.
    /// - Parameter url: Used for extension-based format detection (e.g. "photo.dng").
    public func read(data: Data, url: URL, args: [String] = []) async throws(ExifError) -> String {
        try await runBlocking { try self.read(data: data, url: url, args: args) }
    }
}
