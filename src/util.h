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

#ifndef UTIL_H
#define UTIL_H

#include <ujson.h>
#include <sys/time.h>
#include "types.h"

int blockbar_parse_color_json(JsonObject *jo, const char *key, color dest,
	JsonError *err);
int blockbar_parse_color_string(const char *str, color dest);
void blockbar_stringify_color(const color c, char *s);

void resize_block(struct block *blk);
struct block *create_block(int eachmon);
void remove_block(struct block *blk);
struct block *get_block(int id);

void update_tick_interval();
void update_block_task(struct block *blk);

void get_time(struct timeval *tv);

#endif /* UTIL_H */
