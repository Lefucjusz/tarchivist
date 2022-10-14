/*
 * Copyright (c) 2017 rxi
 * 
 * Copyright (c) 2022 Lefucjusz
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `tarchivist.c` for details.
 */

#ifndef __TARCHIVIST_H__
#define __TARCHIVIST_H__

#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#define TARCHIVIST_TAR_BLOCK_SIZE 512

typedef struct tarchivist_header_t {
    char name[100];
    unsigned mode;
    unsigned uid;
    unsigned gid;
    unsigned size;
    unsigned mtime;
    char typeflag;
    char linkname[100];
    char uname[32];
    char gname[32];
    unsigned devmajor;
    unsigned devminor;
    char prefix[155];
} tarchivist_header_t;

enum tarchivist_error_e {
    TARCHIVIST_SUCCESS    =  0,
    TARCHIVIST_FAILURE    = -1,
    TARCHIVIST_OPENFAIL   = -2,
    TARCHIVIST_READFAIL   = -3,
    TARCHIVIST_WRITEFAIL  = -4,
    TARCHIVIST_SEEKFAIL   = -5,
    TARCHIVIST_CLOSEFAIL  = -6,
    TARCHIVIST_BADCHKSUM  = -7,
    TARCHIVIST_NULLRECORD = -8,
    TARCHIVIST_NOTFOUND   = -9,
    TARCHIVIST_NOMEMORY   = -10
};

enum tarchivist_record_e {
    TARCHIVIST_FILE     =  '0',
    TARCHIVIST_AFILE    = '\0',
    TARCHIVIST_HARDLINK =  '1',
    TARCHIVIST_SYMLINK  =  '2',
    TARCHIVIST_CHARDEV  =  '3',
    TARCHIVIST_BLKDEV   =  '4',
    TARCHIVIST_DIR      =  '5',
    TARCHIVIST_FIFO     =  '6',
    TARCHIVIST_CONT     =  '7'
};

enum tarchivist_seek_origin_e {
    TARCHIVIST_SEEK_SET = 0,
    TARCHIVIST_SEEK_END = 1
};

typedef struct tarchivist_t tarchivist_t;

struct tarchivist_t {
    /* Pointers to IO functions */
    int  (*seek) (tarchivist_t *tar, long offset, int whence);
    long (*tell) (tarchivist_t *tar);
    int  (*read) (tarchivist_t *tar, unsigned size, void *data);
    int  (*write) (tarchivist_t *tar, unsigned size, const void *data);
    int  (*close) (tarchivist_t *tar);

    /* Internal variables */
    void *stream;
    bool finalize;
    unsigned bytes_left;
    long last_header_pos;
};

int tarchivist_skip_closing_record(tarchivist_t *tar);

int tarchivist_open(tarchivist_t *tar, const char *filename, const char *io_mode);
int tarchivist_close(tarchivist_t *tar);

int tarchivist_next(tarchivist_t *tar);
int tarchivist_find(tarchivist_t *tar, const char *filename, tarchivist_header_t *header);

int tarchivist_read_header(tarchivist_t *tar, tarchivist_header_t *header);
long tarchivist_read_data(tarchivist_t *tar, unsigned size, void *data);
int tarchivist_write_header(tarchivist_t *tar, const tarchivist_header_t *header);
long tarchivist_write_data(tarchivist_t *tar, unsigned size, const void *data);

const char *tarchivist_strerror(int error_code);

#endif
