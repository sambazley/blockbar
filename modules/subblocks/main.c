#include <blockbar/blockbar.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <ujson.h>

struct subblock_data {
	int blkid;
	int bar;
	int *widths;
	int count;
};

static struct subblock_data *data = 0;
static int data_count = 0;

static PangoFontDescription *font_desc = 0;

static void setup_font()
{
	struct bar_settings *bar_settings = blockbar_get_settings();

	if (font_desc) {
		pango_font_description_free(font_desc);
	}

	if (bar_settings->font.val.STR) {
		font_desc = pango_font_description_from_string(bar_settings->font.val.STR);
	}
}

int init(struct module_data *data)
{
	data->name = "subblocks";

	setup_font();

	return 0;
}

void setting_update(struct setting *setting)
{
	struct bar_settings *bar_settings = blockbar_get_settings();

	if (setting == &bar_settings->font) {
		setup_font();
	}
}

void unload()
{
	if (data) {
		free(data);
	}
}

void block_remove(struct block *blk)
{
	for (int i = 0; i < data_count; i++) {
		struct subblock_data *sbd = &data[i];

		if (sbd->blkid == blk->id) {
			if (sbd->widths) {
				free(sbd->widths);
			}
			memset(sbd, 0, sizeof(struct subblock_data));
		}
	}
}

int exec(struct block *blk, int bar, struct click *cd)
{
	if (cd) {
		struct bar_settings *settings = blockbar_get_settings();
		struct subblock_data *sbd = 0;

		for (int i = 0; i < data_count; i++) {
			struct subblock_data *sbd_ = &data[i];
			if (sbd_->blkid == blk->id && sbd_->bar == bar * blk->eachmon) {
				sbd = sbd_;
				break;
			}
		}

		if (sbd == 0) {
			return 1;
		}

		int subblock = 0;
		int x = blk->x[bar]
			  + blk->properties.padding.val.INT
			  + blk->properties.paddingleft.val.INT
			  + settings->padding.val.INT;

		while (subblock < sbd->count - 1) {
			x += sbd->widths[subblock];

			if (x > cd->x) {
				break;
			}

			subblock++;
		}

		char str [12] = {0};
		sprintf(str, "%d", subblock);
		blockbar_set_env("SUBBLOCK", str);
	}

	return 0;
}

static void draw_rect(cairo_t *ctx, int x, int y, int w, int h, int r)
{
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

static int draw_subblock(cairo_t *ctx, char *str, int x, color fg,
		color bc, int bw, int bgwidth, int bgheight,
		int bgxpad, int bgypad, int bgrad, color bg)
{
	struct bar_settings *bar_settings = blockbar_get_settings();

	PangoLayout *layout = pango_cairo_create_layout(ctx);
	pango_layout_set_font_description(layout, font_desc);
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
			bgheight = bar_settings->height.val.INT - 2 * bgypad;
		} else {
			bgypad = (bar_settings->height.val.INT - bgheight) / 2;
		}

		if (bw) {
			cairo_set_source_rgba(ctx,
					bc[0]/255.f,
					bc[1]/255.f,
					bc[2]/255.f,
					bc[3]/255.f);

			draw_rect(ctx, x, bgypad, bgwidth, bgheight, bgrad);
		}

		cairo_set_source_rgba(ctx,
				bg[0]/255.f,
				bg[1]/255.f,
				bg[2]/255.f,
				bg[3]/255.f);

		draw_rect(ctx, x + bw, bgypad + bw,
				bgwidth - bw * 2, bgheight - bw * 2, bgrad);

		width = bgwidth;
		x += bgxpad;
	}

	cairo_move_to(ctx, x, bar_settings->height.val.INT / 2 - height / 2);

	cairo_set_source_rgba(ctx,
						  fg[0] / 255.f,
						  fg[1] / 255.f,
						  fg[2] / 255.f,
						  fg[3] / 255.f);

	pango_cairo_show_layout(ctx, layout);

	g_object_unref(layout);

	return width;
}

int render(cairo_t *ctx, struct block *blk, int bar)
{
	int x = 0;
	char *execdata;
	char *exec = blk->properties.exec.val.STR;
	struct subblock_data *sbd = 0;

	struct bar_settings *bar_settings = blockbar_get_settings();

	if (blk->eachmon) {
		execdata = blk->data[bar].exec_data;
	} else {
		execdata = blk->data->exec_data;
	}

	if (!execdata) {
		return 0;
	}

	for (int i = 0; i < data_count; i++) {
		struct subblock_data *sbd_ = &data[i];
		if (sbd_->blkid == blk->id && sbd_->bar == bar * blk->eachmon) {
			sbd = sbd_;
			break;
		}
	}

	if (sbd == 0) {
		for (int i = 0; i < data_count; i++) {
			struct subblock_data *sbd_ = &data[i];
			if (sbd_->blkid == 0) {
				sbd = sbd_;

				sbd->blkid = blk->id;
				sbd->bar = bar;
				break;
			}
		}
	}

	if (sbd == 0) {
		data = realloc(data, sizeof(struct subblock_data) * ++data_count);
		sbd = &data[data_count - 1];

		memset(sbd, 0, sizeof(struct subblock_data));

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
		color border_col = {0, 0, 0, 255};
		int bgwidth = -1, bgheight = -1, bgxpad = 5, bgypad = 1, bgrad = 0;
		int borderwidth = 0;
		int margin = 1;

		memcpy(fg, bar_settings->foreground.val.COL, sizeof(color));

		blockbar_parse_color_json(subblock, "background", bg, &err);
		if (jsonErrorIsSet(&err)) {
			fprintf(stderr, "Error parsing \"background\" array from subblock\n%s\n",
					err.msg);
			jsonErrorCleanup(&err);
			jsonErrorInit(&err);
		}

		blockbar_parse_color_json(subblock, "foreground", fg, &err);
		if (jsonErrorIsSet(&err)) {
			fprintf(stderr, "Error parsing \"foreground\" array from subblock\n%s\n",
					err.msg);
			jsonErrorCleanup(&err);
			jsonErrorInit(&err);
		}

		blockbar_parse_color_json(subblock, "bordercolor", border_col, &err);
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

		bgypad += bar_settings->borderwidth.val.INT;

		x += draw_subblock(ctx, text, x, fg, border_col, borderwidth,
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
