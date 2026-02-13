// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

#include "libexif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined at compile time"
#endif

#define TEST_DATA SOURCE_DIR "/data/"

static int tests_run, tests_failed;
static int test_ok;

#define RUN(fn) do { \
    printf("  %-50s", #fn); \
    fflush(stdout); \
    test_ok = 1; \
    fn(exif); \
    if (test_ok) printf(" OK\n"); \
    tests_run++; \
} while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf(" FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, msg); \
        tests_failed++; \
        test_ok = 0; \
        return; \
    } \
} while (0)

#define ASSERT_SUCCESS(r) \
    ASSERT((r).success, (r).error ? (r).error : "unknown error")

// --- helpers ---

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *len = ftell(f);
    rewind(f);
    char *buf = malloc(*len);
    fread(buf, 1, *len, f);
    fclose(f);
    return buf;
}

// Handles both "Key" and "Group:Key" from -G1 output
static int json_has_key(const char *json, const char *key)
{
    char exact[256], prefixed[256];
    snprintf(exact, sizeof exact, "\"%s\"", key);
    if (strstr(json, exact)) return 1;
    snprintf(prefixed, sizeof prefixed, ":%s\"", key);
    return strstr(json, prefixed) != NULL;
}

// Extract string value for key, handling optional group prefix
static int json_string_value(const char *json, const char *key, char *out, size_t out_sz)
{
    char pat[256];
    const char *p;

    snprintf(pat, sizeof pat, "\"%s\"", key);
    p = strstr(json, pat);
    if (!p) {
        snprintf(pat, sizeof pat, ":%s\"", key);
        p = strstr(json, pat);
    }
    if (!p) return 0;

    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_sz - 1) {
        if (*p == '\\' && *(p + 1)) { p++; }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 1;
}

// --- read tests ---

static void test_read_jpeg(exif_t *exif)
{
    exif_result_t r = exif_read(exif, TEST_DATA "test.jpg", NULL);
    ASSERT_SUCCESS(r);
    ASSERT(r.data_len > 0, "empty output");
    ASSERT(json_has_key(r.data, "FileName"), "missing FileName");
    exif_result_free(exif, &r);
}

static void test_read_png(exif_t *exif)
{
    exif_result_t r = exif_read(exif, TEST_DATA "test.png", NULL);
    ASSERT_SUCCESS(r);
    ASSERT(json_has_key(r.data, "ImageWidth"), "missing ImageWidth");
    exif_result_free(exif, &r);
}

static void test_read_tiff(exif_t *exif)
{
    exif_result_t r = exif_read(exif, TEST_DATA "test.tiff", NULL);
    ASSERT_SUCCESS(r);
    ASSERT(json_has_key(r.data, "FileType"), "missing FileType");
    exif_result_free(exif, &r);
}

static void test_read_exr(exif_t *exif)
{
    exif_result_t r = exif_read(exif, TEST_DATA "test.exr", NULL);
    ASSERT_SUCCESS(r);
    ASSERT(json_has_key(r.data, "ImageWidth"), "missing ImageWidth");
    exif_result_free(exif, &r);
}

static void test_read_dng(exif_t *exif)
{
    exif_result_t r = exif_read(exif, TEST_DATA "Mo_Edge20_ColourfulStreet.dng", NULL);
    ASSERT_SUCCESS(r);
    ASSERT(json_has_key(r.data, "Make"), "missing Make");
    ASSERT(json_has_key(r.data, "Model"), "missing Model");
    exif_result_free(exif, &r);
}

// --- buffer read tests ---

static void test_read_buf_jpeg(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    exif_buf_t buf = { .data = data, .len = len, .filename = "test.jpg" };
    exif_result_t r = exif_read_buf(exif, buf, NULL);
    ASSERT_SUCCESS(r);
    ASSERT(json_has_key(r.data, "FileName"), "missing FileName");
    exif_result_free(exif, &r);
    free(data);
}

static void test_read_buf_dng(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "Mo_Edge20_ColourfulStreet.dng", &len);
    ASSERT(data, "failed to read DNG");

    exif_buf_t buf = { .data = data, .len = len, .filename = "photo.dng" };
    exif_result_t r = exif_read_buf(exif, buf, NULL);
    ASSERT_SUCCESS(r);
    ASSERT(json_has_key(r.data, "Make"), "missing Make");
    exif_result_free(exif, &r);
    free(data);
}

// --- write tests ---

