#include "tarchiver.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>

//TODO restricts

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
    for (size_t i = 0; i < sizeof(tarchiver_raw_header_t); i++) {
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
    tar->bytes_to_read = 0;
    return tarchiver_seek(tar, 0, SEEK_SET);
}

static int tarchiver_raw_to_header(tarchiver_header_t *header, const tarchiver_raw_header_t *raw_header) {
    if (header == NULL || raw_header == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* Checksum starting with a null byte is a null record */
    if (raw_header->checksum[0] == '\0') {
        return TARCHIVER_NULLRECORD; // TODO why?
    }

    if (tarchiver_validate_checksum(raw_header) != TARCHIVER_SUCCESS) {
        return TARCHIVER_BADCHKSUM;
    }

    /* Parse and load raw header to header */
    strncpy(header->name, raw_header->name, sizeof(header->name));
    sscanf(raw_header->mode, "%o", &header->mode);
    sscanf(raw_header->uid, "%o", &header->uid);
    sscanf(raw_header->gid, "%o", &header->gid);
    sscanf(raw_header->size, "%lo", &header->size);
    sscanf(raw_header->mtime, "%lo", &header->mtime);
    header->typeflag = raw_header->typeflag;
    strncpy(header->linkname, raw_header->linkname, sizeof(header->linkname));
    strncpy(header->uname, raw_header->uname, sizeof(header->uname));
    strncpy(header->gname, raw_header->gname, sizeof(header->gname));
    sscanf(raw_header->devmajor, "%o", &header->devmajor);
    sscanf(raw_header->devminor, "%o", &header->devminor);
    strncpy(header->prefix, raw_header->prefix, sizeof(header->prefix));

    return TARCHIVER_SUCCESS;
}

static int tarchiver_header_to_raw(tarchiver_raw_header_t *raw_header, const tarchiver_header_t *header) {

    /* Clear header */
    memset(raw_header, 0, sizeof(tarchiver_raw_header_t));

    /* Parse and load header to raw header */
    strncpy(raw_header->name, header->name, sizeof(raw_header->name));
    snprintf(raw_header->mode, sizeof(raw_header->mode), "%o", header->mode);
    snprintf(raw_header->uid, sizeof(raw_header->uid), "%o", header->uid);
    snprintf(raw_header->gid, sizeof(raw_header->gid), "%o", header->gid);
    snprintf(raw_header->size, sizeof(raw_header->size), "%lo", header->size);
    snprintf(raw_header->mtime, sizeof(raw_header->mtime), "%lo", header->mtime);
    raw_header->typeflag = (header->typeflag == '\0') ? TARCHIVER_NORMAL : header->typeflag; // TODO is it needed?
    strncpy(raw_header->linkname, header->linkname, sizeof(raw_header->linkname));
    strncpy(raw_header->magic, "ustar", sizeof(raw_header->magic));
    strncpy(raw_header->version, "00", sizeof(raw_header->version));
    strncpy(raw_header->uname, header->uname, sizeof(raw_header->uname));
    strncpy(raw_header->gname, header->gname, sizeof(raw_header->gname));
    snprintf(raw_header->devmajor, sizeof(raw_header->devmajor), "%o", header->devmajor);
    snprintf(raw_header->devminor, sizeof(raw_header->devminor), "%o", header->devminor);
    strncpy(raw_header->prefix, header->prefix, sizeof(raw_header->prefix));

    /* Compute checksum */
    unsigned checksum = tarchiver_compute_checksum(raw_header);
    snprintf(raw_header->checksum, sizeof(raw_header->checksum), "%06o", checksum);
    raw_header->checksum[7] = ' ';

    return TARCHIVER_SUCCESS;
}

int tarchiver_open(tarchiver_t *tar, const char *filename, const char *mode) {
    if (tar == NULL) {
        return TARCHIVER_FAILURE;
    }

    /* Clear tar struct */
    memset(tar, 0, sizeof(tarchiver_t));

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

int tarchiver_read_header(tarchiver_t *tar, tarchiver_header_t *header) {
    tarchiver_raw_header_t raw_header;
    /* Save last header position */
    tar->last_header_pos = ftell(tar->stream);

    /* Read the header */
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

int tarchiver_next(tarchiver_t *tar) {
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

int tarchiver_find(tarchiver_t *tar, const char *filename, tarchiver_header_t *header) {
    tarchiver_header_t temp_header;

    /* Look from the beginning of the archive */
    int err = tarchiver_rewind(tar);
    if (err != TARCHIVER_SUCCESS) {
        return err;
    }

    /* Iterate until there's nothing left to read */
    while ((err = tarchiver_read_header(tar, &temp_header)) == TARCHIVER_SUCCESS) {
        if(strcmp(filename, temp_header.name) == 0) {
            if (header != NULL) {
                *header = temp_header;
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

ssize_t tarchiver_read_data(tarchiver_t *tar, size_t size, void *data) {
    int err;

    /* If no bytes left to read then this is the first read, obtain the
     * size from the header and go to the beginning of the data */
    if (tar->bytes_to_read == 0) {
        tarchiver_header_t header;
        err = tarchiver_read_header(tar, &header);
        if (err != TARCHIVER_SUCCESS) {
            return err;
        }
        tar->bytes_to_read = header.size;

        err = tarchiver_seek(tar, ftell(tar->stream) + sizeof(tarchiver_raw_header_t), SEEK_SET);
        if (err != TARCHIVER_SUCCESS) {
            return err;
        }
    }

    /* If requested to read more than left to read */
    if (tar->bytes_to_read < size) {
        size = tar->bytes_to_read;
    }

    err = tarchiver_read(tar, size, data);
    if (err != TARCHIVER_SUCCESS) {
        return err;
    }
    tar->bytes_to_read -= size;

    if (tar->bytes_to_read == 0) {
        err = tarchiver_seek(tar, tar->last_header_pos, SEEK_SET);
        if (err != TARCHIVER_SUCCESS) {
            return err;
        }
    }

    /* If no errors, return real read size */
    return size;
}

//int tarchiver_write_data(tarchiver_t *tar, size_t size, const void *data) {
//
//}

int tarchiver_close(tarchiver_t *tar) {
    if (tar == NULL || tar->stream == NULL) {
        return TARCHIVER_FAILURE;
    }
    if (fclose(tar->stream) != 0) {
        return TARCHIVER_CLOSEFAIL;
    }
    return TARCHIVER_SUCCESS;
}
