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

static int trappedErrorCode = 0;
static int (*oldErrorHandler)(Display *, XErrorEvent *);

static Atom opcode;

int trayBar;

static Window *trayIcons;
static int trayIconCount;
static int iconsDrawn;

static int errorHandler(Display *disp, XErrorEvent *err) {
    (void) disp;

    if (err->error_code != 0) {
        trappedErrorCode = err->error_code;
    }
    return 0;
}

static void trapErrors() {
    trappedErrorCode = 0;
    oldErrorHandler = XSetErrorHandler(errorHandler);
}

static int untrapErrors() {
    XSetErrorHandler(oldErrorHandler);
    return trappedErrorCode;
}

static void
xembedSendMessage(int index, long msg, long detail, long d1, long d2) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));

    Window w = trayIcons[index];

    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = XInternAtom(disp, "_XEMBED", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = msg;
    ev.xclient.data.l[2] = detail;
    ev.xclient.data.l[3] = d1;
    ev.xclient.data.l[4] = d2;

    trapErrors();

    XSendEvent(disp, w, False, NoEventMask, &ev);
    XSync(disp, False);

    int err = untrapErrors();

    if (err) {
        trayIcons[index] = 0;
    }
}

void trayInit(int barIndex) {
    char atomName [22];
    snprintf(atomName, 21, "_NET_SYSTEM_TRAY_S%d", DefaultScreen(disp));

    Atom _NET_SYSTEM_TRAY = XInternAtom(disp, atomName, False);
    opcode = XInternAtom(disp, "_NET_SYSTEM_TRAY_OPCODE", False);

    trayBar = barIndex;
    struct Bar bar = bars[barIndex];

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

void cleanupTray() {
    for (int i = 0; i < trayIconCount; i++) {
        Window embed = trayIcons[i];
        if (embed == 0) {
            continue;
        }

        XUnmapWindow(disp, embed);
        XReparentWindow(disp, embed, DefaultRootWindow(disp), 0, 0);
    }

    if (trayIcons) {
        free(trayIcons);
    }
}

void redrawTray() {
    iconsDrawn = 0;

    for (int i = 0; i < trayIconCount; i++) {
        Window embed = trayIcons[i];

        if (embed == 0) {
            continue;
        }

        int x = settings.trayiconsize.val.INT * iconsDrawn
              + settings.traypadding.val.INT * iconsDrawn
              + settings.padding.val.INT;
        int y = settings.height.val.INT / 2 - settings.trayiconsize.val.INT / 2;

        if (settings.trayside.val.POS == RIGHT) {
            x = bars[trayBar].width - x - settings.trayiconsize.val.INT;
        }

        trapErrors();

        XSetWindowAttributes wa;
        wa.background_pixel = settings.background.val.COL[0] << 16
                            | settings.background.val.COL[1] << 8
                            | settings.background.val.COL[2];

        XChangeWindowAttributes(disp, embed, CWBackPixel, &wa);

        XMoveResizeWindow(disp, embed, x, y,
                settings.trayiconsize.val.INT, settings.trayiconsize.val.INT);

        if (untrapErrors()) {
            trayIcons[i] = 0;
            continue;
        }

        iconsDrawn++;
    }
}

int getTrayWidth() {
    if (iconsDrawn > 0) {
        return settings.trayiconsize.val.INT * iconsDrawn
             + settings.traypadding.val.INT * (iconsDrawn - 1)
             + settings.padding.val.INT * 2;
    } else {
        return 0;
    }
}

static void handleDockRequest(Window embed) {
    Window parent = bars[trayBar].window;

    trapErrors();

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

    for (int i = 0; i < trayIconCount; i++) {
        if (trayIcons[i] == 0) {
            trayIcons[i] = embed;
            index = i;
            break;
        }
    }

    if (index == -1) {
        trayIconCount++;
        trayIcons = realloc(trayIcons, sizeof(Window) * trayIconCount);
        trayIcons[trayIconCount-1] = embed;
        index = trayIconCount-1;
    }

    xembedSendMessage(index, XEMBED_EMBEDDED_NOTIFY, 0, parent,
            XEMBED_VERSION);

    XMapRaised(disp, embed);

    int err = untrapErrors();

    if (err == BadWindow) {
        return;
    }

    for (int i = 0; i < trayIconCount; i++) {
        if (trayIcons[i] == embed && i != index) {
            trayIcons[i] = 0;
        }
    }

    redrawTray();
}

int isTrayEvent(XEvent *ev) {
    return (ev->xclient.message_type == opcode && ev->xclient.format == 32);
}

void handleTrayEvent(XEvent *ev) {
    switch (ev->xclient.data.l[1]) {
        case SYSTEM_TRAY_REQUEST_DOCK:
            handleDockRequest(ev->xclient.data.l[2]);
            break;
    }
}

void handleDestroyEvent(XEvent *ev) {
    Window window = ev->xunmap.window;

    if (ev->type == ReparentNotify &&
            ev->xreparent.parent == bars[trayBar].window) {
        redrawTray();
        return;
    }

    for (int i = 0; i < trayIconCount; i++) {
        if (trayIcons[i] == window) {
            trayIcons[i] = 0;
            break;
        }
    }

    redrawTray();
}

void reparentIcons() {
    for (int i = 0; i < trayIconCount; i++) {
        Window embed = trayIcons[i];
        XWithdrawWindow(disp, embed, 0);
        XReparentWindow(disp, embed, bars[trayBar].window, 0, 0);
        XMapRaised(disp, embed);
    }
    XSync(disp, False);
    redrawTray();
}