static void test_write_roundtrip(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    const char *tags[] = { "-Artist=libexif test", "-Comment=hello" };
    exif_options_t wopts = { .tags = tags, .ntags = 1 };
    exif_buf_t in = { .data = data, .len = len, .filename = "test.jpg" };

    exif_result_t wr = exif_write_buf(exif, in, &wopts);
    ASSERT_SUCCESS(wr);
    ASSERT(wr.data_len > 0, "empty write output");

    exif_buf_t modified = { .data = wr.data, .len = wr.data_len, .filename = "out.jpg" };
    exif_result_t rr = exif_read_buf(exif, modified, NULL);
    ASSERT_SUCCESS(rr);

    char val[256];
    ASSERT(json_string_value(rr.data, "Artist", val, sizeof val), "missing Artist");
    ASSERT(strcmp(val, "libexif test") == 0, "Artist mismatch");
    exif_result_free(exif, &rr);
    exif_result_free(exif, &wr);
    free(data);
}

static void test_write_buf_roundtrip(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    const char *tags[] = { "-Artist=buf_test" };
    exif_options_t wopts = { .tags = tags, .ntags = 1 };
    exif_buf_t in = { .data = data, .len = len, .filename = "test.jpg" };

    exif_result_t wr = exif_write_buf(exif, in, &wopts);
    ASSERT_SUCCESS(wr);
    ASSERT(wr.data_len > 0, "empty write output");

    exif_buf_t modified = { .data = wr.data, .len = wr.data_len, .filename = "out.jpg" };
    exif_result_t rr = exif_read_buf(exif, modified, NULL);
    ASSERT_SUCCESS(rr);

    char val[256];
    ASSERT(json_string_value(rr.data, "Artist", val, sizeof val), "missing Artist");
    ASSERT(strcmp(val, "buf_test") == 0, "Artist mismatch");
    exif_result_free(exif, &rr);
    exif_result_free(exif, &wr);
    free(data);
}

// --- unicode tests ---

static void test_unicode_korean(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    const char *text = "\xec\x95\x88\xeb\x85\x95\xed\x95\x98\xec\x84\xb8\xec\x9a\x94"; // 안녕하세요
    const char *tags[] = { "-Artist=\xec\x95\x88\xeb\x85\x95\xed\x95\x98\xec\x84\xb8\xec\x9a\x94" };
    exif_options_t wopts = { .tags = tags, .ntags = 1 };
    exif_buf_t in = { .data = data, .len = len, .filename = "test.jpg" };

    exif_result_t wr = exif_write_buf(exif, in, &wopts);
    ASSERT_SUCCESS(wr);

    exif_buf_t modified = { .data = wr.data, .len = wr.data_len, .filename = "out.jpg" };
    exif_result_t rr = exif_read_buf(exif, modified, NULL);
    ASSERT_SUCCESS(rr);

    char val[256];
    ASSERT(json_string_value(rr.data, "Artist", val, sizeof val), "missing Artist");
    ASSERT(strcmp(val, text) == 0, "Korean text mismatch");
    ASSERT(!strstr(val, "\xef\xbf\xbd"), "contains replacement character");

    exif_result_free(exif, &rr);
    exif_result_free(exif, &wr);
    free(data);
}

static void test_unicode_japanese(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    const char *text = "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf"; // こんにちは
    const char *tags[] = { "-Artist=\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf" };
    exif_options_t wopts = { .tags = tags, .ntags = 1 };
    exif_buf_t in = { .data = data, .len = len, .filename = "test.jpg" };

    exif_result_t wr = exif_write_buf(exif, in, &wopts);
    ASSERT_SUCCESS(wr);

    exif_buf_t modified = { .data = wr.data, .len = wr.data_len, .filename = "out.jpg" };
    exif_result_t rr = exif_read_buf(exif, modified, NULL);
    ASSERT_SUCCESS(rr);

    char val[256];
    ASSERT(json_string_value(rr.data, "Artist", val, sizeof val), "missing Artist");
    ASSERT(strcmp(val, text) == 0, "Japanese text mismatch");

    exif_result_free(exif, &rr);
    exif_result_free(exif, &wr);
    free(data);
}

static void test_unicode_chinese(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    const char *text = "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c"; // 你好世界
    const char *tags[] = { "-Artist=\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c" };
    exif_options_t wopts = { .tags = tags, .ntags = 1 };
    exif_buf_t in = { .data = data, .len = len, .filename = "test.jpg" };

    exif_result_t wr = exif_write_buf(exif, in, &wopts);
    ASSERT_SUCCESS(wr);

    exif_buf_t modified = { .data = wr.data, .len = wr.data_len, .filename = "out.jpg" };
    exif_result_t rr = exif_read_buf(exif, modified, NULL);
    ASSERT_SUCCESS(rr);

    char val[256];
    ASSERT(json_string_value(rr.data, "Artist", val, sizeof val), "missing Artist");
    ASSERT(strcmp(val, text) == 0, "Chinese text mismatch");

    exif_result_free(exif, &rr);
    exif_result_free(exif, &wr);
    free(data);
}

