#include "tarchiver.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define TARCHIVER_CLOSING_RECORD_SIZE (2 * TARCHIVER_TAR_BLOCK_SIZE)
#define TARCHIVER_MAGIC "ustar"
#define TARCHIVER_VERSION "00"

/* USTAR format */
typedef struct __attribute__((packed)) tarchiver_raw_header_t {
    char name[100];     // Filename, null-terminated if space
    char mode[8];       // Permissions as octal string
    char uid[8];        // User ID as octal string
    char gid[8];        // Group ID as octal string
    char size[12];      // File size in bytes as octal string
    char mtime[12];     // Modification time (seconds from Jan 1, 1970) as octal string
    char checksum[8];   // Sum of bytes in header (with checksum considered as all spaces)
    char typeflag;      // Record type
    char linkname[100]; // Name of the link target, null-terminated if space
    char magic[6];      // "ustar\0"
    char version[2];    // "00" (no null-terminator!)
    char uname[32];     // User name, always null-terminated
    char gname[32];     // Group name, always null-terminated
    char devmajor[8];   // Device major number as octal string
    char devminor[8];   // Device minor number as octal string
    char prefix[155];   // Prefix to file name, null-terminated if space
    char padding[12];   // Padding to 512 bytes
} tarchiver_raw_header_t;

static unsigned tarchiver_compute_checksum(const tarchiver_raw_header_t *header) {
    const uint8_t *header_byte_ptr = (const uint8_t *) header;
    const size_t checksum_start = offsetof(tarchiver_raw_header_t, checksum);
    const size_t checksum_end = checksum_start + sizeof(header->checksum);
    unsigned checksum = 0;

    /* Checksum is computed as if checksum field was all spaces */
    for (size_t i = 0; i < sizeof(tarchiver_raw_header_t); ++i) {
        if (i >= checksum_start && i < checksum_end) {
            checksum += (unsigned)(' ');
        }
        else {
            checksum += header_byte_ptr[i];
        }
    }

    return checksum;
}

static int tarchiver_validate_checksum(const tarchiver_raw_header_t *header) {
    unsigned real_checksum = tarchiver_compute_checksum(header);
    unsigned stored_checksum;

    /* Convert octal string to decimal value */
    sscanf(header->checksum, "%o", &stored_checksum);

    return (real_checksum == stored_checksum) ? TARCHIVER_SUCCESS : TARCHIVER_BADCHKSUM;
}

static size_t tarchiver_round_up(size_t value, size_t multiple) {
    return value + (multiple - (value % multiple)) % multiple;
}

static int tarchiver_seek(tarchiver_t *tar, long offset, int whence) {
    int err = fseek(tar->stream, offset, whence);
    return (err == 0) ? TARCHIVER_SUCCESS : TARCHIVER_SEEKFAIL;
}

static int tarchiver_read(tarchiver_t *tar, size_t size, void *data) {
    size_t read_size = fread(data, 1, size, tar->stream);
    return (read_size == size) ? TARCHIVER_SUCCESS : TARCHIVER_READFAIL;
}

static int tarchiver_write(tarchiver_t *tar, size_t size, const void *data) {
    size_t write_size = fwrite(data, 1, size, tar->stream);
    return (write_size == size) ? TARCHIVER_SUCCESS : TARCHIVER_WRITEFAIL;
}

static int tarchiver_rewind(tarchiver_t *tar) {
    tar->last_header_pos = 0;
    tar->bytes_left = 0;
    return tarchiver_seek(tar, 0, SEEK_SET);
}

