
/*
 * Copyright (c) 2022 Lefucjusz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <getopt.h>
#include "packer.h"

#define PATH_ERROR 1

enum {
    PACK,
    UNPACK,
    UNKNOWN
};

int main(int argc, char **argv) {
    int opt, err = PACKER_SUCCESS;
    int mode = UNKNOWN;
    const char *src_path = NULL;
    const char *dst_path = NULL;

    printf("packer-custom-stream - simple tar-like utility demonstrating custom stream feature\n");
    printf("(c) Lefucjusz 2022\n\n");

    while ((opt = getopt(argc, argv, "pus:d:")) != -1) {
        switch (opt) {
            case 'p':
                mode = PACK;
                break;
            case 'u':
                mode = UNPACK;
                break;
            case 's':
                src_path = optarg;
                break;
            case 'd':
                dst_path = optarg;
                break;
        }
    }

    do
    {
        if (src_path == NULL) {
            printf("Error: no source path specified\n");
            err = PATH_ERROR;
            break;
        }
        if (dst_path == NULL) {
            printf("Error: no destination path specified\n");
            err = PATH_ERROR;
            break;
        }

        switch (mode) {
            case PACK:
                printf("Packing has started...\n");
                err = packer_pack(dst_path, src_path);
                break;
            case UNPACK:
                printf("Unpacking has started...\n");
                err = packer_unpack(dst_path, src_path);
                break;
            default:
                printf("Error: no mode option switch provided\n");
                break;
        }

        if (err != PACKER_SUCCESS) {
            printf("Error in packer: %d\n", err);
            break;
        }

    } while (0);

    return err;
}
