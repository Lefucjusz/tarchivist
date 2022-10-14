/* 
 * Copyright (c) 2022 Lefucjusz
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `packer.c` for details.
 */

#ifndef __PACKER_H__
#define __PACKER_H__

enum {
    PACKER_SUCCESS = 0,
    PACKER_FAILURE = -1,
    PACKER_LIBERROR = -1,
    PACKER_NOMEMORY = -2,
    PACKER_OPENFAIL = -3,
    PACKER_CLOSEFAIL = -4
};

int packer_pack(const char *tarname, const char *dir);
int packer_unpack(const char *dir, const char *tarname);

#endif
