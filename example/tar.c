#include "tar.h"

#include <stdio.h>
#include <stdlib.h>
#include "../tarchiver.h"
#include <string.h>
#include <time.h>
#include <ftw.h>
#include <errno.h>

#define FTW_MAX_DIRS_OPENED 10
#define STREAM_BUFFER_SIZE (1024 * 1024) // 1MiB

typedef struct tar_ctx_t {
    char *buffer;
    size_t buffer_size;
    tarchiver_t tar;
} tar_ctx_t;

tar_ctx_t ctx;

static void tar_remove_duplicated_slashes(char *path) {
    if (path == NULL) {
	return;
    }

    for (size_t i = 0; i < strlen(path) - 1; i++) {
        if (path[i] == '/' && path[i + 1] == '/') {
            memmove(path + i, path + i + 1, strlen(path + i + 1) + 1);
        }
    }
}

static void tar_remove_trailing_slash(char *path) {
    if (path == NULL || strlen(path) < 2) {
	return;
    }

    const size_t end = strlen(path) - 1;
    if (path[end] == '/') {
        path[end] = '\0';
    }
}

static void tar_path_cleanup(char *path) {
    if (path == NULL || strlen(path) < 2) {
        return;
    }

    /* Remove duplicated slashes */
    tar_remove_duplicated_slashes(path);

    /* Remove CWD */
    if (strncmp(path, "./", 2) == 0) {
        memmove(path, path + 2, strlen(path + 2) + 1);
    }

    /* Remove leading slash */
    if (path[0] == '/') {
        memmove(path, path + 1, strlen(path + 1) + 1);
    }

    /* Remove trailing slash */
    tar_remove_trailing_slash(path);
}

static int tar_recursive_mkdir(char *path, mode_t mode) {
    int err = 0;

    /* Try to create directory */
    err = mkdir(path, mode);

    /* If unsuccessful, recursively try to create parent directory */
    if (err != 0) {
        char *bottom_dir = strrchr(path, '/');
        if (bottom_dir != NULL) {
            bottom_dir[0] = '\0';
            err = tar_recursive_mkdir(path, mode);
            bottom_dir[0] = '/';
            err = mkdir(path, mode);
       }
    }

    return err;
}

static int tar_pack_file(const struct stat *statbuf, const char *path) {
    tarchiver_header_t header = {0};

    FILE *src_file = fopen(path, "rb");
    if (src_file == NULL) {
        return TAR_OPENFAIL;
    }

    const size_t path_length = strlen(path) + 1;
    char *path_cleaned = (char *) calloc(1, path_length);
    if (path_cleaned == NULL) {
        printf("Failed to allocate %luB for path buffer\n", path_length);
        return TAR_NOMEMORY;
    }
    strncpy(path_cleaned, path, path_length);
    tar_path_cleanup(path_cleaned);

    time_t timestamp;
    time(&timestamp);

    strncpy(header.name, path_cleaned, sizeof(header.name));
    header.mode = 0664;
    header.uid = 1000;
    header.gid = 1000;
    header.size = statbuf->st_size;
    header.mtime = timestamp;
    header.typeflag = TARCHIVER_FILE;
    strncpy(header.uname, "Lefucjusz", sizeof(header.uname));
    strncpy(header.gname, "Lefucjusz", sizeof(header.gname));

    printf("Appending file %s to %s (%lu.%03luKiB)\n", path, path_cleaned, header.size / 1024, header.size % 1024);
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
    char *path_cleaned = (char *) calloc(1, path_length);
    if (path_cleaned == NULL) {
        printf("Failed to allocate %luB for path buffer\n", path_length);
        return TAR_NOMEMORY;
    }
    strncpy(path_cleaned, path, path_length);
    tar_path_cleanup(path_cleaned);

    time_t timestamp;
    time(&timestamp);

    strncpy(header.name, path_cleaned, sizeof(header.name));
    header.mode = 0755;
    header.uid = 1000;
    header.gid = 1000;
    header.mtime = timestamp;
    header.typeflag = TARCHIVER_DIR;
    strncpy(header.uname, "Lefucjusz", sizeof(header.uname));
    strncpy(header.gname, "Lefucjusz", sizeof(header.gname));

    printf("Appending directory %s to %s\n", path, path_cleaned);
    free(path_cleaned);

    if (tarchiver_write_header(&ctx.tar, &header) != TARCHIVER_SUCCESS) {
        return TAR_LIBERROR;
    }
    return TAR_SUCCESS;
}

static int tar_ftw_callback(const char *path, const struct stat *statbuf, int typeflag) {
    switch (typeflag) {
        case FTW_F:
            return tar_pack_file(statbuf, path);
        case FTW_D:
            return tar_pack_directory(path);
        default:
            printf("Unhandled case in ftw() callback: %d\n", typeflag);
            return TAR_FAILURE;
    }
}

static int tar_unpack_file(tarchiver_header_t *header, const char *dir) {
    const char *name = header->name;
    const size_t path_length = strlen(name) + strlen(dir) + 2; // Two additional for '/' and null-terminator
    char *full_path = (char *) calloc(1, path_length);
    if (full_path == NULL) {
        printf("Failed to allocate %luB for path buffer\n", path_length);
        return TAR_NOMEMORY;
    }

    snprintf(full_path, path_length, "%s/%s", dir, name);

    FILE *dst_file = fopen(full_path, "wb"); // If such file already existed, now it's gone
    if (dst_file == NULL) {
        printf("Failed to open file %s to write\n", full_path);
        free(full_path);
        return TAR_OPENFAIL;
    }

    printf("Unpacking file %s (%lu.%03luKiB)\n", full_path, header->size / 1024, header->size % 1024);
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
    char *full_path = (char *) calloc(1, path_length);
    if (full_path == NULL) {
        printf("Failed to allocate %luB for path buffer\n", path_length);
        return TAR_NOMEMORY;
    }

    snprintf(full_path, path_length, "%s/%s", dir, name);
    tar_remove_trailing_slash(full_path);
    printf("Creating directory %s\n", full_path);

    int err = tar_recursive_mkdir(full_path, 0755);
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

    ctx.buffer_size = STREAM_BUFFER_SIZE;
    ctx.buffer = (char *) calloc(1, ctx.buffer_size);
    if (ctx.buffer == NULL) {
        printf("Failed to allocate %luB for stream buffer\n", ctx.buffer_size);
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

    char *dir_cleaned = (char *) calloc(1, strlen(dir) + 1);
    if (dir_cleaned == NULL) {
        return TAR_NOMEMORY;
    }
    strcpy(dir_cleaned, dir);

    /* Directory path cleanup */
    tar_remove_duplicated_slashes(dir_cleaned);
    tar_remove_trailing_slash(dir_cleaned);

    tarchiver_header_t header;
    while ((lib_err = tarchiver_read_header(&ctx.tar, &header)) == TARCHIVER_SUCCESS) {
        switch (header.typeflag) {
            case TARCHIVER_FILE:
                err = tar_unpack_file(&header, dir_cleaned);
                break;
            case TARCHIVER_DIR:
                err = tar_unpack_directory(&header, dir_cleaned);
                break;
            default:
                printf("Unhandled case in unpack: %d\n", header.typeflag);
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

    free(dir_cleaned);
    tar_deinit();
    return err;
}
