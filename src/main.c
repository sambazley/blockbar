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

#include "blocks.h"
#include "config.h"
#include "render.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void printUsage(const char *file) {
    fprintf(stderr, "Usage: %s [config_file]\n", file);
}

static void blocksInit(struct Block *blocks, int count) {
    for (int i = 0; i < count; i++) {
        struct Block *blk = &blocks[i];
        if (blk->eachmon) {
            if (blk->mode == LEGACY) {
                blk->data.eachMon.legacy =
                    malloc(sizeof(struct LegacyData) * barCount);
            } else {
                blk->data.eachMon.subblock =
                    malloc(sizeof(struct SubblockData) * barCount);
            }
        }
    }
}

static void blocksCleanup(struct Block *blocks, int count) {
    for (int i = 0; i < count; i++) {
        struct Block *blk = &blocks[i];
        if (blk->eachmon) {
            if (blk->mode == LEGACY) {
                free(blk->data.eachMon.legacy);
            } else {
                free(blk->data.eachMon.subblock);
            }
        }
    }
}

int main(int argc, const char *argv[]) {
    const char *config = "";
    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printUsage(argv[0]);
            return 1;
        }

        config = argv[1];
    } else if (argc > 2) {
        printUsage(argv[0]);
        return 1;
    }

    configParse(config);

    renderInit();

    if (createBars() != 0) {
        return 1;
    }

    blocksInit(leftBlocks, leftBlockCount);
    blocksInit(rightBlocks, rightBlockCount);

    while (1) {
        pollEvents();
        redraw();
    }

    blocksCleanup(leftBlocks, leftBlockCount);
    blocksCleanup(rightBlocks, rightBlockCount);

    return 0;
}
