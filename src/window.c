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
#include "blocks.h"
#include "config.h"
#include "exec.h"
#include "tray.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

Display *disp;
int barCount;
struct Bar *bars;

#define ATOM(x) Atom x = XInternAtom(disp, #x, False)

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

        int y = conf.top ? 0 : crtcInfo->height - conf.height;

        bar->window = XCreateWindow(disp, root,
                crtcInfo->x, y, crtcInfo->width, conf.height,
                0, 0, CopyFromParent, CopyFromParent, 0, NULL);

        bar->width = crtcInfo->width;
        bar->height = conf.height;

        bar->output = malloc(oputInfo->nameLen + 1);
        strcpy(bar->output, oputInfo->name);

        bar->sfc[0] = cairo_xlib_surface_create(disp, bar->window,
                DefaultVisual(disp, s), bar->width, bar->height);
        bar->sfc[1] = cairo_surface_create_similar_image(bar->sfc[0],
                CAIRO_FORMAT_RGB24, bar->width, bar->height);

        bar->ctx[0] = cairo_create(bar->sfc[0]);
        bar->ctx[1] = cairo_create(bar->sfc[1]);

        XRRFreeOutputInfo(oputInfo);
        XRRFreeCrtcInfo(crtcInfo);

        XChangeProperty(disp, bar->window, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                PropModeAppend, (unsigned char *) &_NET_WM_WINDOW_TYPE_DOCK, 1);

        XChangeProperty(disp, bar->window, _NET_WM_WINDOW_STATE, XA_ATOM, 32,
                PropModeAppend, (unsigned char *) &_NET_WM_STATE_STICKY, 1);

        XChangeProperty(disp, bar->window, _NET_WM_WINDOW_STATE, XA_ATOM, 32,
                PropModeAppend, (unsigned char *) &_NET_WM_STATE_BELOW, 1);

        XClassHint *classhint = XAllocClassHint();
        classhint->res_name = "blockbar";
        classhint->res_class = "blockbar";
        XSetClassHint(disp, bar->window, classhint);
        XFree(classhint);

        XSelectInput(disp, bar->window,
                ButtonPressMask | SubstructureNotifyMask);

        XMapWindow(disp, bar->window);
    }
    XRRFreeScreenResources(res);
    XFlush(disp);

    return 0;
}

void updateGeom() {
    int s = DefaultScreen(disp);
    Window root = RootWindow(disp, s);

    XRRScreenResources *res = XRRGetScreenResources(disp, root);

    int b = 0;
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *oputInfo = XRRGetOutputInfo(disp, res, res->outputs[i]);

        if (!oputInfo->crtc) {
            XRRFreeOutputInfo(oputInfo);
            continue;
        }

        XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(disp, res, oputInfo->crtc);
        struct Bar *bar = &bars[b];

        int y = conf.top ? 0 : crtcInfo->height - conf.height;

        XMoveResizeWindow(disp, bar->window, crtcInfo->x, y,
                crtcInfo->width, conf.height);

        bar->width = crtcInfo->width;
        bar->height = conf.height;

        cairo_surface_destroy(bar->sfc[0]);
        cairo_surface_destroy(bar->sfc[1]);
        cairo_destroy(bar->ctx[0]);
        cairo_destroy(bar->ctx[1]);

        bar->sfc[0] = cairo_xlib_surface_create(disp, bar->window,
                DefaultVisual(disp, s), bar->width, bar->height);
        bar->sfc[1] = cairo_surface_create_similar_image(bar->sfc[0],
                CAIRO_FORMAT_RGB24, bar->width, bar->height);

        bar->ctx[0] = cairo_create(bar->sfc[0]);
        bar->ctx[1] = cairo_create(bar->sfc[1]);

        XRRFreeOutputInfo(oputInfo);
        XRRFreeCrtcInfo(crtcInfo);

        b++;
    }
}

static void click(struct Click *cd) {
    int cx [SIDES] = {0};
    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];

        if (!blk->id) {
            continue;
        }

        int rendered;
        if (blk->eachmon) {
            rendered = blk->data.mon[cd->bar].type.legacy.rendered;
        } else {
            rendered = blk->data.type.legacy.rendered;
        }

        if (!rendered) {
            continue;
        }

        int rx;
        if (blk->pos == RIGHT) {
            rx = bars[cd->bar].width - cd->x;
        } else {
            rx = cd->x;
        }

        if (cd->bar == trayBar && blk->pos == conf.traySide) {
            rx -= getTrayWidth();
        }

        if (rx < 0) {
            continue;
        }

        cx[blk->pos] += blk->width[cd->bar];

        if (cx[blk->pos] > rx) {
            cd->block = blk;

            if (blk->mode == SUBBLOCK) {
                int subblockCount;
                if (blk->eachmon) {
                    subblockCount =
                        blk->data.mon[cd->bar].type.subblock.subblockCount;
                } else {
                    subblockCount = blk->data.type.subblock.subblockCount;
                }

                int sbx = cx[blk->pos] - blk->width[cd->bar];
                for (int j = 0; j < subblockCount; j++) {
                    if (blk->eachmon) {
                        sbx += blk->data.mon[cd->bar].type.subblock.widths[j];
                    } else {
                        sbx += blk->data.type.subblock.widths[j];
                    }

                    if (sbx > rx) {
                        cd->subblock = j;
                        break;
                    }
                }
            }

            goto found;
        }
    }

    if (!cd->block) return;

found:
    blockExec(cd->block, cd);
}

void pollEvents() {
    XEvent ev;
    while (XPending(disp)) {
        XNextEvent(disp, &ev);
        switch (ev.type) {
            case ButtonPress:
                {
                    int bar;
                    for (bar = 0; bar < barCount; bar++) {
                        if (bars[bar].window == ev.xbutton.window) break;
                    }

                    struct Click cd = {
                        .button = ev.xbutton.button,
                        .x = ev.xbutton.x,
                        .bar = bar,
                    };

                    click(&cd);
                }
                break;
            case ClientMessage:
                if (isTrayEvent(&ev)) {
                    handleTrayEvent(&ev);
                }
                break;
            case ReparentNotify:
            case DestroyNotify:
                handleDestroyEvent(&ev);
                break;
            case SubstructureNotifyMask:
                printf("substructure\n");
                break;
        }
    }
}

void cleanupBars() {
    for (int i = 0; i < barCount; i++) {
        struct Bar *bar = &bars[i];

        free(bar->output);
        cairo_surface_destroy(bar->sfc[0]);
        cairo_surface_destroy(bar->sfc[1]);
        cairo_destroy(bar->ctx[0]);
        cairo_destroy(bar->ctx[1]);
        XDestroyWindow(disp, bar->window);
    }
}
