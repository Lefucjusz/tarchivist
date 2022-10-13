/*
 * Copyright (c) 2017 rxi
 *
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

#include "tarchivist.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define TARCHIVIST_CLOSING_RECORD_SIZE (2 * TARCHIVIST_TAR_BLOCK_SIZE)
#define TARCHIVIST_MAGIC "ustar"
#define TARCHIVIST_VERSION "00"

/* USTAR format */
typedef struct tarchivist_raw_header_t {
    char name[100];     /* Filename, null-terminated if space */
    char mode[8];       /* Permissions as octal string */
    char uid[8];        /* User ID as octal string */
    char gid[8];        /* Group ID as octal string */
    char size[12];      /* File size in bytes as octal string */
    char mtime[12];     /* Modification time (seconds from Jan 1, 1970) as octal string */
    char checksum[8];   /* Sum of bytes in header (with checksum considered as all spaces) */
    char typeflag;      /* Record type */
    char linkname[100]; /* Name of the link target, null-terminated if space */
    char magic[6];      /* "ustar\0" */
    char version[2];    /* "00" (no null-terminator!) */
    char uname[32];     /* User name, always null-terminated */
    char gname[32];     /* Group name, always null-terminated */
    char devmajor[8];   /* Device major number as octal string */
    char devminor[8];   /* Device minor number as octal string */
    char prefix[155];   /* Prefix to file name, null-terminated if space */
    char padding[12];   /* Padding to 512 bytes */
} tarchivist_raw_header_t;

static unsigned tarchivist_compute_checksum(const tarchivist_raw_header_t *header) {
    const uint8_t *header_byte_ptr = (const uint8_t *) header;
    const unsigned checksum_start = offsetof(tarchivist_raw_header_t, checksum);
    const unsigned checksum_end = checksum_start + sizeof(header->checksum);
    unsigned checksum = 0;
    unsigned i;

    /* Checksum is computed as if the checksum field is all spaces */
    for (i = 0; i < sizeof(tarchivist_raw_header_t); ++i) {
        if (i >= checksum_start && i < checksum_end) {
            checksum += (unsigned)(' ');
        }
        else {
            checksum += header_byte_ptr[i];
        }
    }

    return checksum;
}

static int tarchivist_validate_checksum(const tarchivist_raw_header_t *header) {
    const unsigned real_checksum = tarchivist_compute_checksum(header);
    unsigned stored_checksum;

    /* Convert octal string to decimal value */
    sscanf(header->checksum, "%o", &stored_checksum);
    return (real_checksum == stored_checksum) ? TARCHIVIST_SUCCESS : TARCHIVIST_BADCHKSUM;
}

static unsigned tarchivist_round_up(unsigned value, unsigned multiple) {
    return value + (multiple - (value % multiple)) % multiple;
}

static int tarchivist_seek_impl(tarchivist_t *tar, long offset, int whence) {
    int err;
    switch (whence) {
        case TARCHIVIST_SEEK_SET:
            err = fseek(tar->stream, offset, SEEK_SET);
            break;
        case TARCHIVIST_SEEK_END:
            err = fseek(tar->stream, offset, SEEK_END);
            break;
        default:
            return TARCHIVIST_SEEKFAIL;

    }
    return (err == 0) ? TARCHIVIST_SUCCESS : TARCHIVIST_SEEKFAIL;
}

static long tarchivist_tell_impl(tarchivist_t *tar) {
    const long pos = ftell(tar->stream);
    if (pos < 0) {
        return TARCHIVIST_SEEKFAIL;
    }
    return pos;
}

static int tarchivist_read_impl(tarchivist_t *tar, unsigned size, void *data) {
    const unsigned read_size = fread(data, 1, size, tar->stream);
    return (read_size == size) ? TARCHIVIST_SUCCESS : TARCHIVIST_READFAIL;
}

static int tarchivist_write_impl(tarchivist_t *tar, unsigned size, const void *data) {
    const unsigned write_size = fwrite(data, 1, size, tar->stream);
    return (write_size == size) ? TARCHIVIST_SUCCESS : TARCHIVIST_WRITEFAIL;
}

