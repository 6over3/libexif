// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

/// Runtime configuration for the WASM sandbox.
public struct ExifConfig: Sendable {
    public var wasmStackSize: UInt32
    public var wasmHeapSize: UInt32
    public var execStackSize: UInt32

    public init(
        wasmStackSize: UInt32 = 8 << 20,
        wasmHeapSize: UInt32 = 32 << 20,
        execStackSize: UInt32 = 8 << 20
    ) {
        self.wasmStackSize = wasmStackSize
        self.wasmHeapSize = wasmHeapSize
        self.execStackSize = execStackSize
    }
}
