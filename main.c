#include <stdio.h>
#include <stdlib.h>
#include "tarchiver.h"
#include <string.h>

int main(int argc, char **argv) {
    int err;
    tarchiver_t tar;

    err = tarchiver_open(&tar, "test.tar", "r");
    if (err != TARCHIVER_SUCCESS) {
        printf("Failed to open tar! Error %d\n", err);
        return -1;
    }

    for (size_t i = 0; i < 5; i++) {
        tarchiver_next(&tar);
    }

    // tarchiver_next(&tar);

    tarchiver_header_t header;

    err = tarchiver_read_header(&tar, &header);
    if (err != TARCHIVER_SUCCESS) {
        printf("Failed to read header! Error %d\n", err);
        tarchiver_close(&tar);
        return -2;
    }


    printf("name: %s\n", header.name);
    printf("mode: %u\n", header.mode);
    printf("uid: %u\n", header.uid);
    printf("gid: %u\n", header.gid);
    printf("size: %lu\n", header.size);
    printf("mtime: %lu\n", header.mtime);
    printf("typeflag: %c\n", header.typeflag);
    printf("linkname: %s\n", header.linkname);

    const char* filename = strrchr(header.name, '/') == NULL ? header.name : strrchr(header.name, '/') + 1;

    printf("%s\n", filename);

    FILE* result = fopen(filename, "wb");
    if (result == NULL) {
	printf("Failed to open result file\n");
        tarchiver_close(&tar);
        return -3;
    }

    const size_t buf_size = 1024 * 1024;
    char *buffer = malloc(buf_size);
    if (buffer == NULL) {
        printf("Failed to allocate IO buffer\n");
        tarchiver_close(&tar);
        fclose(result);
        return -4;
    }

    size_t read_size;

    do
    {
        read_size = tarchiver_read_data(&tar, buf_size, buffer);
        if (read_size <= 0) {
            printf("Data reading failed!\n");
            break;
        }
        fwrite(buffer, 1, read_size, result);
    } while (tar.bytes_to_read > 0);

    free(buffer);

    fclose(result);

    tarchiver_close(&tar);

   return 0;
}
