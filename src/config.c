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

#include "config.h"
#include "blocks.h"
#include "window.h"
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ujson.h>
#include <unistd.h>

#define ERRCHK() \
    if (jsonErrorIsSet(err)) { \
        fprintf(stderr, "error %u\n%s\n", (uint32_t) __LINE__, err->msg); \
        return; \
    }

int blockCount;
struct Block *blocks;

static void loadDefaults() {
    conf.height = 22;
    conf.padding = 5;
    conf.trayPadding = 2;
    conf.trayIconSize = 18;
    conf.trayBar = 0;

    conf.bg[0] = 0;
    conf.bg[1] = 0;
    conf.bg[2] = 0;

    conf.fg[0] = 0xFF;
    conf.fg[1] = 0xFF;
    conf.fg[2] = 0xFF;

    conf.font = 0;
}

static const char *defaultConfigFile() {
    char *cfgDir;
    char *cfgDirEnv = getenv("XDG_CONFIG_HOME");

    if (cfgDirEnv == 0) {
        char *home = getenv("HOME");
        char *defaultCfgDir = "/.config";

        if (home == 0) {
            struct passwd *pw = getpwuid(getuid());
            home = pw->pw_dir;
        }

        cfgDir = malloc(strlen(home) + strlen(defaultCfgDir) + 1);
        strcpy(cfgDir, home);
        strcat(cfgDir, defaultCfgDir);
    } else {
        cfgDir = malloc(strlen(cfgDirEnv) + 1);
        strcpy(cfgDir, cfgDirEnv);
    }

    char *filename = "/blockbar/config";
    char *ret = malloc(strlen(cfgDir) + strlen(filename) + 1);
    strcpy(ret, cfgDir);
    strcat(ret, filename);

    free(cfgDir);

    return ret;
}

static void
parseInt(JsonObject *jo, const char *key, int *dest, JsonError *err) {
    if (jsonGetPairIndex(jo, key) == -1) {
        return;
    }

    jsonGetInt(jo, key, dest, err);
}

static void
parseBool(JsonObject *jo, const char *key, int *dest, JsonError *err) {
    if (jsonGetPairIndex(jo, key) == -1) {
        return;
    }

    jsonGetBool(jo, key, (unsigned *) dest, 0, err);
}

static void
parseString(JsonObject *jo, const char *key, char **dest, JsonError *err) {
    if (jsonGetPairIndex(jo, key) == -1) {
        return;
    }

    char *str;
    jsonGetString(jo, key, &str, err);

    *dest = malloc(strlen(str) + 1);
    strcpy(*dest, str);
}

static void
parseBlocks(JsonObject *jo, const char *key, enum Pos pos, JsonError *err) {
    JsonArray *arr;

    if (jsonGetPairIndex(jo, key) == -1) {
        return;
    }

    jsonGetArray(jo, key, &arr, err); ERRCHK();

    blocks = realloc(blocks, sizeof(struct Block) * (blockCount + arr->used));

    for (int i = 0; i < arr->used; i++) {
        JsonObject *entry = arr->vals[i];
        if (jsonGetType(entry) != JSON_OBJECT) {
            printf("Skipping invalid block entry\n");
            continue;
        }

        struct Block *blk = &blocks[blockCount];
        memset(blk, 0, sizeof(struct Block));
        char *mode = 0;
        parseString(entry, "mode", &mode, err);
        parseBool(entry, "eachmon", &(blk->eachmon), err);
        parseString(entry, "label", &(blk->label), err);
        parseString(entry, "exec", &(blk->exec), err);
        parseInt(entry, "interval", &(blk->interval), err);
        parseInt(entry, "padding", &(blk->padding), err);
        parseInt(entry, "padding-inside", &(blk->padIn), err);
        parseInt(entry, "padding-outside", &(blk->padOut), err);
        parseBool(entry, "nodiv", &(blk->nodiv), err);

        blk->pos = pos;

        blk->mode = LEGACY;
        if (mode) {
            if (strcmp(mode, "subblocks") == 0) {
                blk->mode = SUBBLOCK;
            }
            free(mode);
        }

        blockCount++;
    }
}

void configParse(const char *config) {
    loadDefaults();

    const char *file;

    if (strcmp(config, "") == 0) {
        file = defaultConfigFile();
    } else {
        file = config;
    }

    printf("Using config %s\n", file);

    JsonError err;
    JSON_ERROR_INIT(err);

    JsonObject *jsonConfig = jsonParseFile(file, &err);

    if (strcmp(config, "") == 0) {
        free((char *) file);
    }

    if (jsonErrorIsSet(&err)) {
        fprintf(stderr, "\nError loading configuration file.\n");
        fprintf(stderr, "%s\n", err.msg);
        fprintf(stderr, "Loading defaults\n\n");

        return;
    }

    parseInt(jsonConfig, "height", &conf.height, &err);
    parseInt(jsonConfig, "padding", &conf.padding, &err);
    parseColor(jsonConfig, "background", conf.bg, &err);
    parseColor(jsonConfig, "foreground", conf.fg, &err);
    parseString(jsonConfig, "font", &conf.font, &err);

    parseInt(jsonConfig, "traypadding", &conf.trayPadding, &err);
    parseInt(jsonConfig, "trayiconsize", &conf.trayIconSize, &err);
    parseString(jsonConfig, "traybar", &conf.trayBar, &err);

    parseBlocks(jsonConfig, "left", LEFT, &err);
    parseBlocks(jsonConfig, "right", RIGHT, &err);

    jsonCleanup(jsonConfig);
}
