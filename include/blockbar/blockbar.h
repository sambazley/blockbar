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
 *	claim that you wrote the original software. If you use this software
 *	in a product, an acknowledgment in the product documentation would be
 *	appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef BLOCKBAR_H
#define BLOCKBAR_H

#include <ujson.h>
#include "types.h"
#include "version.h"

void blockbar_query_blocks(struct block **blocks, int *block_count);

struct bar_settings *blockbar_get_settings();

int blockbar_get_bar_width(int bar);

void blockbar_set_env(const char *key, const char *val);

int blockbar_parse_color_json(JsonObject *jo, const char *key, color dest,
		JsonError *err);

int blockbar_parse_color_string(const char *str, color dest);

void blockbar_stringify_color(const color c, char *s);

#endif /* BLOCKBAR_H */
