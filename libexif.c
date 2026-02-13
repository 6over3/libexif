// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

#include "libexif.h"
#include "wasm_export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const unsigned char zeroperl_aot[] = {
#embed "resources/zeroperl.aot"
};

static const unsigned char exiftool_script[] = {
#embed "resources/exiftool"
};

#define DEFAULT_STACK  (8u << 20)
#define DEFAULT_HEAP   (32u << 20)

static void *exif__default_alloc(size_t size, void *ctx)
{
    (void)ctx;
    return malloc(size);
}

static void exif__default_free(void *ptr, size_t size, void *ctx)
{
    (void)size; (void)ctx;
    free(ptr);
}

static exif_allocator_t exif__default_allocator = {
    exif__default_alloc, exif__default_free, NULL
};

struct exif {
    exif_allocator_t     alloc;
    wasm_module_t        module;
    wasm_module_inst_t   inst;
    wasm_exec_env_t      env;
    wasm_function_inst_t fn_reset;
    wasm_function_inst_t fn_run_file;
    wasm_function_inst_t fn_flush;
    wasm_function_inst_t fn_last_error;
    wasm_function_inst_t fn_free_interp;
    uint8_t             *wasm_buf;
    int                  stdout_fd;
    char                *script_path;
    char                 errbuf[512];
};

static uint64_t exif__wasm_alloc_string(exif_t *ctx, const char *str)
{
    size_t len = strlen(str) + 1;
    void *native = NULL;
    uint64_t offset = wasm_runtime_module_malloc(ctx->inst, len, &native);
    if (!offset) return 0;
    memcpy(native, str, len);
    return offset;
}

static const char *exif__wasm_read_cstring(exif_t *ctx, uint32_t offset)
{
    if (!offset) return NULL;
    return wasm_runtime_addr_app_to_native(ctx->inst, (uint64_t)offset);
}

static bool exif__call_wasm(exif_t *ctx, wasm_function_inst_t func, int32_t *out)
{
    wasm_val_t result = { .kind = WASM_I32 };
    uint32_t nresults = wasm_func_get_result_count(func, ctx->inst);
    if (!wasm_runtime_call_wasm_a(ctx->env, func, nresults,
                                   nresults ? &result : NULL, 0, NULL)) {
        snprintf(ctx->errbuf, sizeof ctx->errbuf, "%s",
                 wasm_runtime_get_exception(ctx->inst));
        wasm_runtime_clear_exception(ctx->inst);
        return false;
    }
    if (out && nresults) *out = result.of.i32;
    return true;
}

static int32_t exif__call_host_stub(wasm_exec_env_t env, int32_t fn_id,
                              int32_t argv_off, int32_t argc)
{
    (void)env; (void)fn_id; (void)argv_off; (void)argc;
    return 0;
}

static NativeSymbol exif__native_syms[] = {
    { "call_host_function", (void *)exif__call_host_stub, "(iii)i", NULL },
};

static exif_result_t exif__err_result(exif_allocator_t *alloc, const char *msg, int32_t code)
{
    size_t len = strlen(msg) + 1;
    char *error = alloc->alloc(len, alloc->ctx);
    if (error) memcpy(error, msg, len);
    return (exif_result_t){ .error = error, .exit_code = code };
}

static exif_result_t exif__ok_result(char *data, size_t len, int32_t code)
{
    return (exif_result_t){
        .success = true, .data = data, .data_len = len, .exit_code = code
    };
}

static char *exif__read_stdout(exif_t *ctx, size_t *out_len)
{
    exif_allocator_t *a = &ctx->alloc;
    off_t size = lseek(ctx->stdout_fd, 0, SEEK_END);
    if (size <= 0) { *out_len = 0; return NULL; }
    lseek(ctx->stdout_fd, 0, SEEK_SET);
    char *buf = a->alloc(size + 1, a->ctx);
    if (!buf) { *out_len = 0; return NULL; }
    ssize_t n = read(ctx->stdout_fd, buf, size);
    *out_len = n > 0 ? (size_t)n : 0;
    buf[*out_len] = '\0';
    return buf;
}

