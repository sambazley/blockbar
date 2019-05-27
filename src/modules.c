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

#include "modules.h"
#include "config.h"
#include "render.h"
#include "window.h"
#include "version.h"
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Module *modules;
int moduleCount;

static int inConfig = 1;

struct Module *loadModule(char *path, int zindex, FILE *out, FILE *errout) {
    char *err = dlerror();

    struct Module *m = 0;

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];
        if (mod->dl == 0) {
            m = mod;
            break;
        }
    }

    if (!m) {
        modules = realloc(modules, sizeof(struct Module) * ++moduleCount);
        m = &modules[moduleCount - 1];
    }

    memset(m, 0, sizeof(struct Module));
    memset(&(m->data), 0, sizeof(struct ModuleData));

    m->dl = dlopen(path, RTLD_NOW);
    err = dlerror();

    if (err || !m->dl) {
        fprintf(errout, "%s\n", err);
        moduleCount--;
        return 0;
    }

    int (*init)(struct ModuleData *) = dlsym(m->dl, "init");
    err = dlerror();

    if (err) {
        fprintf(errout, "Error loading module \"%s\":\n%s\n", path, err);
        dlclose(m->dl);
        moduleCount--;
        return 0;
    }

    if (!init) {
        fprintf(errout, "Module \"%s\" has no init function\n", path);
        dlclose(m->dl);
        moduleCount--;
        return 0;
    }

    int ret = init(&m->data);

    if (ret != 0) {
        fprintf(errout, "Module \"%s\" failed to initialize (%d)\n", path, ret);
        dlclose(m->dl);
        moduleCount--;
        return 0;
    }

    if (!m->data.name) {
        fprintf(errout, "Module \"%s\" has no name\n", path);
        dlclose(m->dl);
        moduleCount--;
        return 0;
    }

    for (int i = 0; i < moduleCount; i++) {
        if (!modules[i].dl || &modules[i] == m) {
            continue;
        }

        if (strcmp(m->data.name, modules[i].data.name) == 0) {
            if (inConfig) {
                fprintf(errout, "Module \"%s\" failed to initialize\n", path);
                fprintf(errout, "Module with name \"%s\" already loaded\n",
                        m->data.name);
            }
            dlclose(m->dl);
            moduleCount--;
            return 0;
        }
    }

    int *version = dlsym(m->dl, "API_VERSION");
    err = dlerror();

    if (err) {
        fprintf(errout, "%s\n", err);
        dlclose(m->dl);
        moduleCount--;
        return 0;
    }

    if (*version != API_VERSION) {
        fprintf(errout, "\"%s\" module is out of date\n", m->data.name);
        dlclose(m->dl);
        moduleCount--;
        return 0;
    }

    m->path = malloc(strlen(path) + 1);
    strcpy(m->path, path);

    m->inConfig = inConfig;

    if (m->data.type == BLOCK) {
        for (int i = 0; i < blockCount; i++) {
            struct Block *blk = &blocks[i];

            if (!blk->id) {
                continue;
            }

            if (strcmp(blk->properties.module.val.STR, m->data.name) == 0) {
                moduleRegisterBlock(blk, m->data.name, errout);

                redrawBlock(blk);
            }
        }
    } else if (m->data.type == RENDER) {
        if (zindex == 0) {
            zindex = -1;
        }
        m->zindex = zindex;

        for (int i = 0; i < moduleCount; i++) {
            struct Module *mod = &modules[i];

            if (mod == m) {
                continue;
            }

            if (m->zindex < 0 && m->zindex >= mod->zindex) {
                mod->zindex--;
            } else if (m->zindex > 0 && m->zindex <= mod->zindex) {
                mod->zindex++;
            }
        }
        m->sfc = malloc(sizeof(cairo_surface_t *) * barCount);
        memset(m->sfc, 0, sizeof(cairo_surface_t *) * barCount);

        resizeModule(m);

        if (m->data.interval != 0) {
            updateTickInterval();
        }
    }

    fprintf(out, "Loaded \"%s\" module (%s)\n", m->data.name, path);
    return m;
}

