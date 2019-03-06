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
#include "version.h"
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Module *modules;
int moduleCount;

static int inConfig = 1;

struct Module *loadModule(char *path, FILE *out, FILE *errout) {
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
            fprintf(errout, "Module \"%s\" failed to initialize\n", path);
            fprintf(errout, "Module with name \"%s\" already loaded\n",
                    m->data.name);
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

    fprintf(out, "Loaded \"%s\" module (%s)\n", m->data.name, path);
    return m;
}

static void unload(struct Module *mod) {
    void (*unloadFunc)() = moduleGetFunction(mod->data.name, "unload");
    if (unloadFunc) {
        unloadFunc();
    }

    dlclose(mod->dl);
    mod->dl = 0;

    free(mod->path);
}

int unloadModule(char *name) {
    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];
        if (!mod->dl) {
            continue;
        }

        if (strcmp(name, mod->data.name) == 0) {
            unload(mod);
            return 0;
        }
    }

    return 1;
}

void initModules() {
    char *libdir = LIBDIR "/blockbar/";
    DIR *dir = opendir(libdir);

    if (!dir) {
        fprintf(stderr, "Module directory (%s) does not exist\n", libdir);
        return;
    }

    inConfig = 0;

    struct dirent *dp;

    while ((dp = readdir(dir))) {
        if (dp->d_type != DT_REG && dp->d_type != DT_LNK) {
            continue;
        }

        char *file = malloc(strlen(libdir) + strlen(dp->d_name) + 1);
        strcpy(file, libdir);
        strcpy(file + strlen(libdir), dp->d_name);
        loadModule(file, stdout, stderr);
        free(file);
    }

    closedir(dir);

    inConfig = 1;
}

void cleanupModules() {
    for (int i = 0; i < moduleCount; i++) {
        if (modules[i].dl) {
            unload(&modules[i]);
        }
    }

    free(modules);
}

void (*moduleGetFunction(char *modName, char *funcName)) {
    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (!mod->dl) {
            continue;
        }

        if (strcmp(modName, mod->data.name) == 0) {
            dlerror();

            void (*func) = dlsym(mod->dl, funcName);
            char *err = dlerror();

            if (err) {
                return 0;
            } else {
                return func;
            }
        }
    }

    fprintf(stderr, "Module \"%s\" does not exist\n", modName);

    return 0;
}

int moduleHasFlag(char *modName, long mflag) {
    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (!mod->dl) {
            continue;
        }

        if (strcmp(modName, mod->data.name) == 0) {
            return mod->data.flags & mflag;
        }
    }

    return 0;
}

int moduleRegisterBlock(struct Block *blk, char *new, FILE *err) {
    if (new) {
        struct Module *mod = 0;

        for (int i = 0; i < moduleCount; i++) {
            struct Module *_mod = &modules[i];

            if (strcmp(_mod->data.name, new) == 0) {
                mod = _mod;
                break;
            }
        }

        if (!mod) {
            fprintf(err, "Module \"%s\" not found\n", new);
            return 1;
        }

        if (mod->data.type != BLOCK) {
            fprintf(err, "Module \"%s\" not a block module\n", new);
            return 1;
        }
    }

    void (*rm)(struct Block *) =
        moduleGetFunction(blk->properties.module.val.STR, "blockRemove");

    if (rm) {
        rm(blk);
    }

    if (!new) {
        return 0;
    }

    setSetting(&blk->properties.module, (union Value) new);

    void (*add)(struct Block *) =
        moduleGetFunction(blk->properties.module.val.STR, "blockAdd");

    if (add) {
        add(blk);
    }

    return 0;
}
