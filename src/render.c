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

#include "render.h"
#include "config.h"
#include "modules.h"
#include "tray.h"
#include "types.h"
#include "window.h"
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <ujson.h>

static void drawRect(cairo_t *ctx, int x, int y, int w, int h, int r) {
    if (r) {
        int mx [] = {w - r, r};
        int my [] = {h - r, r};
        double t = 3.14159 / 2.;
        cairo_new_sub_path(ctx);
        cairo_arc(ctx, x+mx[0], y+my[0], r,   0,   t);
        cairo_arc(ctx, x+mx[1], y+my[0], r,   t, 2*t);
        cairo_arc(ctx, x+mx[1], y+my[1], r, 2*t, 3*t);
        cairo_arc(ctx, x+mx[0], y+my[1], r, 3*t, 4*t);
        cairo_close_path(ctx);
    } else {
        cairo_rectangle(ctx, x, y, w, h);
    }
    cairo_fill(ctx);
}

static void drawDiv(int bar, cairo_t *ctx, int x) {
    int width = settings.divwidth.val.INT;
    int height;
    int y;

    if (settings.divheight.val.INT >= 0) {
        height = settings.divheight.val.INT;
        y = (bars[bar].height - height) / 2;
    } else {
        height = bars[bar].height - settings.divvertmargin.val.INT * 2;
        y = settings.divvertmargin.val.INT;
    }

    if (height <= 0 ||
        settings.divwidth.val.INT <= 0 ||
        settings.divcolor.val.COL[0] < 0 ||
        settings.divcolor.val.COL[1] < 0 ||
        settings.divcolor.val.COL[2] < 0 ||
        settings.divcolor.val.COL[3] <= 0) {
        return;
    }

    cairo_set_source_rgba(ctx,
                          settings.divcolor.val.COL[0] / 255.f,
                          settings.divcolor.val.COL[1] / 255.f,
                          settings.divcolor.val.COL[2] / 255.f,
                          settings.divcolor.val.COL[3] / 255.f);

    drawRect(ctx, x - width / 2 - 1, y, width, height, 0);
}

static void drawBlocks(int bar, int *x) {
    cairo_t *ctx;

    if (bar == trayBar) {
        x[settings.trayside.val.POS] = getTrayWidth();
    }

    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        enum Pos pos = blk->properties.pos.val.POS;

        if (!blk->id) {
            continue;
        }

        if (pos == CENTER) {
            ctx = bars[bar].ctx[RI_CENTER];
        } else {
            ctx = bars[bar].ctx[RI_BUFFER];
        }

        int rendered;

        if (blk->eachmon) {
            rendered = blk->data[bar].rendered;
        } else {
            rendered = blk->data->rendered;
        }

        if (!rendered) {
            continue;
        }

        blk->x[bar] = x[pos];

        int leftpadding = 0;
        leftpadding += settings.padding.val.INT;
        leftpadding += blk->properties.padding.val.INT;
        leftpadding += blk->properties.paddingleft.val.INT;

        int sfcx;
        if (pos == RIGHT) {
            sfcx = bars[bar].width - x[pos] - blk->width[bar] + leftpadding;
        } else {
            sfcx = x[pos] + leftpadding;
        }

        cairo_set_source_surface(ctx, blk->sfc[bar], sfcx, 0);
        cairo_paint(ctx);

        x[pos] += blk->width[bar];
    }

    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        enum Pos pos = blk->properties.pos.val.POS;

        if (!blk->id) {
            continue;
        }

        int rendered;

        if (blk->eachmon) {
            rendered = blk->data[bar].rendered;
        } else {
            rendered = blk->data->rendered;
        }

        if (!rendered) {
            continue;
        }

        if (pos == CENTER) {
            blk->x[bar] = blk->x[bar] + bars[bar].width / 2 - x[CENTER] / 2;
        } else if (pos == RIGHT) {
            blk->x[bar] = bars[bar].width - blk->x[bar] - blk->width[bar];
        }
    }
}

