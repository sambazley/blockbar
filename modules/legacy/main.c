#include <blockbar/blockbar.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static PangoFontDescription *fontDesc = 0;

static void setupFont() {
    if (fontDesc) {
        pango_font_description_free(fontDesc);
    }

    if (settings.font.val.STR) {
        fontDesc = pango_font_description_from_string(settings.font.val.STR);
    }
}

int init(struct ModuleData *data) {
    data->name = "legacy";

    setupFont();

    return 0;
}

void settingUpdate(struct Setting *setting) {
    if (setting == &settings.font) {
        setupFont();
    }
}

static int drawString(cairo_t *ctx, char *str, int x, color col) {
    PangoLayout *layout = pango_cairo_create_layout(ctx);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_markup(layout, str, -1);

    int width, height;

    pango_layout_get_pixel_size(layout, &width, &height);

    cairo_move_to(ctx, x, settings.height.val.INT / 2 - height / 2);

    cairo_set_source_rgba(ctx,
                          col[0] / 255.f,
                          col[1] / 255.f,
                          col[2] / 255.f,
                          col[3] / 255.f);

    pango_cairo_show_layout(ctx, layout);

    g_object_unref(layout);

    return width;
}

int render(cairo_t *ctx, struct Block *blk, int bar, int shortMode) {
    int x = 0;
    char *execdata;

    if (blk->eachmon) {
        execdata = blk->data[bar].execData;
    } else {
        execdata = blk->data->execData;
    }

    char *data = malloc(strlen(execdata) + 1);
    strcpy(data, execdata);

    char *longText = data;
    char *shortText = data;
    color col;
    memcpy(col, settings.foreground.val.COL, sizeof(color));

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
                    blockbarParseColorString(str, col);
                }
            }
            data[i] = 0;
            j++;
        }
    }

    char *text;
    if (shortMode) {
        text = shortText;
    } else {
        text = longText;
    }

    x += drawString(ctx, text, x, col);

    free(data);

    return x;
}
