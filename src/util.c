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

#include "util.h"
#include "config.h"
#include "exec.h"
#include "modules.h"
#include "task.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int blockbar_parse_color_json(JsonObject *jo, const char *key, color dest,
		JsonError *err)
{
	int index = jsonGetPairIndex(jo, key);
	if (index == -1) {
		return 1;
	}

	void *val = jo->pairs[index].val;

	if (jsonGetType(val) == JSON_STRING) {
		char *str;
		jsonGetString(jo, key, &str, err);

		if (str && *str == '#') {
			return blockbar_parse_color_string(str+1, dest);
		}

		return 1;
	}

	JsonArray *col;
	jsonGetArray(jo, key, &col, err);
	if (jsonErrorIsSet(err)) {
		fprintf(stderr, "Error parsing array \"%s\"\n", key);
		jsonErrorCleanup(err);
		jsonErrorInit(err);
		return 1;
	}

	if (col->used != 3 && col->used != 4) {
		fprintf(stderr, "\"%s\" array must contain 3 or 4 values\n", key);
		jsonErrorCleanup(err);
		jsonErrorInit(err);
		return 1;
	}

	for (unsigned int j = 0; j < col->used; j++) {
		void *val = col->vals[j];
		if (jsonGetType(val) != JSON_NUMBER) {
			fprintf(stderr, "value in \"%s\" array is not a valid int\n", key);
			jsonErrorCleanup(err);
			jsonErrorInit(err);
			return 1;
		}

		JsonNumber *n = (JsonNumber *) val;
		dest[j] = n->data;
	}

	if (col->used == 3) {
		dest[3] = 0xFF;
	}

	return 0;
}

int blockbar_parse_color_string(const char *str, color dest)
{
	char *end = 0;

	int c = strtol(str, &end, 16);
	int col_int = 0xFFFFFFFF;

	if (end && *end) {
		return 1;
	}

	if (strlen(str) == 3 || strlen(str) == 4) {
		if (strlen(str) == 3) {
			c <<= 4;
			c |= 0xF;
		}

		col_int = ((c & 0xF000) << 16) | ((c & 0xF000) << 12)
   			| ((c & 0x0F00) << 12) | ((c & 0x0F00) << 8)
   			| ((c & 0x00F0) << 8) | ((c & 0x00F0) << 4)
   			| ((c & 0x000F) << 4) | (c & 0x000F);
	} else if (strlen(str) == 6) {
		col_int = (c << 8) | 0xFF;
	} else if (strlen(str) == 8) {
		col_int = c;
	} else {
		return 1;
	}

	dest[0] = col_int >> 24;
	dest[1] = (col_int >> 16) & 0xFF;
	dest[2] = (col_int >> 8) & 0xFF;
	dest[3] = col_int & 0xFF;

	return 0;
}

void blockbar_stringify_color(const color c, char *s)
{
	sprintf(s, "#%02x%02x%02x%02x", c[0], c[1], c[2], c[3]);
}

void resize_block(struct block *blk)
{
	for (int bar = 0; bar < bar_count; bar++) {
		if (blk->sfc[bar]) {
			cairo_surface_destroy(blk->sfc[bar]);
		}

		blk->sfc[bar] = cairo_surface_create_similar_image(
				bars[bar].sfc, CAIRO_FORMAT_ARGB32,
				bars[bar].width, settings.height.val.INT);
	}
}

struct block *create_block(int eachmon)
{
	struct block *blk = 0;

	for (int i = 0; i < block_count; i++) {
		if (blocks[i].id == 0) {
			blk = &blocks[i];
			memset(blk, 0, sizeof(struct block));
			blk->id = i + 1;
			break;
		}
	}

	if (blk == 0) {
		blocks = realloc(blocks, sizeof(struct block) * ++block_count);
		blk = &blocks[block_count - 1];
		memset(blk, 0, sizeof(struct block));
		blk->id = block_count;
	}

	blk->eachmon = eachmon;

	if (eachmon) {
		blk->data = malloc(sizeof(struct block_data) * bar_count);
		memset(blk->data, 0, sizeof(struct block_data) * bar_count);
	} else {
		blk->data = malloc(sizeof(struct block_data));
		memset(blk->data, 0, sizeof(struct block_data));
	}

	memcpy(&(blk->properties), &def_properties, sizeof(blk->properties));

	for (int i = 0; i < property_count; i++) {
		struct setting *property = &((struct setting *) &(blk->properties))[i];
		memset(&(property->val), 0, sizeof(property->val));

		set_setting(property, property->def);
	}

	blk->width = malloc(sizeof(int) * bar_count);
	blk->x = malloc(sizeof(int) * bar_count);
	blk->sfc = malloc(sizeof(cairo_surface_t *) * bar_count);
	memset(blk->sfc, 0, sizeof(cairo_surface_t *) * bar_count);

	resize_block(blk);

	update_block_task(blk);

	return blk;
}

void remove_block(struct block *blk)
{
	module_register_block(blk, 0, 0);

	if (blk->task) {
		cancel_task(blk->task);
	}

	blk->id = 0;

	if (blk->eachmon) {
		for (int i = 0; i < bar_count; i++) {
			char *exec_data = blk->data[i].exec_data;

			if (exec_data) {
				free(exec_data);
			}
		}
	} else {
		char *exec_data = blk->data->exec_data;

		if (exec_data) {
			free(exec_data);
		}
	}

	free(blk->data);

	for (int i = 0; i < property_count; i++) {
		struct setting *property = &((struct setting *) &(blk->properties))[i];

		if (property->type == STR && property->val.STR) {
			free(property->val.STR);
		}
	}

	for (int i = 0; i < bar_count; i++) {
		cairo_surface_destroy(blk->sfc[i]);
	}

	free(blk->width);
	free(blk->x);
	free(blk->sfc);
}

struct block *get_block(int id)
{
	for (int i = 0; i < block_count && id; i++) {
		struct block *blk = &blocks[i];
		if (blk->id == id) {
			return blk;
		}
	}

	return 0;
}

static void block_task_exec(int id)
{
	for (int i = 0; i < block_count; i++) {
		struct block *blk = &blocks[i];

		if (!blk->id) {
			continue;
		}

		if (blk->task == id) {
			block_exec(blk, 0);
		}
	}
}

void update_block_task(struct block *blk)
{
	if (blk->task) {
		cancel_task(blk->task);
	}

	if (blk->properties.interval.val.INT == 0) {
		blk->task = 0;
	} else {
		blk->task = schedule_task(block_task_exec,
				blk->properties.interval.val.INT, 1);
	}
}

void get_time(struct timeval *tv)
{
	clock_gettime(CLOCK_MONOTONIC_RAW, (struct timespec *) tv);
	tv->tv_usec /= 1000;
}
