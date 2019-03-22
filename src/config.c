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
#include "modules.h"
#include "tray.h"
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

const char *typeStrings [] = {
    "int",
    "bool",
    "string",
    "color",
    "position",
};

#define S(n, t, d, ...) \
    .n = { \
        .name = #n, \
        .type = t, \
        .desc = d, \
        .def.t = __VA_ARGS__, \
        .val.t = __VA_ARGS__, \
    },

struct Settings settings = {
    S(height, INT, "Height of the bar", 22)
    S(marginvert, INT, "Margin above or below the bar", 0)
    S(marginhoriz, INT, "Margin on the left and right of the bar", 0)
    S(xoffset, INT, "Horizontal offset of the bar", 0)
    S(radius, INT, "Radius of the curvature of the corners of the bar", 0)
    S(padding, INT, "Padding on both sides of each block", 5)
    S(background, COL, "Background color of the bar", {0, 0, 0, 0xFF})
    S(foreground, COL, "Default text color", {0xFF, 0xFF, 0xFF, 0xFF})
    S(font, STR, "Font name and size", 0)
    S(position, STR, "Position of the bar on the screen (\"top\" or \"bottom\")", "top")
    S(divwidth, INT, "Divider width", 1)
    S(divheight, INT, "Divider height", -1)
    S(divvertmargin, INT, "Margin above and below dividers", 4)
    S(divcolor, COL, "Divider color", {0x33, 0x33, 0x33, 0xFF})
    S(borderwidth, INT, "Width of the border", 0)
    S(bordercolor, COL, "Color of the border", {0, 0, 0, 0})
    S(traydiv, BOOL, "Whether a divider is drawn next to the tray", 1)
    S(traypadding, INT, "Padding to the right of each tray icon", 2)
    S(trayiconsize, INT, "Width and height of each tray icon", 18)
    S(traybar, STR, "Name of the output that the tray appears on", 0)
    S(trayside, POS, "Position of the tray on the bar (\"left\" or \"right\")", RIGHT)
};

struct Properties defProperties = {
    S(module, STR, "The name of the module that handles the block", "legacy")
    S(exec, STR, "Command to be executed", "")
    S(pos, POS, "Position of the block", LEFT)
    S(interval, INT, "Time between each execution of the block's script", 0)
    S(padding, INT, "Additional padding on both sides of the block", 0)
    S(paddingleft, INT, "Additional padding on the left of the block", 0)
    S(paddingright, INT, "Additonal padding on the right of the block", 0)
    S(nodiv, BOOL, "Disables the divider to the right of the block", 0)
};

#undef S

int settingCount;
int propertyCount;

int setSetting(struct Setting *setting, union Value val) {
    switch (setting->type) {
    case INT:
    case BOOL:
        if (setting == &settings.divvertmargin) {
            settings.divheight.val.INT = -1;
        }
        setting->val.INT = val.INT;
        break;
    case POS:
        if (val.POS < LEFT || val.POS > CENTER) {
            return 1;
        }

        if (setting == &settings.trayside && val.POS == CENTER) {
            return 1;
        }

        setting->val.POS = val.POS;
        break;
    case STR:
        if (!val.STR) {
            return 1;
        }

        if (setting == &settings.position) {
            if (strcmp(val.STR, "top") && strcmp(val.STR, "bottom")) {
                return 1;
            }
        } else if (setting == &settings.traybar) {
            int traybar = -1;
            for (int i = 0; i < barCount; i++) {
                struct Bar bar = bars[i];
                if (strcmp(bar.output, val.STR) == 0) {
                    traybar = i;
                    break;
                }
            }

            if (traybar != -1) {
                trayInit(traybar);
                reparentIcons();
            } else {
                return 1;
            }
        }

        if (setting->val.STR) {
            free(setting->val.STR);
        }

        setting->val.STR = malloc(strlen(val.STR) + 1);
        strcpy(setting->val.STR, val.STR);
        break;
    case COL:
        memcpy(setting->val.COL, val.COL, sizeof(color));
        break;
    }

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (!mod->dl) {
            continue;
        }

        void (*func)(struct Setting *) =
            moduleGetFunction(mod, "settingUpdate");

        if (func) {
            func(setting);
        }
    }

    return 0;
}

