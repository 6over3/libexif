// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import CLibExif
import Foundation

extension Exif {
    /// Write tags to a file on disk.
    /// - Parameter outputURL: Destination file. `nil` overwrites the source.
    @discardableResult
    public func write(
        to url: URL,
        outputURL: URL? = nil,
        tags: [String],
        args: [String] = []
    ) throws(ExifError) -> String {
        try ctx.withLock { (ptr: inout OpaquePointer) throws(ExifError) -> String in
            try string(ctx: ptr, from: withOptions(args: args, tags: tags) {
                exif_write(ptr, url.path, outputURL?.path, &$0)
            })
        }
    }

    /// Write tags to a file on disk.
    /// - Parameter outputURL: Destination file. `nil` overwrites the source.
    @discardableResult
    public func write(
        to url: URL,
        outputURL: URL? = nil,
        tags: [String],
        args: [String] = []
    ) async throws(ExifError) -> String {
        try await runBlocking { try self.write(to: url, outputURL: outputURL, tags: tags, args: args) }
    }

    /// Write tags to an in-memory buffer and return the modified file bytes.
    /// - Parameter url: Used for extension-based format detection (e.g. "photo.jpg").
    public func write(
        data: Data,
        url: URL,
        tags: [String],
        args: [String] = []
    ) throws(ExifError) -> Data {
        try ctx.withLock { (ptr: inout OpaquePointer) throws(ExifError) -> Data in
            let filename = url.lastPathComponent
            let result = data.withUnsafeBytes { bytes in
                filename.withCString { fname in
                    let buf = exif_buf_t(data: bytes.baseAddress, len: bytes.count, filename: fname)
                    return withOptions(args: args, tags: tags) { exif_write_buf(ptr, buf, &$0) }
                }
            }
            return try bytes(ctx: ptr, from: result)
        }
    }

    /// Write tags to an in-memory buffer and return the modified file bytes.
    /// - Parameter url: Used for extension-based format detection (e.g. "photo.jpg").
    public func write(
        data: Data,
        url: URL,
        tags: [String],
        args: [String] = []
    ) async throws(ExifError) -> Data {
        try await runBlocking { try self.write(data: data, url: url, tags: tags, args: args) }
    }
}
