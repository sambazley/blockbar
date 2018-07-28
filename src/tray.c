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
    trappedErrorCode = err->error_code;
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
xembedSendMessage(Window w, long msg, long detail, long d1, long d2) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));

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
        fprintf(stderr, "xembed error %d\n", err);
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

void trayCleanup() {
    for (int i = 0; i < trayIconCount; i++) {
        Window embed = trayIcons[i];
        if (embed == 0) {
            continue;
        }

        XUnmapWindow(disp, embed);
        XReparentWindow(disp, embed, DefaultRootWindow(disp), 0, 0);
        XSync(disp, False);
    }
}

void redrawTray() {
    iconsDrawn = 0;

    for (int i = 0; i < trayIconCount; i++) {
        Window embed = trayIcons[i];

        if (embed == 0) {
            continue;
        }

        int x = conf.trayIconSize*iconsDrawn + conf.trayPadding*(iconsDrawn+1);
        int y = bars[trayBar].height / 2 - conf.trayIconSize / 2;

        if (conf.traySide == RIGHT) {
            x = bars[trayBar].width - x - conf.trayIconSize;
        }

        trapErrors();

        XSetWindowAttributes o;
        o.background_pixel = conf.bg[0] << 16
                           | conf.bg[1] << 8
                           | conf.bg[2];
        XChangeWindowAttributes(disp, embed, CWBackPixel, &o);

        XMoveResizeWindow(disp, embed, x, y,
                conf.trayIconSize, conf.trayIconSize);

        if (untrapErrors()) {
            trayIcons[i] = 0;
            continue;
        }

        iconsDrawn++;
    }
}

int getTrayWidth() {
    if (iconsDrawn > 0) {
        return (conf.trayIconSize + conf.trayPadding)*iconsDrawn + conf.padding;
    } else {
        return 0;
    }
}

static void handleDockRequest(Window embed) {
    trapErrors();

    XChangeSaveSet(disp, embed, SetModeInsert);
    XWithdrawWindow(disp, embed, 0);
    XReparentWindow(disp, embed, bars[trayBar].window, 0, 0);
    XSync(disp, False);

    XSetWindowAttributes o;
    o.background_pixel = conf.bg[0] << 16
                       | conf.bg[1] << 8
                       | conf.bg[2];
    XChangeWindowAttributes(disp, embed, CWBackPixel, &o);

    int inserted = 0;

    for (int i = 0; i < trayIconCount; i++) {
        if (trayIcons[i] == 0) {
            trayIcons[i] = embed;
            inserted = 1;
            break;
        }
    }

    if (!inserted) {
        trayIconCount++;
        trayIcons = realloc(trayIcons, sizeof(embed) * trayIconCount);
        trayIcons[trayIconCount-1] = embed;
    }

    xembedSendMessage(embed, XEMBED_EMBEDDED_NOTIFY, 0, bars[trayBar].window,
            XEMBED_VERSION);

    XMapRaised(disp, embed);

    int err = untrapErrors();

    if (err == BadWindow) {
        return;
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

    if (ev->xreparent.parent == bars[trayBar].window) {
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
    XSync(disp, 0);
    redrawTray();
}
