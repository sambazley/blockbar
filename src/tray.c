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

#include "tray.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define XEMBED_EMBEDDED_NOTIFY		0
#define XEMBED_WINDOW_ACTIVATE  	1
#define XEMBED_WINDOW_DEACTIVATE  	2
#define XEMBED_REQUEST_FOCUS	 	3
#define XEMBED_FOCUS_IN 	 	4
#define XEMBED_FOCUS_OUT  		5
#define XEMBED_FOCUS_NEXT 		6
#define XEMBED_FOCUS_PREV 		7
#define XEMBED_MODALITY_ON 		10
#define XEMBED_MODALITY_OFF 		11
#define XEMBED_REGISTER_ACCELERATOR     12
#define XEMBED_UNREGISTER_ACCELERATOR   13
#define XEMBED_ACTIVATE_ACCELERATOR     14

#define XEMBED_VERSION 0

static int trapped_error_code = 0;
static int (*old_error_handler)(Display *, XErrorEvent *);

static Atom opcode;

int tray_bar;

static Window *tray_icons;
static int tray_icon_count;
static int icons_drawnn;

static int error_handler(Display *disp, XErrorEvent *err)
{
	(void) disp;

	if (err->error_code != 0) {
		trapped_error_code = err->error_code;
	}
	return 0;
}

static void trap_errors()
{
	trapped_error_code = 0;
	old_error_handler = XSetErrorHandler(error_handler);
}

static int untrap_errors()
{
	XSetErrorHandler(old_error_handler);
	return trapped_error_code;
}

static void
xembed_send_message(int index, long msg, long detail, long d1, long d2)
{
	XEvent ev;
	memset(&ev, 0, sizeof(ev));

	Window w = tray_icons[index];

	ev.xclient.type = ClientMessage;
	ev.xclient.window = w;
	ev.xclient.message_type = XInternAtom(disp, "_XEMBED", False);
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;
	ev.xclient.data.l[1] = msg;
	ev.xclient.data.l[2] = detail;
	ev.xclient.data.l[3] = d1;
	ev.xclient.data.l[4] = d2;

	trap_errors();

	XSendEvent(disp, w, False, NoEventMask, &ev);
	XSync(disp, False);

	int err = untrap_errors();

	if (err) {
		tray_icons[index] = 0;
	}
}

void tray_init(int bar_index)
{
	char atom_name [22];
	snprintf(atom_name, 21, "_NET_SYSTEM_TRAY_S%d", DefaultScreen(disp));

	Atom _NET_SYSTEM_TRAY = XInternAtom(disp, atom_name, False);
	opcode = XInternAtom(disp, "_NET_SYSTEM_TRAY_OPCODE", False);

	tray_bar = bar_index;
	struct bar bar = bars[bar_index];

	XSetSelectionOwner(disp, _NET_SYSTEM_TRAY, bar.window, False);

	XEvent ev;
	memset(&ev, 0, sizeof(ev));

	ev.xclient.type = ClientMessage;
	ev.xclient.message_type = XInternAtom(disp, "MANAGER", False);
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;
	ev.xclient.data.l[1] = _NET_SYSTEM_TRAY;
	ev.xclient.data.l[2] = bar.window;
	ev.xclient.data.l[3] = 0;
	ev.xclient.data.l[4] = 0;
	XSendEvent(disp, DefaultRootWindow(disp), False, StructureNotifyMask, &ev);
}

void cleanup_tray()
{
	for (int i = 0; i < tray_icon_count; i++) {
		Window embed = tray_icons[i];
		if (embed == 0) {
			continue;
		}

		XUnmapWindow(disp, embed);
		XReparentWindow(disp, embed, DefaultRootWindow(disp), 0, 0);
	}

	if (tray_icons) {
		free(tray_icons);
	}
}

