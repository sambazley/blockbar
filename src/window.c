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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

static Display *disp;

static int barCount;
static struct Bar *bars;

#define ATOM(x) Atom x = XInternAtom(disp, #x, False);

int createBars() {
    disp = XOpenDisplay(NULL);
    if (!disp) {
        fprintf(stderr, "Error opening display\n");
        return 1;
    }

    int s = DefaultScreen(disp);
    Window root = RootWindow(disp, s);

    XRRScreenResources *res = XRRGetScreenResources(disp, root);

    barCount = 0;

    ATOM(_NET_WM_WINDOW_TYPE);
    ATOM(_NET_WM_WINDOW_TYPE_DOCK);
    ATOM(_NET_WM_WINDOW_STATE);
    ATOM(_NET_WM_STATE_STICKY);
    ATOM(_NET_WM_STATE_BELOW);

    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *oputInfo = XRRGetOutputInfo(disp, res, res->outputs[i]);

        if (!oputInfo->crtc) {
            XRRFreeOutputInfo(oputInfo);
            continue;
        }

        barCount++;
        bars = realloc(bars, sizeof(struct Bar) * barCount);

        XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(disp, res, oputInfo->crtc);

        struct Bar *bar = &bars[barCount-1];

        bar->window = XCreateWindow(disp, root,
                crtcInfo->x, 0, crtcInfo->width, conf.height,
                0, 0, CopyFromParent, CopyFromParent, 0, NULL);

        bar->width = crtcInfo->width;
        bar->height = conf.height;

        bar->output = malloc(oputInfo->nameLen + 1);
        strcpy(bar->output, oputInfo->name);

        XRRFreeOutputInfo(oputInfo);
        XRRFreeCrtcInfo(crtcInfo);

        XChangeProperty(disp, bar->window, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                PropModeAppend, (unsigned char *) &_NET_WM_WINDOW_TYPE_DOCK, 1);

        XChangeProperty(disp, bar->window, _NET_WM_WINDOW_STATE, XA_ATOM, 32,
                PropModeAppend, (unsigned char *) &_NET_WM_STATE_STICKY, 1);

        XChangeProperty(disp, bar->window, _NET_WM_WINDOW_STATE, XA_ATOM, 32,
                PropModeAppend, (unsigned char *) &_NET_WM_STATE_BELOW, 1);

        XClassHint *classhint = XAllocClassHint();
        classhint->res_name = "BlockBar";
        classhint->res_class = "BlockBar";
        XSetClassHint(disp, bar->window, classhint);
        XFree(classhint);

        XSelectInput(disp, bar->window, ButtonPressMask);

        XMapWindow(disp, bar->window);
    }
    XRRFreeScreenResources(res);
    XFlush(disp);

    return 0;
}

void pollEvents() {
    XEvent ev;
    while (XPending(disp)) {
        XNextEvent(disp, &ev);
        switch (ev.type) {
        }
    }
}

void cleanupBars() {
    for (int i = 0; i < barCount; i++) {
        struct Bar *bar = &bars[i];

        free(bar->output);

        XDestroyWindow(disp, bar->window);
    }
}