static void drawDivs(int bar) {
    int x [SIDES] = {0};
    struct Block *last [SIDES] = {0};

    int traywidth = getTrayWidth();

    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        enum Pos pos = blk->properties.pos.val.POS;

        if (!blk->id) {
            continue;
        }

        int rendered;
        if (blk->eachmon) {
            rendered = blk->data[bar].rendered;
        } else {
            rendered = blk->data->rendered;
        }

        if (!rendered) {
            continue;
        }

        if (pos != RIGHT || last[RIGHT] == 0) {
            last[pos] = blk;
        }
    }

    if (bar == trayBar) {
        x[settings.trayside.val.POS] = traywidth;
    }

    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];
        enum Pos pos = blk->properties.pos.val.POS;

        if (!blk->id) {
            continue;
        }

        int rendered;
        if (blk->eachmon) {
            rendered = blk->data[bar].rendered;
        } else {
            rendered = blk->data->rendered;
        }

        if (!rendered) {
            continue;
        }

        if (pos != RIGHT) {
            x[pos] += blk->width[bar];
        }

        if (!blk->properties.nodiv.val.INT && last[pos] != blk) {
            cairo_t *ctx;
            if (pos == CENTER) {
                ctx = bars[bar].ctx[RI_CENTER];
            } else {
                ctx = bars[bar].ctx[RI_BUFFER];
            }

            int divx;

            if (pos == RIGHT) {
                divx = bars[bar].width - x[pos];
            } else {
                divx = x[pos];
            }

            drawDiv(bar, ctx, divx);
        }

        if (pos == RIGHT) {
            x[pos] += blk->width[bar];
        }
    }

    if (last[settings.trayside.val.POS] && settings.traydiv.val.INT &&
            bar == trayBar && traywidth) {
        int divx;

        if (settings.trayside.val.POS == RIGHT) {
            divx = bars[bar].width - traywidth;
        } else {
            divx = traywidth;
        }

        drawDiv(bar, bars[bar].ctx[RI_BUFFER], divx);
    }
}

static void drawBar(int bar) {
    cairo_t *ctx = bars[bar].ctx[RI_CENTER];

    cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
    cairo_paint(ctx);
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);

    ctx = bars[bar].ctx[RI_BUFFER];

    cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
    cairo_paint(ctx);

    cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);

    int b = settings.borderwidth.val.INT;
    int r = settings.radius.val.INT;
    int w = bars[bar].width;
    int h = bars[bar].height;

    if (b) {
        cairo_set_source_rgba(ctx,
                              settings.bordercolor.val.COL[0]/255.f,
                              settings.bordercolor.val.COL[1]/255.f,
                              settings.bordercolor.val.COL[2]/255.f,
                              settings.bordercolor.val.COL[3]/255.f);

        drawRect(ctx, 0, 0, w, h, r);
    }

    cairo_set_source_rgba(ctx,
                          settings.background.val.COL[0]/255.f,
                          settings.background.val.COL[1]/255.f,
                          settings.background.val.COL[2]/255.f,
                          settings.background.val.COL[3]/255.f);

    if (r || b) {
        drawRect(ctx, b, b, w - b * 2, h - b * 2, r);
    } else {
        cairo_paint(ctx);
    }

    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);

    int x [SIDES] = {0};

    if (bar == trayBar) {
        x[settings.trayside.val.POS] = getTrayWidth();
    }

    drawBlocks(bar, x);
    drawDivs(bar);

    if (x[RI_CENTER]) {
        cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
        cairo_set_source_surface(ctx, bars[bar].sfc[RI_CENTER],
                bars[bar].width / 2 - x[RI_CENTER] / 2, 0);
        cairo_paint(ctx);
    }

    ctx = bars[bar].ctx[RI_VISIBLE];
    cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(ctx, bars[bar].sfc[RI_BUFFER], 0, 0);
    cairo_paint(ctx);
}

void redraw() {
    for (int i = 0; i < barCount; i++) {
        drawBar(i);
    }

    XSync(disp, False);
}

void redrawBlock(struct Block *blk) {
    if (!blk->id) {
        return;
    }

    for (int bar = 0; bar < barCount; bar++) {
        int *rendered;

        if (blk->eachmon) {
            rendered = &(blk->data[bar].rendered);
        } else {
            rendered = &(blk->data->rendered);
        }

        *rendered = 0;

        int (*func)(cairo_t *, struct Block *, int) =
            moduleGetFunction(blk->properties.module.val.STR, "render");

        if (!func) {
            continue;
        }

        cairo_t *ctx = cairo_create(blk->sfc[bar]);

        cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
        cairo_paint(ctx);
        cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);

        int width = func(ctx, blk, bar);
        cairo_destroy(ctx);

        if (width == 0) {
            continue;
        }

        width += settings.padding.val.INT * 2;
        width += blk->properties.padding.val.INT * 2;
        width += blk->properties.paddingleft.val.INT;
        width += blk->properties.paddingright.val.INT;

        *rendered = 1;

        blk->width[bar] = width;
    }
}
