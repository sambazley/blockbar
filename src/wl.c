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

#include "window.h"
#include "config.h"
#include "exec.h"
#include "modules.h"
#include "render.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct wl_display *disp = NULL;

static struct wl_compositor *compositor = NULL;
static struct wl_shm *shm = NULL;
static struct zwlr_layer_shell_v1 *layer_shell = NULL;

static struct wl_seat *seat = NULL;
static struct wl_pointer *pointer = NULL;

static int pointer_bar;
static int pointer_x;

static int axis_discrete [2] = {0};
static int axis_value [2] = {0};
static uint32_t axis_time [2] = {0};

#define SCROLL_TIMEOUT 100
#define SCROLL_THRESHOLD 10000

int bar_count;
struct bar *bars;

static void output_geometry(void *data, struct wl_output *wl_output, int32_t x,
		int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel,
		const char *make, const char *model, int32_t transform)
{
	(void) wl_output;
	(void) x;
	(void) y;
	(void) width_mm;
	(void) height_mm;
	(void) subpixel;
	(void) make;
	(void) model;

	struct bar *bar = &bars[(long) data];

	bar->output_rotate = transform & 1;
}

static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh)
{
	(void) wl_output;
	(void) flags;
	(void) refresh;

	struct bar *bar = &bars[(long) data];
	bar->output_width = bar->output_rotate ? height : width;

	if (bar->sfc) {
		update_geom();

		for (int i = 0; i < block_count; i++) {
			struct block *blk = &blocks[i];

			if (blk->id) {
				redraw_block(blk);
			}
		}
		redraw();
	}
}

static void output_done(void *data, struct wl_output *wl_output)
{
	(void) data;
	(void) wl_output;
}

static void output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
	(void) data;
	(void) wl_output;
	(void) factor;
}

static void output_name(void *data, struct wl_output *wl_output, const char *name)
{
	(void) wl_output;

	struct bar *bar = &bars[(long) data];
	bar->output = strdup(name);
}

static void output_description(void *data, struct wl_output *wl_output, const char *description)
{
	(void) data;
	(void) wl_output;
	(void) description;
}

static const struct wl_output_listener output_listener = {
	output_geometry,
	output_mode,
	output_done,
	output_scale,
	output_name,
	output_description
};

static void pointer_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x,
		wl_fixed_t surface_y)
{
	(void) data;
	(void) wl_pointer;
	(void) serial;
	(void) surface_y;

	for (int i = 0; i < bar_count; i++) {
		if (bars[i].surface == surface) {
			pointer_bar = i;
			break;
		}
	}
	pointer_x = wl_fixed_to_int(surface_x);
}

static void pointer_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface)
{
	(void) data;
	(void) wl_pointer;
	(void) serial;
	(void) surface;
}

static void pointer_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	(void) data;
	(void) wl_pointer;
	(void) time;
	(void) surface_y;

	pointer_x = wl_fixed_to_int(surface_x);
}

static void pointer_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	(void) data;
	(void) wl_pointer;
	(void) serial;
	(void) time;

	if (state != 1) {
		return;
	}

	int btn_mapped;

	switch (button) {
	case BTN_MIDDLE:
		btn_mapped = 2;
		break;
	case BTN_RIGHT:
		btn_mapped = 3;
		break;
	default:
		btn_mapped = button - BTN_MOUSE + 1;
	}

	struct click cd = {
		.button = btn_mapped,
		.x = pointer_x,
		.bar = pointer_bar
	};

	click(&cd);
}

static void pointer_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value)
{
	(void) data;
	(void) wl_pointer;

	if (axis >= 2) {
		return;
	}

	if (time - axis_time[axis] > SCROLL_TIMEOUT) {
		axis_value[axis] = 0;
	}

	axis_time[axis] = time;
	axis_value[axis] += value;
}

static void axis_to_button(int axis, int value)
{
	struct click cd = {
		.button = 4 + 2 * axis + (value > 0 ? 1 : 0),
		.x = pointer_x,
		.bar = pointer_bar
	};

	click(&cd);
}

static void pointer_frame(void *data, struct wl_pointer *wl_pointer)
{
	(void) data;
	(void) wl_pointer;

	for (int i = 0; i < 2; i++) {
		if (axis_discrete[i] == 0 && axis_value[i] != 0) {
			if (abs(axis_value[i]) > SCROLL_THRESHOLD) {
				axis_to_button(i, axis_value[i]);
				axis_value[i] = 0;
				axis_discrete[i] = 0;
			}
		} else if (axis_discrete[i] != 0 && axis_value[i] != 0) {
			axis_to_button(i, axis_value[i]);
			axis_value[i] = 0;
			axis_discrete[i] = 0;
		}
	}
}

