#ifndef __TAR_H__
#define __TAR_H__

enum {
    TAR_SUCCESS = 0,
    TAR_FAILURE = -1,
    TAR_LIBERROR = -1,
    TAR_NOMEMORY = -2,
    TAR_OPENFAIL = -3,
    TAR_CLOSEFAIL = -4
};

int tar_pack(const char *tarname, const char *dir);
int tar_unpack(const char *dir, const char *tarname);

#endif
