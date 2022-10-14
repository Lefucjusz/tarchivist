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

#include "packer.h"
#include "../../tarchivist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ftw.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define FTW_MAX_DIRS_OPENED 10
#define STREAM_BUFFER_SIZE (1024 * 1024) // 1MiB

typedef struct tar_ctx_t {
    char *buffer;
    size_t buffer_size;
    tarchivist_t tar;
} tar_ctx_t;

static tar_ctx_t ctx;

static void packer_remove_duplicated_slashes(char *path) {
    if (path == NULL) {
        return;
    }

    for (size_t i = 0; i < strlen(path) - 1; i++) {
        if (path[i] == '/' && path[i + 1] == '/') {
            memmove(path + i, path + i + 1, strlen(path + i + 1) + 1);
        }
    }
}

static void packer_remove_trailing_slash(char *path) {
    if (path == NULL || strlen(path) < 2) {
	    return;
    }

    const size_t end = strlen(path) - 1;
    if (path[end] == '/') {
        path[end] = '\0';
    }
}

static void packer_path_cleanup(char *path) {
    if (path == NULL || strlen(path) < 2) {
        return;
    }

    packer_remove_duplicated_slashes(path);

    /* Remove CWD */
    if (strncmp(path, "./", 2) == 0) {
        memmove(path, path + 2, strlen(path + 2) + 1);
    }

    /* Remove leading slash */
    if (path[0] == '/') {
        memmove(path, path + 1, strlen(path + 1) + 1);
    }

    packer_remove_trailing_slash(path);
}

static int packer_recursive_mkdir(char *path, mode_t mode) {
    int err = 0;

    /* Try to create directory */
    err = mkdir(path, mode);

    /* If unsuccessful, recursively try to create parent directory */
    if (err != 0) {
        char *bottom_dir = strrchr(path, '/');
        if (bottom_dir != NULL) {
            bottom_dir[0] = '\0';
            err = packer_recursive_mkdir(path, mode);
            bottom_dir[0] = '/';
            err = mkdir(path, mode);
       }
    }

    return err;
}

static int packer_pack_file(const struct stat *statbuf, const char *path) {
    tarchivist_header_t header = {0};

    int src_file = open(path, O_RDONLY);
    if (src_file <= 0) {
        return PACKER_OPENFAIL;
    }

    const size_t path_length = strlen(path) + 1;
    char *path_cleaned = calloc(1, path_length);
    if (path_cleaned == NULL) {
        printf("Failed to allocate %zuB for path buffer\n", path_length);
        return PACKER_NOMEMORY;
    }
    snprintf(path_cleaned, path_length, "%s", path);
    packer_path_cleanup(path_cleaned);

    time_t timestamp;
    time(&timestamp);

    snprintf(header.name, sizeof(header.name), "%s", path_cleaned);
    header.mode = 0644;
    header.uid = 1000;
    header.gid = 1000;
    header.size = statbuf->st_size;
    header.mtime = timestamp;
    header.typeflag = TARCHIVIST_FILE;
    snprintf(header.uname, sizeof(header.uname), "Lefucjusz");
    snprintf(header.gname, sizeof(header.gname), "Lefucjusz");

    printf("Appending file %s to %s (%zu.%03zuKiB)\n", path, path_cleaned, header.size / 1024, header.size % 1024);
    free(path_cleaned);

    int err = tarchivist_write_header(&ctx.tar, &header);
    if (err != TARCHIVIST_SUCCESS) {
        return PACKER_LIBERROR;
    }

    size_t read_size;
    do
    {
        read_size = read(src_file, ctx.buffer, ctx.buffer_size);
        err = tarchivist_write_data(&ctx.tar, read_size, ctx.buffer);
        if (err <= TARCHIVIST_SUCCESS) {
            return PACKER_LIBERROR;
        }

    } while (ctx.tar.bytes_left > 0);

    if (close(src_file) != 0) {
        return PACKER_CLOSEFAIL;
    }
    return PACKER_SUCCESS;
}

static int packer_pack_directory(const char *path) {
    tarchivist_header_t header = {0};

    const size_t path_length = strlen(path) + 1;
    char *path_cleaned = calloc(1, path_length);
    if (path_cleaned == NULL) {
        printf("Failed to allocate %zuB for path buffer\n", path_length);
        return PACKER_NOMEMORY;
    }
    snprintf(path_cleaned, path_length, "%s", path);
    packer_path_cleanup(path_cleaned);

    time_t timestamp;
    time(&timestamp);

    snprintf(header.name, sizeof(header.name), "%s", path_cleaned);
    header.mode = 0755;
    header.uid = 1000;
    header.gid = 1000;
    header.mtime = timestamp;
    header.typeflag = TARCHIVIST_DIR;
    snprintf(header.uname, sizeof(header.uname), "Lefucjusz");
    snprintf(header.gname, sizeof(header.gname), "Lefucjusz");

    printf("Appending directory %s to %s\n", path, path_cleaned);
    free(path_cleaned);

    if (tarchivist_write_header(&ctx.tar, &header) != TARCHIVIST_SUCCESS) {
        return PACKER_LIBERROR;
    }
    return PACKER_SUCCESS;
}

