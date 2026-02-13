// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

//! @file libexif.h
//! Read and write image metadata via exiftool in a WASM sandbox.

#ifndef LIBEXIF_H
#define LIBEXIF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef EXIF_SHARED
    #ifdef _WIN32
        #ifdef EXIF_BUILD
            #define EXIF_API __declspec(dllexport)
        #else
            #define EXIF_API __declspec(dllimport)
        #endif
    #elif __GNUC__ >= 4
        #define EXIF_API __attribute__((visibility("default")))
    #else
        #define EXIF_API
    #endif
#else
    #define EXIF_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct exif exif_t;

//! Custom allocator. All strings in exif_result_t are allocated through this.
typedef struct exif_allocator {
    void *(*alloc)(size_t size, void *ctx);  // return NULL on failure; size is never 0
    void  (*free)(void *ptr, size_t size, void *ctx);  // size may be 0 when unknown
    void   *ctx;  // forwarded as last arg to alloc and free
} exif_allocator_t;

//! Runtime configuration. Zero-init for defaults.
//! @param allocator  NULL uses malloc/free.
typedef struct exif_config {
    exif_allocator_t *allocator;
    uint32_t          wasm_stack_size;   // default: 8 MiB
    uint32_t          wasm_heap_size;    // default: 32 MiB
    uint32_t          exec_stack_size;   // default: 8 MiB
} exif_config_t;

//! Transform raw exiftool stdout before returning it in a result.
//! Return a caller-owned string; the library frees the original data.
typedef char *(*exif_transform_fn)(const char *data, size_t len, void *ctx);

//! Per-operation options. Zero-init for defaults. All fields optional.
typedef struct exif_options {
    const char       **args;            // extra exiftool CLI args
    int                argc;
    const char        *config_path;     // passed as -config <path>
    const char       **tags;            // write tags, e.g. "-Artist=John"
    int                ntags;
    exif_transform_fn  transform;       // post-process stdout before return
    void              *transform_ctx;
} exif_options_t;

//! Named in-memory buffer. Filename extension determines format handling.
typedef struct exif_buf {
    const void *data;
    size_t      len;
    const char *filename;
} exif_buf_t;

//! Operation result. Owned by the context's allocator; free with exif_result_free.
//! On success: data/data_len hold output, error is NULL.
//! On failure: error holds a message, data is NULL.
typedef struct exif_result {
    bool     success;
    char    *data;
    size_t   data_len;
    char    *error;
    int32_t  exit_code;
} exif_result_t;

//! Load the AOT module and initialize the WASM runtime.
//! @param cfg  Runtime configuration. NULL for defaults.
//! @return     Opaque context, or NULL on failure.
EXIF_API exif_t *exif_create(const exif_config_t *cfg);

//! Destroy ctx and release all resources.
//! @param ctx  Context to destroy. NULL is a no-op.
EXIF_API void exif_destroy(exif_t *ctx);

//! Read metadata from a file path.
//! Always returns structured JSON (-json -a -s -n -G1 -b).
//! @param ctx   Context from exif_create.
//! @param path  Path to the image file.
//! @param opts  Extra CLI args, config, transform. NULL for defaults.
EXIF_API exif_result_t exif_read(exif_t *ctx, const char *path,
                                 const exif_options_t *opts);

//! Read metadata from an in-memory buffer. Spills to a temp file internally.
//! Always returns structured JSON (-json -a -s -n -G1 -b).
//! @param ctx    Context from exif_create.
//! @param input  Source data; filename extension determines format handling.
//! @param opts   Extra CLI args, config, transform. NULL for defaults.
EXIF_API exif_result_t exif_read_buf(exif_t *ctx, exif_buf_t input,
                                     const exif_options_t *opts);

//! Write tags to a file.
//! @param ctx       Context from exif_create.
//! @param in_path   Source image path.
//! @param out_path  Destination path. NULL to overwrite in_path.
//! @param opts      Must contain tags to write. args and config_path optional.
EXIF_API exif_result_t exif_write(exif_t *ctx, const char *in_path,
                                  const char *out_path,
                                  const exif_options_t *opts);

//! Write tags to an in-memory buffer.
//! @param ctx    Context from exif_create.
//! @param input  Source file data; filename extension determines format.
//! @param opts   Must contain tags to write. args and config_path optional.
//! @return       Result data holds the modified file bytes.
EXIF_API exif_result_t exif_write_buf(exif_t *ctx, exif_buf_t input,
                                      const exif_options_t *opts);

//! Free data and error strings in a result.
//! @param ctx  Context whose allocator owns the strings. NULL falls back to free().
//! @param r    Result to free. NULL is a no-op.
EXIF_API void exif_result_free(exif_t *ctx, exif_result_t *r);

#ifdef __cplusplus
}
#endif

#endif // LIBEXIF_H
