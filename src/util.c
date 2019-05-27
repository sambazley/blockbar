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
#include "modules.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int blockbarParseColorJson(JsonObject *jo, const char *key, color dest,
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
            return blockbarParseColorString(str+1, dest);
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

    for (unsigned int j = 0; j < col->used; j++) {
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

int blockbarParseColorString(const char *str, color dest) {
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

void blockbarStringifyColor(const color c, char *s) {
    sprintf(s, "#%02x%02x%02x%02x", c[0], c[1], c[2], c[3]);
}

void resizeBlock(struct Block *blk) {
    for (int bar = 0; bar < barCount; bar++) {
        if (blk->sfc[bar]) {
            cairo_surface_destroy(blk->sfc[bar]);
        }

        blk->sfc[bar] = cairo_surface_create_similar_image(
                bars[bar].sfc[0], CAIRO_FORMAT_ARGB32,
                bars[bar].width, bars[bar].height);
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
        blk->data = malloc(sizeof(struct BlockData) * barCount);
        memset(blk->data, 0, sizeof(struct BlockData) * barCount);
    } else {
        blk->data = malloc(sizeof(struct BlockData));
        memset(blk->data, 0, sizeof(struct BlockData));
    }

    memcpy(&(blk->properties), &defProperties, sizeof(blk->properties));

    for (int i = 0; i < propertyCount; i++) {
        struct Setting *property = &((struct Setting *) &(blk->properties))[i];
        memset(&(property->val), 0, sizeof(property->val));

        setSetting(property, property->def);
    }

    blk->width = malloc(sizeof(int) * barCount);
    blk->x = malloc(sizeof(int) * barCount);
    blk->sfc = malloc(sizeof(cairo_surface_t *) * barCount);
    memset(blk->sfc, 0, sizeof(cairo_surface_t *) * barCount);

    resizeBlock(blk);

    return blk;
}

void removeBlock(struct Block *blk) {
    moduleRegisterBlock(blk, 0, 0);

    blk->id = 0;

    if (blk->eachmon) {
        for (int i = 0; i < barCount; i++) {
            char *execData = blk->data[i].execData;

            if (execData) {
                free(execData);
            }
        }
    } else {
        char *execData = blk->data->execData;

        if (execData) {
            free(execData);
        }
    }

    free(blk->data);

    for (int i = 0; i < propertyCount; i++) {
        struct Setting *property = &((struct Setting *) &(blk->properties))[i];

        if (property->type == STR && property->val.STR) {
            free(property->val.STR);
        }
    }

    for (int i = 0; i < barCount; i++) {
        cairo_surface_destroy(blk->sfc[i]);
    }

    free(blk->width);
    free(blk->x);
    free(blk->sfc);
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
        struct Block *blk = &blocks[i];

        if (!blk->id) {
            continue;
        }

        time = gcd(time, blk->properties.interval.val.INT);
    }

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (!mod->dl || mod->data.type != RENDER) {
            continue;
        }

        time = gcd(time, mod->data.interval);
    }

    extern int interval;
    interval = time;
}