static int packer_ftw_callback(const char *path, const struct stat *statbuf, int typeflag) {
    switch (typeflag) {
        case FTW_F:
            return packer_pack_file(statbuf, path);
        case FTW_D:
            return packer_pack_directory(path);
        default:
            printf("Unhandled case in ftw() callback: %d\n", typeflag);
            return PACKER_FAILURE;
    }
}

static int packer_unpack_file(tarchivist_header_t *header, const char *dir) {
    const char *name = header->name;
    const size_t path_length = strlen(name) + strlen(dir) + 2; // Two additional for '/' and null-terminator
    char *full_path = calloc(1, path_length);
    if (full_path == NULL) {
        printf("Failed to allocate %zuB for path buffer\n", path_length);
        return PACKER_NOMEMORY;
    }

    snprintf(full_path, path_length, "%s/%s", dir, name);

    int dst_file = open(full_path, O_WRONLY | O_CREAT, 0644); // If such file already existed, now it's gone
    if (dst_file <= 0) {
        printf("Failed to open file %s to write\n", full_path);
        free(full_path);
        return PACKER_OPENFAIL;
    }

    printf("Unpacking file %s (%zu.%03zuKiB)\n", full_path, header->size / 1024, header->size % 1024);
    free(full_path);

    size_t read_size;
    do
    {
        read_size = tarchivist_read_data(&ctx.tar, ctx.buffer_size, ctx.buffer);
        if (read_size <= TARCHIVIST_SUCCESS) {
            close(dst_file);
            return PACKER_LIBERROR;
        }
        write(dst_file, ctx.buffer, read_size);

    } while (ctx.tar.bytes_left > 0);

    if (close(dst_file) != 0) {
        return PACKER_CLOSEFAIL;
    }
    return PACKER_SUCCESS;
}

static int packer_unpack_directory(tarchivist_header_t *header, const char *dir) {
    const char *name = header->name;
    const size_t path_length = strlen(name) + strlen(dir) + 2; // Two additional for '/' and null-terminator
    char *full_path = calloc(1, path_length);
    if (full_path == NULL) {
        printf("Failed to allocate %zuB for path buffer\n", path_length);
        return PACKER_NOMEMORY;
    }

    snprintf(full_path, path_length, "%s/%s", dir, name);
    packer_remove_trailing_slash(full_path);
    printf("Creating directory %s\n", full_path);

    int err = packer_recursive_mkdir(full_path, 0755);
    if (err != 0) {
        if (errno == EEXIST) {
            printf("Directory %s already exists\n", full_path);
        }
        else {
            printf("Failed to create directory %s\n", full_path);
            free(full_path);
            return PACKER_FAILURE;
        }
    }

    free(full_path);
    return PACKER_SUCCESS;
}

/* Custom stream callbacks */
static int custom_seek(tarchivist_t *tar, long offset, int whence) {
    const int fd = *(int*)(tar->stream);
    off_t pos;
    switch (whence) {
        case TARCHIVIST_SEEK_SET:
            pos = lseek(fd, offset, SEEK_SET);
            break;
        case TARCHIVIST_SEEK_END:
            pos = lseek(fd, offset, SEEK_END);
            break;
        default:
            return TARCHIVIST_SEEKFAIL;
    }
    return (pos != -1) ? TARCHIVIST_SUCCESS : TARCHIVIST_SEEKFAIL;
}

static long custom_tell(tarchivist_t *tar) {
    const int fd = *(int*)(tar->stream);
    const off_t pos = lseek(fd, 0, SEEK_CUR);
    return (pos != -1) ? pos : TARCHIVIST_SEEKFAIL;
}

static int custom_read(tarchivist_t *tar, unsigned size, void *data) {
    const int fd = *(int*)(tar->stream);
    const size_t ret = read(fd, data, size);
    return (ret == size) ? TARCHIVIST_SUCCESS : TARCHIVIST_READFAIL;
}

static int custom_write(tarchivist_t *tar, unsigned size, const void *data) {
    const int fd = *(int*)(tar->stream);
    const size_t ret = write(fd, data, size);
    return (ret == size) ? TARCHIVIST_SUCCESS : TARCHIVIST_WRITEFAIL;
}

static int custom_close(tarchivist_t *tar) {
    const int fd = *(int*)(tar->stream);
    const int err = close(fd);
    free(tar->stream);
    return (err == 0) ? TARCHIVIST_SUCCESS : TARCHIVIST_CLOSEFAIL;
}

