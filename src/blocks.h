/* Copyright (C) 2018 Sam Bazley
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef BLOCKS_H
#define BLOCKS_H

struct LegacyData {
    char *execData;
    int xdiv;
};

struct SubblockData {
    char *execData;
    int *xdivs;
    int subblockCount;
};

enum Mode {
    LEGACY,
    SUBBLOCK
};

struct Block {
    char *exec;

    char *label;
    int eachmon;

    int padding, padIn, padOut;
    int nodiv;

    int interval;
    int ticks;
    int tickCount;

    enum Mode mode;

    union {
        union {
            struct LegacyData legacy;
            struct SubblockData subblock;
        } type;

        struct {
            union {
                struct LegacyData legacy;
                struct SubblockData subblock;
            } type;
        } *mon;
    } data;
};

extern int leftBlockCount, rightBlockCount;
extern struct Block *leftBlocks, *rightBlocks;

#endif /* BLOCKS_H */
