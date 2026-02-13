// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

import CLibExif
import Foundation
import Synchronization

/// Read and write image metadata via exiftool in a WASM sandbox.
///
/// Thread-safe. Each instance serializes operations on its WASM exec environment.
/// Reuse a single instance to avoid repeated startup cost.
///
///     let exif = try Exif()
///     let json = try exif.read(from: photoURL)
///     try exif.write(to: photoURL, tags: ["-Artist=Jane"])
public final class Exif: Sendable {
    let ctx: Mutex<OpaquePointer>

    /// Load the AOT module and initialize the WASM runtime.
    public init(_ config: ExifConfig = .init()) throws(ExifError) {
        var cfg = exif_config_t(
            allocator: nil,
            wasm_stack_size: config.wasmStackSize,
            wasm_heap_size: config.wasmHeapSize,
            exec_stack_size: config.execStackSize
        )
        guard let ptr = exif_create(&cfg) else {
            throw .initializationFailed
        }
        self.ctx = Mutex(ptr)
    }

    deinit {
        ctx.withLock { exif_destroy($0) }
    }

    func string(ctx: OpaquePointer, from result: exif_result_t) throws(ExifError) -> String {
        var r = result
        defer { exif_result_free(ctx, &r) }
        guard r.success, let ptr = r.data else { throw error(from: r) }
        return String(cString: ptr)
    }

    func bytes(ctx: OpaquePointer, from result: exif_result_t) throws(ExifError) -> Data {
        var r = result
        defer { exif_result_free(ctx, &r) }
        guard r.success, let ptr = r.data else { throw error(from: r) }
        return Data(bytes: ptr, count: r.data_len)
    }

    private func error(from r: exif_result_t) -> ExifError {
        .operationFailed(
            message: r.error.map { String(cString: $0) } ?? "unknown error",
            exitCode: r.exit_code
        )
    }

    func withOptions(
        args: [String] = [],
        tags: [String] = [],
        body: (inout exif_options_t) -> exif_result_t
    ) -> exif_result_t {
        withCStringArray(args) { argsPtr in
            withCStringArray(tags) { tagsPtr in
                var opts = exif_options_t()
                opts.args = argsPtr
                opts.argc = Int32(args.count)
                opts.tags = tagsPtr
                opts.ntags = Int32(tags.count)
                return body(&opts)
            }
        }
    }
}

extension Exif {
    func runBlocking<T: Sendable>(
        _ work: @Sendable @escaping () throws -> T
    ) async throws(ExifError) -> T {
        let result: Result<T, any Error> = await Task.detached {
            Result { try work() }
        }.value
        switch result {
        case .success(let v): return v
        case .failure(let e as ExifError): throw e
        case .failure(let e):
            throw .operationFailed(message: e.localizedDescription, exitCode: -1)
        }
    }
}

private func withCStringArray<R>(
    _ strings: [String],
    body: (UnsafeMutablePointer<UnsafePointer<CChar>?>?) -> R
) -> R {
    guard !strings.isEmpty else { return body(nil) }
    let cStrings = strings.compactMap { strdup($0) }
    defer { cStrings.forEach { free($0) } }
    guard cStrings.count == strings.count else { return body(nil) }
    var ptrs = cStrings.map { UnsafePointer<CChar>?($0) }
    return ptrs.withUnsafeMutableBufferPointer { body($0.baseAddress) }
}