static int tarchiver_skip_closing_record(tarchiver_t *tar) {
    int err;

    /* Perform only during the first write and in append mode */
    if (tar->mode != 'a') {
        tar->first_write = false;
        return TARCHIVER_SUCCESS;
    }
    if (tar->first_write == false) {
        return TARCHIVER_SUCCESS;
    }

    char *buffer = (char *) calloc(1, TARCHIVER_CLOSING_RECORD_SIZE);
    char *zeros = (char *) calloc(1, TARCHIVER_CLOSING_RECORD_SIZE);
    if (buffer == NULL || zeros == NULL) {
        return TARCHIVER_NOMEMORY;
    }

    do
    {
        /* Seek to the beginning of the closing record */
        err = tarchiver_seek(tar, -TARCHIVER_CLOSING_RECORD_SIZE, SEEK_END);
        if (err != TARCHIVER_SUCCESS) {
            break;
        }

        /* Read the content of the closing record */
        err = tarchiver_read(tar, TARCHIVER_CLOSING_RECORD_SIZE, buffer);
        if (err != TARCHIVER_SUCCESS) {
            break;
        }

        /* Check whether it is closing record indeed */
        if (memcmp(buffer, zeros, TARCHIVER_CLOSING_RECORD_SIZE) == 0) {
            /* Seek to the beginning of the closing record so that the next write will overwrite it */
            err = tarchiver_seek(tar, -TARCHIVER_CLOSING_RECORD_SIZE, SEEK_END);
            if (err != TARCHIVER_SUCCESS) {
                break;
            }
            tar->first_write = false;
            break;
        }
        /* If it's not a closing record, do nothing */

    } while (0);

    free(zeros);
    free(buffer);
    return err;
}

static int tarchiver_raw_to_header(tarchiver_header_t *header, const tarchiver_raw_header_t *raw_header) {
    /* Assume that checksum starting with a null byte indicates a null record */
    if (raw_header->checksum[0] == '\0') {
        return TARCHIVER_NULLRECORD;
    }

    if (tarchiver_validate_checksum(raw_header) != TARCHIVER_SUCCESS) {
        return TARCHIVER_BADCHKSUM;
    }

    /* Parse and load raw header to header */
    memcpy(header->name, raw_header->name, sizeof(header->name));
    sscanf(raw_header->mode, "%o", &header->mode);
    sscanf(raw_header->uid, "%o", &header->uid);
    sscanf(raw_header->gid, "%o", &header->gid);
    sscanf(raw_header->size, "%lo", &header->size);
    sscanf(raw_header->mtime, "%lo", &header->mtime);
    header->typeflag = raw_header->typeflag;
    memcpy(header->linkname, raw_header->linkname, sizeof(header->linkname));
    memcpy(header->uname, raw_header->uname, sizeof(header->uname));
    memcpy(header->gname, raw_header->gname, sizeof(header->gname));
    sscanf(raw_header->devmajor, "%o", &header->devmajor);
    sscanf(raw_header->devminor, "%o", &header->devminor);
    memcpy(header->prefix, raw_header->prefix, sizeof(header->prefix));

    return TARCHIVER_SUCCESS;
}

static int tarchiver_header_to_raw(tarchiver_raw_header_t *raw_header, const tarchiver_header_t *header) {

    /* Clear header */
    memset(raw_header, 0, sizeof(tarchiver_raw_header_t));

    /* Parse and load header to raw header */
    memcpy(raw_header->name, header->name, sizeof(raw_header->name));
    snprintf(raw_header->mode, sizeof(raw_header->mode), "%o", header->mode);
    snprintf(raw_header->uid, sizeof(raw_header->uid), "%o", header->uid);
    snprintf(raw_header->gid, sizeof(raw_header->gid), "%o", header->gid);
    snprintf(raw_header->size, sizeof(raw_header->size), "%lo", header->size);
    snprintf(raw_header->mtime, sizeof(raw_header->mtime), "%lo", header->mtime);
    raw_header->typeflag = header->typeflag;
    memcpy(raw_header->linkname, header->linkname, sizeof(raw_header->linkname));
    memcpy(raw_header->magic, TARCHIVER_MAGIC, sizeof(raw_header->magic));
    memcpy(raw_header->version, TARCHIVER_VERSION, sizeof(raw_header->version));
    memcpy(raw_header->uname, header->uname, sizeof(raw_header->uname));
    memcpy(raw_header->gname, header->gname, sizeof(raw_header->gname));
    snprintf(raw_header->devmajor, sizeof(raw_header->devmajor), "%o", header->devmajor);
    snprintf(raw_header->devminor, sizeof(raw_header->devminor), "%o", header->devminor);
    memcpy(raw_header->prefix, header->prefix, sizeof(raw_header->prefix));

    /* Compute checksum */
    unsigned checksum = tarchiver_compute_checksum(raw_header);
    snprintf(raw_header->checksum, sizeof(raw_header->checksum), "%06o", checksum);
    raw_header->checksum[7] = ' ';

    return TARCHIVER_SUCCESS;
}

