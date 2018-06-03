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
#include "window.h"
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <ujson.h>

enum Pos {
    LEFT,
    RIGHT
};

static PangoFontDescription *fontDesc;

void renderInit() {
    if (conf.font) {
        fontDesc = pango_font_description_from_string(conf.font);
    }
}

static int
drawString(struct Bar *bar, const char *str, int x, int pos, color fg,
        int bgWidth, int bgHeight, int bgXPad, int bgYPad, color bg) {
    PangoLayout *layout = pango_cairo_create_layout(bar->ctx[1]);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_markup(layout, str, -1);

    int width, height;
    pango_layout_get_pixel_size(layout, &width, &height);

    if (!(bg == 0 || bg[0] < 0 || bg[1] < 0 || bg[2] < 0)) {
        cairo_set_source_rgb(bar->ctx[1], bg[0]/255.f, bg[1]/255.f, bg[2]/255.f);

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

        cairo_rectangle(bar->ctx[1], x, bgYPad, bgWidth, bgHeight);
        cairo_fill(bar->ctx[1]);

        width = bgWidth;
        x += bgXPad;
    } else {
        if (pos == RIGHT) {
            x = bar->width - x - width;
        }
    }

    cairo_set_source_rgb(bar->ctx[1], fg[0]/255.f, fg[1]/255.f, fg[2]/255.f);
    cairo_move_to(bar->ctx[1], x, bar->height/2 - height/2);
    pango_cairo_show_layout(bar->ctx[1], layout);

    g_object_unref(layout);

    return width;
}

static int
drawLegacyBlock(struct Block *blk, int x, enum Pos pos, int bar) {
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
    color col = {0xff, 0xff, 0xff};

    int j = 0;
    int len = strlen(data);
    for (int i = 0; i < len; i++) {
        if (data[i] == '\0') {
            break;
        }

        if (data[i] == '\n') {
            if (j == 0) {
                shortText = data + i + 1;
            } else if (j == 1 && len - i - 3 == 6) {
                long colLong = strtol(data+i+2, 0, 16);

                col[0] = colLong >> 16;
                col[1] = (colLong >> 8) & 0xFF;
                col[2] = colLong & 0xFF;
            }
            data[i] = 0;
            j++;
        }
    }

    x += conf.padding + blk->padding + blk->padOut;

    if (pos == LEFT && blk->label && strcmp(blk->label, "") != 0) {
        x += drawString(&bars[bar], blk->label, x, pos, col, 0, 0, 0, 0, 0);
    }

    x += drawString(&bars[bar], longText, x, pos, col, 0, 0, 0, 0, 0);

    if (pos == RIGHT && blk->label && strcmp(blk->label, "") != 0) {
        x += drawString(&bars[bar], blk->label, x, pos, col, 0, 0, 0, 0, 0);
    }

    x += conf.padding + blk->padding + blk->padIn;

    free(data);

    return x;
}

static int drawSubblocks(struct Block *blk, int x, enum Pos pos, int bar) {
    char *data;
    char *exec = blk->exec;

    if (blk->eachmon) {
        data = blk->data.mon[bar].type.subblock.execData;
    } else {
        data = blk->data.type.subblock.execData;
    }

    JsonError err;
    JSON_ERROR_INIT(err);

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

    for (int i = 0; i < subblocks->used; i++) {
        void *val = subblocks->vals[i];
        if (jsonGetType(val) != JSON_OBJECT) {
            fprintf(stderr, "Expecting object in \"subblocks\" array\n");
            goto end;
        }

        JsonObject *subblock = (JsonObject *) val;

        char *text = "";
        if (jsonGetPairIndex(subblock, "text") != -1) {
            jsonGetString(subblock, "text", &text, &err);
            if (jsonErrorIsSet(&err)) {
                fprintf(stderr,
                        "Error parsing \"text\" string from subblock\n%s\n",
                        err.msg);
                goto end;
            }
        }

        color bg = {-1, -1, -1};
        color fg = {0xff, 0xff, 0xff};
        int bgwidth = -1, bgheight = -1, bgxpad = -1, bgypad = -1;

        parseColor(subblock, "background", bg, &err);
        parseColor(subblock, "foreground", fg, &err);

        #define INT(x) \
            if (jsonGetPairIndex(subblock, #x) != -1) { \
                jsonGetInt(subblock, #x, &x, &err); \
                if (jsonErrorIsSet(&err)) { \
                    fprintf(stderr, "Error parsing \"" #x  "\"string " \
                            "from subblock\n%s\n", err.msg); \
                } \
            }


        INT(bgwidth);
        INT(bgheight);
        INT(bgxpad);
        INT(bgypad);

        #undef INT

        x += drawString(&bars[bar], text, x, pos, fg,
                bgwidth, bgheight, bgxpad, bgypad, bg) + 1;
    }

end:
    jsonCleanup(jo);
    return x + 2;
}

static void drawBlocks(struct Block *blks, int blkCount, enum Pos pos) {
    for (int i = 0; i < barCount; i++) {
        int x = 0;

        for (int j = 0; j < blkCount; j++) {
            struct Block *blk = &blks[j];

            char *execData;
            if (blk->eachmon) {
                execData = blk->data.mon[i].type.legacy.execData;
            } else {
                execData = blk->data.type.legacy.execData;
            }

            if (execData == 0 && blk->label == 0) {
                continue;
            }

            if ((execData && strcmp(execData, "") == 0) ||
                    (blk->label && strcmp(blk->label, "") == 0)) {
                continue;
            }

            int divx = x;

            if (execData) {
                if (blk->mode == LEGACY) {
                    x = drawLegacyBlock(blk, x, pos, i);
                } else {
                    x = drawSubblocks(blk, x, pos, i);
                }
            } else if (blk->label) {
                x += conf.padding + blk->padding + blk->padOut;

                color col = {0xff, 0xff, 0xff};
                x += drawString(&bars[i], blk->label, x, pos, col,
                        0, 0, 0, 0, 0);

                x += conf.padding + blk->padding + blk->padIn;
            }

            if (!blk->nodiv && j != 0) {
                cairo_set_source_rgb(bars[i].ctx[1], 0.2f, 0.2f, 0.2f);

                if (pos == RIGHT) {
                    divx = bars[i].width - divx;
                }

                cairo_rectangle(bars[i].ctx[1], divx, 4, 1, bars[i].height-8);
                cairo_fill(bars[i].ctx[1]);

                x++;
            }
        }
    }
}

void redraw() {
    for (int i = 0; i < barCount; i++) {
        cairo_t *ctx = bars[i].ctx[1];
        cairo_set_source_rgba(ctx,
                conf.bg[0]/255.f,
                conf.bg[1]/255.f,
                conf.bg[2]/255.f, 1);
        cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
        cairo_paint(ctx);
    }

    drawBlocks(leftBlocks, leftBlockCount, LEFT);
    drawBlocks(rightBlocks, rightBlockCount, RIGHT);

    for (int i = 0; i < barCount; i++) {
        cairo_set_source_surface(bars[i].ctx[0], bars[i].sfc[1], 0, 0);
        cairo_paint(bars[i].ctx[0]);
    }

    XSync(disp, False);
}
