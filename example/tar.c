#include "tar.h"

#include <stdio.h>
#include <stdlib.h>
#include "../tarchiver.h"
#include <string.h>
#include <time.h>
#include <ftw.h>
#include <errno.h>

#define FTW_MAX_DIRS_OPENED 10

typedef struct tar_ctx_t {
    char *buffer;
    size_t buffer_size;
    tarchiver_t tar;
} tar_ctx_t;

tar_ctx_t ctx;

static void tar_path_cleanup(char *path) {
    /* Remove './' */
    if (strncmp(path, "./", 2) == 0) {
        memmove(path, path + 2, strlen(path) - 1);
    }

    /* Remove leading '/' */
    if (path[0] == '/') {
        memmove(path, path + 1, strlen(path));
    }

    /* Remove trailing '/' */
    size_t end = strlen(path) - 1;
    if (path[end] == '/') {
        path[end] = '\0';
    }
}

static int tar_pack_file(const struct stat *statbuf, const char *path) {
    tarchiver_header_t header = {0};

    FILE *src_file = fopen(path, "rb");
    if (src_file == NULL) {
        return TAR_OPENFAIL;
    }

    const size_t path_length = strlen(path) + 1;
    char *path_cleaned = (char *) malloc(path_length);
    if (path_cleaned == NULL) {
        printf("Failed to allocate path buffer of size %luB\n", path_length);
        return TAR_NOMEMORY;
    }
    strncpy(path_cleaned, path, path_length);
    tar_path_cleanup(path_cleaned);

    time_t timestamp;
    time(&timestamp);

    strncpy(header.name, path_cleaned, sizeof(header.name));
    header.size = statbuf->st_size;
    header.mode = 0664;
    header.mtime = timestamp;
    header.typeflag = TARCHIVER_FILE;

    printf("Appending file %s to %s (%lu.%lukB)\n", path, path_cleaned, header.size / 1024, header.size % 1024);
    free(path_cleaned);

    int err = tarchiver_write_header(&ctx.tar, &header);
    if (err != TARCHIVER_SUCCESS) {
        return TAR_LIBERROR;
    }

    size_t read_size;
    do
    {
        read_size = fread(ctx.buffer, 1, ctx.buffer_size, src_file);
        err = tarchiver_write_data(&ctx.tar, read_size, ctx.buffer);
        if (err <= TARCHIVER_SUCCESS) {
            return TAR_LIBERROR;
        }
    } while (ctx.tar.bytes_left > 0);

    if (fclose(src_file) != 0) {
        return TAR_CLOSEFAIL;
    }
    return TAR_SUCCESS;
}

static int tar_pack_directory(const char *path) {
    tarchiver_header_t header = {0};

    const size_t path_length = strlen(path) + 1;
    char *path_cleaned = (char *) malloc(path_length);
    if (path_cleaned == NULL) {
        printf("Failed to allocate path buffer of size %luB\n", path_length);
        return TAR_NOMEMORY;
    }
    strncpy(path_cleaned, path, path_length);
    tar_path_cleanup(path_cleaned);

    time_t timestamp;
    time(&timestamp);

    strncpy(header.name, path_cleaned, sizeof(header.name));
    header.mode = 0755;
    header.mtime = timestamp;
    header.typeflag = TARCHIVER_DIR;

    printf("Appending directory %s to %s\n", path, path_cleaned);
    free(path_cleaned);

    if (tarchiver_write_header(&ctx.tar, &header) != TARCHIVER_SUCCESS) {
        return TAR_LIBERROR;
    }
    return TAR_SUCCESS;
}

static int tar_ftw_callback(const char *path, const struct stat *statbuf, int typeflag) {
    int err;
    switch (typeflag) {
        case FTW_F:
            err = tar_pack_file(statbuf, path);
            break;
        case FTW_D:
            err = tar_pack_directory(path);
            break;
        default:
            printf("Unhandled case in FTW callback: %d", typeflag);
            err = TAR_FAILURE;
            break;
    }
    return err;
}

