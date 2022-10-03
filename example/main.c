#include <stdio.h>
#include <unistd.h>
#include "tar.h"

enum {
    PACK,
    UNPACK,
    UNKNOWN
};

int main(int argc, char **argv) {
    int opt, err;
    int mode = UNKNOWN;
    const char *src_path = NULL;
    const char *dst_path = NULL;

    while ((opt = getopt(argc, argv, "pus:d:")) != -1) {
        switch (opt) {
            case 'p':
                mode = PACK;
                break;
            case 'u':
                mode = UNPACK;
                break;
            case 's':
                src_path = optarg;
                break;
            case 'd':
                dst_path = optarg;
                break;
        }
    }

    switch (mode) {
        case PACK:
            err = tar_pack(dst_path, src_path);
            break;
        case UNPACK:
            err = tar_unpack(dst_path, src_path);
            break;
        default:
            printf("No mode option switch provided\n");
            break;
    }

    if (err != TAR_SUCCESS) {
        printf("Error while packing: %d\n", err);
        return err;
    }

    return 0;
}