static int tarchivist_close_impl(tarchivist_t *tar) {
    const int err = fclose(tar->stream);
    return (err == 0) ? TARCHIVIST_SUCCESS : TARCHIVIST_CLOSEFAIL;
}

static int tarchivist_rewind(tarchivist_t *tar) {
    tar->last_header_pos = 0;
    tar->bytes_left = 0;
    return tar->seek(tar, 0, TARCHIVIST_SEEK_SET);
}

static int tarchivist_skip_closing_record(tarchivist_t *tar) {
    char *buffer;
    char *zeros;
    int err;

    /* This algorithm will fail if tar is not finalized and last 1024 bytes of last file content are zeros */
    do
    {
        buffer = calloc(1, TARCHIVIST_CLOSING_RECORD_SIZE);
        zeros = calloc(1, TARCHIVIST_CLOSING_RECORD_SIZE);
        if (buffer == NULL || zeros == NULL) {
            err = TARCHIVIST_NOMEMORY;
            break;
        }

        /* Seek to the beginning of the closing record */
        err = tar->seek(tar, -TARCHIVIST_CLOSING_RECORD_SIZE, TARCHIVIST_SEEK_END);
        if (err != TARCHIVIST_SUCCESS) {
            break;
        }

        /* Read the content of the closing record */
        err = tar->read(tar, TARCHIVIST_CLOSING_RECORD_SIZE, buffer);
        if (err != TARCHIVIST_SUCCESS) {
            break;
        }

        /* Check whether it is closing record indeed */
        if (memcmp(buffer, zeros, TARCHIVIST_CLOSING_RECORD_SIZE) == 0) {
            /* Seek to the beginning of the closing record so that the next write will overwrite it */
            err = tar->seek(tar, -TARCHIVIST_CLOSING_RECORD_SIZE, TARCHIVIST_SEEK_END);
            break;
        }
        /* If it's not a closing record, do nothing */

    } while (0);

    free(zeros);
    free(buffer);
    return err;
}

static int tarchivist_raw_to_header(tarchivist_header_t *header, const tarchivist_raw_header_t *raw_header) {
    /* Assume that checksum starting with a null byte indicates a null record */
    if (raw_header->checksum[0] == '\0') {
        return TARCHIVIST_NULLRECORD;
    }

    if (tarchivist_validate_checksum(raw_header) != TARCHIVIST_SUCCESS) {
        return TARCHIVIST_BADCHKSUM;
    }

    /* Parse and load raw header to header */
    memcpy(header->name, raw_header->name, sizeof(header->name));
    sscanf(raw_header->mode, "%o", &header->mode);
    sscanf(raw_header->uid, "%o", &header->uid);
    sscanf(raw_header->gid, "%o", &header->gid);
    sscanf(raw_header->size, "%o", &header->size);
    sscanf(raw_header->mtime, "%o", &header->mtime);
    header->typeflag = raw_header->typeflag;
    memcpy(header->linkname, raw_header->linkname, sizeof(header->linkname));
    memcpy(header->uname, raw_header->uname, sizeof(header->uname));
    memcpy(header->gname, raw_header->gname, sizeof(header->gname));
    sscanf(raw_header->devmajor, "%o", &header->devmajor);
    sscanf(raw_header->devminor, "%o", &header->devminor);
    memcpy(header->prefix, raw_header->prefix, sizeof(header->prefix));

    return TARCHIVIST_SUCCESS;
}

