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

#define S(n, t, d, v) \
    .n = { \
        .name = #n, \
        .type = t, \
        .desc = d, \
        .def.t = v, \
        .val.t = v, \
    },

const char *typeStrings [] = {
    "int",
    "bool",
    "string",
    "color",
    "position"
};

struct Settings settings = {
    S(height, INT, "Height of the bar", 22)
    S(marginvert, INT, "Margin above or below the bar", 0)
    S(marginhoriz, INT, "Margin on the left and right of the bar", 0)
    S(radius, INT, "Radius of the curvature of the corners of the bar", 0)
    S(padding, INT, "Padding on both sides of each block", 5)
    S(background, COL, "Background color of the bar", ((color) {0, 0, 0, 0xFF}))
    S(foreground, COL, "Default text color", ((color) {0xFF, 0xFF, 0xFF, 0xFF}))
    S(font, STR, "Font name and size", 0)
    S(shortlabels, BOOL, "Whether a block's label should render in short mode or not", 1)
    S(position, STR, "Position of the bar on the screen (\"top\" or \"bottom\")", "top")
    S(divwidth, INT, "Divider width", 1)
    S(divheight, INT, "Divider height", -1)
    S(divvertmargin, INT, "Margin above and below dividers", 4)
    S(divcolor, COL, "Divider color", ((color) {0x33, 0x33, 0x33, 0xFF}))
    S(traydiv, BOOL, "Whether a divider is drawn next to the tray or not", 1)
    S(traypadding, INT, "Padding to the right of each tray icon", 2)
    S(trayiconsize, INT, "Width and height of each tray icon", 18)
    S(traybar, STR, "Name of the output that the tray appears on", 0)
    S(trayside, POS, "Side that the tray appears on the bar (\"left\" or \"right\")", RIGHT)
};

#undef S

int settingCount;

int setSetting(struct Setting *setting, union Value val) {
    if (setting->type == INT || setting->type == BOOL) {
        if (setting == &settings.divvertmargin) {
            settings.divheight.val.INT = -1;
        }
        setting->val.INT = val.INT;
    } else if (setting->type == POS) {
        if (val.POS < LEFT || val.POS > CENTER) {
            return 1;
        }

        if (setting == &settings.trayside && val.POS == CENTER) {
            return 1;
        }

        setting->val.POS = val.POS;
    } else if (setting->type == STR) {
        if (!val.STR) {
            return 1;
        }

        if (setting == &settings.position) {
            if (strcmp(val.STR, "top") && strcmp(val.STR, "bottom")) {
                return 1;
            }
        }

        if (setting->val.STR) {
            free(setting->val.STR);
        }

        setting->val.STR = malloc(strlen(val.STR) + 1);
        strcpy(setting->val.STR, val.STR);
    } else if (setting->type == COL) {
        memcpy(setting->val.COL, val.COL, sizeof(color));
    }

    return 0;
}

static void initSettings() {
    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];

        if (setting->type == STR) {
            setting->val.STR = 0;
            setSetting(setting, setting->def);
        }
    }
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
    settingCount = sizeof(settings) / sizeof(struct Setting);
    initSettings();

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

    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];
        union Value val = setting->val;

        if (jsonGetPairIndex(jsonConfig, setting->name) == -1) {
            continue;
        }

        switch (setting->type) {
        case INT:
            parseInt(jsonConfig, setting->name, &(val.INT), &err);
            ERR(&err);
            break;
        case BOOL:
            parseBool(jsonConfig, setting->name, &(val.BOOL), &err);
            ERR(&err);
            break;
        case STR:
            parseString(jsonConfig, setting->name, &(val.STR), &err);
            ERR(&err);
            break;
        case COL:
            parseColorJson(jsonConfig, setting->name, val.COL, &err);
            ERR(&err);
            break;
        case POS:
            {
                char *str = 0;
                parseString(jsonConfig, setting->name, &str, &err);
                val.POS = -1;
                ERR(&err);
                if (str) {
                    if (strcmp(str, "left") == 0) {
                        val.POS = LEFT;
                    } else if (strcmp(str, "right") == 0) {
                        val.POS = RIGHT;
                    } else if (strcmp(str, "center") == 0) {
                        val.POS = CENTER;
                    }
                    free(str);
                }
            }
        }

        if (setSetting(setting, val)) {
           fprintf(stderr, "Invalid value for setting \"%s\"\n", setting->name);
        }
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

#define STR(name, var, p) \
    if (var) { \
        jsonAddString(name, var, p, &err); \
        ERR_; \
    } else if (explicit) { \
        jsonAddString(name, "", p, &err); \
            ERR_; \
    }

#define NUM(name, var) \
    if (blk->var || explicit) { \
        jsonAddNumber(name, blk->var, jblk, &err); \
        ERR_; \
    }

char *configSave(FILE *file, int explicit) {
    JsonObject *jo = jsonCreateBaseObject();
    JsonError err;

    jsonErrorInit(&err);

    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];

        switch (setting->type) {
        case INT:
            if (setting->val.INT != setting->def.INT || explicit) {
                jsonAddNumber(setting->name, setting->val.INT, jo, &err);
                ERR_;
            }
            break;
        case BOOL:
            if (setting->val.BOOL != setting->def.BOOL || explicit) {
                jsonAddBoolNull(setting->name,
                                setting->val.BOOL ? JSON_TRUE : JSON_FALSE,
                                jo, &err);
                ERR_;
            }
            break;
        case STR:
            if ((setting->val.STR && setting->def.STR &&
                    strcmp(setting->val.STR, setting->def.STR)) ||
                    !setting->def.STR != !setting->val.STR || explicit) {
                jsonAddString(setting->name, setting->val.STR, jo, &err);
                ERR_;
            } else if (explicit) {
                jsonAddString(setting->name, "", jo, &err);
            }
            break;
        case COL:
            if (memcmp(setting->val.COL, setting->def.COL, sizeof(color))
                    || explicit) {
                char col [10];
                stringifyColor(setting->val.COL, col);
                jsonAddString(setting->name, col, jo, &err);
            }
            break;
        case POS:
            if (setting->val.POS != setting->def.POS || explicit) {
                char *str = "";
                switch (setting->val.POS) {
                case LEFT:
                    str = "left";
                    break;
                case RIGHT:
                    str = "right";
                    break;
                case CENTER:
                    str = "center";
                    break;
                case SIDES:
                    break;
                }
                jsonAddString(setting->name, str, jo, &err);
            }
            break;
        }
    }

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

        NUM("interval", interval);
        NUM("padding", padding);
        NUM("padding-left", padLeft);
        NUM("padding-right", padRight);

        if (blk->nodiv || explicit) {
            jsonAddBoolNull("nodiv", blk->nodiv ? JSON_TRUE : JSON_FALSE, jblk,
                    &err); ERR_;
        }
    }

    jsonWriteObject(file, jo, 4);

    jsonCleanup(jo);

    return 0;
}