int tarchiver_open(tarchiver_t *tar, const char *filename, const char *mode) {
    if (tar == NULL || filename == NULL || mode == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* Prepare tar struct */
    memset(tar, 0, sizeof(tarchiver_t));
    tar->first_write = true;

    /* Ensure that file is accessed in binary mode */
    tar->mode = mode[0];
    switch (tar->mode) {
        case 'r':
            mode = "rb";
            break;
        case 'a':
            mode = "rb+"; // Little hack to be able to append to arbitrary places in file
            break;
        case 'w':
            mode = "wb";
            break;
        default:
            return TARCHIVER_FAILURE;
    }

    /* Try to open file in selected mode */
    tar->stream = fopen(filename, mode);

    /* If that has failed and requested mode is 'append', maybe the file doesn't exist
     * Try to open it in normal write mode */
    if (tar->stream == NULL && tar->mode == 'a') {
        tar->mode = 'w';
        tar->stream = fopen(filename, "wb");
    }

    /* If that has failed too, give up */
    if (tar->stream == NULL) {
        return TARCHIVER_OPENFAIL;
    }

    /* If in read mode - check if the opened file is a valid tar archive */
    if (tar->mode == 'r') {
        tarchiver_header_t header;
        int err = tarchiver_read_header(tar, &header);
        if (err != TARCHIVER_SUCCESS) {
            fclose(tar->stream);
            return err;
        }
    }

    return TARCHIVER_SUCCESS;
}

int tarchiver_next(tarchiver_t *tar) {
    if (tar == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* Read header */
    tarchiver_header_t header;
    int err = tarchiver_read_header(tar, &header);
    if (err != TARCHIVER_SUCCESS) {
        return err;
    }

    /* Compute record size */
    size_t record_size = tarchiver_round_up(header.size, TARCHIVER_TAR_BLOCK_SIZE) + sizeof(tarchiver_raw_header_t);
    return tarchiver_seek(tar, ftell(tar->stream) + record_size, SEEK_SET);
}

int tarchiver_find(tarchiver_t *tar, const char *path, tarchiver_header_t *header) {
    if (tar == NULL || path == NULL || header == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* Search from the beginning of the archive */
    int err = tarchiver_rewind(tar);
    if (err != TARCHIVER_SUCCESS) {
        return err;
    }

    const char *name;
    size_t prefix_length, name_length;
    const size_t path_length = strlen(path);

    /* Case where path is stored in the prefix field */
    if (path_length > sizeof(header->name)) {
        /* Extract file name */
        name = strrchr(path, '/');

        /* Pure filename longer than 100 chars, cannot be stored in USTAR archive */
        if (name == NULL) {
            return TARCHIVER_NULLRECORD;
        }

        /* Compute lengths */
        name++; // Skip slash
        name_length = strlen(name);
        prefix_length = path_length - name_length - 1;

        /* This path cannot be stored in USTAR archive */
        if (name_length > sizeof(header->name) || prefix_length > sizeof(header->prefix)) {
            return TARCHIVER_NULLRECORD;
        }
    }

    /* Iterate until there's nothing left to read */
    while ((err = tarchiver_read_header(tar, header)) == TARCHIVER_SUCCESS) {
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

        tarchiver_next(tar);
    }

    if (err == TARCHIVER_NULLRECORD) {
        return TARCHIVER_NOTFOUND;
    }
    return err;
}

int tarchiver_read_header(tarchiver_t *tar, tarchiver_header_t *header) {
    if (tar == NULL || header == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* Save last header position */
    tar->last_header_pos = ftell(tar->stream);

    /* Read the header */
    tarchiver_raw_header_t raw_header;
    int read_status = tarchiver_read(tar, sizeof(tarchiver_raw_header_t), &raw_header);

    /* Go back to the beginning of the header */
    int seek_status = tarchiver_seek(tar, tar->last_header_pos, SEEK_SET);

    /* Report status */
    if (read_status != TARCHIVER_SUCCESS) {
        return read_status;
    }
    if (seek_status != TARCHIVER_SUCCESS) {
        return seek_status;
    }
    return tarchiver_raw_to_header(header, &raw_header);
}

ssize_t tarchiver_read_data(tarchiver_t *tar, size_t size, void *data) {
    if (tar == NULL || data == NULL) {
        return TARCHIVER_FAILURE;
    }

    int err;

    /* If no bytes left to read then this is the first read, obtain the
     * size from the header and go to the beginning of the data */
    if (tar->bytes_left == 0) {
        tarchiver_header_t header;
        err = tarchiver_read_header(tar, &header);
        if (err != TARCHIVER_SUCCESS) {
            return err;
        }
        tar->bytes_left = header.size;

        err = tarchiver_seek(tar, ftell(tar->stream) + sizeof(tarchiver_raw_header_t), SEEK_SET);
        if (err != TARCHIVER_SUCCESS) {
            return err;
        }
    }

    /* If requested to read more than left to read */
    if (tar->bytes_left < size) {
        size = tar->bytes_left;
    }

    /* Read data */
    err = tarchiver_read(tar, size, data);
    if (err != TARCHIVER_SUCCESS) {
        return err;
    }

    tar->bytes_left -= size;

    /* If no data left, rewind back to the beginning of the record */
    if (tar->bytes_left == 0) {
        err = tarchiver_seek(tar, tar->last_header_pos, SEEK_SET);
        if (err != TARCHIVER_SUCCESS) {
            return err;
        }
    }

    /* If no errors, return real read size */
    return size;
}

int tarchiver_write_header(tarchiver_t *tar, const tarchiver_header_t *header) {
    if (tar == NULL || header == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* Skip closing record */
    int err = tarchiver_skip_closing_record(tar);
    if (err != TARCHIVER_SUCCESS) {
        return err;
    }

    /* Prepare raw header */
    tarchiver_raw_header_t raw_header;
    tarchiver_header_to_raw(&raw_header, header);
    tar->bytes_left = header->size; // Store size to know how many bytes of data has to be written
    return tarchiver_write(tar, sizeof(tarchiver_raw_header_t), &raw_header);
}

ssize_t tarchiver_write_data(tarchiver_t *tar, size_t size, const void *data) {
    if (tar == NULL || data == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* Skip closing record */
    int err = tarchiver_skip_closing_record(tar);
    if (err != TARCHIVER_SUCCESS) {
        return err;
    }

    /* If requested to write more than left to write */
    if (tar->bytes_left < size) {
        size = tar->bytes_left;
    }

    /* Write data */
    err = tarchiver_write(tar, size, data);
    if (err != TARCHIVER_SUCCESS) {
        return err;
    }
    tar->bytes_left -= size;

    /* Pad with zeros to multiple of a block size */
    long pos = ftell(tar->stream);
    size_t pad_size = tarchiver_round_up(pos, TARCHIVER_TAR_BLOCK_SIZE) - pos;

    char *zeros = (char *) calloc(1, pad_size);
    if (zeros == NULL) {
        return TARCHIVER_NOMEMORY;
    }

    err = tarchiver_write(tar, pad_size, zeros);
    if (err != TARCHIVER_SUCCESS) {
        free(zeros);
        return err;
    }

    free(zeros);
    return size;
}

int tarchiver_close(tarchiver_t *tar) {
    if (tar == NULL || tar->stream == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* If not in read mode and new data have been written, finalize the archive */
    if (tar->mode != 'r' && tar->first_write == false) {
        char *zeros = (char *) calloc(1, TARCHIVER_CLOSING_RECORD_SIZE);
        if (zeros == NULL) {
            return TARCHIVER_NOMEMORY;
        }

        int err = tarchiver_write(tar, TARCHIVER_CLOSING_RECORD_SIZE, zeros);
        if (err != TARCHIVER_SUCCESS) {
            free(zeros);
            return err;
        }

        free(zeros);
    }

    if (fclose(tar->stream) != 0) {
        return TARCHIVER_CLOSEFAIL;
    }
    return TARCHIVER_SUCCESS;
}
