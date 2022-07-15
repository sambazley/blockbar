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
#ifndef WAYLAND
#include "tray.h"
#endif
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

int block_count;
struct block *blocks;

const char *type_strings [] = {
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

struct bar_settings settings = {
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

struct properties def_properties = {
	S(module, STR, "The name of the module that handles the block", "text")
	S(exec, STR, "Command to be executed", "")
	S(pos, POS, "Position of the block", LEFT)
	S(interval, INT, "Time in milliseconds between each execution of the block's script", 0)
	S(padding, INT, "Additional padding on both sides of the block", 0)
	S(paddingleft, INT, "Additional padding on the left of the block", 0)
	S(paddingright, INT, "Additonal padding on the right of the block", 0)
	S(nodiv, BOOL, "Disables the divider to the right of the block", 0)
};

#undef S

int setting_count;
int property_count;

void blockbar_query_blocks(struct block **_blocks, int *_block_count)
{
	*_blocks = blocks;
	*_block_count = block_count;
}

struct bar_settings *blockbar_get_settings()
{
	return &settings;
}

int set_setting(struct setting *setting, union value val)
{
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

#ifndef WAYLAND
		if (setting == &settings.trayside && val.POS == CENTER) {
			return 1;
		}
#endif

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
#ifndef WAYLAND
		} else if (setting == &settings.traybar) {
			int traybar = -1;
			for (int i = 0; i < bar_count; i++) {
				struct bar bar = bars[i];
				if (strcmp(bar.output, val.STR) == 0) {
					traybar = i;
					break;
				}
			}

			if (traybar != -1) {
				tray_init(traybar);
				reparent_icons();
			} else {
				return 1;
			}
#endif
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

	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];

		if (!mod->dl) {
			continue;
		}

		void (*func)(struct setting *) =
			module_get_function(mod, "setting_update");

		if (func) {
			func(setting);
		}
	}

	return 0;
}

static void settings_init()
{
	for (int i = 0; i < setting_count; i++) {
		struct setting *setting = &((struct setting *) &settings)[i];

		memset(&(setting->val), 0, sizeof(setting->val));
		set_setting(setting, setting->def);
	}
}

static int parse_setting(JsonObject *jo, struct setting *setting,
		union value *val, JsonError *err)
{
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
		int e = blockbar_parse_color_json(jo, setting->name, val->COL, err);
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

void cleanup_settings()
{
	for (int i = 0; i < setting_count; i++) {
		struct setting *setting = &((struct setting *) &settings)[i];

		if (setting->type == STR) {
			if (setting->val.STR) {
				free(setting->val.STR);
			}
		}
	}
}

static const char *default_config_file()
{
	char *cfg_dir;
	char *cfg_dir_env = getenv("XDG_CONFIG_HOME");

	if (cfg_dir_env == 0) {
		char *home = getenv("HOME");
		char *default_cfg_dir = "/.config";

		if (home == 0) {
			struct passwd *pw = getpwuid(getuid());
			home = pw->pw_dir;
		}

		cfg_dir = malloc(strlen(home) + strlen(default_cfg_dir) + 1);
		strcpy(cfg_dir, home);
		strcat(cfg_dir, default_cfg_dir);
	} else {
		cfg_dir = malloc(strlen(cfg_dir_env) + 1);
		strcpy(cfg_dir, cfg_dir_env);
	}

	char *filename = "/blockbar/config";
	char *ret = malloc(strlen(cfg_dir) + strlen(filename) + 1);
	strcpy(ret, cfg_dir);
	strcat(ret, filename);

	free(cfg_dir);

	return ret;
}

static void
parse_blocks(JsonObject *jo, const char *key, enum pos pos, JsonError *err)
{
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

		struct block *blk = create_block(eachmon);

		for (int i = 0; i < property_count; i++) {
			struct setting *property =
				&((struct setting *) &(blk->properties))[i];

			union value val;

			if (strcmp("pos", property->name) == 0) {
				continue;
			}

			if (jsonGetPairIndex(entry, property->name) == -1) {
				continue;
			}

			int e = parse_setting(entry, property, &val, err);

			if (!e && strcmp("module", property->name) == 0) {
				module_register_block(blk, val.STR, stderr);
				continue;
			}

			if (e || set_setting(property, val)) {
				fprintf(stderr, "Invalid value for property \"%s\"\n",
						property->name);

				if (jsonErrorIsSet(err)) {
					jsonErrorCleanup(err);
					jsonErrorInit(err);
				}
			}
		}

		update_block_task(blk);

		blk->properties.pos.val.POS = pos;
	}
}

JsonObject *config_init(const char *config)
{
	setting_count = sizeof(settings) / sizeof(struct setting);
	property_count = sizeof(def_properties) / sizeof(struct setting);
	settings_init();

	const char *file;

	if (strcmp(config, "") == 0) {
		file = default_config_file();
	} else {
		file = config;
	}

	printf("Using config %s\n", file);

	JsonError err;
	jsonErrorInit(&err);

	JsonObject *json_config = jsonParseFile(file, &err);

	if (strcmp(config, "") == 0) {
		free((char *) file);
	}

	int err_set = jsonErrorIsSet(&err);
	if (json_config == 0 || err_set) {
		fprintf(stderr, "\nError loading configuration file.\n");

		if (err_set) {
			fprintf(stderr, "%s\n", err.msg);
			jsonErrorCleanup(&err);
		}

		if (json_config) {
			jsonCleanup(json_config);
		}

		fprintf(stderr, "Loading defaults\n\n");

		return 0;
	}

	return json_config;
}