static void pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis_source)
{
	(void) data;
	(void) wl_pointer;
	(void) axis_source;
}

static void pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis_source)
{
	(void) data;
	(void) wl_pointer;
	(void) time;
	(void) axis_source;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis, int32_t discrete)
{
	(void) data;
	(void) wl_pointer;

	if (axis >= 2) {
		return;
	}

	axis_discrete[axis] += discrete;
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_enter,
	pointer_leave,
	pointer_motion,
	pointer_button,
	pointer_axis,
	pointer_frame,
	pointer_axis_source,
	pointer_axis_stop,
	pointer_axis_discrete
};

static void seat_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps)
{
	(void) data;

	int has_pointer = caps & WL_SEAT_CAPABILITY_POINTER;

	if (!has_pointer && pointer) {
		wl_pointer_release(pointer);
		pointer = NULL;
	} else if (has_pointer && !pointer) {
		pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(pointer, &pointer_listener, NULL);
	}
}

static void seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	(void) data;
	(void) wl_seat;
	(void) name;
}

static const struct wl_seat_listener seat_listener = {
	seat_capabilities,
	seat_name
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t id,
		const char *interface, uint32_t version)
{
	(void) data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(registry, id, &wl_seat_interface, 5);
		wl_seat_add_listener(seat, &seat_listener, NULL);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		bars = realloc(bars, sizeof(struct bar) * ++bar_count);
		struct bar *bar = &bars[bar_count - 1];
		memset(bar, 0, sizeof(*bar));

		bar->wl_output = wl_registry_bind(registry, id, &wl_output_interface, 4);
		wl_output_add_listener(bar->wl_output, &output_listener, (void *) (long) (bar_count - 1));
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		layer_shell = wl_registry_bind(registry, id, &zwlr_layer_shell_v1_interface, version < 4 ? version : 4);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t id)
{
	(void) data;
	(void) registry;
	(void) id;
}

static const struct wl_registry_listener registry_listener = {
	handle_global,
	handle_global_remove
};

void layer_surface_configure(void *data, struct zwlr_layer_surface_v1
		*layer_surface, uint32_t serial, uint32_t width, uint32_t height)
{
	(void) width;
	(void) height;

	struct bar *bar = &bars[(long) data];

	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
	wl_surface_commit(bar->surface);
}

void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1)
{
	(void) data;
	(void) zwlr_layer_surface_v1;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	layer_surface_configure,
	layer_surface_closed
};

static int wl_init()
{
	disp = wl_display_connect(NULL);
	errno = 0;
	if (!disp) {
		perror("wl_display_connect");
		return 1;
	}

	struct wl_registry *registry = wl_display_get_registry(disp);
	wl_registry_add_listener(registry, &registry_listener, NULL);

	wl_display_roundtrip(disp);
	wl_display_dispatch(disp);

	if (compositor == NULL) {
		fprintf(stderr, "compositor not available\n");
		return 1;
	}

	if (shm == NULL) {
		fprintf(stderr, "wl_shm not available\n");
		return 1;
	}

	if (layer_shell == NULL) {
		fprintf(stderr, "layer_shell not available\n");
		return 1;
	}

	return 0;
}

static int create_shm(struct bar *bar, int size)
{
	int n = 0;
	int fd;

	strcpy(bar->shm_path, shm_fmt);

	do {
		snprintf(bar->shm_path + strlen(shm_fmt) - 6, 7, "%06d", n++);
		errno = 0;
		fd = shm_open(bar->shm_path, O_RDWR | O_CREAT | O_EXCL, 0600);

		if (n > 999999) {
			fprintf(stderr, "shm_open failed\n");
			return -1;
		}
	} while (errno == EEXIST);

	if (fd < 0) {
		perror("shm_open");
		return -1;
	}

	if (ftruncate(fd, size) < 0) {
		perror("ftruncate");
		fprintf(stderr, "%d %d\n", fd, size);
		close(fd);
		return -1;
	}

	return fd;
}

static struct wl_buffer *create_buffer(struct bar *bar, int height)
{
	int stride = 4 * bar->width;
	int size = stride * height;
	bar->fd = create_shm(bar, size);

	if (bar->fd < 0) {
		fprintf(stderr, "Failed to create shm\n");
		return 0;
	}

