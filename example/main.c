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
    const char *input_path = NULL;
    const char *output_path = NULL;

    while ((opt = getopt(argc, argv, "pui:o:")) != -1) {
        switch (opt) {
            case 'p':
                mode = PACK;
                break;
            case 'u':
                mode = UNPACK;
                break;
            case 'i':
                input_path = optarg;
                break;
            case 'o':
                output_path = optarg;
                break;
        }
    }

    switch (mode) {
        case PACK:
            err = tar_pack(output_path, input_path);
            break;
        case UNPACK:
            err = tar_unpack(output_path, input_path);
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
