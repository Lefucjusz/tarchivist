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
#include <string.h>

const char *const tar_to_write = "example-write.tar";
const char *const dir_to_write = "example_directory";

const char *const first_file = "file_1.txt";
const char *const first_file_content = "Some text to be written to the first file.";

const char *const second_file = "file_2.txt";
const char *const second_file_content = "This text will be written to the second file.";

void prepare_dir_header(tarchivist_header_t *header, const char* name);
void prepare_file_header(tarchivist_header_t *header, const char* name, size_t size);

int main(void) {
    tarchivist_t tar;
    tarchivist_header_t header;
    char *compound_path = NULL;
    int err;

    printf("write_demo - demonstration of tarchivist library writing functionalities\n");
    printf("(c) Lefucjusz 2022\n\n");

    do
    {
        printf("Opening the archive %s...\n", tar_to_write);
        err = tarchivist_open(&tar, tar_to_write, "w");
        if (err != TARCHIVIST_SUCCESS) {
            printf("Error: failed to open %s, error: %s!\n", tar_to_write, tarchivist_strerror(err));
            break;
        }

        /* Adding file to root directory */
        printf("Writing %s to %s...\n", first_file, tar_to_write);
        prepare_file_header(&header, first_file, strlen(first_file_content));
        err = tarchivist_write_header(&tar, &header);
        if (err != TARCHIVIST_SUCCESS) {
            printf("Error: failed to write file header, error: %s\n", tarchivist_strerror(err));
            break;
        }
        err = tarchivist_write_data(&tar, strlen(first_file_content), first_file_content);
        if (err <= 0) {
            printf("Error: failed to write file data, error: %s\n", tarchivist_strerror(err));
            break;
        }

        /* Adding directory */
        printf("Writing %s to %s...\n", dir_to_write, tar_to_write);
        prepare_dir_header(&header, dir_to_write);
        err = tarchivist_write_header(&tar, &header);
        if (err != TARCHIVIST_SUCCESS) {
            printf("Error: failed to write directory header, error: %s\n", tarchivist_strerror(err));
            break;
        }

        /* Adding file to the directory */
        const size_t path_length = strlen(dir_to_write) + strlen(second_file) + 2;
        compound_path = calloc(1, path_length);
        if (compound_path == NULL) {
            printf("Error: failed to allocate %zuB for path buffer!\n", path_length);
            break;
        }
        sprintf(compound_path, "%s/%s", dir_to_write, second_file);

        printf("Writing %s to %s...\n", compound_path, tar_to_write);
        prepare_file_header(&header, compound_path, strlen(second_file_content));
        err = tarchivist_write_header(&tar, &header);
        if (err != TARCHIVIST_SUCCESS) {
            printf("Error: failed to write file header, error: %s\n", tarchivist_strerror(err));
            break;
        }
        err = tarchivist_write_data(&tar, strlen(second_file_content), second_file_content);
        if (err <= 0) {
            printf("Error: failed to write file data, error: %s\n", tarchivist_strerror(err));
            break;
        }

        printf("Closing the archive %s...\n", tar_to_write);
        err = tarchivist_close(&tar);
        if (err != TARCHIVIST_SUCCESS) {
            printf("Error: failed to close the archive, error: %s!\n", tarchivist_strerror(err));
            break;
        }
        printf("Done!\n");

    } while (0);

    free(compound_path);
    return 0;
}

void prepare_dir_header(tarchivist_header_t *header, const char* name) {
    time_t timestamp;
    time(&timestamp);

    memset(header, 0, sizeof(tarchivist_header_t));
    
    strcpy(header->name, name);
    header->mode = 0755;
    header->mtime = timestamp;
    header->typeflag = TARCHIVIST_DIR;
    strcpy(header->uname, "Lefucjusz");
    strcpy(header->gname, "Lefucjusz");
}

void prepare_file_header(tarchivist_header_t *header, const char* name, size_t size) {
    time_t timestamp;
    time(&timestamp);

    memset(header, 0, sizeof(tarchivist_header_t));
    
    strcpy(header->name, name);
    header->mode = 0644;
    header->size = size;
    header->mtime = timestamp;
    header->typeflag = TARCHIVIST_FILE;
    strcpy(header->uname, "Lefucjusz");
    strcpy(header->gname, "Lefucjusz");
}
