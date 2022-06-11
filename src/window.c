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
#include "task.h"
#include "tray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

Display *disp;
int bar_count;
struct bar *bars;

static Visual *visual;

#define ATOM(x) Atom x = XInternAtom(disp, #x, False)

static const unsigned char xdnd_version = 5;
static Atom xdnd_position;
static Atom xdnd_status;
static Atom xdnd_leave;

static int taskid = 0;
static int dndx, dndoldx, dndoldy;
static int dndbar;

int create_bars()
{
	disp = XOpenDisplay(NULL);
	if (!disp) {
		fprintf(stderr, "Error opening display\n");
		return 1;
	}

	int s = DefaultScreen(disp);
	Window root = RootWindow(disp, s);

	XRRScreenResources *res = XRRGetScreenResources(disp, root);

	bar_count = 0;

	ATOM(_NET_WM_WINDOW_TYPE);
	ATOM(_NET_WM_WINDOW_TYPE_DOCK);
	ATOM(_NET_WM_STATE);
	ATOM(_NET_WM_STATE_STICKY);
	ATOM(_NET_WM_STATE_BELOW);
	ATOM(XdndAware);

	xdnd_position = XInternAtom(disp, "XdndPosition", False);
	xdnd_status = XInternAtom(disp, "XdndStatus", False);
	xdnd_leave = XInternAtom(disp, "XdndLeave", False);

	XVisualInfo vinfo;
	XMatchVisualInfo(disp, s, 32, TrueColor, &vinfo);

	visual = vinfo.visual;

	for (int i = 0; i < res->noutput; i++) {
		XRROutputInfo *oput_info = XRRGetOutputInfo(disp, res,
				res->outputs[i]);

		if (!oput_info->crtc) {
			XRRFreeOutputInfo(oput_info);
			continue;
		}

		bar_count++;
		bars = realloc(bars, sizeof(struct bar) * bar_count);

		XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(disp, res,
				oput_info->crtc);

		struct bar *bar = &bars[bar_count-1];

		XSetWindowAttributes wa;
		wa.colormap = XCreateColormap(disp, root, visual, AllocNone);
		wa.border_pixel = 0;

		bar->window = XCreateWindow(disp, root, 0, 0, 10, 10,
				0, vinfo.depth, InputOutput, visual,
				CWColormap | CWBorderPixel,
				&wa);

		bar->output = malloc(oput_info->nameLen + 1);
		strcpy(bar->output, oput_info->name);

		XRRFreeOutputInfo(oput_info);
		XRRFreeCrtcInfo(crtc_info);

		XChangeProperty(disp, bar->window, _NET_WM_WINDOW_TYPE, XA_ATOM,
				32, PropModeAppend,
				(unsigned char *) &_NET_WM_WINDOW_TYPE_DOCK, 1);

		XChangeProperty(disp, bar->window, _NET_WM_STATE, XA_ATOM,
				32, PropModeAppend,
				(unsigned char *) &_NET_WM_STATE_STICKY, 1);

		XChangeProperty(disp, bar->window, _NET_WM_STATE, XA_ATOM,
				32, PropModeAppend,
				(unsigned char *) &_NET_WM_STATE_BELOW, 1);

		XChangeProperty(disp, bar->window, XdndAware, XA_ATOM, 32,
				PropModeReplace, &xdnd_version, 1);

		XClassHint *classhint = XAllocClassHint();
		classhint->res_name = "blockbar";
		classhint->res_class = "blockbar";
		XSetClassHint(disp, bar->window, classhint);
		XFree(classhint);

		XSelectInput(disp, bar->window,
				ButtonPressMask | SubstructureNotifyMask | ExposureMask);

		bar->sfc[RI_VISIBLE] = 0;
		bar->sfc[RI_BUFFER] = 0;
		bar->ctx[RI_VISIBLE] = 0;
		bar->ctx[RI_BUFFER] = 0;
	}
	XRRFreeScreenResources(res);
	XFlush(disp);

	return 0;
}

