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
#include "blocks.h"
#include "config.h"
#include "tray.h"
#include "window.h"
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <ujson.h>

static PangoFontDescription *fontDesc;

static int shortMode;

void renderInit() {
    if (conf.font) {
        fontDesc = pango_font_description_from_string(conf.font);
    }
}

static int
drawString(struct Bar *bar, const char *str, int x, int pos, color fg,
        int bgWidth, int bgHeight, int bgXPad, int bgYPad, color bg) {

    cairo_t *ctx;

    if (pos == RI_CENTER) {
        ctx = bar->ctx[RI_CENTER];
    } else {
        ctx = bar->ctx[RI_BUFFER];
    }

    PangoLayout *layout = pango_cairo_create_layout(ctx);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_markup(layout, str, -1);

    int width, height;
    pango_layout_get_pixel_size(layout, &width, &height);

    if (!(bg == 0 || bg[0] < 0 || bg[1] < 0 || bg[2] < 0 || bg[3] < 0)) {
        cairo_set_source_rgba(ctx,
                              bg[0]/255.f,
                              bg[1]/255.f,
                              bg[2]/255.f,
                              bg[3]/255.f);

        if (bgWidth <= 0) {
            if (bgXPad <= 0) {
                bgXPad = 5;
            }

            bgWidth = width + 2*bgXPad;
        } else {
            bgXPad = (bgWidth - width) / 2;
        }

        if (bgHeight <= 0) {
            if (bgYPad <= 0) {
                bgYPad = 1;
            }

            bgHeight = bar->height - 2*bgYPad;
        } else {
            bgYPad = (bar->height - bgHeight) / 2;
        }

        if (pos == RIGHT) {
            x = bar->width - x - bgWidth;
        }

        cairo_rectangle(ctx, x, bgYPad, bgWidth, bgHeight);
        cairo_fill(ctx);

        width = bgWidth;
        x += bgXPad;
    } else {
        if (pos == RIGHT) {
            x = bar->width - x - width;
        }
    }

    cairo_set_source_rgba(ctx,
                          fg[0]/255.f,
                          fg[1]/255.f,
                          fg[2]/255.f,
                          fg[3]/255.f);
    cairo_move_to(ctx, x, bar->height/2 - height/2);
    pango_cairo_show_layout(ctx, layout);

    g_object_unref(layout);

    return width;
}

static int drawLegacyBlock(struct Block *blk, int x, int bar) {
    char *dataOrig;

    if (blk->eachmon) {
        dataOrig = blk->data.mon[bar].type.legacy.execData;
    } else {
        dataOrig = blk->data.type.legacy.execData;
    }

    char *data = malloc(strlen(dataOrig) + 1);
    strcpy(data, dataOrig);

    char *longText = data;
    char *shortText = data;
    color col = {conf.fg[0], conf.fg[1], conf.fg[2], conf.fg[3]};

    int j = 0;
    int len = strlen(data);

    for (int i = 0; i < len; i++) {
        if (data[i] == '\0') {
            break;
        }

        if (data[i] == '\n') {
            if (j == 0) {
                shortText = data + i + 1;
            } else if (j == 1 && data[i + 1] == '#') {
                char str [9] = "00000000";
                int colLen = len - i - 2;
                if (colLen == 3 || colLen == 4 || colLen == 6 || colLen == 8) {
                    strncpy(str, data+i+2, colLen+1);
                    parseColorString(str, col);
                }
            }
            data[i] = 0;
            j++;
        }
    }

    x += conf.padding + blk->padding;

    if (blk->pos == RIGHT) {
        x += blk->padRight;
    } else {
        x += blk->padLeft;
    }

    int dl = shortMode ? conf.shortLabels : 1;

    if (dl && (blk->pos == LEFT || blk->pos == CENTER)
            && blk->label && strcmp(blk->label, "") != 0) {
        x += drawString(&bars[bar], blk->label, x, blk->pos, col, 0,0,0,0,0);
    }

    char *text;
    if (shortMode) {
        text = shortText;
    } else {
        text = longText;
    }

    x += drawString(&bars[bar], text, x, blk->pos, col, 0,0,0,0,0);

    if (dl && blk->pos == RIGHT && blk->label && strcmp(blk->label, "") != 0) {
        x += drawString(&bars[bar], blk->label, x, blk->pos, col, 0,0,0,0,0);
    }

    x += conf.padding + blk->padding;

    if (blk->pos == RIGHT) {
        x += blk->padLeft;
    } else {
        x += blk->padRight;
    }

    free(data);

    return x;
}