static const char *exif__suffix_of(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    return dot ? dot + 1 : NULL;
}

static char *exif__write_tmpfile(exif_allocator_t *alloc, const void *data,
                           size_t len, const char *suffix)
{
    char path_buf[256];
    snprintf(path_buf, sizeof path_buf, "/tmp/libexif_XXXXXX%s%s",
             suffix ? "." : "", suffix ? suffix : "");

    int fd = mkstemps(path_buf, suffix ? (int)strlen(suffix) + 1 : 0);
    if (fd < 0) return NULL;

    const unsigned char *src = data;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t written = write(fd, src, remaining);
        if (written < 0) { close(fd); unlink(path_buf); return NULL; }
        src += written;
        remaining -= written;
    }
    close(fd);

    size_t path_len = strlen(path_buf) + 1;
    char *path = alloc->alloc(path_len, alloc->ctx);
    if (path) memcpy(path, path_buf, path_len);
    return path;
}

static char *exif__read_file(exif_allocator_t *alloc, const char *path, size_t *out_len)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buf = alloc->alloc(size + 1, alloc->ctx);
    if (buf) {
        *out_len = fread(buf, 1, size, file);
        buf[*out_len] = '\0';
    }
    fclose(file);
    return buf;
}

static exif_result_t exif__run(exif_t *ctx, const char **tail, int ntail,
                               const exif_options_t *opts)
{
    exif_allocator_t *alloc = &ctx->alloc;
    exif_result_t result = {0};
    uint64_t argv_off = 0, script_off = 0;
    int nargs = 0;

    int nopt_args    = opts ? opts->argc : 0;
    int nconfig_args = (opts && opts->config_path) ? 2 : 0;
    int ntag_args    = opts ? opts->ntags : 0;
    int total        = nopt_args + nconfig_args + ntag_args + ntail;

    uint64_t wasm_ptrs[total];
    memset(wasm_ptrs, 0, sizeof wasm_ptrs);

    bool thread_env_owned = false;
    if (!wasm_runtime_thread_env_inited()) {
        if (!wasm_runtime_init_thread_env())
            return exif__err_result(alloc, "failed to init WAMR thread env", -1);
        thread_env_owned = true;
    }

    int32_t rc;
    if (!exif__call_wasm(ctx, ctx->fn_reset, &rc) || rc != 0) {
        result = exif__err_result(alloc, "zeroperl_reset failed", rc);
        goto cleanup;
    }

    for (int i = 0; i < nopt_args; i++) {
        wasm_ptrs[nargs] = exif__wasm_alloc_string(ctx, opts->args[i]);
        if (!wasm_ptrs[nargs]) goto oom;
        nargs++;
    }

    if (nconfig_args) {
        wasm_ptrs[nargs] = exif__wasm_alloc_string(ctx, "-config");
        if (!wasm_ptrs[nargs]) goto oom;
        nargs++;
        wasm_ptrs[nargs] = exif__wasm_alloc_string(ctx, opts->config_path);
        if (!wasm_ptrs[nargs]) goto oom;
        nargs++;
    }

    for (int i = 0; i < ntag_args; i++) {
        wasm_ptrs[nargs] = exif__wasm_alloc_string(ctx, opts->tags[i]);
        if (!wasm_ptrs[nargs]) goto oom;
        nargs++;
    }

    for (int i = 0; i < ntail; i++) {
        wasm_ptrs[nargs] = exif__wasm_alloc_string(ctx, tail[i]);
        if (!wasm_ptrs[nargs]) goto oom;
        nargs++;
    }

    void *argv_native = NULL;
    argv_off = wasm_runtime_module_malloc(ctx->inst,
                                          nargs * sizeof(int32_t),
                                          &argv_native);
    if (!argv_off) goto oom;
    for (int i = 0; i < nargs; i++)
        ((int32_t *)argv_native)[i] = (int32_t)wasm_ptrs[i];

    script_off = exif__wasm_alloc_string(ctx, ctx->script_path);
    if (!script_off) goto oom;

    ftruncate(ctx->stdout_fd, 0);
    lseek(ctx->stdout_fd, 0, SEEK_SET);

    wasm_val_t call_args[3] = {
        { .kind = WASM_I32, .of.i32 = (int32_t)script_off },
        { .kind = WASM_I32, .of.i32 = nargs },
        { .kind = WASM_I32, .of.i32 = (int32_t)argv_off },
    };
    wasm_val_t call_ret = { .kind = WASM_I32 };
    int32_t exit_code = -1;
    const char *wasm_error = NULL;

    if (wasm_runtime_call_wasm_a(ctx->env, ctx->fn_run_file,
                                  1, &call_ret, 3, call_args)) {
        exit_code = call_ret.of.i32;
    } else {
        const char *exc = wasm_runtime_get_exception(ctx->inst);
        if (exc && strstr(exc, "wasi proc exit")) {
            exit_code = (int32_t)wasm_runtime_get_wasi_exit_code(ctx->inst);
        } else {
            snprintf(ctx->errbuf, sizeof ctx->errbuf, "%s", exc ? exc : "unknown");
            wasm_error = ctx->errbuf;
        }
        wasm_runtime_clear_exception(ctx->inst);
    }

    exif__call_wasm(ctx, ctx->fn_flush, NULL);

    if (!wasm_error) {
        int32_t error_ptr = 0;
        exif__call_wasm(ctx, ctx->fn_last_error, &error_ptr);
        const char *perl_error = exif__wasm_read_cstring(ctx, error_ptr);
        if (perl_error && *perl_error)
            wasm_error = perl_error;
    }

    if (wasm_error) {
        result = exif__err_result(alloc, wasm_error, exit_code);
    } else if (exit_code != 0) {
        result = exif__err_result(alloc, "exiftool exited with error", exit_code);
    } else {
        size_t out_len;
        char *data = exif__read_stdout(ctx, &out_len);
        result = exif__ok_result(data, out_len, exit_code);
    }
    goto cleanup;

 oom:
    result = exif__err_result(alloc, "WASM memory allocation failed", -1);

cleanup:
    for (int i = 0; i < nargs; i++)
        if (wasm_ptrs[i]) wasm_runtime_module_free(ctx->inst, wasm_ptrs[i]);
    if (argv_off)   wasm_runtime_module_free(ctx->inst, argv_off);
    if (script_off) wasm_runtime_module_free(ctx->inst, script_off);
    if (thread_env_owned)
        wasm_runtime_destroy_thread_env();
    return result;
}

