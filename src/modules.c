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
#include "task.h"
#include "window.h"
#include "version.h"
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct module *modules;
int module_count;

int module_redraw_dirty;

static int in_config = 1;

static void module_task_exec(int id)
{
	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];

		if (!mod->dl || mod->data.type != RENDER ||
				mod->data.interval == 0) {
			continue;
		}

		if (mod->task == id) {
			for (int i = 0; i < bar_count; i++) {
				redraw_module(mod, i);
			}
			module_redraw_dirty = 1;
		}
	}
}

struct module *load_module(char *path, int zindex, FILE *out, FILE *errout)
{
	char *err = dlerror();

	struct module *m = 0;

	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];
		if (mod->dl == 0) {
			m = mod;
			break;
		}
	}

	if (!m) {
		modules = realloc(modules,
				sizeof(struct module) * ++module_count);
		m = &modules[module_count - 1];
	}

	memset(m, 0, sizeof(struct module));
	memset(&(m->data), 0, sizeof(struct module_data));

	m->dl = dlopen(path, RTLD_NOW);
	err = dlerror();

	if (err || !m->dl) {
		fprintf(errout, "%s\n", err);
		module_count--;
		return 0;
	}

	int *version = dlsym(m->dl, "API_VERSION");
	err = dlerror();

	if (err) {
		fprintf(errout, "%s\n", err);
		dlclose(m->dl);
		module_count--;
		return 0;
	}

	if (*version != API_VERSION) {
		fprintf(errout, "Module \"%s\" is out of date\n", path);
		dlclose(m->dl);
		module_count--;
		return 0;
	}

	int (*init)(struct module_data *) = dlsym(m->dl, "init");
	err = dlerror();

	if (err) {
		fprintf(errout, "Error loading module \"%s\":\n%s\n",
				path, err);
		dlclose(m->dl);
		module_count--;
		return 0;
	}

	if (!init) {
		fprintf(errout, "Module \"%s\" has no init function\n", path);
		dlclose(m->dl);
		module_count--;
		return 0;
	}

	int ret = init(&m->data);

	if (ret != 0) {
		fprintf(errout, "Module \"%s\" failed to initialize (%d)\n",
				path, ret);
		dlclose(m->dl);
		module_count--;
		return 0;
	}

	if (!m->data.name) {
		fprintf(errout, "Module \"%s\" has no name\n", path);
		dlclose(m->dl);
		module_count--;
		return 0;
	}

	for (int i = 0; i < module_count; i++) {
		if (!modules[i].dl || &modules[i] == m) {
			continue;
		}

		if (strcmp(m->data.name, modules[i].data.name) == 0) {
			if (in_config) {
				fprintf(errout, "Module \"%s\" failed to initialize\n", path);
				fprintf(errout, "Module with name \"%s\" already loaded\n",
						m->data.name);
			}
			dlclose(m->dl);
			module_count--;
			return 0;
		}
	}

	m->path = malloc(strlen(path) + 1);
	strcpy(m->path, path);

	m->in_config = in_config;

	if (m->data.type == BLOCK) {
		for (int i = 0; i < block_count; i++) {
			struct block *blk = &blocks[i];

			if (!blk->id) {
				continue;
			}

			if (strcmp(blk->properties.module.val.STR, m->data.name) == 0) {
				module_register_block(blk, m->data.name, errout);

				redraw_block(blk);
			}
		}
	} else if (m->data.type == RENDER) {
		if (zindex == 0) {
			zindex = -1;
		}
		m->zindex = zindex;

		for (int i = 0; i < module_count; i++) {
			struct module *mod = &modules[i];

			if (mod == m) {
				continue;
			}

			if (m->zindex < 0 && m->zindex >= mod->zindex) {
				mod->zindex--;
			} else if (m->zindex > 0 && m->zindex <= mod->zindex) {
				mod->zindex++;
			}
		}
		m->sfc = malloc(sizeof(cairo_surface_t *) * bar_count);
		memset(m->sfc, 0, sizeof(cairo_surface_t *) * bar_count);

		resize_module(m);

		if (m->data.interval != 0) {
			m->task = schedule_task(
					module_task_exec, m->data.interval, 1);
		}
	}

	fprintf(out, "Loaded \"%s\" module (%s)\n", m->data.name, path);
	return m;
}

