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

#include "../../tarchivist.h"
#include <stdio.h>
#include <stdlib.h>

const char *const tar_to_read = "example-read.tar";
const char *const file_to_read = "file_1.txt";

int main(void) {
    tarchivist_t tar;
    tarchivist_header_t header;
    char *file_content = NULL;
    int err;

    printf("read_demo - demonstration of tarchivist library reading functionalities\n");
    printf("(c) Lefucjusz 2022\n\n");

    do
    {
        printf("Opening the archive %s...\n", tar_to_read);
        err = tarchivist_open(&tar, tar_to_read, "r");
        if (err != TARCHIVIST_SUCCESS) {
            printf("Error: failed to open %s, error code %d!\n", tar_to_read, err);
            break;
        }

        printf("Listing the files present in the archive...\n\n");
        printf("|  name  | size |  timestamp  |  type  |  user name  |  group name  |\n\n");
        while (tarchivist_read_header(&tar, &header) == TARCHIVIST_SUCCESS) {
            printf("| %s | %uB | %u | %c | %s | %s |\n", header.name, header.size, header.mtime, header.typeflag, header.uname, header.gname);
            tarchivist_next(&tar);
        }

        printf("\nSearching file %s and printing its content...\n\n", file_to_read);
        err = tarchivist_find(&tar, file_to_read, &header);
        if (err != TARCHIVIST_SUCCESS) {
            printf("Error: failed to find %s, error code %d!\n", file_to_read, err);
            break;
        }

        file_content = calloc(1, header.size + 1);
        if (file_content == NULL) {
            printf("Error: failed to allocate %zuB for file content buffer!\n", header.size + 1);
            break;
        }

	err = tarchivist_read_data(&tar, header.size + 1, file_content);
	if (err <= 0) {
            printf("Error: failed to read data from %s, error code %d!\n", file_to_read, err);
            break;
        }
        printf("Content: %s\n", file_content);

        printf("Closing the archive %s...\n", tar_to_read);
        err = tarchivist_close(&tar);
        if (err != TARCHIVIST_SUCCESS) {
            printf("Error: failed to close the archive, error code %d!\n", err);
            break;
        }
        printf("Done!\n");

    } while (0);

    free(file_content);
    return 0;
}
