// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

#include "libexif.h"
#include <stdio.h>
#include <time.h>

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char *argv[])
{
    if (argc < 2) { fprintf(stderr, "Usage: %s <image>\n", argv[0]); return 1; }

    double t0 = now_ms();
    exif_t *exif = exif_create(NULL);
    double t_create = now_ms() - t0;
    if (!exif) { fprintf(stderr, "create failed\n"); return 1; }

    printf("create: %.1f ms\n\n", t_create);

    double t1 = now_ms();
    exif_result_t read_result = exif_read(exif, argv[1], NULL);
    printf("read:  %.1f ms\n", now_ms() - t1);
    if (read_result.success) printf("%.*s\n", (int)read_result.data_len, read_result.data);
    else printf("ERROR: %s\n", read_result.error);
    exif_result_free(exif, &read_result);

    printf("\n");
    const char *write_tags[] = { "-Comment=libexif test", "-Artist=bench" };
    exif_options_t write_opts = { .tags = write_tags, .ntags = 2 };

    double t2 = now_ms();
    exif_result_t write_result = exif_write(exif, argv[1], "/tmp/libexif_bench_out.png", &write_opts);
    printf("write:  %.1f ms\n", now_ms() - t2);
    if (!write_result.success) printf("ERROR: %s\n", write_result.error);
    exif_result_free(exif, &write_result);

    double t3 = now_ms();
    exif_result_t readback_result = exif_read(exif, "/tmp/libexif_bench_out.png", NULL);
    printf("\nread-back:  %.1f ms\n", now_ms() - t3);
    if (readback_result.success) printf("%.*s\n", (int)readback_result.data_len, readback_result.data);
    else printf("ERROR: %s\n", readback_result.error);
    exif_result_free(exif, &readback_result);

    exif_destroy(exif);
}