exif_t *exif_create(const exif_config_t *cfg)
{
    exif_allocator_t alloc = exif__default_allocator;
    if (cfg && cfg->allocator) alloc = *cfg->allocator;

    uint32_t wasm_stack = DEFAULT_STACK;
    uint32_t wasm_heap  = DEFAULT_HEAP;
    uint32_t exec_stack = DEFAULT_STACK;
    if (cfg) {
        if (cfg->wasm_stack_size) wasm_stack = cfg->wasm_stack_size;
        if (cfg->wasm_heap_size)  wasm_heap  = cfg->wasm_heap_size;
        if (cfg->exec_stack_size) exec_stack = cfg->exec_stack_size;
    }

    if (!wasm_runtime_init()) return NULL;

    if (!wasm_runtime_register_natives("env", exif__native_syms,
                                       sizeof exif__native_syms / sizeof exif__native_syms[0]))
        goto fail_runtime;

    char wamr_errbuf[256];
    // WAMR mutates the buffer during load
    uint8_t *wasm_buf = alloc.alloc(sizeof zeroperl_aot, alloc.ctx);
    if (!wasm_buf) goto fail_runtime;
    memcpy(wasm_buf, zeroperl_aot, sizeof zeroperl_aot);

    wasm_module_t module = wasm_runtime_load(wasm_buf, sizeof zeroperl_aot,
                                             wamr_errbuf, sizeof wamr_errbuf);
    if (!module) goto fail_buf;

    exif_t *ctx = alloc.alloc(sizeof *ctx, alloc.ctx);
    if (!ctx) goto fail_module;
    memset(ctx, 0, sizeof *ctx);
    ctx->alloc = alloc;
    ctx->module = module;
    ctx->wasm_buf = wasm_buf;

    ctx->script_path = exif__write_tmpfile(&alloc, exiftool_script,
                                     sizeof exiftool_script, NULL);
    if (!ctx->script_path) goto fail_ctx;

    char stdout_tmpl[] = "/tmp/libexif_stdout_XXXXXX";
    ctx->stdout_fd = mkstemp(stdout_tmpl);
    if (ctx->stdout_fd < 0) goto fail_ctx;
    unlink(stdout_tmpl);

    const char *dirs[] = { "/", "/tmp", "/dev" };
    char *wasi_argv[] = { "zeroperl" };
    wasm_runtime_set_wasi_args_ex(module, dirs, 3, NULL, 0, NULL, 0,
                                  wasi_argv, 1, -1, ctx->stdout_fd,
                                  STDERR_FILENO);

    ctx->inst = wasm_runtime_instantiate(module, wasm_stack, wasm_heap,
                                         wamr_errbuf, sizeof wamr_errbuf);
    if (!ctx->inst) goto fail_ctx;

    ctx->env = wasm_runtime_create_exec_env(ctx->inst, exec_stack);
    if (!ctx->env) goto fail_ctx;

    ctx->fn_reset       = wasm_runtime_lookup_function(ctx->inst, "zeroperl_reset");
    ctx->fn_run_file    = wasm_runtime_lookup_function(ctx->inst, "zeroperl_run_file");
    ctx->fn_flush       = wasm_runtime_lookup_function(ctx->inst, "zeroperl_flush");
    ctx->fn_last_error  = wasm_runtime_lookup_function(ctx->inst, "zeroperl_last_error");
    ctx->fn_free_interp = wasm_runtime_lookup_function(ctx->inst, "zeroperl_free_interpreter");

    wasm_function_inst_t fn_init = wasm_runtime_lookup_function(ctx->inst, "zeroperl_init");
    if (!fn_init || !ctx->fn_reset || !ctx->fn_run_file || !ctx->fn_flush)
        goto fail_ctx;

    int32_t rc;
    if (!exif__call_wasm(ctx, fn_init, &rc) || rc != 0) goto fail_ctx;

    return ctx;

fail_ctx:
    exif_destroy(ctx);
    return NULL;
fail_module:
    wasm_runtime_unload(module);
fail_buf:
    alloc.free(wasm_buf, sizeof zeroperl_aot, alloc.ctx);
fail_runtime:
    wasm_runtime_destroy();
    return NULL;
}