static void initSettings() {
    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];

        memset(&(setting->val), 0, sizeof(setting->val));
        setSetting(setting, setting->def);
    }
}

static int parseSetting(JsonObject *jo, struct Setting *setting,
        union Value *val, JsonError *err) {
    switch (setting->type) {
    case INT:
        jsonGetInt(jo, setting->name, &(val->INT), err);
        if (jsonErrorIsSet(err)) {
            return 1;
        }
        break;
    case BOOL:
        jsonGetBool(jo, setting->name, (unsigned *) &(val->BOOL), 0, err);
        if (jsonErrorIsSet(err)) {
            return 1;
        }
        break;
    case STR:
        jsonGetString(jo, setting->name, &(val->STR), err);
        if (jsonErrorIsSet(err)) {
            return 1;
        }
        break;
    case COL:
        {
            int e = blockbarParseColorJson(jo, setting->name, val->COL, err);
            if (e || jsonErrorIsSet(err)) {
                return 1;
            }
            break;
        }
    case POS:
        {
            char *str = 0;
            jsonGetString(jo, setting->name, &str, err);
            if (jsonErrorIsSet(err)) {
                return 1;
            }
            if (str) {
                if (strcmp(str, "left") == 0) {
                    val->POS = LEFT;
                } else if (strcmp(str, "right") == 0) {
                    val->POS = RIGHT;
                } else if (strcmp(str, "center") == 0) {
                    val->POS = CENTER;
                } else {
                    free(str);
                    return 1;
                }
            }
            break;
        }
    }

    return 0;
}

