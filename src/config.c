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

static struct Config defaultConf = {
    .height = 22,
    .marginV = 0,
    .marginH = 0,
    .radius = 0,
    .padding = 5,
    .bg = {0, 0, 0, 0xFF},
    .fg = {0xFF, 0xFF, 0xFF, 0xFF},
    .font = 0,
    .shortLabels = 1,
    .top = 1,

    .divWidth = 1,
    .divHeight = -1,
    .divVertMarg = 4,
    .divCol = {0x33, 0x33, 0x33, 0xFF},
    .trayDiv = 1,

    .trayPadding = 2,
    .trayIconSize = 18,
    .trayBar = 0,
    .traySide = RIGHT,
};

static void loadDefaults() {
    memcpy(&conf, &defaultConf, sizeof(struct Config));
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
        parseInt(entry, "padding-left", &(blk->padLeft), err); ERR(err)
        parseInt(entry, "padding-right", &(blk->padRight), err); ERR(err)
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

    parseInt(jsonConfig, "divwidth", &conf.divWidth, &err); ERR(&err);
    parseInt(jsonConfig, "divheight", &conf.divHeight, &err); ERR(&err);
    parseInt(jsonConfig, "divvertmargin", &conf.divVertMarg, &err); ERR(&err);
    parseColorJson(jsonConfig, "divcolor", conf.divCol, &err); ERR(&err);
    parseBool(jsonConfig, "traydiv", &conf.trayDiv, &err); ERR(&err);

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
    parseBlocks(jsonConfig, "center", CENTER, &err); ERR(&err);
    parseBlocks(jsonConfig, "right", RIGHT, &err); ERR(&err);
}

void configCleanup(JsonObject *jsonConfig) {
    jsonCleanup(jsonConfig);
}

#define ERR_ \
    if (jsonErrorIsSet(&err)) { \
        char *out = malloc(strlen(err.msg) + 1); \
        strcpy(out, err.msg); \
        jsonErrorCleanup(&err); \
        return out; \
    }

#define NUM(name, var) \
    if (conf.var != defaultConf.var || explicit) { \
        jsonAddNumber(name, conf.var, jo, &err); \
        ERR_; \
    }

#define BOOL(name, var) \
    if (conf.var != defaultConf.var || explicit) { \
        jsonAddBoolNull(name, conf.var ? JSON_TRUE : JSON_FALSE, jo, &err); \
        ERR_; \
    }

#define STR(name, var, p) \
    if (var) { \
        jsonAddString(name, var, p, &err); \
        ERR_; \
    } else if (explicit) { \
        jsonAddString(name, "", p, &err); \
            ERR_; \
    }

#define BNUM(name, var) \
    if (blk->var || explicit) { \
        jsonAddNumber(name, blk->var, jblk, &err); \
        ERR_; \
    }

char *configSave(FILE *file, int explicit) {
    JsonObject *jo = jsonCreateBaseObject();
    JsonError err;

    jsonErrorInit(&err);

    char bg [10];
    char fg [10];
    stringifyColor(conf.bg, bg);
    stringifyColor(conf.fg, fg);

    NUM("height", height);
    NUM("margin-vert", marginV);
    NUM("margin-horiz", marginH);
    NUM("radius", radius);
    NUM("padding", padding);

    if (memcmp(conf.bg, defaultConf.bg, sizeof(color) != 0) || explicit) {
        jsonAddString("background", bg, jo, &err);
        ERR_;
    }

    if (memcmp(conf.fg, defaultConf.fg, sizeof(color) != 0) || explicit) {
        jsonAddString("foreground", fg, jo, &err);
        ERR_;
    }

    STR("font", conf.font, jo);

    BOOL("shortlabels", shortLabels);

    if (conf.top != defaultConf.top || explicit) {
        jsonAddString("position", conf.top ? "top" : "bottom", jo, &err);
        ERR_;
    }

    NUM("traypadding", trayPadding);
    NUM("trayiconsize", trayIconSize);
    STR("traybar", conf.trayBar, jo);

    JsonArray *bArr [SIDES];
    bArr[LEFT] = jsonAddArray("left", jo, &err);
    ERR_;
    bArr[CENTER] = jsonAddArray("center", jo, &err);
    ERR_;
    bArr[RIGHT] = jsonAddArray("right", jo, &err);
    ERR_;

    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];

        if (!blk->id) {
            continue;
        }

        JsonObject *jblk = jsonAddObject(0, bArr[blk->pos], &err);
        ERR_;

        if (blk->mode != LEGACY || explicit) {
            jsonAddString("mode", blk->mode == LEGACY ? "legacy" : "subblocks",
                    jblk, &err);
            ERR_;
        }

        if (blk->eachmon || explicit) {
            jsonAddBoolNull("eachmon", blk->eachmon ? JSON_TRUE : JSON_FALSE,
                    jblk, &err);
            ERR_;
        }

        STR("label", blk->label, jblk);
        STR("exec", blk->exec, jblk);

        BNUM("interval", interval);
        BNUM("padding", padding);
        BNUM("padding-left", padLeft);
        BNUM("padding-right", padRight);

        if (blk->nodiv || explicit) {
            jsonAddBoolNull("nodiv", blk->nodiv ? JSON_TRUE : JSON_FALSE, jblk,
                    &err); ERR_;
        }
    }

    jsonWriteObject(file, jo, 4);

    jsonCleanup(jo);

    return 0;
}