void unload_module(struct module *mod)
{
	void (*unload_func)() = module_get_function(mod, "unload");
	if (unload_func) {
		unload_func();
	}

	if (mod->task) {
		cancel_task(mod->task);
	}

	dlclose(mod->dl);
	mod->dl = 0;

	free(mod->path);

	if (mod->data.type == RENDER) {
		if (mod->sfc[0]) {
			for (int bar = 0; bar < bar_count; bar++) {
				cairo_surface_destroy(mod->sfc[bar]);
			}
		}
		free(mod->sfc);
	}

	for (int i = 0; i < module_count; i++) {
		struct module *_mod = &modules[i];

		if (mod->zindex < 0 && _mod->zindex < mod->zindex) {
			_mod->zindex++;
		} else if (mod->zindex > 0 && _mod->zindex > mod->zindex) {
			_mod->zindex--;
		}
	}
}

void resize_module(struct module *mod)
{
	if (mod->sfc[0]) {
		for (int bar = 0; bar < bar_count; bar++) {
			cairo_surface_destroy(mod->sfc[bar]);
		}
	}

	for (int bar = 0; bar < bar_count; bar++) {
		if (bars[bar].sfc[0]) {
			mod->sfc[bar] = cairo_surface_create_similar_image(
					bars[bar].sfc[0], CAIRO_FORMAT_ARGB32,
					bars[bar].width, settings.height.val.INT);
		}
	}
}

static void load_modules_in_dir(char *path)
{
	DIR *dir = opendir(path);
	struct dirent *dp;

	if (!dir) {
		return;
	}

	while ((dp = readdir(dir))) {
		if (dp->d_type != DT_REG && dp->d_type != DT_LNK) {
			continue;
		}

		if (strlen(dp->d_name) < 4) {
			continue;
		}

		if (strcmp(".so", dp->d_name + strlen(dp->d_name) - 3)) {
			continue;
		}

		int file_len = strlen(path) + strlen(dp->d_name) + 2;
		char *file = malloc(file_len);
		memset(file, 0, file_len);

		strcat(file, path);
		strcat(file, "/");
		strcat(file, dp->d_name);

		load_module(file, -1, stdout, stderr);
		free(file);
	}

	closedir(dir);
}

void modules_init()
{
#ifndef MODDIRS
#   define MODDIRS
#endif

	char *dirs [] = {
		MODDIRS "",
		"/usr/local/lib/blockbar/modules",
		"/usr/lib/blockbar/modules",
	};
	int dir_count = sizeof(dirs) / sizeof(char *);

	in_config = 0;

	for (int i = 0; i < dir_count; i++) {
		if (dirs[i][0] != '\0') {
			load_modules_in_dir(dirs[i]);
		}
	}

	in_config = 1;
}

void cleanup_modules()
{
	for (int i = 0; i < module_count; i++) {
		if (modules[i].dl) {
			unload_module(&modules[i]);
		}
	}

	free(modules);
}

struct module *get_module_by_name(char *name)
{
	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];

		if (!mod->dl) {
			continue;
		}

		if (strcmp(mod->data.name, name) == 0) {
			return mod;
		}
	}

	return 0;
}

void (*module_get_function(struct module *mod, char *func_name))
{
	dlerror();

	void (*func) = dlsym(mod->dl, func_name);
	char *err = dlerror();

	if (err) {
		return 0;
	} else {
		return func;
	}
}

int module_register_block(struct block *blk, char *new, FILE *err)
{
	struct module *old_mod = get_module_by_name(blk->properties.module.val.STR);
	struct module *new_mod = 0;

	if (new) {
		new_mod = get_module_by_name(new);

		if (!new_mod) {
			fprintf(err, "Module \"%s\" not found\n", new);
			return 1;
		}

		if (new_mod->data.type != BLOCK) {
			fprintf(err, "Module \"%s\" not a block module\n", new);
			return 1;
		}
	}

	if (old_mod && old_mod != new_mod) {
		void (*rm)(struct block *) =
			module_get_function(old_mod, "block_remove");

		if (rm) {
			rm(blk);
		}
	}

	if (new) {
		set_setting(&blk->properties.module, (union value) new);

		void (*add)(struct block *) =
			module_get_function(new_mod, "block_add");

		if (add) {
			add(blk);
		}
	}

	return 0;
}
