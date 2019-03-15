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

static Visual *visual;

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

    XVisualInfo vinfo;
    XMatchVisualInfo(disp, s, 32, TrueColor, &vinfo);

    visual = vinfo.visual;

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

        XSetWindowAttributes wa;
        wa.override_redirect = True;
        wa.colormap = XCreateColormap(disp, root, visual, AllocNone);
        wa.border_pixel = 0;

        bar->window = XCreateWindow(disp, root, 0, 0, 10, 10,
                0, vinfo.depth, InputOutput, visual,
                CWOverrideRedirect | CWColormap | CWBorderPixel,
                &wa);

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
        classhint->res_name = "blockbar";
        classhint->res_class = "blockbar";
        XSetClassHint(disp, bar->window, classhint);
        XFree(classhint);

        XSelectInput(disp, bar->window,
                ButtonPressMask | SubstructureNotifyMask);

        bar->sfc[RI_VISIBLE] = 0;
        bar->sfc[RI_BUFFER] = 0;
        bar->sfc[RI_CENTER] = 0;
        bar->ctx[RI_VISIBLE] = 0;
        bar->ctx[RI_BUFFER] = 0;
        bar->ctx[RI_CENTER] = 0;
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

        int top = 1;
        if (strcmp(settings.position.val.STR, "bottom") == 0) {
            top = 0;
        }

        int x = crtcInfo->x + settings.marginhoriz.val.INT
                            + settings.xoffset.val.INT;

        int y = top ? settings.marginvert.val.INT
                         : crtcInfo->height
                         - settings.height.val.INT
                         - settings.marginvert.val.INT;

        int width = crtcInfo->width - settings.marginhoriz.val.INT * 2;

        XMoveResizeWindow(disp, bar->window, x, y,
                          width, settings.height.val.INT);

        bar->x = x;
        bar->width = width;
        bar->height = settings.height.val.INT;

        for (int ri = 0; ri < RI_COUNT; ri++) {
            if (bar->sfc[ri]) {
                cairo_surface_destroy(bar->sfc[ri]);
            }

            if (bar->ctx[ri]) {
                cairo_destroy(bar->ctx[ri]);
            }

            if (ri == 0) {
                bar->sfc[0] = cairo_xlib_surface_create(disp, bar->window,
                    visual, bar->width, bar->height);
            } else {
                bar->sfc[ri] = cairo_surface_create_similar_image(bar->sfc[0],
                    CAIRO_FORMAT_ARGB32, bar->width, bar->height);
            }

            bar->ctx[ri] = cairo_create(bar->sfc[ri]);
        }

        XRRFreeOutputInfo(oputInfo);
        XRRFreeCrtcInfo(crtcInfo);

        XMapWindow(disp, bar->window);

        b++;
    }

    XRRFreeScreenResources(res);
    XFlush(disp);

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (mod->dl && mod->data.type == RENDER) {
            resizeModule(mod);
        }
    }
}

static void click(struct Click *cd) {
    int cx [SIDES] = {0};
    int centerWidth = 0;

    struct Block *block = 0;

    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];

        if (!blk->id) {
            continue;
        }

        int rendered;
        if (blk->eachmon) {
            rendered = blk->data[cd->bar].rendered;
        } else {
            rendered = blk->data->rendered;
        }

        if (rendered && blk->properties.pos.val.POS == CENTER) {
            centerWidth += blk->width[cd->bar];
        }
    }

    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        enum Pos pos = blk->properties.pos.val.POS;

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

        int rx = 0;
        if (pos == LEFT) {
            rx = cd->x;
        } else if (pos == CENTER) {
            rx = cd->x - bars[cd->bar].width / 2 + centerWidth / 2;
        } else if (pos == RIGHT) {
            rx = bars[cd->bar].width - cd->x - 1;
        }

        if (cd->bar == trayBar && pos == settings.trayside.val.POS) {
            rx -= getTrayWidth();
        }

        if (rx < 0) {
            continue;
        }

        cx[pos] += blk->width[cd->bar];

        if (cx[pos] > rx) {
            block = blk;

            break;
        }
    }

    if (!block) return;

    blockExec(block, cd);
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

    free(bars);
}

int blockbarGetBarWidth(int bar) {
    return bars[bar].width;
}