void unloadModule(struct Module *mod) {
    void (*unloadFunc)() = moduleGetFunction(mod, "unload");
    if (unloadFunc) {
        unloadFunc();
    }

    dlclose(mod->dl);
    mod->dl = 0;

    free(mod->path);

    if (mod->data.type == RENDER) {
        if (mod->sfc[0]) {
            for (int bar = 0; bar < barCount; bar++) {
                cairo_surface_destroy(mod->sfc[bar]);
            }
        }
        free(mod->sfc);
    }

    for (int i = 0; i < moduleCount; i++) {
        struct Module *_mod = &modules[i];

        if (mod->zindex < 0 && _mod->zindex < mod->zindex) {
            _mod->zindex++;
        } else if (mod->zindex > 0 && _mod->zindex > mod->zindex) {
            _mod->zindex--;
        }
    }

    if (mod->data.interval != 0) {
        updateTickInterval();
    }
}

void resizeModule(struct Module *mod) {
    if (mod->sfc[0]) {
        for (int bar = 0; bar < barCount; bar++) {
            cairo_surface_destroy(mod->sfc[bar]);
        }
    }

    for (int bar = 0; bar < barCount; bar++) {
        if (bars[bar].sfc[0]) {
            mod->sfc[bar] = cairo_surface_create_similar_image(
                    bars[bar].sfc[0], CAIRO_FORMAT_ARGB32,
                    bars[bar].width, bars[bar].height);
        }
    }
}

static void loadModulesInDir(char *path) {
    DIR *dir = opendir(path);
    struct dirent *dp;

    if (!dir) {
        return;
    }

    while ((dp = readdir(dir))) {
        if (dp->d_type != DT_REG && dp->d_type != DT_LNK) {
            continue;
        }

        int fileLen = strlen(path) + strlen(dp->d_name) + 2;
        char *file = malloc(fileLen);
        memset(file, 0, fileLen);

        strcat(file, path);
        strcat(file, "/");
        strcat(file, dp->d_name);

        loadModule(file, -1, stdout, stderr);
        free(file);
    }

    closedir(dir);
}

void initModules() {
#ifndef MODDIRS
#   define MODDIRS
#endif

    char *dirs [] = {
        MODDIRS "",
        "/usr/local/lib/blockbar/modules",
        "/usr/lib/blockbar/modules",
    };
    int dirCount = sizeof(dirs) / sizeof(char *);

    inConfig = 0;

    for (int i = 0; i < dirCount; i++) {
        if (dirs[i][0] != '\0') {
            loadModulesInDir(dirs[i]);
        }
    }

    inConfig = 1;
}

void cleanupModules() {
    for (int i = 0; i < moduleCount; i++) {
        if (modules[i].dl) {
            unloadModule(&modules[i]);
        }
    }

    free(modules);
}

struct Module *getModuleByName(char *name) {
    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (!mod->dl) {
            continue;
        }

        if (strcmp(mod->data.name, name) == 0) {
            return mod;
        }
    }

    return 0;
}

void (*moduleGetFunction(struct Module *mod, char *funcName)) {
    dlerror();

    void (*func) = dlsym(mod->dl, funcName);
    char *err = dlerror();

    if (err) {
        return 0;
    } else {
        return func;
    }
}

int moduleRegisterBlock(struct Block *blk, char *new, FILE *err) {
    struct Module *oldMod = getModuleByName(blk->properties.module.val.STR);
    struct Module *newMod = 0;

    if (new) {
        newMod = getModuleByName(new);

        if (!newMod) {
            fprintf(err, "Module \"%s\" not found\n", new);
            return 1;
        }

        if (newMod->data.type != BLOCK) {
            fprintf(err, "Module \"%s\" not a block module\n", new);
            return 1;
        }
    }

    if (oldMod && oldMod != newMod) {
        void (*rm)(struct Block *) = moduleGetFunction(oldMod, "blockRemove");

        if (rm) {
            rm(blk);
        }
    }

    if (new) {
        setSetting(&blk->properties.module, (union Value) new);

        void (*add)(struct Block *) = moduleGetFunction(newMod, "blockAdd");

        if (add) {
            add(blk);
        }
    }

    return 0;
}