static int packer_tar_open(tarchivist_t *tar, const char *filename, const char *io_mode) {
    tarchivist_header_t header;
    int err;
    int *fd_ptr;

    /* Clear tar struct */
    memset(tar, 0, sizeof(tarchivist_t));

    /* Assign stream callbacks */
    tar->seek = custom_seek;
    tar->tell = custom_tell;
    tar->read = custom_read;
    tar->write = custom_write;
    tar->close = custom_close;

    fd_ptr = calloc(1, sizeof(int));
    if (fd_ptr == NULL) {
        printf("Failed to allocate memory for file descriptor!\n");
        return TARCHIVIST_FAILURE;
    }

    switch (io_mode[0]) {
        case 'r':
            tar->finalize = false;
            *fd_ptr = open(filename, O_RDONLY);
            if (*fd_ptr <= 0) {
                return TARCHIVIST_OPENFAIL;
            }
            tar->stream = fd_ptr;

            /* Validate the file */
            err = tarchivist_read_header(tar, &header);
            if (err != TARCHIVIST_SUCCESS) {
                close(*fd_ptr);
                return err;
            }
            break;

        case 'w':
            tar->finalize = true;
            *fd_ptr = open(filename, O_WRONLY | O_CREAT, 0644);
            if (*fd_ptr <= 0) {
                return TARCHIVIST_OPENFAIL;
            }
            tar->stream = fd_ptr;
            break;

        case 'a':
            tar->finalize = true;
            *fd_ptr = open(filename, O_RDWR | O_CREAT, 0644);
            if (*fd_ptr <= 0) {
                return TARCHIVIST_OPENFAIL;
            }
            tar->stream = fd_ptr;

            err = tarchivist_skip_closing_record(tar);
            if (err != TARCHIVIST_SUCCESS) {
                close(*fd_ptr);
                return err;
            }
            break;

        default:
            return TARCHIVIST_OPENFAIL;
    }
    return TARCHIVIST_SUCCESS;
}

static int packer_init(const char *tarname, const char *mode) {
    if (packer_tar_open(&ctx.tar, tarname, mode) != TARCHIVIST_SUCCESS) {
        printf("Failed to open archive %s in mode %s\n", tarname, mode);
        return PACKER_LIBERROR;
    }

    ctx.buffer_size = STREAM_BUFFER_SIZE;
    ctx.buffer = calloc(1, ctx.buffer_size);
    if (ctx.buffer == NULL) {
        printf("Failed to allocate %zuB for stream buffer\n", ctx.buffer_size);
        return PACKER_NOMEMORY;
    }

    return PACKER_SUCCESS;
}

static int packer_deinit(void) {
    if (tarchivist_close(&ctx.tar) != TARCHIVIST_SUCCESS) {
        printf("Failed to close archive\n");
        free(ctx.buffer);
        return PACKER_CLOSEFAIL;
    }

    free(ctx.buffer);
    return PACKER_SUCCESS;
}

int packer_pack(const char *tarname, const char *dir) {
    int err = packer_init(tarname, "a");
    if (err != PACKER_SUCCESS) {
        return err;
    }

    err = ftw(dir, packer_ftw_callback, FTW_MAX_DIRS_OPENED);

    packer_deinit();
    return err;
}

int packer_unpack(const char *dir, const char *tarname) {
    int lib_err;
    int err = packer_init(tarname, "r");
    if (err != PACKER_SUCCESS) {
        return err;
    }

    const char dir_length = strlen(dir) + 1;
    char *dir_cleaned = calloc(1, dir_length);
    if (dir_cleaned == NULL) {
	printf("Failed to allocate %zuB for path buffer\n", dir_length);
        return PACKER_NOMEMORY;
    }
    snprintf(dir_cleaned, dir_length, "%s", dir);

    /* Directory path cleanup */
    packer_remove_duplicated_slashes(dir_cleaned);
    packer_remove_trailing_slash(dir_cleaned);

    tarchivist_header_t header;
    while ((lib_err = tarchivist_read_header(&ctx.tar, &header)) == TARCHIVIST_SUCCESS) {
        switch (header.typeflag) {
            case TARCHIVIST_FILE:
                err = packer_unpack_file(&header, dir_cleaned);
                break;
            case TARCHIVIST_DIR:
                err = packer_unpack_directory(&header, dir_cleaned);
                break;
            default:
                printf("Unhandled case in unpack: %d\n", header.typeflag);
                err = PACKER_FAILURE;
                break;
        }

        if (lib_err != TARCHIVIST_SUCCESS) {
            err = PACKER_LIBERROR;
            break;
        }
        if (err != PACKER_SUCCESS) {
            break;
        }

        err = tarchivist_next(&ctx.tar);
        if (err != TARCHIVIST_SUCCESS) {
            err = PACKER_LIBERROR;
            break;
        }
    }

    if (err == PACKER_SUCCESS && lib_err != TARCHIVIST_NULLRECORD) {
        err = PACKER_LIBERROR;
    }

    free(dir_cleaned);
    packer_deinit();
    return err;
}