void exif_destroy(exif_t *ctx)
{
    if (!ctx) return;
    exif_allocator_t alloc = ctx->alloc;

    if (ctx->fn_free_interp && ctx->env)
        exif__call_wasm(ctx, ctx->fn_free_interp, NULL);

    if (ctx->env)    wasm_runtime_destroy_exec_env(ctx->env);
    if (ctx->inst)   wasm_runtime_deinstantiate(ctx->inst);
    if (ctx->module) wasm_runtime_unload(ctx->module);
    if (ctx->wasm_buf) alloc.free(ctx->wasm_buf, sizeof zeroperl_aot, alloc.ctx);
    wasm_runtime_destroy();

    if (ctx->stdout_fd > 0) close(ctx->stdout_fd);

    if (ctx->script_path) {
        unlink(ctx->script_path);
        alloc.free(ctx->script_path, 0, alloc.ctx);
    }

    alloc.free(ctx, sizeof *ctx, alloc.ctx);
}

static const char *exif__read_defaults[] = { "-json", "-a", "-s", "-n", "-G1", "-b" };
#define EXIF__N_READ_DEFAULTS (int)(sizeof exif__read_defaults / sizeof exif__read_defaults[0])

static void exif__apply_transform(exif_allocator_t *alloc, exif_result_t *result,
                            const exif_options_t *opts)
{
    if (!result->success || !opts || !opts->transform)
        return;
    char *transformed = opts->transform(result->data, result->data_len,
                                        opts->transform_ctx);
    alloc->free(result->data, 0, alloc->ctx);
    result->data = transformed;
    result->data_len = transformed ? strlen(transformed) : 0;
}

