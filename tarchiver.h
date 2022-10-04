#ifndef __TARCHIVER_H__
#define __TARCHIVER_H__

#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#define TARCHIVER_TAR_BLOCK_SIZE 512

typedef struct tarchiver_header_t {
    char name[100];
    unsigned mode;
    unsigned uid;
    unsigned gid;
    size_t size;
    time_t mtime;
    char typeflag;
    char linkname[100];
    char uname[32];
    char gname[32];
    unsigned devmajor;
    unsigned devminor;
    char prefix[155];
} tarchiver_header_t;

enum {
    TARCHIVER_SUCCESS    =  0,
    TARCHIVER_FAILURE    = -1,
    TARCHIVER_OPENFAIL   = -2,
    TARCHIVER_READFAIL   = -3,
    TARCHIVER_WRITEFAIL  = -4,
    TARCHIVER_SEEKFAIL   = -5,
    TARCHIVER_CLOSEFAIL  = -6,
    TARCHIVER_BADCHKSUM  = -7,
    TARCHIVER_NULLRECORD = -8,
    TARCHIVER_NOTFOUND   = -9,
    TARCHIVER_NOMEMORY   = -10
};

enum {
     TARCHIVER_FILE     = '0',
     TARCHIVER_HARDLINK = '1',
     TARCHIVER_SYMLINK  = '2',
     TARCHIVER_CHARDEV  = '3',
     TARCHIVER_BLKDEV   = '4',
     TARCHIVER_DIR      = '5',
     TARCHIVER_FIFO     = '6'
};

typedef struct tarchiver_t {
    FILE *stream;
    char mode;
    long last_header_pos;
    size_t bytes_left;
    bool first_write;
} tarchiver_t;

int tarchiver_open(tarchiver_t *tar, const char *filename, const char *mode);
int tarchiver_next(tarchiver_t *tar);
int tarchiver_find(tarchiver_t *tar, const char *filename, tarchiver_header_t *header);
int tarchiver_read_header(tarchiver_t *tar, tarchiver_header_t *header);
long tarchiver_read_data(tarchiver_t *tar, size_t size, void *data);
int tarchiver_write_header(tarchiver_t *tar, const tarchiver_header_t *header);
long tarchiver_write_data(tarchiver_t *tar, size_t size, const void *data);
int tarchiver_close(tarchiver_t *tar);

#endif