static void test_unicode_mixed(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    // "Hello 안녕 こんにちは 你好"
    const char *mixed = "Hello "
        "\xec\x95\x88\xeb\x85\x95 "
        "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf "
        "\xe4\xbd\xa0\xe5\xa5\xbd";

    char tag[512];
    snprintf(tag, sizeof tag, "-ImageDescription=%s", mixed);
    const char *tags[] = { tag };
    exif_options_t wopts = { .tags = tags, .ntags = 1 };
    exif_buf_t in = { .data = data, .len = len, .filename = "test.jpg" };

    exif_result_t wr = exif_write_buf(exif, in, &wopts);
    ASSERT_SUCCESS(wr);

    exif_buf_t modified = { .data = wr.data, .len = wr.data_len, .filename = "out.jpg" };
    exif_result_t rr = exif_read_buf(exif, modified, NULL);
    ASSERT_SUCCESS(rr);

    char val[512];
    ASSERT(json_string_value(rr.data, "ImageDescription", val, sizeof val),
           "missing ImageDescription");
    ASSERT(strcmp(val, mixed) == 0, "mixed unicode mismatch");

    exif_result_free(exif, &rr);
    exif_result_free(exif, &wr);
    free(data);
}

// --- transform tests ---

static char *uppercase_transform(const char *data, size_t len, void *ctx)
{
    (void)ctx;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++)
        out[i] = (data[i] >= 'a' && data[i] <= 'z') ? data[i] - 32 : data[i];
    out[len] = '\0';
    return out;
}

static void test_read_transform(exif_t *exif)
{
    exif_options_t opts = { .transform = uppercase_transform };

    exif_result_t r = exif_read(exif, TEST_DATA "test.jpg", &opts);
    ASSERT_SUCCESS(r);
    ASSERT(r.data_len > 0, "empty output");
    ASSERT(strstr(r.data, "FILENAME"), "transform not applied");
    exif_result_free(exif, &r);
}

static void test_read_buf_transform(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    exif_options_t opts = { .transform = uppercase_transform };
    exif_buf_t buf = { .data = data, .len = len, .filename = "test.jpg" };

    exif_result_t r = exif_read_buf(exif, buf, &opts);
    ASSERT_SUCCESS(r);
    ASSERT(strstr(r.data, "FILENAME"), "transform not applied to buf read");
    exif_result_free(exif, &r);
    free(data);
}

// --- edge cases ---

static void test_multiple_reads(exif_t *exif)
{
    size_t len;
    char *data = read_file(TEST_DATA "test.jpg", &len);
    ASSERT(data, "failed to read test.jpg");

    exif_buf_t buf = { .data = data, .len = len, .filename = "test.jpg" };
    for (int i = 0; i < 5; i++) {
        exif_result_t r = exif_read_buf(exif, buf, NULL);
        ASSERT_SUCCESS(r);
        ASSERT(r.data_len > 0, "empty output on repeated read");
        exif_result_free(exif, &r);
    }
    free(data);
}

static void test_read_nonexistent(exif_t *exif)
{
    exif_result_t r = exif_read(exif, "/tmp/does_not_exist_12345.jpg", NULL);
    exif_result_free(exif, &r);
}

// --- main ---

int main(void)
{
    printf("Creating exif context...\n");
    exif_t *exif = exif_create(NULL);
    if (!exif) {
        fprintf(stderr, "exif_create failed\n");
        return 1;
    }

    printf("\nRead tests:\n");
    RUN(test_read_jpeg);
    RUN(test_read_png);
    RUN(test_read_tiff);
    RUN(test_read_exr);
    RUN(test_read_dng);

    printf("\nBuffer read tests:\n");
    RUN(test_read_buf_jpeg);
    RUN(test_read_buf_dng);

    printf("\nWrite tests:\n");
    RUN(test_write_roundtrip);
    RUN(test_write_buf_roundtrip);

    printf("\nUnicode tests:\n");
    RUN(test_unicode_korean);
    RUN(test_unicode_japanese);
    RUN(test_unicode_chinese);
    RUN(test_unicode_mixed);

    printf("\nTransform tests:\n");
    RUN(test_read_transform);
    RUN(test_read_buf_transform);

    printf("\nEdge cases:\n");
    RUN(test_multiple_reads);
    RUN(test_read_nonexistent);

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);

    exif_destroy(exif);
    return tests_failed ? 1 : 0;
}