void cleanupSettings() {
    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];

        if (setting->type == STR) {
            if (setting->val.STR) {
                free(setting->val.STR);
            }
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
parseBlocks(JsonObject *jo, const char *key, enum Pos pos, JsonError *err) {
    JsonArray *arr;

    if (jsonGetPairIndex(jo, key) == -1) {
        return;
    }

    jsonGetArray(jo, key, &arr, err); ERR(err);

    for (unsigned int i = 0; i < arr->used; i++) {
        JsonObject *entry = arr->vals[i];
        if (jsonGetType(entry) != JSON_OBJECT) {
            fprintf(stderr, "Skipping invalid block entry\n");
            continue;
        }

        unsigned int eachmon = 0;
        if (jsonGetPairIndex(entry, "eachmon") != -1) {
            jsonGetBool(entry, "eachmon", &eachmon, 0, err);
            ERR(err);
        }

        struct Block *blk = createBlock(eachmon);

        for (int i = 0; i < propertyCount; i++) {
            struct Setting *property =
                &((struct Setting *) &(blk->properties))[i];

            union Value val;

            if (strcmp("pos", property->name) == 0) {
                continue;
            }

            if (jsonGetPairIndex(entry, property->name) == -1) {
                continue;
            }

            int e = parseSetting(entry, property, &val, err);

            if (!e && strcmp("module", property->name) == 0) {
                moduleRegisterBlock(blk, val.STR, stderr);
                continue;
            }

            if (e || setSetting(property, val)) {
                fprintf(stderr, "Invalid value for property \"%s\"\n",
                        property->name);

                if (jsonErrorIsSet(err)) {
                    jsonErrorCleanup(err);
                    jsonErrorInit(err);
                }
            }
        }

        blk->properties.pos.val.POS = pos;
    }
}

JsonObject *configInit(const char *config) {
    settingCount = sizeof(settings) / sizeof(struct Setting);
    propertyCount = sizeof(defProperties) / sizeof(struct Setting);
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

    int errSet = jsonErrorIsSet(&err);
    if (jsonConfig == 0 || errSet) {
        fprintf(stderr, "\nError loading configuration file.\n");

        if (errSet) {
            fprintf(stderr, "%s\n", err.msg);
            jsonErrorCleanup(&err);
        }

        if (jsonConfig) {
            jsonCleanup(jsonConfig);
        }

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
        union Value val;

        if (jsonGetPairIndex(jsonConfig, setting->name) == -1) {
            continue;
        }

        int e = parseSetting(jsonConfig, setting, &val, &err);

        if (e || setSetting(setting, val)) {
           fprintf(stderr, "Invalid value for setting \"%s\"\n", setting->name);

           if (jsonErrorIsSet(&err)) {
                jsonErrorCleanup(&err);
                jsonErrorInit(&err);
           }
        }
    }

    if (jsonGetPairIndex(jsonConfig, "modules") == -1) {
        return;
    }

    JsonArray *mods;
    jsonGetArray(jsonConfig, "modules", &mods, &err);
    if (jsonErrorIsSet(&err)) {
        fprintf(stderr, "Error parsing \"modules\" array\n%s\n", err.msg);
        jsonErrorCleanup(&err);
        jsonErrorInit(&err);

        return;
    }

    for (unsigned int i = 0; i < mods->used; i++) {
        JsonObject *obj = mods->vals[i];
        if (jsonGetType(obj) != JSON_OBJECT) {
            fprintf(stderr, "Skipping invalid module\n");
            continue;
        }

        char *path;
        jsonGetString(obj, "path", &path, &err);
        if (jsonErrorIsSet(&err)) {
            fprintf(stderr, "Error parsing module \"path\" string\n%s\n",
                    err.msg);
            jsonErrorCleanup(&err);
            jsonErrorInit(&err);

            continue;
        }

        int zindex = -1;
        if (jsonGetPairIndex(obj, "zindex") != -1) {
            jsonGetInt(obj, "zindex", &zindex, &err);
            if (jsonErrorIsSet(&err)) {
                fprintf(stderr, "Error parsing module \"zindex\" string\n%s\n",
                        err.msg);
                jsonErrorCleanup(&err);
                jsonErrorInit(&err);
            }
        }

        struct Module *mod = loadModule(path, zindex, stdout, stderr);

        if (mod == 0) {
            continue;
        }

        if (jsonGetPairIndex(obj, "settings") == -1) {
            continue;
        }

        JsonObject *settings;
        jsonGetObject(obj, "settings", &settings, &err);
        if (jsonErrorIsSet(&err)) {
            fprintf(stderr, "Error parsing module \"settings\" object\n%s\n",
                    err.msg);
            jsonErrorCleanup(&err);
            jsonErrorInit(&err);
        }

        for (int j = 0; j < mod->data.settingCount; j++) {
            struct Setting *setting = &mod->data.settings[i];

            union Value val;

            if (jsonGetPairIndex(settings, setting->name) == -1) {
                continue;
            }

            int e = parseSetting(settings, setting, &val, &err);

            if (e || setSetting(setting, val)) {
                fprintf(stderr, "Invalid value for setting \"%s\"\n",
                        setting->name);

                if (jsonErrorIsSet(&err)) {
                    jsonErrorCleanup(&err);
                    jsonErrorInit(&err);
                }
            }
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

int isSettingModified(struct Setting *setting) {
    switch (setting->type) {
    case INT:
        if (setting->val.INT != setting->def.INT) {
            return 1;
        }
        return 0;
    case BOOL:
        if (setting->val.BOOL != setting->def.BOOL) {
            return 1;
        }
        return 0;
    case STR:
        if ((setting->val.STR && setting->def.STR && strcmp(setting->val.STR, setting->def.STR)) ||
                (setting->val.STR == 0 && setting->def.STR && *setting->def.STR) ||
                (setting->def.STR == 0 && setting->val.STR && *setting->val.STR)) {
            return 1;
        }
        return 0;
    case COL:
        if (memcmp(setting->val.COL, setting->def.COL, sizeof(color))) {
            return 1;
        }
        return 0;
    case POS:
        if (setting->val.POS != setting->def.POS) {
            return 1;
        }
        return 0;
    }

    return 0;
}

static void addSetting(struct Setting *setting, int explicit,
        JsonObject *jo, JsonError *err) {
    if (!isSettingModified(setting) && !explicit) {
        return;
    }

    switch (setting->type) {
    case INT:
        jsonAddNumber(setting->name, setting->val.INT, jo, err);
        break;
    case BOOL:
        jsonAddBoolNull(setting->name,
                        setting->val.BOOL ? JSON_TRUE : JSON_FALSE,
                        jo, err);
        break;
    case STR:
    {
        char *str = 0;
        if (isSettingModified(setting)) {
            str = setting->val.STR;
        } else if (explicit) {
            str = setting->def.STR;
        }
        if (str == 0) {
            str = "";
        }
        jsonAddString(setting->name, str, jo, err);
    }
        break;
    case COL:
    {
        char col [10];
        blockbarStringifyColor(setting->val.COL, col);
        jsonAddString(setting->name, col, jo, err);
    }
        break;
    case POS:
    {
        switch (setting->val.POS) {
        case LEFT:
            jsonAddString(setting->name, "left", jo, err);
            break;
        case RIGHT:
            jsonAddString(setting->name, "right", jo, err);
            break;
        case CENTER:
            jsonAddString(setting->name, "center", jo, err);
            break;
        case SIDES:
            break;
        }
    }
        break;
    }
}

#define ERR_ \
    if (jsonErrorIsSet(&err)) { \
        char *out = malloc(strlen(err.msg) + 1); \
        strcpy(out, err.msg); \
        jsonErrorCleanup(&err); \
        return out; \
    }

char *configSave(FILE *file, int explicit) {
    JsonObject *jo = jsonCreateBaseObject();
    JsonError err;

    jsonErrorInit(&err);

    for (int i = 0; i < settingCount; i++) {
        struct Setting *setting = &((struct Setting *) &settings)[i];

        addSetting(setting, explicit, jo, &err);
        ERR_;
    }

    JsonArray *mods = jsonAddArray("modules", jo, &err);

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];
        if (!mod->dl) {
            continue;
        }

        int hasModifiedSettings = 0;

        for (int j = 0; j < mod->data.settingCount; j++) {
            if (isSettingModified(&mod->data.settings[j])) {
                hasModifiedSettings = 1;
                break;
            }
        }

        if (!mod->inConfig && !hasModifiedSettings) {
            continue;
        }

        JsonObject *modObj = jsonAddObject(0, mods, &err);

        jsonAddString("path", mod->path, modObj, &err);
        ERR_;

        if (mod->data.type == RENDER && mod->zindex != -1) {
            jsonAddNumber("zindex", mod->zindex, modObj, &err);
            ERR_;
        }

        JsonObject *settings = jsonAddObject("settings", modObj, &err);
        ERR_;

        for (int j = 0; j < mod->data.settingCount; j++) {
            addSetting(&mod->data.settings[j], explicit, settings, &err);
            ERR_;
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

        JsonObject *jblk = jsonAddObject(0, bArr[blk->properties.pos.val.POS],
                &err);
        ERR_;

        for (int j = 0; j < propertyCount; j++) {
            struct Setting *property =
                &((struct Setting *) &(blk->properties))[j];

            if (strcmp("pos", property->name) == 0) {
                continue;
            }

            addSetting(property, explicit, jblk, &err);

            if (jsonErrorIsSet(&err)) {
                ERR_;
            }
        }

        if (blk->eachmon || explicit) {
            jsonAddBoolNull("eachmon", blk->eachmon ? JSON_TRUE : JSON_FALSE,
                    jblk, &err);
            ERR_;
        }
    }

    jsonWriteObject(file, jo, 4);

    jsonCleanup(jo);

    return 0;
}