static int tar_unpack_file(tarchiver_header_t *header, const char *dir) {
    const char *name = header->name;
    const size_t path_length = strlen(name) + strlen(dir) + 2; // Two additional for '/' and null-terminator
    char *full_path = (char *) malloc(path_length);
    if (full_path == NULL) {
        printf("Failed to allocate buffer of %luB\n", path_length);
        return TAR_NOMEMORY;
    }

    snprintf(full_path, path_length, "%s/%s", dir, name);

    FILE *dst_file = fopen(full_path, "wb"); // If such file already existed, now it's gone
    if (dst_file == NULL) {
        printf("Failed to open file %s to write\n", full_path);
        free(full_path);
        return TAR_OPENFAIL;
    }

    printf("Unpacking file %s (%lu.%lukB)\n", full_path, header->size / 1024, header->size % 1024);
    free(full_path);

    size_t read_size;
    do
    {
        read_size = tarchiver_read_data(&ctx.tar, ctx.buffer_size, ctx.buffer);
        if (read_size <= TARCHIVER_SUCCESS) {
            fclose(dst_file);
            return TAR_LIBERROR;
        }
        fwrite(ctx.buffer, 1, read_size, dst_file);

    } while (ctx.tar.bytes_left > 0);

    fclose(dst_file);
    return TAR_SUCCESS;
}

static int tar_unpack_directory(tarchiver_header_t *header, const char *dir) {
    const char *name = header->name;
    const size_t path_length = strlen(name) + strlen(dir) + 2; // Two additional for '/' and null-terminator
    char *full_path = (char *) malloc(path_length);
    if (full_path == NULL) {
        printf("Failed to allocate buffer of %luB\n", path_length);
        return TAR_NOMEMORY;
    }

    snprintf(full_path, path_length, "%s/%s", dir, name);
    printf("Creating directory %s\n", full_path);

    int err = mkdir(full_path, 0755);
    if (err != 0) {
        if (errno == EEXIST) {
            printf("Directory %s already exists\n", full_path);
        }
        else {
            printf("Failed to create directory %s\n", full_path);
            free(full_path);
            return TAR_FAILURE;
        }
    }

    free(full_path);
    return TAR_SUCCESS;
}

static int tar_init(const char *tarname, const char *mode) {
    if (tarchiver_open(&ctx.tar, tarname, mode) != TARCHIVER_SUCCESS) {
        printf("Failed to open archive %s in mode %s\n", tarname, mode);
        return TAR_LIBERROR;
    }

    ctx.buffer_size = 1024 * 1024; // 1MiB
    ctx.buffer = (char *) malloc(ctx.buffer_size);
    if (ctx.buffer == NULL) {
        printf("Failed to allocate memory for tar buffer\n");
        return TAR_NOMEMORY;
    }

    return TAR_SUCCESS;
}

static int tar_deinit(void) {
    if (tarchiver_close(&ctx.tar) != TARCHIVER_SUCCESS) {
        printf("Failed to close archive\n");
        free(ctx.buffer);
        return TAR_CLOSEFAIL;
    }

    free(ctx.buffer);
    return TAR_SUCCESS;
}

int tar_pack(const char *tarname, const char *dir) {
    int err = tar_init(tarname, "a");
    if (err != TAR_SUCCESS) {
        return err;
    }

    err = ftw(dir, tar_ftw_callback, FTW_MAX_DIRS_OPENED);

    tar_deinit();
    return err;
}

int tar_unpack(const char *dir, const char *tarname) {
    int lib_err;
    int err = tar_init(tarname, "r");
    if (err != TAR_SUCCESS) {
        return err;
    }

    tarchiver_header_t header;
    while ((lib_err = tarchiver_read_header(&ctx.tar, &header)) == TARCHIVER_SUCCESS) {
        switch (header.typeflag) {
            case TARCHIVER_FILE:
                err = tar_unpack_file(&header, dir);
                break;
            case TARCHIVER_DIR:
                err = tar_unpack_directory(&header, dir);
                break;
            default:
                printf("Unhandled case in unpack: %d", header.typeflag);
                err = TAR_FAILURE;
                break;
        }

        if (lib_err != TARCHIVER_SUCCESS) {
            err = TAR_LIBERROR;
            break;
        }
        if (err != TAR_SUCCESS) {
            break;
        }

        err = tarchiver_next(&ctx.tar);
        if (err != TARCHIVER_SUCCESS) {
            err = TAR_LIBERROR;
            break;
        }
    }

    if (err == TAR_SUCCESS && lib_err != TARCHIVER_NULLRECORD) {
        err = TAR_LIBERROR;
    }

    tar_deinit();
    return err;
}
