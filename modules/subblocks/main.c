#include <blockbar/blockbar.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <ujson.h>

struct SubblockData {
    int blkid;
    int bar;
    int *widths;
    int count;
};

static struct SubblockData *data = 0;
static int dataCount = 0;

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
    data->name = "subblocks";

    setupFont();

    return 0;
}

void settingUpdate(struct Setting *setting) {
    if (setting == &settings.font) {
        setupFont();
    }
}

void unload() {
    if (data) {
        free(data);
    }
}

void blockRemove(struct Block *blk) {
    for (int i = 0; i < dataCount; i++) {
        struct SubblockData *sbd = &data[i];

        if (sbd->blkid == blk->id) {
            if (sbd->widths) {
                free(sbd->widths);
            }
            memset(sbd, 0, sizeof(struct SubblockData));
        }
    }
}

int exec(struct Block *blk, int bar, struct Click *cd) {
    if (cd) {
        struct SubblockData *sbd = 0;

        for (int i = 0; i < dataCount; i++) {
            struct SubblockData *sbd_ = &data[i];
            if (sbd_->blkid == blk->id && sbd_->bar == bar * blk->eachmon) {
                sbd = sbd_;
                break;
            }
        }

        if (sbd == 0) {
            return 1;
        }

        int subblock = 0;
        int x = 0;

        while (subblock < sbd->count - 1) {
            x += sbd->widths[subblock];

            if (x > cd->x - blk->x[bar]) {
                break;
            }

            subblock++;
        }

        char str [12] = {0};
        sprintf(str, "%d", subblock);
        blockbarSetEnv("SUBBLOCK", str);
    }

    return 0;
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

static int drawSubblock(cairo_t *ctx, char *str, int x, color fg,
                        color bc, int bw, int bgwidth, int bgheight,
                        int bgxpad, int bgypad, int bgrad, color bg) {
    PangoLayout *layout = pango_cairo_create_layout(ctx);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_markup(layout, str, -1);

    int width, height;
    pango_layout_get_pixel_size(layout, &width, &height);

    if (bg[3] != 0) {
        if (bgwidth <= 0) {
            bgwidth = width + 2 * bgxpad;
        } else {
            bgxpad = (bgwidth - width) / 2;
        }

        if (bgheight <= 0) {
            bgheight = settings.height.val.INT - 2 * bgypad;
        } else {
            bgypad = (settings.height.val.INT - bgheight) / 2;
        }

        if (bw) {
            cairo_set_source_rgba(ctx,
                                  bc[0]/255.f,
                                  bc[1]/255.f,
                                  bc[2]/255.f,
                                  bc[3]/255.f);

            drawRect(ctx, x, bgypad, bgwidth, bgheight, bgrad);
        }

        cairo_set_source_rgba(ctx,
                              bg[0]/255.f,
                              bg[1]/255.f,
                              bg[2]/255.f,
                              bg[3]/255.f);

        drawRect(ctx,
                 x + bw, bgypad + bw,
                 bgwidth - bw * 2, bgheight - bw * 2, bgrad);

        width = bgwidth;
        x += bgxpad;
    }

    cairo_move_to(ctx, x, settings.height.val.INT / 2 - height / 2);

    cairo_set_source_rgba(ctx,
                          fg[0] / 255.f,
                          fg[1] / 255.f,
                          fg[2] / 255.f,
                          fg[3] / 255.f);

    pango_cairo_show_layout(ctx, layout);

    g_object_unref(layout);

    return width;
}

int render(cairo_t *ctx, struct Block *blk, int bar) {
    int x = 0;
    char *execdata;
    char *exec = blk->properties.exec.val.STR;
    struct SubblockData *sbd = 0;

    if (blk->eachmon) {
        execdata = blk->data[bar].execData;
    } else {
        execdata = blk->data->execData;
    }

    if (!execdata) {
        return 0;
    }

    for (int i = 0; i < dataCount; i++) {
        struct SubblockData *sbd_ = &data[i];
        if (sbd_->blkid == blk->id && sbd_->bar == bar * blk->eachmon) {
            sbd = sbd_;
            break;
        }
    }

    if (sbd == 0) {
        for (int i = 0; i < dataCount; i++) {
            struct SubblockData *sbd_ = &data[i];
            if (sbd_->blkid == 0) {
                sbd = sbd_;

                sbd->blkid = blk->id;
                sbd->bar = bar;
                break;
            }
        }
    }

    if (sbd == 0) {
        data = realloc(data, sizeof(struct SubblockData) * ++dataCount);
        sbd = &data[dataCount - 1];

        memset(sbd, 0, sizeof(struct SubblockData));

        sbd->bar = bar;
        sbd->blkid = blk->id;
    }

    JsonError err;
    jsonErrorInit(&err);

    JsonObject *jo = jsonParseString(execdata, &err);

    if (!jo) {
        goto end;
    }

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
        goto end;
    }

    sbd->widths = realloc(sbd->widths, sizeof(int) * subblocks->used);
    sbd->count = subblocks->used;

    for (unsigned int i = 0; i < subblocks->used; i++) {
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

        color bg = {0, 0, 0, 0};
        color fg;
        color borderCol = {0, 0, 0, 255};
        int bgwidth = -1, bgheight = -1, bgxpad = 5, bgypad = 1, bgrad = 0;
        int borderwidth = 0;
        int margin = 1;

        memcpy(fg, settings.foreground.val.COL, sizeof(color));

        blockbarParseColorJson(subblock, "background", bg, &err);
        if (jsonErrorIsSet(&err)) {
            fprintf(stderr,
                    "Error parsing \"background\" array from subblock\n%s\n",
                    err.msg);
            jsonErrorCleanup(&err);
            jsonErrorInit(&err);
        }

        blockbarParseColorJson(subblock, "foreground", fg, &err);
        if (jsonErrorIsSet(&err)) {
            fprintf(stderr,
                    "Error parsing \"foreground\" array from subblock\n%s\n",
                    err.msg);
            jsonErrorCleanup(&err);
            jsonErrorInit(&err);
        }

        blockbarParseColorJson(subblock, "bordercolor", borderCol, &err);
        if (jsonErrorIsSet(&err)) {
            fprintf(stderr,
                    "Error parsing \"bordercolor\" array from subblock\n%s\n",
                    err.msg);
            jsonErrorCleanup(&err);
            jsonErrorInit(&err);
        }

        #define INT(x) \
            if (jsonGetPairIndex(subblock, #x) != -1) { \
                jsonGetInt(subblock, #x, &x, &err); \
                if (jsonErrorIsSet(&err)) { \
                    fprintf(stderr, "Error parsing \"" #x "\"string " \
                            "from subblock\n%s\n", err.msg); \
                    jsonErrorCleanup(&err); \
                    jsonErrorInit(&err); \
                } \
            }

        INT(bgwidth);
        INT(bgheight);
        INT(bgxpad);
        INT(bgypad);
        INT(bgrad);
        INT(borderwidth);
        INT(margin);

        #undef INT

        int startx = x;

        bgypad += settings.borderwidth.val.INT;

        x += drawSubblock(ctx, text, x, fg, borderCol, borderwidth,
                          bgwidth, bgheight, bgxpad, bgypad, bgrad, bg);

        if (i != subblocks->used - 1) {
            x += margin;
        }

        sbd->widths[i] = x - startx;

    }

end:
    if (jsonErrorIsSet(&err)) {
        jsonErrorCleanup(&err);
    }

    if (jo) {
        jsonCleanup(jo);
    }

    return x;
}
