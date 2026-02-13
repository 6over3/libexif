// Copyright (c) 6OVER3 Institute. All rights reserved.
// SPDX-License-Identifier: AGPL-3.0-only

#include "libexif.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image-file>\n", argv[0]);
        return 1;
    }

    exif_t *exif = exif_create(NULL);
    if (!exif) {
        fprintf(stderr, "Failed to initialize exiftool\n");
        return 1;
    }

    exif_result_t result = exif_read(exif, argv[1], NULL);
    if (result.success)
        printf("%.*s\n", (int)result.data_len, result.data);
    else
        fprintf(stderr, "Error: %s (exit %d)\n", result.error, result.exit_code);

    exif_result_free(exif, &result);
    exif_destroy(exif);
    return result.success ? 0 : 1;
}
