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
#include "version.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Module *modules;
int moduleCount;

void loadModule(char *path) {
    char *err = dlerror();

    modules = realloc(modules, sizeof(struct Module) * ++moduleCount);

    struct Module *m = &modules[moduleCount - 1];
    memset(&(m->data), 0, sizeof(struct ModuleData));

    m->dl = dlopen(path, RTLD_NOW);
    err = dlerror();

    if (err || !m->dl) {
        fprintf(stderr, "%s\n", err);
        moduleCount--;
        return;
    }

    int (*init)(struct ModuleData *) = dlsym(m->dl, "init");
    err = dlerror();

    if (err) {
        fprintf(stderr, "Error loading module \"%s\":\n%s\n", path, err);
        dlclose(m->dl);
        moduleCount--;
        return;
    }

    if (!init) {
        fprintf(stderr, "Module \"%s\" has no init function\n", path);
        dlclose(m->dl);
        moduleCount--;
        return;
    }

    int ret = init(&m->data);

    if (ret != 0) {
        fprintf(stderr, "Module \"%s\" failed to initialize (%d)\n", path, ret);
        dlclose(m->dl);
        moduleCount--;
        return;
    }

    int *version = dlsym(m->dl, "API_VERSION");
    err = dlerror();

    if (err) {
        fprintf(stderr, "%s\n", err);
        dlclose(m->dl);
        moduleCount--;
        return;
    }

    if (*version != API_VERSION) {
        fprintf(stderr, "\"%s\" module is out of date\n", m->data.name);
        dlclose(m->dl);
        moduleCount--;
        return;
    }

    m->path = malloc(strlen(path) + 1);
    strcpy(m->path, path);

    printf("Loaded \"%s\" module (%s)\n", m->data.name, path);
}

void cleanupModules() {
    for (int i = 0; i < moduleCount; i++) {
        dlclose(modules[i].dl);
        free(modules[i].path);
    }

    free(modules);
}

void (*moduleGetFunction(char *modName, char *funcName)) {
    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];
        if (strcmp(modName, mod->data.name) == 0) {
            dlerror();

            void (*func) = dlsym(mod->dl, funcName);
            char *err = dlerror();

            if (err) {
                fprintf(stderr, "%s\n", err);

                return 0;
            } else {
                return func;
            }
        }
    }

    fprintf(stderr, "Module \"%s\" does not exist\n", modName);

    return 0;
}
