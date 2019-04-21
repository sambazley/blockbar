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

static void drawDiv(int bar, int x) {
    cairo_t *ctx = bars[bar].ctx[RI_BUFFER];
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
        settings.divcolor.val.COL[3] == 0) {
        return;
    }

    cairo_set_source_rgba(ctx,
                          settings.divcolor.val.COL[0] / 255.f,
                          settings.divcolor.val.COL[1] / 255.f,
                          settings.divcolor.val.COL[2] / 255.f,
                          settings.divcolor.val.COL[3] / 255.f);

    drawRect(ctx, x - width / 2 - 1, y, width, height, 0);
}

static void drawModules(int bar, int above) {
    cairo_t *ctx = bars[bar].ctx[RI_BUFFER];

    int lastZ = 0;
    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (!mod->dl || mod->data.type != RENDER) {
            continue;
        }

        if ((mod->zindex < lastZ && mod->zindex < 0 && !above) ||
            ((mod->zindex < lastZ || lastZ == 0) && mod->zindex > 0 && above)) {
            lastZ = mod->zindex;
        }
    }

    if (!lastZ) {
        return;
    }

    lastZ--;

    while (1) {
        int minZ = 0;
        struct Module *mod = 0;

        for (int i = 0; i < moduleCount; i++) {
            struct Module *_mod = &modules[i];

            if (!_mod->dl || _mod->data.type != RENDER) {
                continue;
            }

            if (_mod->zindex > lastZ && (_mod->zindex < minZ || minZ == 0)) {
                mod = _mod;
                lastZ = _mod->zindex;
                minZ = _mod->zindex;
            }
        }

        if (!mod) {
            break;
        }

        if (!mod->sfc) {
            continue;
        }

        cairo_set_source_surface(ctx, mod->sfc[bar], 0, 0);
        cairo_paint(ctx);
    }
}

static void drawBlocks(int bar) {
    cairo_t *ctx = bars[bar].ctx[RI_BUFFER];

    for (int i = 0; i < blockCount; i++) {
        struct Block *blk = &blocks[i];

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

        int leftpad = 0;
        leftpad += settings.padding.val.INT;
        leftpad += blk->properties.padding.val.INT;
        leftpad += blk->properties.paddingleft.val.INT;

        cairo_set_source_surface(ctx, blk->sfc[bar], blk->x[bar] + leftpad, 0);
        cairo_paint(ctx);
    }
}

static void drawDivs(int bar) {
    struct Block *last [SIDES] = {0};
    int x [SIDES] = {0};

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

        x[pos] += blk->width[bar];
    }

    int maxWidth = bars[bar].width / 2 - x[CENTER] / 2;

    if (x[LEFT] > maxWidth) {
        last[LEFT] = last[CENTER];
    } else if (x[RIGHT] > maxWidth) {
        last[CENTER] = last[RIGHT];
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

        if (!blk->properties.nodiv.val.INT && last[pos] != blk) {
            drawDiv(bar, blk->x[bar] + blk->width[bar]);
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

        drawDiv(bar, divx);
    }
}

static void calculateBlockX(int bar) {
    int x [SIDES] = {0};

    if (bar == trayBar) {
        x[settings.trayside.val.POS] = getTrayWidth();
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

        blk->x[bar] = x[pos];
        x[pos] += blk->width[bar];
    }

    enum Pos overlap = CENTER;
    int maxWidth = bars[bar].width / 2 - x[CENTER] / 2;

    if (x[LEFT] > maxWidth) {
        overlap = LEFT;
    } else if (x[RIGHT] > maxWidth) {
        overlap = RIGHT;
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

        if (pos == CENTER && overlap == CENTER) {
            blk->x[bar] = blk->x[bar] + bars[bar].width / 2 - x[CENTER] / 2;
        } else if (pos == RIGHT) {
            blk->x[bar] = bars[bar].width - blk->x[bar] - blk->width[bar];
        }
    }

    for (int i = 0; i < blockCount * (overlap != CENTER); i++) {
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

        if (pos != CENTER) {
            continue;
        }

        if (overlap == LEFT) {
            blk->x[bar] = x[LEFT] + blk->x[bar];
        } else if (overlap == RIGHT) {
            blk->x[bar] = bars[bar].width - x[RIGHT] - x[CENTER] + blk->x[bar];
        }
    }
}

static void drawBar(int bar) {
    cairo_t *ctx = bars[bar].ctx[RI_BUFFER];

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

    calculateBlockX(bar);

    for (int i = 0; i < moduleCount; i++) {
        struct Module *mod = &modules[i];

        if (!mod->dl || mod->data.type != RENDER) {
            continue;
        }

        cairo_t *_ctx = cairo_create(mod->sfc[bar]);
        cairo_set_operator(_ctx, CAIRO_OPERATOR_CLEAR);
        cairo_paint(_ctx);
        cairo_set_operator(_ctx, CAIRO_OPERATOR_OVER);

        int (*func)(cairo_t *, int) =
            moduleGetFunction(mod, "render");

        func(_ctx, bar);
        cairo_destroy(_ctx);
    }

    drawModules(bar, 0);

    drawBlocks(bar);
    drawDivs(bar);

    drawModules(bar, 1);

    ctx = bars[bar].ctx[RI_VISIBLE];
    cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(ctx, bars[bar].sfc[RI_BUFFER], 0, 0);
    cairo_paint(ctx);
}

void redraw() {
    for (int i = 0; i < barCount; i++) {
        drawBar(i);
    }
}

void redrawBlock(struct Block *blk) {
    for (int bar = 0; bar < barCount; bar++) {
        int *rendered;

        if (blk->eachmon) {
            rendered = &(blk->data[bar].rendered);
        } else {
            rendered = &(blk->data->rendered);
        }

        *rendered = 0;

        struct Module *mod = getModuleByName(blk->properties.module.val.STR);

        if (!mod) {
            continue;
        }

        int (*func)(cairo_t *, struct Block *, int) =
            moduleGetFunction(mod, "render");

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
