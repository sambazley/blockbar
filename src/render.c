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

static PangoFontDescription *fontDesc;

static int shortMode;

void renderInit() {
    if (settings.font.val.STR) {
        fontDesc = pango_font_description_from_string(settings.font.val.STR);
    }
}

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

int drawString(int bar, const char *str, int x, int pos, color fg) {
    cairo_t *ctx;

    if (pos == RI_CENTER) {
        ctx = bars[bar].ctx[RI_CENTER];
    } else {
        ctx = bars[bar].ctx[RI_BUFFER];
    }

    PangoLayout *layout = pango_cairo_create_layout(ctx);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_markup(layout, str, -1);

    int width, height;
    pango_layout_get_pixel_size(layout, &width, &height);

    if (pos == RIGHT) {
        x = bars[bar].width - x - width;
    }

    cairo_set_source_rgba(ctx,
                          fg[0]/255.f,
                          fg[1]/255.f,
                          fg[2]/255.f,
                          fg[3]/255.f);
    cairo_move_to(ctx, x, bars[bar].height/2 - height/2);
    pango_cairo_show_layout(ctx, layout);

    g_object_unref(layout);

    return width;
}

static void drawDiv(int i, cairo_t *ctx, int x) {
    int width = settings.divwidth.val.INT;
    int height;
    int y;

    if (settings.divheight.val.INT >= 0) {
        height = settings.divheight.val.INT;
        y = (bars[i].height - height) / 2;
    } else {
        height = bars[i].height - settings.divvertmargin.val.INT * 2;
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

static int drawBlocks(int i, int *x) {
    cairo_t *ctx;

    int last [SIDES];
    memset(last, -1, sizeof(int) * SIDES);

    if (i == trayBar) {
        x[settings.trayside.val.POS] = getTrayWidth();
    }

    for (int j = 0; j < blockCount; j++) {
        struct Block *blk = &blocks[j];
        enum Pos pos = blk->properties.pos.val.POS;

        if (!blk->id) {
            continue;
        }

        if (pos == CENTER) {
            ctx = bars[i].ctx[RI_CENTER];
        } else {
            ctx = bars[i].ctx[RI_BUFFER];
        }

        char *execData;
        int *rendered;
        if (blk->eachmon) {
            execData = blk->data[i].execData;
            rendered = &(blk->data[i].rendered);
        } else {
            execData = blk->data->execData;
            rendered = &(blk->data->rendered);
        }

        *rendered = 0;

        if ((execData == 0 || strcmp(execData, "") == 0) &&
                strcmp(blk->properties.label.val.STR, "") == 0) {
            continue;
        }

        int prex = x[pos];

        if (pos == RIGHT) {
            x[pos] += 1;
        }

        x[pos] += settings.padding.val.INT + blk->properties.padding.val.INT;

        if (pos == RIGHT) {
            x[pos] += blk->properties.paddingright.val.INT;
        } else {
            x[pos] += blk->properties.paddingleft.val.INT;
        }

        if (execData) {
            int (*func)(cairo_t *, struct Block *, int, int) =
                moduleGetFunction(blk->properties.module.val.STR, "render");

            if (func) {
                cairo_surface_t *sfc = cairo_surface_create_similar_image(
                        bars[i].sfc[0], CAIRO_FORMAT_ARGB32,
                        bars[i].width, bars[i].height);

                cairo_t *ctx_ = cairo_create(sfc);

                int width = func(ctx_, blk, i, shortMode);

                int sfcx = x[pos];

                if (width == 0) {
                    cairo_surface_destroy(sfc);
                    cairo_destroy(ctx_);

                    x[pos] = prex;
                    blk->x[i] = -1;
                    blk->width[i] = 0;
                    continue;
                }

                *rendered = 1;

                if (pos == RIGHT) {
                    sfcx = bars[i].width - sfcx - width;
                }

                cairo_set_source_surface(ctx, sfc, sfcx, 0);
                cairo_paint(ctx);

                cairo_surface_destroy(sfc);
                cairo_destroy(ctx_);

                x[pos] += width;
            }
        } else if (blk->properties.label.val.STR &&
                (shortMode ? settings.shortlabels.val.INT : 1)) {

            x[pos] += drawString(i, blk->properties.label.val.STR,
                    x[pos], pos, settings.foreground.val.COL);
        }

        x[pos] += settings.padding.val.INT + blk->properties.padding.val.INT;

        if (pos == RIGHT) {
            x[pos] += blk->properties.paddingleft.val.INT;
        } else {
            x[pos] += blk->properties.paddingright.val.INT;
        }

        if (pos != RIGHT) {
            x[pos] += 1;
        }

        blk->width[i] = x[pos] - prex;
        if (pos == RIGHT) {
            blk->x[i] = bars[i].width - x[pos];
        } else {
            blk->x[i] = prex;
        }

        if (pos != RIGHT || last[RIGHT] == -1) {
            last[pos] = j;
        }
    }

    if (!shortMode && x[CENTER]) {
        if (MAX(x[LEFT], x[RIGHT]) >= bars[i].width / 2 - x[CENTER] / 2) {
            shortMode = 1;
            return 1;
        }
    } else if (!shortMode && x[RIGHT] + x[LEFT] >= bars[i].width) {
        shortMode = 1;
        return 1;
    }

    shortMode = 0;

    int dx[SIDES] = {0};

    if (i == trayBar) {
        dx[settings.trayside.val.POS] = getTrayWidth();
    }

    for (int j = 0; j < blockCount; j++) {
        struct Block *blk = &blocks[j];
        enum Pos pos = blk->properties.pos.val.POS;

        if (!blk->id) {
            continue;
        }

        int rendered;

        if (blk->eachmon) {
            rendered = blk->data[i].rendered;
        } else {
            rendered = blk->data->rendered;
        }

        if (!rendered) {
            continue;
        }

        if (pos != RIGHT) {
            dx[pos] += blk->width[i];
        }

        if (pos == CENTER) {
            blk->x[i] = blk->x[i] + bars[i].width / 2 - x[CENTER] / 2;
        }

        if (!blk->properties.nodiv.val.INT && last[pos] != j) {
            if (pos == CENTER) {
                ctx = bars[i].ctx[RI_CENTER];
            } else {
                ctx = bars[i].ctx[RI_BUFFER];
            }

            int divx = dx[pos];

            if (pos == RIGHT) {
                divx = bars[i].width - dx[pos];
            } else {
                divx = dx[pos];
            }

            drawDiv(i, ctx, divx);
        }

        if (pos == RIGHT) {
            dx[pos] += blk->width[i];
        }
    }

    int trayWidth = getTrayWidth();
    if (last[settings.trayside.val.POS] != -1 && settings.traydiv.val.INT &&
            i == trayBar && trayWidth) {
        int divx;

        if (settings.trayside.val.POS == RIGHT) {
            divx = bars[i].width - trayWidth;
        } else {
            divx = trayWidth;
        }

        drawDiv(i, bars[i].ctx[RI_BUFFER], divx);
    }

    return 0;
}

static void drawBar(int i) {
    cairo_t *ctx = bars[i].ctx[RI_CENTER];

    cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
    cairo_paint(ctx);
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);

    ctx = bars[i].ctx[RI_BUFFER];

    cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
    cairo_paint(ctx);

    cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);

    int b = settings.borderwidth.val.INT;
    int r = settings.radius.val.INT;
    int w = bars[i].width;
    int h = bars[i].height;

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

    if (drawBlocks(i, x) == 0) {
        if (x[RI_CENTER]) {
            cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
            cairo_set_source_surface(ctx, bars[i].sfc[RI_CENTER],
                                     bars[i].width /2 - x[RI_CENTER] / 2, 0);
            cairo_paint(ctx);
        }

        ctx = bars[i].ctx[RI_VISIBLE];
        cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(ctx, bars[i].sfc[RI_BUFFER], 0, 0);
        cairo_paint(ctx);
    } else {
        drawBar(i);
    }
}

void redraw() {
    for (int i = 0; i < barCount; i++) {
        drawBar(i);
    }

    XSync(disp, False);
}