static int drawSubblocks(struct Block *blk, int x, int bar) {
    char *data;
    char *exec = blk->exec;
    int **widths;
    int *subblockCount;

    if (blk->eachmon) {
        data = blk->data.mon[bar].type.subblock.execData;
        widths = &(blk->data.mon[bar].type.subblock.widths);
        subblockCount = &(blk->data.mon[bar].type.subblock.subblockCount);
    } else {
        data = blk->data.type.subblock.execData;
        widths = &(blk->data.type.subblock.widths);
        subblockCount = &(blk->data.type.subblock.subblockCount);
    }

    JsonError err;
    jsonErrorInit(&err);

    JsonObject *jo = jsonParseString(data, &err);

    if (!jo) return x;

    if (jsonErrorIsSet(&err)) {
        fprintf(stderr, "Invalid subblock data (exec=%s)\n%s\n",
                exec, err.msg);
        goto end;
    }

    if (jsonGetPairIndex(jo, "subblocks") == -1) {
        goto end;
    }

    JsonArray *subblocks;
    jsonGetArray(jo, "subblocks", &subblocks, &err);
    if (jsonErrorIsSet(&err)) {
        fprintf(stderr, "Error parsing \"subblocks\" array\n%s\n", err.msg);
        goto end;
    }

    if (subblocks->used == 0) {
        return x;
    }

    *widths = realloc(*widths, sizeof(int) * subblocks->used);
    *subblockCount = subblocks->used;

    for (int i = 0; i < subblocks->used; i++) {
        void *val = subblocks->vals[i];
        if (jsonGetType(val) != JSON_OBJECT) {
            fprintf(stderr, "Expecting object in \"subblocks\" array\n");
            break;
        }

        JsonObject *subblock = (JsonObject *) val;

        char *text = "";
        if (jsonGetPairIndex(subblock, "text") != -1) {
            jsonGetString(subblock, "text", &text, &err);
            if (jsonErrorIsSet(&err)) {
                fprintf(stderr,
                        "Error parsing \"text\" string from subblock\n%s\n",
                        err.msg);
                jsonErrorCleanup(&err);
                jsonErrorInit(&err);
            }
        }

        color bg = {-1, -1, -1, -1};
        color fg = {conf.fg[0], conf.fg[1], conf.fg[2], conf.fg[3]};
        int bgwidth = -1, bgheight = -1, bgxpad = -1, bgypad = -1;

        parseColorJson(subblock, "background", bg, &err);
        if (jsonErrorIsSet(&err)) {
            fprintf(stderr,
                    "Error parsing \"background\" array form subblock\n%s\n",
                    err.msg);
            jsonErrorCleanup(&err);
            jsonErrorInit(&err);
        }

        parseColorJson(subblock, "foreground", fg, &err);
        if (jsonErrorIsSet(&err)) {
            fprintf(stderr,
                    "Error parsing \"foreground\" array form subblock\n%s\n",
                    err.msg);
            jsonErrorCleanup(&err);
            jsonErrorInit(&err);
        }

        #define INT(x) \
            if (jsonGetPairIndex(subblock, #x) != -1) { \
                jsonGetInt(subblock, #x, &x, &err); \
                if (jsonErrorIsSet(&err)) { \
                    fprintf(stderr, "Error parsing \"" #x  "\"string " \
                            "from subblock\n%s\n", err.msg); \
                    jsonErrorCleanup(&err); \
                    jsonErrorInit(&err); \
                } \
            }

        INT(bgwidth);
        INT(bgheight);
        INT(bgxpad);
        INT(bgypad);

        #undef INT

        int startx = x;

        if (i == 0) {
            if (blk->pos == RIGHT) {
                x += blk->padRight;
            } else {
                x += blk->padLeft;
            }
        }

        x += drawString(&bars[bar], text, x, blk->pos, fg,
                bgwidth, bgheight, bgxpad, bgypad, bg) + 1;

        if (i == *subblockCount - 1) {
            if (blk->pos == RIGHT) {
                x += 2 + blk->padLeft;
            } else {
                x += 2 + blk->padRight;
            }
        }

        (*widths)[i] = x - startx;
    }

end:
    jsonCleanup(jo);
    return x;
}

static void drawDiv(int i, cairo_t *ctx, int x) {
    int height;
    int y;

    if (conf.divHeight >= 0) {
        height = conf.divHeight;
        y = (bars[i].height - conf.divHeight) / 2;
    } else {
        height = bars[i].height - conf.divVertMarg * 2;
        y = conf.divVertMarg;
    }

    if (height <= 0 || conf.divWidth <= 0 || conf.divCol[3] <= 0) {
        return;
    }

    cairo_set_source_rgba(ctx,
                          conf.divCol[0] / 255.f,
                          conf.divCol[1] / 255.f,
                          conf.divCol[2] / 255.f,
                          conf.divCol[3] / 255.f);

    cairo_rectangle(ctx, x, y, conf.divWidth, height);
    cairo_fill(ctx);
}

static int drawBlocks(int i, int *x) {
    cairo_t *ctx;

    int last [SIDES];
    memset(last, -1, sizeof(int) * SIDES);

    if (i == trayBar) {
        x[conf.traySide] = getTrayWidth();
    }

    for (int j = 0; j < blockCount; j++) {
        struct Block *blk = &blocks[j];

        if (!blk->id) {
            continue;
        }

        if (blk->pos == CENTER) {
            ctx = bars[i].ctx[RI_CENTER];
        } else {
            ctx = bars[i].ctx[RI_BUFFER];
        }

        char *execData;
        int *rendered;
        if (blk->eachmon) {
            execData = blk->data.mon[i].type.legacy.execData;
            rendered = &(blk->data.mon[i].type.legacy.rendered);
        } else {
            execData = blk->data.type.legacy.execData;
            rendered = &(blk->data.type.legacy.rendered);
        }
        *rendered = 0;

        if (execData == 0 && blk->label == 0) {
            continue;
        }

        if (execData && strcmp(execData, "") == 0) {
            continue;
        }

        int prex = x[blk->pos];

        if (execData) {
            if (blk->mode == LEGACY) {
                x[blk->pos] = drawLegacyBlock(blk, x[blk->pos], i);
            } else {
                x[blk->pos] = drawSubblocks(blk, x[blk->pos], i);
            }
        } else if (blk->label && (shortMode ? conf.shortLabels : 1)) {
            x[blk->pos] += conf.padding + blk->padding;
            if (blk->pos == RIGHT) {
                x[blk->pos] += blk->padRight;
            } else {
                x[blk->pos] += blk->padLeft;
            }

            x[blk->pos] += drawString(&bars[i], blk->label, x[blk->pos],
                    blk->pos, conf.fg, 0, 0, 0, 0, 0);

            x[blk->pos] += conf.padding + blk->padding;
            if (blk->pos == RIGHT) {
                x[blk->pos] += blk->padLeft;
            } else {
                x[blk->pos] += blk->padRight;
            }
        }

        *rendered = prex != x[blk->pos];

        blk->width[i] = x[blk->pos] - prex;

        if ((blk->pos != RIGHT || last[RIGHT] == -1) && *rendered) {
            last[blk->pos] = j;
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
        dx[conf.traySide] = getTrayWidth();
    }

    for (int j = 0; j < blockCount; j++) {
        struct Block *blk = &blocks[j];

        if (!blk->id) {
            continue;
        }

        int rendered;

        if (blk->eachmon) {
            rendered = blk->data.mon[i].type.legacy.rendered;
        } else {
            rendered = blk->data.type.legacy.rendered;
        }

        if (!rendered) {
            continue;
        }

        if (blk->pos != RIGHT) {
            dx[blk->pos] += blk->width[i];
        }

        if (!blk->nodiv && last[blk->pos] != j) {
            if (blk->pos == CENTER) {
                ctx = bars[i].ctx[RI_CENTER];
            } else {
                ctx = bars[i].ctx[RI_BUFFER];
            }

            int divx = dx[blk->pos];

            if (blk->pos == RIGHT) {
                divx = bars[i].width - dx[blk->pos];
            } else {
                divx = dx[blk->pos];
            }

            drawDiv(i, ctx, divx);
        }

        if (blk->pos == RIGHT) {
            dx[blk->pos] += blk->width[i];
        }
    }

    if (last[conf.traySide] != -1 && conf.trayDiv) {
        int divx;

        if (conf.traySide == RIGHT) {
            divx = bars[i].width - getTrayWidth();
        } else {
            divx = getTrayWidth();
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

    cairo_set_source_rgba(ctx,
                          conf.bg[0]/255.f,
                          conf.bg[1]/255.f,
                          conf.bg[2]/255.f,
                          conf.bg[3]/255.f);

    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);

    int r = conf.radius;
    int w = bars[i].width;
    int h = bars[i].height;
    double pi = 3.14159;

    if (r) {
        int x [] = {w - r, r};
        int y [] = {h - r, r};
        double t = pi / 2.;
        cairo_new_sub_path(ctx);
        cairo_arc(ctx, x[0], y[0], r,   0,   t);
        cairo_arc(ctx, x[1], y[0], r,   t, 2*t);
        cairo_arc(ctx, x[1], y[1], r, 2*t, 3*t);
        cairo_arc(ctx, x[0], y[1], r, 3*t, 4*t);
        cairo_close_path(ctx);
        cairo_fill(ctx);
    } else {
        cairo_paint(ctx);
    }

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