static int tarchivist_header_to_raw(tarchivist_raw_header_t *raw_header, const tarchivist_header_t *header) {
    unsigned checksum;

    /* Clear header */
    memset(raw_header, 0, sizeof(tarchivist_raw_header_t));

    /* Parse and load header to raw header */
    memcpy(raw_header->name, header->name, sizeof(raw_header->name));
    sprintf(raw_header->mode, "%o", header->mode);
    sprintf(raw_header->uid, "%o", header->uid);
    sprintf(raw_header->gid, "%o", header->gid);
    sprintf(raw_header->size, "%o", header->size);
    sprintf(raw_header->mtime, "%o", header->mtime);
    raw_header->typeflag = header->typeflag;
    memcpy(raw_header->linkname, header->linkname, sizeof(raw_header->linkname));
    memcpy(raw_header->magic, TARCHIVIST_MAGIC, sizeof(raw_header->magic));
    memcpy(raw_header->version, TARCHIVIST_VERSION, sizeof(raw_header->version));
    memcpy(raw_header->uname, header->uname, sizeof(raw_header->uname));
    memcpy(raw_header->gname, header->gname, sizeof(raw_header->gname));
    sprintf(raw_header->devmajor, "%o", header->devmajor);
    sprintf(raw_header->devminor, "%o", header->devminor);
    memcpy(raw_header->prefix, header->prefix, sizeof(raw_header->prefix));

    /* Compute checksum */
    checksum = tarchivist_compute_checksum(raw_header);
    sprintf(raw_header->checksum, "%06o", checksum);
    raw_header->checksum[7] = ' ';

    return TARCHIVIST_SUCCESS;
}

int tarchivist_open(tarchivist_t *tar, const char *filename, const char *io_mode) {
    tarchivist_header_t header;
    long size;
    int err;

    if (tar == NULL || filename == NULL || io_mode == NULL) {
        return TARCHIVIST_FAILURE;
    }

    /* Clear tar struct */
    memset(tar, 0, sizeof(tarchivist_t));

    /* Assign default IO functions */
    tar->seek = tarchivist_seek_impl;
    tar->tell = tarchivist_tell_impl;
    tar->read = tarchivist_read_impl;
    tar->write = tarchivist_write_impl;
    tar->close = tarchivist_close_impl;

    /* Ensure that file is opened and prepared properly */
    switch (io_mode[0]) {
        case 'r':
            tar->finalize = false;
            tar->stream = fopen(filename, "rb");
            if (tar->stream == NULL) {
                return TARCHIVIST_OPENFAIL;
            }
            /* Validate the file */
            err = tarchivist_read_header(tar, &header);
            if (err != TARCHIVIST_SUCCESS) {
                fclose(tar->stream);
                return err;
            }
            return TARCHIVIST_SUCCESS;

        case 'w':
            tar->finalize = true;
            tar->stream = fopen(filename, "wb");
            if (tar->stream == NULL) {
                return TARCHIVIST_OPENFAIL;
            }
            return TARCHIVIST_SUCCESS;

        case 'a':
            tar->finalize = true;
            tar->stream = fopen(filename, "rb+"); /* Little hack to be able to append to arbitrary places in file */
            if (tar->stream == NULL) {
                tar->stream = fopen(filename, "wb");
                if (tar->stream == NULL) {
                    return TARCHIVIST_OPENFAIL;
                }
            }

            tar->seek(tar, 0, TARCHIVIST_SEEK_END);
            size = tar->tell(tar);
            tar->seek(tar, 0, TARCHIVIST_SEEK_SET);

            if (size < TARCHIVIST_CLOSING_RECORD_SIZE) {
                return TARCHIVIST_SUCCESS; /* File contains some non-tar garbage that will be overwritten */
            }

            /* Check the first header */
            err = tarchivist_read_header(tar, &header);
            switch (err) {
                case TARCHIVIST_BADCHKSUM:
                case TARCHIVIST_NULLRECORD:
                    return TARCHIVIST_SUCCESS; /* Again - some garbage or malformed tar */

                case TARCHIVIST_SUCCESS:
                    err = tarchivist_skip_closing_record(tar);
                    if (err != TARCHIVIST_SUCCESS) {
                        fclose(tar->stream);
                    }
                    break;

                default:
                    break;

            }
            return err;

        default:
            return TARCHIVIST_OPENFAIL;
    }
}

