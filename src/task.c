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

#include "task.h"
#include "util.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static struct Task *tasks;
static int task_count;

static int id;

int schedule_task(void (*callback)(int id), int interval, int repeat)
{
	struct Task *t = 0;

	for (int i = 0; i < task_count; i++) {
		struct Task *task = &tasks[i];
		if (task->id == 0) {
			t = task;
			break;
		}
	}

	if (!t) {
		tasks = realloc(tasks, sizeof(struct Task) * ++task_count);
		t = &tasks[task_count - 1];
	}

	memset(t, 0, sizeof(struct Task));

	while (!t->id) {
start:
		id++;

		if (id == INT_MAX) {
			id = 1;
		}

		for (int i = 0; i < task_count; i++) {
			if (tasks[i].id == id) {
				goto start;
			}
		}

		t->id = id;
	}

	t->callback = callback;
	t->interval = interval;
	t->repeat = repeat;
	get_time(&t->start);

	return t->id;
}

void cancel_task(int id)
{
	for (int i = 0; i < task_count; i++) {
		if (tasks[i].id == id) {
			tasks[i].id = 0;
		}
	}
}

#define TIMEDIFF(a, b) (long) (((b.tv_sec - a.tv_sec) * 1000000) \
		+ (b.tv_usec - a.tv_usec))

struct timeval get_time_to_next_task()
{
	int i;
	struct timeval ret, now, interval;

	ret.tv_sec = ret.tv_usec = -1;

	for (i = 0; i < task_count; i++) {
		if (tasks[i].id) {
			interval.tv_sec = tasks[i].interval / 1000;
			interval.tv_usec = (tasks[i].interval % 1000) * 1000;

			timeradd(&tasks[i].start, &interval, &ret);
			break;
		}
	}

	for (i++; i < task_count; i++) {
		if (!tasks[i].id) {
			continue;
		}

		struct timeval t = tasks[i].start;

		interval.tv_sec = tasks[i].interval / 1000;
		interval.tv_usec = (tasks[i].interval % 1000) * 1000;

		timeradd(&t, &interval, &t);

		if (t.tv_sec * 1000000 + t.tv_usec <
				ret.tv_sec * 1000000 + ret.tv_usec) {
			ret = t;
		}
	}

	get_time(&now);

	if (ret.tv_sec || ret.tv_usec) {
		timersub(&ret, &now, &ret);
	}

	if (ret.tv_sec < 0) {
		ret.tv_sec = ret.tv_usec = 0;
	}

	return ret;
}

void tick_tasks()
{
	int id;
	struct timeval now, interval;

	get_time(&now);

	for (int i = 0; i < task_count; i++) {
		if (!tasks[i].id) {
			continue;
		}

		struct timeval t = tasks[i].start;

		interval.tv_sec = tasks[i].interval / 1000;
		interval.tv_usec = (tasks[i].interval % 1000) * 1000;

		timeradd(&t, &interval, &t);

		if (t.tv_sec * 1000000 + t.tv_usec <
				now.tv_sec * 1000000 + now.tv_usec) {
			id = tasks[i].id;

			if (!tasks[i].repeat) {
				tasks[i].id = 0;
			} else {
				tasks[i].start = now;
			}

			tasks[i].callback(id);
		}
	}
}

void cleanup_tasks()
{
	if (tasks) {
		free(tasks);
	}
}
