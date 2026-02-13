# libexif

C library for reading and writing image metadata. Runs exiftool inside a WebAssembly sandbox via WAMR in AOT mode.

## License

AGPL-3.0. See [LICENSE](LICENSE).

## Build

Requires CMake 3.14+ and a C23 compiler.

```
git submodule update --init
cmake -B build
cmake --build build
```

Produces `libexif.a`, a static library with WAMR baked in.

### Prerequisites

The AOT-compiled exiftool module must exist at `resources/zeroperl.aot`. Generate it with:

```
./scripts/update-aot.sh <path/to/zeroperl.wasm>
```

This requires LLVM 18: `brew install llvm@18`.

## C API

```c
#include "libexif.h"
```

### Lifecycle

```c
exif_t *ctx = exif_create(NULL);
// ... use ctx ...
exif_destroy(ctx);
```

`exif_create` loads the AOT module and initializes the WASM runtime. Reuse a single context to amortize startup cost, ~30ms.

### Read

```c
exif_result_t r = exif_read(ctx, "/path/to/photo.jpg", NULL);
if (r.success)
    printf("%.*s\n", (int)r.data_len, r.data);
exif_result_free(ctx, &r);
```

Reads always return structured JSON with default flags `-json -a -s -n -G1 -b`.

From an in-memory buffer:

```c
exif_buf_t buf = { .data = bytes, .len = nbytes, .filename = "photo.jpg" };
exif_result_t r = exif_read_buf(ctx, buf, NULL);
```

The filename extension determines format handling.

### Write

```c
const char *tags[] = { "-Artist=Jane", "-Comment=test" };
exif_options_t opts = { .tags = tags, .ntags = 2 };
exif_result_t r = exif_write(ctx, "/path/to/photo.jpg", "/path/to/output.jpg", &opts);
exif_result_free(ctx, &r);
```

Pass `NULL` as `out_path` to overwrite the source file.

Buffer variant returns the modified file bytes:

```c
exif_result_t r = exif_write_buf(ctx, buf, &opts);
// r.data contains the modified file
```

### Options

Extra exiftool CLI args can be passed per-call:

```c
const char *args[] = { "-api", "geolocation" };
exif_options_t opts = { .args = args, .argc = 2 };
exif_result_t r = exif_read(ctx, path, &opts);
```

A transform callback can post-process stdout before it's returned:

```c
char *my_transform(const char *data, size_t len, void *ctx) {
    // return a caller-owned string; the library frees the original
}
exif_options_t opts = { .transform = my_transform };
```

### Configuration

```c
exif_config_t cfg = {
    .allocator = &my_allocator,
    .wasm_stack_size = 8 << 20,   // 8 MiB, the default
    .wasm_heap_size = 32 << 20,   // 32 MiB, the default
    .exec_stack_size = 8 << 20,   // 8 MiB, the default
};
exif_t *ctx = exif_create(&cfg);
```

### Thread safety

A single `exif_t` context is not thread-safe. Use one context per thread, or synchronize externally.

## Swift wrapper

The `Exif` Swift package wraps the C API. Thread-safe via internal `Mutex`.

```swift
let exif = try Exif()

// Read from file or in-memory buffer
let json = try exif.read(from: photoURL)
let json2 = try exif.read(data: imageData, url: photoURL)

// Write to file
try exif.write(to: photoURL, tags: ["-Artist=Jane"])
try exif.write(to: photoURL, outputURL: outputURL, tags: ["-Artist=Jane"])

// Write to buffer
let modified = try exif.write(data: imageData, url: photoURL, tags: ["-Artist=Jane"])
```

### Build

Swift tests link against `libexif.a` from `build/`:

```
cmake -B build && cmake --build build
swift test
```

## Tests

```
./build/exif_test
swift test
```

## Benchmark

```
./build/exif_bench <image>
```

Apple M-series, AOT mode:

| Operation | Time |
|-----------|------|
| create    | ~30ms |
| read      | ~60ms |
| write     | ~200ms |