int tarchivist_next(tarchivist_t *tar) {
    tarchivist_header_t header;
    unsigned record_size;
    int err;

    if (tar == NULL) {
        return TARCHIVIST_FAILURE;
    }

    /* Read header */
    err = tarchivist_read_header(tar, &header);
    if (err != TARCHIVIST_SUCCESS) {
        return err;
    }

    /* Compute record size */
    record_size = tarchivist_round_up(header.size, TARCHIVIST_TAR_BLOCK_SIZE) + sizeof(tarchivist_raw_header_t);
    return tar->seek(tar, tar->tell(tar) + record_size, TARCHIVIST_SEEK_SET);
}

int tarchivist_find(tarchivist_t *tar, const char *path, tarchivist_header_t *header) {
    unsigned prefix_length, name_length, path_length;
    const char *name;
    int err;

    if (tar == NULL || path == NULL || header == NULL) {
        return TARCHIVIST_FAILURE;
    }

    /* Search from the beginning of the archive */
    err = tarchivist_rewind(tar);
    if (err != TARCHIVIST_SUCCESS) {
        return err;
    }

    path_length = strlen(path);

    /* Case where path is stored in the prefix field */
    if (path_length > sizeof(header->name)) {
        /* Extract file name */
        name = strrchr(path, '/');

        /* Pure filename longer than 100 chars, cannot be stored in USTAR archive */
        if (name == NULL) {
            return TARCHIVIST_NULLRECORD;
        }

        /* Compute lengths */
        name++; /* Skip slash */
        name_length = strlen(name);
        prefix_length = path_length - name_length - 1;

        /* This path cannot be stored in USTAR archive */
        if (name_length > sizeof(header->name) || prefix_length > sizeof(header->prefix)) {
            return TARCHIVIST_NULLRECORD;
        }
    }

    /* Iterate until there's nothing left to read */
    while ((err = tarchivist_read_header(tar, header)) == TARCHIVIST_SUCCESS) {
        if (path_length <= sizeof(header->name)) {
            if (strcmp(path, header->name) == 0) {
                break;
            }
        }
        else {
            if (strncmp(path, header->prefix, prefix_length) == 0 && strcmp(name, header->name) == 0) {
                break;
            }
        }

        tarchivist_next(tar);
    }

    if (err == TARCHIVIST_NULLRECORD) {
        return TARCHIVIST_NOTFOUND;
    }
    return err;
}

int tarchivist_read_header(tarchivist_t *tar, tarchivist_header_t *header) {
    tarchivist_raw_header_t raw_header;
    int read_status, seek_status;

    if (tar == NULL || header == NULL) {
        return TARCHIVIST_FAILURE;
    }

    /* Save last header position */
    tar->last_header_pos = tar->tell(tar);

    /* Read the header */
    read_status = tar->read(tar, sizeof(tarchivist_raw_header_t), &raw_header);

    /* Go back to the beginning of the header */
    seek_status = tar->seek(tar, tar->last_header_pos, TARCHIVIST_SEEK_SET);

    /* Report status */
    if (read_status != TARCHIVIST_SUCCESS) {
        return read_status;
    }
    if (seek_status != TARCHIVIST_SUCCESS) {
        return seek_status;
    }
    return tarchivist_raw_to_header(header, &raw_header);
}

long tarchivist_read_data(tarchivist_t *tar, unsigned size, void *data) {
    tarchivist_header_t header;
    int err;

    if (tar == NULL || data == NULL) {
        return TARCHIVIST_FAILURE;
    }

    /* If no bytes left to read then this is the first read, obtain the
     * size from the header and go to the beginning of the data */
    if (tar->bytes_left == 0) {
        err = tarchivist_read_header(tar, &header);
        if (err != TARCHIVIST_SUCCESS) {
            return err;
        }
        tar->bytes_left = header.size;

        err = tar->seek(tar, tar->tell(tar) + sizeof(tarchivist_raw_header_t), TARCHIVIST_SEEK_SET);
        if (err != TARCHIVIST_SUCCESS) {
            return err;
        }
    }

    /* If requested to read more than left to read */
    if (tar->bytes_left < size) {
        size = tar->bytes_left;
    }

    /* Read data */
    err = tar->read(tar, size, data);
    if (err != TARCHIVIST_SUCCESS) {
        return err;
    }
    tar->bytes_left -= size;

    /* If no data left, rewind back to the beginning of the record */
    if (tar->bytes_left == 0) {
        err = tar->seek(tar, tar->last_header_pos, TARCHIVIST_SEEK_SET);
        if (err != TARCHIVIST_SUCCESS) {
            return err;
        }
    }

    /* If no errors, return real read size */
    return size;
}

