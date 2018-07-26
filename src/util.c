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

#include "util.h"
#include "blocks.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void parseColor(JsonObject *jo, const char *key, color dest, JsonError *err) {
    if (jsonGetPairIndex(jo, key) == -1) {
        return;
    }

    JsonArray *col;
    jsonGetArray(jo, key, &col, err);
    if (jsonErrorIsSet(err)) {
        fprintf(stderr, "Error parsing array \"%s\"\n", key);
        jsonErrorCleanup(err);
        jsonErrorInit(err);
        return;
    }

    if (col->used != 3) {
        fprintf(stderr, "\"%s\" array must contain 3 values\n", key);
        jsonErrorCleanup(err);
        jsonErrorInit(err);
        return;
    }

    for (int j = 0; j < 3; j++) {
        void *val = col->vals[j];
        if (jsonGetType(val) != JSON_NUMBER) {
            fprintf(stderr, "Value in \"%s\" array is not a valid int\n", key);
            jsonErrorCleanup(err);
            jsonErrorInit(err);
            return;
        }

        JsonNumber *n = (JsonNumber *) val;
        dest[j] = n->data;
    }
}

struct Block *createBlock(int eachmon) {
    struct Block *blk = 0;

    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].id == 0) {
            blk = &blocks[i];
            memset(blk, 0, sizeof(struct Block));
            blk->id = i + 1;
            break;
        }
    }

    if (blk == 0) {
        blocks = realloc(blocks, sizeof(struct Block) * ++blockCount);
        blk = &blocks[blockCount - 1];
        memset(blk, 0, sizeof(struct Block));
        blk->id = blockCount;
    }

    blk->eachmon = eachmon;

    if (eachmon) {
        blk->data.mon = malloc(sizeof(*(blk->data.mon)) * barCount);
        memset(blk->data.mon, 0, sizeof(*(blk->data.mon)) * barCount);
    }

    blk->width = malloc(sizeof(int) * barCount);

    return blk;
}

void removeBlock(struct Block *blk) {
    blk->id = 0;

    if (blk->eachmon) {
        free(blk->data.mon);
    }

    free(blk->width);
}

struct Block *getBlock(int id) {
    for (int i = 0; i < blockCount && id; i++) {
        struct Block *blk = &blocks[i];
        if (blk->id == id) {
            return blk;
        }
    }

    return 0;
}

static int gcd(int a, int b) {
    while (b) {
        a %= b;
        a ^= b;
        b ^= a;
        a ^= b;
    }
    return a;
}

void updateTickInterval() {
    int time = 0;

    for (int i = 0; i < blockCount; i++) {
        time = gcd(time, blocks[i].interval);
    }

    extern int interval;
    interval = time;
}
