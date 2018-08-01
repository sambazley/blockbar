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
#include "util.h"
#include "window.h"
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERR(err) \
    if (jsonErrorIsSet(err)) { \
        fprintf(stderr, "Error parsing JSON file:\n%s\n", (err)->msg); \
        jsonErrorCleanup(err); \
        jsonErrorInit(err); \
    }

int blockCount;
struct Block *blocks;

static void loadDefaults() {
    conf.height = 22;
    conf.marginV = 0;
    conf.marginH = 0;
    conf.radius = 0;
    conf.padding = 5;
    conf.trayPadding = 2;
    conf.trayIconSize = 18;
    conf.trayBar = 0;
    conf.traySide = RIGHT;
    conf.shortLabels = 1;
    conf.top = 1;

    conf.bg[0] = 0;
    conf.bg[1] = 0;
    conf.bg[2] = 0;
    conf.bg[3] = 0xFF;

    conf.fg[0] = 0xFF;
    conf.fg[1] = 0xFF;
    conf.fg[2] = 0xFF;
    conf.fg[3] = 0xFF;

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

    jsonGetArray(jo, key, &arr, err); ERR(err);

    for (int i = 0; i < arr->used; i++) {
        JsonObject *entry = arr->vals[i];
        if (jsonGetType(entry) != JSON_OBJECT) {
            printf("Skipping invalid block entry\n");
            continue;
        }

        int eachmon = 0;
        parseBool(entry, "eachmon", &eachmon, err); ERR(err)

        struct Block *blk = createBlock(eachmon);

        char *mode = 0;
        parseString(entry, "mode", &mode, err); ERR(err)
        parseString(entry, "label", &(blk->label), err); ERR(err)
        parseString(entry, "exec", &(blk->exec), err); ERR(err)
        parseInt(entry, "interval", &(blk->interval), err); ERR(err)
        parseInt(entry, "padding", &(blk->padding), err); ERR(err)
        parseInt(entry, "padding-inside", &(blk->padIn), err); ERR(err)
        parseInt(entry, "padding-outside", &(blk->padOut), err); ERR(err)
        parseBool(entry, "nodiv", &(blk->nodiv), err); ERR(err)

        blk->pos = pos;

        blk->mode = LEGACY;
        if (mode) {
            if (strcmp(mode, "subblocks") == 0) {
                blk->mode = SUBBLOCK;
            }
            free(mode);
        }
    }
}

JsonObject *configInit(const char *config) {
    loadDefaults();

    const char *file;

    if (strcmp(config, "") == 0) {
        file = defaultConfigFile();
    } else {
        file = config;
    }

    printf("Using config %s\n", file);

    JsonError err;
    jsonErrorInit(&err);

    JsonObject *jsonConfig = jsonParseFile(file, &err);

    if (strcmp(config, "") == 0) {
        free((char *) file);
    }

    if (jsonConfig == 0 || jsonErrorIsSet(&err)) {
        fprintf(stderr, "\nError loading configuration file.\n");
        fprintf(stderr, "%s\n", err.msg);
        fprintf(stderr, "Loading defaults\n\n");

        return 0;
    }

    return jsonConfig;
}

void configParseGeneral(JsonObject *jsonConfig) {
    JsonError err;
    jsonErrorInit(&err);

    char *position = 0;

    parseInt(jsonConfig, "height", &conf.height, &err); ERR(&err);
    parseInt(jsonConfig, "margin-vert", &conf.marginV, &err); ERR(&err);
    parseInt(jsonConfig, "margin-horiz", &conf.marginH, &err); ERR(&err);
    parseInt(jsonConfig, "radius", &conf.radius, &err); ERR(&err);
    parseInt(jsonConfig, "padding", &conf.padding, &err); ERR(&err);
    parseColorJson(jsonConfig, "background", conf.bg, &err); ERR(&err);
    parseColorJson(jsonConfig, "foreground", conf.fg, &err); ERR(&err);
    parseString(jsonConfig, "font", &conf.font, &err); ERR(&err);
    parseBool(jsonConfig, "shortlabels", &conf.shortLabels, &err); ERR(&err);
    parseString(jsonConfig, "position", &position, &err); ERR(&err);

    parseInt(jsonConfig, "traypadding", &conf.trayPadding, &err); ERR(&err);
    parseInt(jsonConfig, "trayiconsize", &conf.trayIconSize, &err); ERR(&err);
    parseString(jsonConfig, "traybar", &conf.trayBar, &err); ERR(&err);

    char *trayside = 0;
    parseString(jsonConfig, "trayside", &trayside, &err); ERR(&err);

    conf.traySide = RIGHT;
    if (trayside) {
        if (strcmp(trayside, "left") == 0) {
            conf.traySide = LEFT;
        }
        free(trayside);
    }

    if (position) {
        if (strcmp(position, "bottom") == 0) {
            conf.top = 0;
        }
        free(position);
    }
}

void configParseBlocks(JsonObject *jsonConfig) {
    JsonError err;
    jsonErrorInit(&err);

    parseBlocks(jsonConfig, "left", LEFT, &err); ERR(&err);
    parseBlocks(jsonConfig, "right", RIGHT, &err); ERR(&err);
}

void configCleanup(JsonObject *jsonConfig) {
    jsonCleanup(jsonConfig);
}