void config_parse_general(JsonObject *json_config)
{
	JsonError err;
	jsonErrorInit(&err);

	for (int i = 0; i < setting_count; i++) {
		struct setting *setting = &((struct setting *) &settings)[i];
		union value val;

		if (jsonGetPairIndex(json_config, setting->name) == -1) {
			continue;
		}

		int e = parse_setting(json_config, setting, &val, &err);

		if (e || set_setting(setting, val)) {
			fprintf(stderr, "Invalid value for setting \"%s\"\n",
					setting->name);

			if (jsonErrorIsSet(&err)) {
				jsonErrorCleanup(&err);
				jsonErrorInit(&err);
			}
		}
	}

	if (jsonGetPairIndex(json_config, "modules") == -1) {
		return;
	}

	JsonArray *mods;
	jsonGetArray(json_config, "modules", &mods, &err);
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

		struct module *mod = load_module(path, zindex, stdout, stderr);

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

		for (int j = 0; j < mod->data.setting_count; j++) {
			struct setting *setting = &mod->data.settings[j];

			union value val;

			if (jsonGetPairIndex(settings, setting->name) == -1) {
				continue;
			}

			int e = parse_setting(settings, setting, &val, &err);

			if (e || set_setting(setting, val)) {
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

void config_parse_blocks(JsonObject *json_config)
{
	JsonError err;
	jsonErrorInit(&err);

	parse_blocks(json_config, "left", LEFT, &err); ERR(&err);
	parse_blocks(json_config, "center", CENTER, &err); ERR(&err);
	parse_blocks(json_config, "right", RIGHT, &err); ERR(&err);
}

void config_cleanup(JsonObject *json_config)
{
	jsonCleanup(json_config);
}

int is_setting_modified(struct setting *setting)
{
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

static void add_setting(struct setting *setting, int explicit,
		JsonObject *jo, JsonError *err)
{
	if (!is_setting_modified(setting) && !explicit) {
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
		if (is_setting_modified(setting)) {
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
		blockbar_stringify_color(setting->val.COL, col);
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

char *config_save(FILE *file, int explicit)
{
	JsonObject *jo = jsonCreateBaseObject();
	JsonError err;

	jsonErrorInit(&err);

	for (int i = 0; i < setting_count; i++) {
		struct setting *setting = &((struct setting *) &settings)[i];

		add_setting(setting, explicit, jo, &err);
		ERR_;
	}

	JsonArray *mods = jsonAddArray("modules", jo, &err);

	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];
		if (!mod->dl) {
			continue;
		}

		int has_modified_settings = 0;

		for (int j = 0; j < mod->data.setting_count; j++) {
			if (is_setting_modified(&mod->data.settings[j])) {
				has_modified_settings = 1;
				break;
			}
		}

		if (!mod->in_config && !has_modified_settings) {
			continue;
		}

		JsonObject *mod_obj = jsonAddObject(0, mods, &err);

		jsonAddString("path", mod->path, mod_obj, &err);
		ERR_;

		if (mod->data.type == RENDER && mod->zindex != -1) {
			jsonAddNumber("zindex", mod->zindex, mod_obj, &err);
			ERR_;
		}

		JsonObject *settings = jsonAddObject("settings", mod_obj, &err);
		ERR_;

		for (int j = 0; j < mod->data.setting_count; j++) {
			add_setting(&mod->data.settings[j], explicit, settings,
					&err);
			ERR_;
		}
	}

	JsonArray *blk_arr [SIDES];
	blk_arr[LEFT] = jsonAddArray("left", jo, &err);
	ERR_;
	blk_arr[CENTER] = jsonAddArray("center", jo, &err);
	ERR_;
	blk_arr[RIGHT] = jsonAddArray("right", jo, &err);
	ERR_;

	for (int i = 0; i < block_count; i++) {
		struct block *blk = &blocks[i];

		if (!blk->id) {
			continue;
		}

		JsonObject *jblk = jsonAddObject(
				0, blk_arr[blk->properties.pos.val.POS], &err);
		ERR_;

		for (int j = 0; j < property_count; j++) {
			struct setting *property =
				&((struct setting *) &(blk->properties))[j];

			if (strcmp("pos", property->name) == 0) {
				continue;
			}

			add_setting(property, explicit, jblk, &err);

			if (jsonErrorIsSet(&err)) {
				ERR_;
			}
		}

		if (blk->eachmon || explicit) {
			jsonAddBoolNull("eachmon",
					blk->eachmon ?  JSON_TRUE : JSON_FALSE,
					jblk, &err);
			ERR_;
		}
	}

	jsonWriteObject(file, jo, 4);

	jsonCleanup(jo);

	return 0;
}