	bar->map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, bar->fd, 0);
	if (bar->map == MAP_FAILED) {
		perror("mmap\n");
		close(bar->fd);
		return 0;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, bar->fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, bar->width,
			height, stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);

	memset(bar->map, 0, size);

	return buffer;
}

static void close_buffer(struct bar *bar)
{
	wl_buffer_destroy(bar->buffer);
	shm_unlink(bar->shm_path);
	close(bar->fd);
}

int create_bars()
{
	if (wl_init()) {
		return 1;
	}

	for (int i = 0; i < bar_count; i++) {
		struct bar *bar = &bars[i];

		bar->surface = wl_compositor_create_surface(compositor);
		if (bar->surface == NULL) {
			fprintf(stderr, "Failed to create surface\n");
			return 1;
		}

		bar->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
				layer_shell,
				bar->surface,
				bar->wl_output,
				ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
				"blockbar");

		zwlr_layer_surface_v1_add_listener(bar->layer_surface, &layer_surface_listener, (void *) (long) i);
		zwlr_layer_surface_v1_set_size(bar->layer_surface, 10, 10);

		wl_surface_commit(bar->surface);
	}

	wl_display_dispatch(disp);

	return 0;
}

void update_geom()
{
	for (int i = 0; i < bar_count; i++) {
		struct bar *bar = &bars[i];

		bar->width = bar->output_width - settings.marginhoriz.val.INT * 2;

		zwlr_layer_surface_v1_set_size(bar->layer_surface, bar->width,
				settings.height.val.INT);

		if (strcmp(settings.position.val.STR, "bottom") == 0) {
			zwlr_layer_surface_v1_set_anchor(bar->layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
		} else {
			zwlr_layer_surface_v1_set_anchor(bar->layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP);
		}

		zwlr_layer_surface_v1_set_margin(bar->layer_surface,
				settings.marginvert.val.INT, 0, settings.marginvert.val.INT, 0);

		zwlr_layer_surface_v1_set_exclusive_zone(bar->layer_surface,
				settings.height.val.INT);

		if (bar->sfc) {
			cairo_surface_destroy(bar->sfc);
		}
		if (bar->ctx) {
			cairo_destroy(bar->ctx);
		}

		wl_surface_attach(bar->surface, NULL, 0, 0);

		if (bar->buffer) {
			close_buffer(bar);
		}

		bar->buffer = create_buffer(bar, settings.height.val.INT);
		if (!bar->buffer) {

			exit(1);
		}

		wl_surface_attach(bar->surface, bar->buffer, 0, 0);
		wl_surface_commit(bar->surface);

		bar->sfc = cairo_image_surface_create_for_data(bar->map,
				CAIRO_FORMAT_ARGB32, bar->width, settings.height.val.INT,
				cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, bar->width));

		if (cairo_surface_status(bar->sfc) != CAIRO_STATUS_SUCCESS) {
			fprintf(stderr, "Failed to create cairo surface\n");
			exit(1);
		}

		bar->ctx = cairo_create(bar->sfc);

		wl_redraw(bar);
	}

	for (int i = 0; i < block_count; i++) {
		struct block *blk = &blocks[i];

		if (blk->id) {
			resize_block(blk);
		}
	}

	for (int i = 0; i < module_count; i++) {
		struct module *mod = &modules[i];

		if (mod->dl && mod->data.type == RENDER) {
			resize_module(mod);
		}
	}
}

void wl_redraw(struct bar *bar)
{
	cairo_surface_flush(bar->sfc);

	wl_surface_attach(bar->surface, bar->buffer, 0, 0);
	wl_surface_damage(bar->surface, 0, 0, bar->width, settings.height.val.INT);
	wl_surface_commit(bar->surface);

	wl_display_dispatch(disp);
}

void poll_events()
{
	if (wl_display_dispatch_pending(disp) == -1) {
		fprintf(stderr, "wl_display_dispatch failed\n");
		exit(1);
	}
}

void cleanup_bars()
{
	for (int i = 0; i < bar_count; i++) {
		struct bar *bar = &bars[i];

		free(bar->output);
		cairo_surface_destroy(bar->sfc);
		cairo_destroy(bar->ctx);
		close_buffer(bar);

		if (pointer) {
			wl_pointer_release(pointer);
		}

		zwlr_layer_surface_v1_destroy(bar->layer_surface);
		wl_surface_destroy(bar->surface);
	}

	free(bars);
}

int blockbar_get_bar_width(int bar)
{
	return bars[bar].width;
}
