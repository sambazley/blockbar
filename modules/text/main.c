#include <blockbar/blockbar.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	data->name = "text";

	setup_font();

	return 0;
}

void setting_update(struct setting *setting)
{
	struct bar_settings *bar_settings = blockbar_get_settings();
	if (setting == &(bar_settings->font)) {
		setup_font();
	}
}

int exec(struct block *blk, int bar, struct click *cd)
{
	(void) blk;
	(void) bar;

	if (cd) {
		if (cd->button <= 0) {
			return 1;
		}
	}

	return 0;
}

static int draw_string(cairo_t *ctx, char *str, color col)
{
	struct bar_settings *bar_settings = blockbar_get_settings();

	PangoLayout *layout = pango_cairo_create_layout(ctx);
	pango_layout_set_font_description(layout, font_desc);
	pango_layout_set_markup(layout, str, -1);

	int width, height;

	pango_layout_get_pixel_size(layout, &width, &height);

	cairo_move_to(ctx, 0, bar_settings->height.val.INT / 2 - height / 2);

	cairo_set_source_rgba(ctx,
						  col[0] / 255.f,
						  col[1] / 255.f,
						  col[2] / 255.f,
						  col[3] / 255.f);

	pango_cairo_show_layout(ctx, layout);

	g_object_unref(layout);

	return width;
}

int render(cairo_t *ctx, struct block *blk, int bar)
{
	struct bar_settings *bar_settings = blockbar_get_settings();

	char *execdata;

	if (blk->eachmon) {
		execdata = blk->data[bar].exec_data;
	} else {
		execdata = blk->data->exec_data;
	}

	if (!execdata) {
		return 0;
	}

	char *data = malloc(strlen(execdata) + 1);
	strcpy(data, execdata);

	color col;
	memcpy(col, bar_settings->foreground.val.COL, sizeof(color));

	int len = strlen(data);

	for (int i = 0; i < len; i++) {
		if (data[i] == '\0') {
			break;
		}

		if (data[i] == '\n') {
			if (data[i + 1] == '#') {
				char str [9] = "00000000";
				int col_len = len - i - 2;
				if (col_len == 3 || col_len == 4 || col_len == 6 || col_len == 8) {
					strncpy(str, data+i+2, col_len+1);
					blockbar_parse_color_string(str, col);
				}
			}
			data[i] = 0;
		}
	}

	int width = draw_string(ctx, data, col);

	free(data);

	return width;
}