void redraw_tray()
{
	icons_drawnn = 0;

	for (int i = 0; i < tray_icon_count; i++) {
		Window embed = tray_icons[i];

		if (embed == 0) {
			continue;
		}

		int x = settings.trayiconsize.val.INT * icons_drawnn
  			+ settings.traypadding.val.INT * icons_drawnn
  			+ settings.padding.val.INT;
		int y = settings.height.val.INT / 2 - settings.trayiconsize.val.INT / 2;

		if (settings.trayside.val.POS == RIGHT) {
			x = bars[tray_bar].width - x - settings.trayiconsize.val.INT;
		}

		trap_errors();

		XSetWindowAttributes wa;
		wa.background_pixel = settings.background.val.COL[0] << 16
			| settings.background.val.COL[1] << 8
			| settings.background.val.COL[2];

		XChangeWindowAttributes(disp, embed, CWBackPixel, &wa);

		XMoveResizeWindow(disp, embed, x, y,
				settings.trayiconsize.val.INT,
				settings.trayiconsize.val.INT);

		if (untrap_errors()) {
			tray_icons[i] = 0;
			continue;
		}

		icons_drawnn++;
	}
}

int get_tray_width()
{
	if (icons_drawnn > 0) {
		return settings.trayiconsize.val.INT * icons_drawnn
			+ settings.traypadding.val.INT * (icons_drawnn - 1)
			+ settings.padding.val.INT * 2;
	} else {
		return 0;
	}
}

static void handle_dock_request(Window embed)
{
	Window parent = bars[tray_bar].window;

	trap_errors();

	XSetWindowAttributes wa;
	wa.background_pixel = settings.background.val.COL[0] << 16
		| settings.background.val.COL[1] << 8
		| settings.background.val.COL[2];

	XChangeWindowAttributes(disp, embed, CWBackPixel, &wa);

	XChangeSaveSet(disp, embed, SetModeInsert);
	XWithdrawWindow(disp, embed, 0);
	XReparentWindow(disp, embed, parent, 0, 0);
	XSync(disp, False);

	int index = -1;

	for (int i = 0; i < tray_icon_count; i++) {
		if (tray_icons[i] == 0) {
			tray_icons[i] = embed;
			index = i;
			break;
		}
	}

	if (index == -1) {
		tray_icon_count++;
		tray_icons = realloc(tray_icons, sizeof(Window) * tray_icon_count);
		tray_icons[tray_icon_count-1] = embed;
		index = tray_icon_count-1;
	}

	xembed_send_message(index, XEMBED_EMBEDDED_NOTIFY, 0, parent,
			XEMBED_VERSION);

	XMapRaised(disp, embed);

	int err = untrap_errors();

	if (err == BadWindow) {
		return;
	}

	for (int i = 0; i < tray_icon_count; i++) {
		if (tray_icons[i] == embed && i != index) {
			tray_icons[i] = 0;
		}
	}

	redraw_tray();
}

int is_tray_event(XEvent *ev)
{
	return (ev->xclient.message_type == opcode && ev->xclient.format == 32);
}

void handle_tray_event(XEvent *ev)
{
	switch (ev->xclient.data.l[1]) {
		case SYSTEM_TRAY_REQUEST_DOCK:
			handle_dock_request(ev->xclient.data.l[2]);
			break;
	}
}

void handle_destroy_event(XEvent *ev)
{
	Window window = ev->xunmap.window;

	if (ev->type == ReparentNotify &&
			ev->xreparent.parent == bars[tray_bar].window) {
		redraw_tray();
		return;
	}

	for (int i = 0; i < tray_icon_count; i++) {
		if (tray_icons[i] == window) {
			tray_icons[i] = 0;
			break;
		}
	}

	redraw_tray();
}

void reparent_icons()
{
	for (int i = 0; i < tray_icon_count; i++) {
		Window embed = tray_icons[i];
		XWithdrawWindow(disp, embed, 0);
		XReparentWindow(disp, embed, bars[tray_bar].window, 0, 0);
		XMapRaised(disp, embed);
	}
	XSync(disp, False);
	redraw_tray();
}