void update_geom()
{
	int s = DefaultScreen(disp);
	Window root = RootWindow(disp, s);

	ATOM(_NET_WM_STRUT);
	ATOM(_NET_WM_STRUT_PARTIAL);

	XRRScreenResources *res = XRRGetScreenResources(disp, root);

	int b = 0;
	for (int i = 0; i < res->noutput; i++) {
		XRROutputInfo *oput_info = XRRGetOutputInfo(disp, res,
				res->outputs[i]);

		if (!oput_info->crtc) {
			XRRFreeOutputInfo(oput_info);
			continue;
		}

		XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(disp, res,
				oput_info->crtc);
		struct bar *bar = &bars[b];

		XUnmapWindow(disp, bar->window);

		int top = 1;
		if (strcmp(settings.position.val.STR, "bottom") == 0) {
			top = 0;
		}

		int x = crtc_info->x + settings.marginhoriz.val.INT
			+ settings.xoffset.val.INT;

		int y = top ? settings.marginvert.val.INT
			: (signed) crtc_info->height
			- settings.height.val.INT
			- settings.marginvert.val.INT;

		int width = crtc_info->width - settings.marginhoriz.val.INT * 2;

		XMoveResizeWindow(disp, bar->window, x, y,
				width, settings.height.val.INT);

		bar->x = x;
		bar->width = width;

		for (int ri = 0; ri < RI_COUNT; ri++) {
			if (bar->sfc[ri]) {
				cairo_surface_destroy(bar->sfc[ri]);
			}

			if (bar->ctx[ri]) {
				cairo_destroy(bar->ctx[ri]);
			}

			if (ri == 0) {
				bar->sfc[0] = cairo_xlib_surface_create(
						disp,
						bar->window,
						visual,
						bar->width,
						settings.height.val.INT);
			} else {
				bar->sfc[ri] = cairo_surface_create_similar_image(
						bar->sfc[0],
						CAIRO_FORMAT_ARGB32,
						bar->width,
						settings.height.val.INT);
			}

			bar->ctx[ri] = cairo_create(bar->sfc[ri]);
		}

		long geom [12] = {0};
		int height = settings.height.val.INT + settings.marginvert.val.INT * 2;

		if (top) {
			geom[2] = height;
			geom[8] = crtc_info->x;
			geom[9] = crtc_info->x + crtc_info->width;
		} else {
			geom[3] = height;
			geom[10] = crtc_info->x;
			geom[11] = crtc_info->x + crtc_info->width;
		}

		XChangeProperty(disp, bar->window, _NET_WM_STRUT, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *) &geom, 4);
		XChangeProperty(disp, bar->window, _NET_WM_STRUT_PARTIAL, XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *) &geom, 12);

		XRRFreeOutputInfo(oput_info);
		XRRFreeCrtcInfo(crtc_info);

		XMapWindow(disp, bar->window);

		XMoveResizeWindow(disp, bar->window, x, y,
				width, settings.height.val.INT);

		b++;
	}

	XRRFreeScreenResources(res);
	XFlush(disp);

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

static void click(struct click *cd)
{
	for (int i = 0; i < block_count; i++) {
		struct block *blk = &blocks[i];

		if (!blk->id) {
			continue;
		}

		int rendered;
		if (blk->eachmon) {
			rendered = blk->data[cd->bar].rendered;
		} else {
			rendered = blk->data->rendered;
		}

		if (!rendered) {
			continue;
		}

		if (cd->x > blk->x[cd->bar] &&
				cd->x < blk->x[cd->bar] + blk->width[cd->bar]) {
			block_exec(blk, cd);
			break;
		}
	}
}

static int is_xdnd_event(XEvent *ev)
{
	if (ev->xclient.message_type == xdnd_position ||
			ev->xclient.message_type == xdnd_leave) {
		return 1;
	}

	return 0;
}

static void dnd_task(int id)
{
	if (id == taskid) {
		taskid = 0;

		struct click cd = {
			.button = -1,
			.x = dndx - bars[dndbar].x,
			.bar = dndbar
		};

		click(&cd);
	}
}

static void handle_xdnd_event(XEvent *ev)
{
	if (ev->xclient.message_type == xdnd_position) {
		int bar;
		uint16_t x = ev->xclient.data.l[2] >> 16;
		uint16_t y = ev->xclient.data.l[2] & 0xffff;

		for (bar = 0; bar < bar_count; bar++) {
			if (bars[bar].window == ev->xbutton.window) break;
		}

		dndx = x;

		if (abs(x - dndoldx) > 5 || abs(y - dndoldy) > 5) {
			dndoldx = x;
			dndoldy = y;
			dndbar = bar;

			if (taskid) {
				cancel_task(taskid);
			}

			taskid = schedule_task(&dnd_task, 500, 0);
		}

		XEvent resp;
		memset(&resp, 0, sizeof(resp));
		resp.xclient.type = ClientMessage;
		resp.xclient.window = ev->xclient.data.l[0];
		resp.xclient.message_type = xdnd_status;
		resp.xclient.format = 32;
		resp.xclient.data.l[0] = ev->xclient.window;
		resp.xclient.data.l[1] = 0;
		XSendEvent(disp, resp.xclient.window, False, NoEventMask, &resp);
	} else if (ev->xclient.message_type == xdnd_leave) {
		if (taskid) {
			cancel_task(taskid);
		}
	}
}

void poll_events()
{
	XEvent ev;
	while (XPending(disp)) {
		XNextEvent(disp, &ev);
		switch (ev.type) {
			case ButtonPress:
			{
				int bar;
				for (bar = 0; bar < bar_count; bar++) {
					if (bars[bar].window == ev.xbutton.window) break;
				}

				if (ev.xbutton.x < 0 || ev.xbutton.y < 0 ||
						ev.xbutton.x > bars[bar].width ||
						ev.xbutton.y > settings.height.val.INT) {
					break;
				}

				struct click cd = {
					.button = ev.xbutton.button,
					.x = ev.xbutton.x,
					.bar = bar,
				};

				click(&cd);
			}
			break;
			case ClientMessage:
				if (is_tray_event(&ev)) {
					handle_tray_event(&ev);
				} else if (is_xdnd_event(&ev)) {
					handle_xdnd_event(&ev);
				}
				break;
			case ReparentNotify:
			case DestroyNotify:
				handle_destroy_event(&ev);
				break;
		}
	}
}

void cleanup_bars()
{
	for (int i = 0; i < bar_count; i++) {
		struct bar *bar = &bars[i];

		free(bar->output);
		cairo_surface_destroy(bar->sfc[0]);
		cairo_surface_destroy(bar->sfc[1]);
		cairo_destroy(bar->ctx[0]);
		cairo_destroy(bar->ctx[1]);
		XDestroyWindow(disp, bar->window);
	}

	free(bars);
}

int blockbar_get_bar_width(int bar)
{
	return bars[bar].width;
}
