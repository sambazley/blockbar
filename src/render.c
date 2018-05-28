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
#include "window.h"
#include <pango/pangocairo.h>

enum {
    LEFT,
    RIGHT
} Pos;

static PangoFontDescription *fontDesc;

void renderInit() {
    if (conf.font) {
        fontDesc = pango_font_description_from_string(conf.font);
    }
}

static void
drawString(struct Bar *bar, const char *str, int x, int pos, color col) {
    PangoLayout *layout = pango_cairo_create_layout(bar->ctx[1]);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_markup(layout, str, -1);

    int width, height;
    pango_layout_get_pixel_size(layout, &width, &height);

    int xpos = 0;
    switch (pos) {
        case LEFT:
            xpos = x;
            break;
        case RIGHT:
            xpos = bar->width - x - width;
            break;
    }

    cairo_set_source_rgb(bar->ctx[1], col[0]/255.f, col[1]/255.f, col[2]/255.f);
    cairo_move_to(bar->ctx[1], xpos, bar->height/2 - height/2);
    pango_cairo_show_layout(bar->ctx[1], layout);

    g_object_unref(layout);
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

    for (int i = 0; i < barCount; i++) {
        cairo_set_source_surface(bars[i].ctx[0], bars[i].sfc[1], 0, 0);
        cairo_paint(bars[i].ctx[0]);
    }

    XSync(disp, False);
}