int tarchivist_write_header(tarchivist_t *tar, const tarchivist_header_t *header) {
    tarchivist_raw_header_t raw_header;

    if (tar == NULL || header == NULL) {
        return TARCHIVIST_FAILURE;
    }

    /* Prepare raw header */
    tarchivist_header_to_raw(&raw_header, header);
    tar->bytes_left = header->size; /* Store size to know how many bytes of data has to be written */
    return tar->write(tar, sizeof(tarchivist_raw_header_t), &raw_header);
}

long tarchivist_write_data(tarchivist_t *tar, unsigned size, const void *data) {
    unsigned pad_size;
    long pos;
    int err;
    char *zeros;

    if (tar == NULL || data == NULL) {
        return TARCHIVIST_FAILURE;
    }

    /* If requested to write more than left to write */
    if (tar->bytes_left < size) {
        size = tar->bytes_left;
    }

    /* Write data */
    err = tar->write(tar, size, data);
    if (err != TARCHIVIST_SUCCESS) {
        return err;
    }
    tar->bytes_left -= size;

    /* Pad with zeros to multiple of a block size */
    pos = tar->tell(tar);
    pad_size = tarchivist_round_up(pos, TARCHIVIST_TAR_BLOCK_SIZE) - pos;

    /* If no padding required, job done */
    if (pad_size == 0) {
        return size;
    }

    zeros = calloc(1, pad_size);
    if (zeros == NULL) {
        return TARCHIVIST_NOMEMORY;
    }

    err = tar->write(tar, pad_size, zeros);
    if (err != TARCHIVIST_SUCCESS) {
        free(zeros);
        return err;
    }

    free(zeros);
    return size;
}

int tarchivist_close(tarchivist_t *tar) {
    char *zeros;
    int err;

    if (tar == NULL || tar->stream == NULL) {
        return TARCHIVIST_FAILURE;
    }

    /* Finalize the archive if required */
    if (tar->finalize) {
        zeros = calloc(1, TARCHIVIST_CLOSING_RECORD_SIZE);
        if (zeros == NULL) {
            return TARCHIVIST_NOMEMORY;
        }

        err = tar->write(tar, TARCHIVIST_CLOSING_RECORD_SIZE, zeros);
        if (err != TARCHIVIST_SUCCESS) {
            free(zeros);
            return err;
        }

        free(zeros);
    }

    return tar->close(tar);
}

const char *tarchivist_strerror(int error_code) {
    switch (error_code) {
        case TARCHIVIST_SUCCESS:
            return "success";   
        case TARCHIVIST_FAILURE: 
            return "general failure";    
        case TARCHIVIST_OPENFAIL: 
            return "failed to open";   
        case TARCHIVIST_READFAIL:  
            return "failed to read data";  
        case TARCHIVIST_WRITEFAIL: 
            return "failed to write data";  
        case TARCHIVIST_SEEKFAIL: 
            return "failed to seek";   
        case TARCHIVIST_CLOSEFAIL: 
            return "failed to close";  
        case TARCHIVIST_BADCHKSUM: 
            return "bad header checksum";  
        case TARCHIVIST_NULLRECORD:
            return "record is null";  
        case TARCHIVIST_NOTFOUND: 
            return "record not found";   
        case TARCHIVIST_NOMEMORY:
            return "no memory left";  
        default:
            return "unknown"; 
    }
}
