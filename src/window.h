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

#ifndef WINDOW_H
#define WINDOW_H

#include "types.h"
#include <cairo/cairo-xlib.h>
#ifdef WAYLAND
#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#else
#include <X11/Xlib.h>
#endif

#ifdef WAYLAND
static const char shm_fmt [] = "/blockbar-shm-xxxxxx";
#endif

struct bar {
#ifdef WAYLAND
	struct wl_output *wl_output;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;

	struct wl_buffer *buffer;
	void *map;
	int fd;
	char shm_path [sizeof(shm_fmt) / sizeof(*shm_fmt)];

	int output_rotate;
	int output_width;
#else
	Window window;
#endif
	int x;
	int width;
	char *output;

	cairo_surface_t *sfc;
	cairo_t *ctx;
#ifndef WAYLAND
	cairo_surface_t *sfc_visible;
	cairo_t *ctx_visible;
#endif
};

#ifdef WAYLAND
extern struct wl_display *disp;
#else
extern Display *disp;
#endif

extern int bar_count;
extern struct bar *bars;

int create_bars();
void update_geom();
void poll_events();
void cleanup_bars();

void click(struct click *cd);

#ifdef WAYLAND
void wl_redraw(struct bar *bar);
#endif

#endif /* WINDOW_H */
