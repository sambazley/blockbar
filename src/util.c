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
#include "config.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parseColorJson(JsonObject *jo, const char *key, color dest,
                    JsonError *err) {
    int index = jsonGetPairIndex(jo, key);
    if (index == -1) {
        return 1;
    }

    void *val = jo->pairs[index].val;

    if (jsonGetType(val) == JSON_STRING) {
        char *str;
        jsonGetString(jo, key, &str, err);

        if (str && *str == '#') {
            return parseColorString(str+1, dest);
        }

        return 1;
    }

    JsonArray *col;
    jsonGetArray(jo, key, &col, err);
    if (jsonErrorIsSet(err)) {
        fprintf(stderr, "Error parsing array \"%s\"\n", key);
        jsonErrorCleanup(err);
        jsonErrorInit(err);
        return 1;
    }

    if (col->used != 3 && col->used != 4) {
        fprintf(stderr, "\"%s\" array must contain 3 or 4 values\n", key);
        jsonErrorCleanup(err);
        jsonErrorInit(err);
        return 1;
    }

    for (int j = 0; j < col->used; j++) {
        void *val = col->vals[j];
        if (jsonGetType(val) != JSON_NUMBER) {
            fprintf(stderr, "Value in \"%s\" array is not a valid int\n", key);
            jsonErrorCleanup(err);
            jsonErrorInit(err);
            return 1;
        }

        JsonNumber *n = (JsonNumber *) val;
        dest[j] = n->data;
    }

    if (col->used == 3) {
        dest[3] = 0xFF;
    }

    return 0;
}

int parseColorString(char *str, color dest) {
    char *end = 0;

    int c = strtol(str, &end, 16);
    int colInt = 0xFFFFFFFF;

    if (end && *end) {
        return 1;
    }

    if (strlen(str) == 3 || strlen(str) == 4) {
        if (strlen(str) == 3) {
            c <<= 4;
            c |= 0xF;
        }

        colInt = ((c & 0xF000) << 16) | ((c & 0xF000) << 12)
               | ((c & 0x0F00) << 12) | ((c & 0x0F00) << 8)
               | ((c & 0x00F0) << 8) | ((c & 0x00F0) << 4)
               | ((c & 0x000F) << 4) | (c & 0x000F);
    } else if (strlen(str) == 6) {
        colInt = (c << 8) | 0xFF;
    } else if (strlen(str) == 8) {
        colInt = c;
    } else {
        return 1;
    }

    dest[0] = colInt >> 24;
    dest[1] = (colInt >> 16) & 0xFF;
    dest[2] = (colInt >> 8) & 0xFF;
    dest[3] = colInt & 0xFF;

    return 0;
}

void stringifyColor(color c, char *s) {
    sprintf(s, "#%02x%02x%02x%02x", c[0], c[1], c[2], c[3]);
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

    memcpy(&(blk->properties), &defProperties, sizeof(blk->properties));

    for (int i = 0; i < propertyCount; i++) {
        struct Setting *property = &((struct Setting *) &(blk->properties))[i];
        memset(&(property->val), 0, sizeof(property->val));

        setSetting(property, property->def);
    }

    blk->width = malloc(sizeof(int) * barCount);

    return blk;
}

void removeBlock(struct Block *blk) {
    blk->id = 0;

    if (blk->properties.mode.val.MODE == SUBBLOCK) {
        if (blk->eachmon) {
            for (int i = 0; i < barCount; i++) {
                int *widths = blk->data.mon[i].type.subblock.widths;
                if (widths) {
                    free(widths);
                }
            }
        } else {
            int *widths = blk->data.type.subblock.widths;
            if (widths) {
                free(widths);
            }
        }
    }

    if (blk->eachmon) {
        for (int i = 0; i < barCount; i++) {
            char *execData = blk->data.mon[i].type.legacy.execData;

            if (execData) {
                free(execData);
            }
        }

        free(blk->data.mon);
    } else {
        char *execData = blk->data.type.legacy.execData;

        if (execData) {
            free(execData);
        }
    }

    if (blk->properties.label.val.STR) {
        free(blk->properties.label.val.STR);
    }

    if (blk->properties.exec.val.STR) {
        free(blk->properties.exec.val.STR);
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
        time = gcd(time, blocks[i].properties.interval.val.INT);
    }

    extern int interval;
    interval = time;
}