exif_result_t exif_read(exif_t *ctx, const char *path,
                        const exif_options_t *opts)
{
    const char *tail[EXIF__N_READ_DEFAULTS + 1];
    for (int i = 0; i < EXIF__N_READ_DEFAULTS; i++)
        tail[i] = exif__read_defaults[i];
    tail[EXIF__N_READ_DEFAULTS] = path;

    exif_result_t result = exif__run(ctx, tail, EXIF__N_READ_DEFAULTS + 1, opts);
    exif__apply_transform(&ctx->alloc, &result, opts);
    return result;
}

exif_result_t exif_read_buf(exif_t *ctx, exif_buf_t input,
                            const exif_options_t *opts)
{
    exif_allocator_t *alloc = &ctx->alloc;

    char *tmp_path = exif__write_tmpfile(alloc, input.data, input.len,
                                   exif__suffix_of(input.filename));
    if (!tmp_path)
        return exif__err_result(alloc, "failed to write temp file", -1);

    const char *tail[EXIF__N_READ_DEFAULTS + 1];
    for (int i = 0; i < EXIF__N_READ_DEFAULTS; i++)
        tail[i] = exif__read_defaults[i];
    tail[EXIF__N_READ_DEFAULTS] = tmp_path;

    exif_result_t result = exif__run(ctx, tail, EXIF__N_READ_DEFAULTS + 1, opts);
    exif__apply_transform(alloc, &result, opts);

    unlink(tmp_path);
    alloc->free(tmp_path, 0, alloc->ctx);
    return result;
}

exif_result_t exif_write(exif_t *ctx, const char *in_path,
                         const char *out_path, const exif_options_t *opts)
{
    if (out_path) {
        const char *tail[] = { "-o", out_path, in_path };
        return exif__run(ctx, tail, 3, opts);
    }
    const char *tail[] = { "-overwrite_original", in_path };
    return exif__run(ctx, tail, 2, opts);
}

exif_result_t exif_write_buf(exif_t *ctx, exif_buf_t input,
                             const exif_options_t *opts)
{
    exif_allocator_t *alloc = &ctx->alloc;
    exif_result_t result;
    const char *suffix = exif__suffix_of(input.filename);

    char *in_path = exif__write_tmpfile(alloc, input.data, input.len, suffix);
    if (!in_path)
        return exif__err_result(alloc, "failed to write input temp file", -1);

    char out_path[256];
    snprintf(out_path, sizeof out_path, "/tmp/libexif_out_XXXXXX%s%s",
             suffix ? "." : "", suffix ? suffix : "");
    int out_fd = mkstemps(out_path, suffix ? (int)strlen(suffix) + 1 : 0);
    if (out_fd < 0) {
        result = exif__err_result(alloc, "failed to create output temp", -1);
        goto cleanup;
    }
    close(out_fd);
    unlink(out_path);

    const char *tail[] = { "-o", out_path, in_path };
    result = exif__run(ctx, tail, 3, opts);

    if (result.success) {
        alloc->free(result.data, 0, alloc->ctx);
        result.data = exif__read_file(alloc, out_path, &result.data_len);
        if (!result.data)
            result = exif__err_result(alloc, "output file not produced", -1);
    }

    unlink(out_path);

cleanup:
    unlink(in_path);
    alloc->free(in_path, 0, alloc->ctx);
    return result;
}

void exif_result_free(exif_t *ctx, exif_result_t *result)
{
    if (!result) return;
    exif_allocator_t *alloc = ctx ? &ctx->alloc : &exif__default_allocator;
    if (result->data)  alloc->free(result->data, 0, alloc->ctx);
    if (result->error) alloc->free(result->error, 0, alloc->ctx);
    result->data = NULL;
    result->error = NULL;
}
