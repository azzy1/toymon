#include "tm_x.h"
#include "tm_main.h"
#include <stdlib.h>
#include <pango/pangocairo.h>
#include <librsvg/rsvg.h>
#include <math.h>
#include <xcb/shape.h>
#include <string.h>

struct tm_x {
	/* User configuration variables. */
	const char		*display;
	u32			x_bg;
	u32			x_fg;
	s16			x;
	s16			y;
	u16			width;
	u16			height;
	int			cairo_antialias;
	double			cairo_line_width;
	double			dashes;
	double			icon_scale_factor;
	double			side_icon_scale_factor;
	const char		*font_desc;

	/* Other variables. */
	xcb_connection_t	*c;
	xcb_visualtype_t	*v;
	xcb_window_t		win;
	cairo_t			*cr;
	cairo_surface_t		*surface;
	PangoLayout		*layout;
	int			font_max_height;
	int			font_dot_width;
	int			font_max_digit_width;
	int			font_max_unit_width;
	double			margin;

	pthread_t		tid_wait_ev;
};

static struct tm_x *tm_x(struct tm_context *tc)
{
	return tm_get_object(tc, TM_OBJECT_X);
}

void tm_x_flush(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	cairo_surface_flush(x->surface);
	xcb_flush(x->c);
}

u32 tm_x_get_color_from_str(const char *s)
{
	int ret;
	u32 col;

	if (*s == '#')
		s++;

	col = 0;

	ret = sscanf(s, "%x", &col);
	if (ret != 1)
		fprintf(stderr, "Can't get color: %s\n", s);

	return col;
}

/* Return width each objects can use. */
int tm_x_width(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	return x->width - tm_x_margin(tc) * 2 - tm_x_margin_icon(tc) * 2;
}

double tm_x_margin(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	return x->margin;
}

double tm_x_margin_icon(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	return x->margin / 2;
}

void tm_x_text_size(struct tm_context *tc, const char *s, size_t len,
		    double *width, double *height)
{
	PangoLayout *layout;
	struct tm_x *x;
	int wid, hei;

	x = tm_x(tc);
	layout = x->layout;

	if (len) {
		pango_layout_set_text(layout, s, len);
		pango_layout_get_pixel_size(layout, &wid, &hei);
	} else {
		wid = hei = 0;
	}

	if (width)
		*width = (double)wid;
	if (height)
		*height = (double)hei;
}

int tm_x_font_max_height(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	return x->font_max_height;
}

int tm_x_font_dot_width(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	return x->font_dot_width;
}

int tm_x_font_max_digit_width(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	return x->font_max_digit_width;
}

int tm_x_font_max_unit_width(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	return x->font_max_unit_width;
}

void tm_x_translate(struct tm_context *tc, double dx, double dy)
{
	struct tm_x *x;

	x = tm_x(tc);

	if (dx || dy)
		cairo_translate(x->cr, dx, dy);
}

int __tm_x_load_icon(struct tm_context *tc, const char *file, int type,
		     struct tm_icon *icon)
{
	double scale, icon_scale_factor;
	int err, ret, width, height;
	RsvgDimensionData icon_dim;
	cairo_surface_t *surface;
	cairo_status_t status;
	cairo_matrix_t mat;
	RsvgHandle *rhdle;
	struct tm_x *x;
	cairo_t *cr;

	err = 1;
	x = tm_x(tc);
	icon_scale_factor = type & TM_ICON_MAIN ? x->icon_scale_factor :
						  x->side_icon_scale_factor;

	rhdle = rsvg_handle_new_from_file(file, NULL);
	if (!rhdle) {
		fprintf(stderr, "rsvg_handle_new_from_file failed.\n");
		goto out;
	}

	rsvg_handle_get_dimensions(rhdle, &icon_dim);
	scale = x->font_max_height * icon_scale_factor / icon_dim.height;
	width = (int)ceil(icon_dim.width * scale);
	height = (int)ceil(icon_dim.height * scale);

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width,
					     height);
	status = cairo_surface_status(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "cairo_image_surface_create failed.\n");
		goto err_unref_rhdle;
	}

	cr = cairo_create(surface);
	status = cairo_status(cr);
	cairo_surface_destroy(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "cairo_create failed.\n");
		goto err_unref_rhdle;
	}

	mat = (cairo_matrix_t){
		.xx	= scale,
		.yy	= scale
	};

	if (type & TM_ICON_FLIP) {
		mat.xx = -mat.xx;
		mat.x0 = icon_dim.width * scale;
	}

	cairo_transform(cr, &mat);

	ret = rsvg_handle_render_cairo(rhdle, cr);
	if (!ret) {
		fprintf(stderr, "rsvg_handle_render_cairo failed.\n");
		goto err_cr_destroy;

	}
	cairo_surface_flush(surface);

	/* Now icon image is stored in cairo image surface, so no need to keep
	 * rhdle at this point.
	 */
	g_object_unref(rhdle);

	icon->cr = cr;
	icon->surface = surface;
	icon->width = width;
	icon->height = height;

	err = 0;
out:
	return err;
err_cr_destroy:
	cairo_destroy(cr);
err_unref_rhdle:
	g_object_unref(rhdle);
	goto out;
}

void tm_x_unload_icon(struct tm_icon *icon)
{
	cairo_destroy(icon->cr);
}

static double tm_x_get_col(u32 col, int shift)
{
	return ((col >> shift) & 0xff) / 255.0;
}

static double tm_x_get_red(u32 col)
{
	return tm_x_get_col(col, 16);
}

static double tm_x_get_green(u32 col)
{
	return tm_x_get_col(col, 8);
}

static double tm_x_get_blue(u32 col)
{
	return tm_x_get_col(col, 0);
}

static void tm_x_set_source_rgb(cairo_t *cr, u32 col)
{
	cairo_set_source_rgb(cr, tm_x_get_red(col), tm_x_get_green(col),
			     tm_x_get_blue(col));
}

static void tm_x_clear_area(struct tm_x *x, double pos_x, double pos_y,
			    double width, double height)
{
	cairo_t *cr;

	cr = x->cr;

	if (!width || !height)
		return;

	cairo_rectangle(cr, pos_x, pos_y, width, height);
#if 0
	tm_x_set_source_rgb(cr, item->bg);
#else
	tm_x_set_source_rgb(cr, x->x_bg);
#endif
	cairo_fill(cr);
}

void tm_x_draw_icon(struct tm_context *tc, struct tm_icon *icon, double pos_x,
		    double pos_y)
{
	struct tm_x *x;
	cairo_t *cr;

	x = tm_x(tc);
	cr = x->cr;

	tm_x_clear_area(x, pos_x, pos_y, icon->width, icon->height);

	cairo_set_source_surface(cr, icon->surface, pos_x, pos_y);
	cairo_paint(cr);
	cairo_set_source_surface(cr, x->surface, 0, 0);
}

static void tm_x_set_dash(struct tm_x *x)
{
	cairo_set_dash(x->cr, &x->dashes, 1, 0);
}

void tm_x_draw_line(struct tm_context *tc, double pos_x, double pos_y)
{
	struct tm_x *x;
	cairo_t *cr;
	double len;

	x = tm_x(tc);
	cr = x->cr;
	len = x->width - x->margin * 2;

	cairo_save(cr);
	cairo_move_to(cr, pos_x, pos_y);
	cairo_line_to(cr, pos_x + len, pos_y);
	tm_x_set_dash(x);
	tm_x_set_source_rgb(cr, x->x_fg);
	cairo_stroke(cr);
	cairo_restore(cr);
}

void tm_x_draw_text_one(struct tm_context *tc, struct tm_item *item)
{
	PangoLayout *layout;
	struct tm_x *x;
	cairo_t *cr;
	double dx;

	x = tm_x(tc);
	cr = x->cr;
	layout = x->layout;

	/* Clear existing area. */
	tm_x_clear_area(x, item->x, item->y, item->width, item->height);

	if (!item->len)
		return;

	tm_x_set_source_rgb(cr, item->fg);

	dx = item->x;
	pango_layout_set_text(layout, item->str, item->len);
	if (item->flags & (TM_ITEM_WIDTH_CHANGEABLE | TM_ITEM_ALIGN_RIGHT)) {
		int width;

		pango_layout_get_pixel_size(layout, &width, NULL);
		if (item->flags & TM_ITEM_WIDTH_CHANGEABLE)
			item->width = (double)width;
		else if (item->flags & TM_ITEM_ALIGN_RIGHT)
			dx += item->width - (double)width;
	}

	cairo_move_to(cr, dx, item->y);
	pango_cairo_show_layout(cr, layout);
}

void tm_x_draw_text(struct tm_context *tc, struct tm_item *items,
		    size_t nr_items)
{
	int i;

	for (i = 0; i < nr_items; i++)
		tm_x_draw_text_one(tc, &items[i]);
}

static int tm_x_parse_opts(struct tm_x *x, int argc, char **argv)
{
	int i, err;

	err = 1;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--display")) {
			if (++i >= argc) {
				fprintf(stderr, "--display needs argument.\n");
				goto out;
			}
			x->display = argv[i];
		} else if (!strcmp(argv[i], "--x_bg")) {
			if (++i >= argc) {
				fprintf(stderr, "--x_bg needs argument.\n");
				goto out;
			}
			x->x_bg = tm_x_get_color_from_str(argv[i]);
		} else if (!strcmp(argv[i], "--x_fg")) {
			if (++i >= argc) {
				fprintf(stderr, "--x_fg needs argument.\n");
				goto out;
			}
			x->x_fg = tm_x_get_color_from_str(argv[i]);
		} else if (!strcmp(argv[i], "--x")) {
			if (++i >= argc) {
				fprintf(stderr, "--x needs argument.\n");
				goto out;
			}
			x->x = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--y")) {
			if (++i >= argc) {
				fprintf(stderr, "--y needs argument.\n");
				goto out;
			}
			x->y = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--width")) {
			if (++i >= argc) {
				fprintf(stderr, "--width needs argument.\n");
				goto out;
			}
			x->width = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--height")) {
			if (++i >= argc) {
				fprintf(stderr, "--height needs argument.\n");
				goto out;
			}
			x->height = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--line_width")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--line_width needs argument.\n");
				goto out;
			}
			x->cairo_line_width = atof(argv[i]);
		} else if (!strcmp(argv[i], "--dashes")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--dashes needs argument.\n");
				goto out;
			}
			x->dashes = atof(argv[i]);
		} else if (!strcmp(argv[i], "--icon_scale")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--icon_scale needs argument.\n");
				goto out;
			}
			x->icon_scale_factor = atof(argv[i]);
		} else if (!strcmp(argv[i], "--side_icon_scale")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--side_icon_scale needs argument.\n");
				goto out;
			}
			x->side_icon_scale_factor = atof(argv[i]);
		} else if (!strcmp(argv[i], "--font")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--font needs argument.\n");
				goto out;
			}
			x->font_desc = argv[i];
		}
	}

	err = 0;
out:
	return err;
}

static xcb_screen_t *tm_x_get_screen(xcb_connection_t *c, int nr_screen)
{
	xcb_screen_iterator_t iter;
	const xcb_setup_t *setup;
	xcb_screen_t *screen;

	setup = xcb_get_setup(c);
	iter = xcb_setup_roots_iterator(setup);
	do {
		screen = iter.data;
		if (!nr_screen--)
			break;
		xcb_screen_next(&iter);
	} while (iter.rem);

	return screen;
}

static xcb_visualtype_t *
tm_x_get_visualtype(const xcb_screen_t *screen, xcb_visualid_t vid)
{
	xcb_depth_iterator_t i;

	for (i = xcb_screen_allowed_depths_iterator(screen); i.rem;
	     xcb_depth_next(&i)) {
		xcb_visualtype_iterator_t j;

		for (j = xcb_depth_visuals_iterator(i.data); j.rem;
		     xcb_visualtype_next(&j))
			if (j.data->visual_id == vid)
				return j.data;
	}

	return NULL;
}

static int tm_x_create_window(struct tm_x *x)
{
	xcb_screen_t *screen;
	xcb_connection_t *c;
	xcb_visualtype_t *v;
	u32 mask, values[2];
	int err, nr_screen;
	xcb_window_t win;

	c = xcb_connect(x->display, &nr_screen);
	err = xcb_connection_has_error(c);
	if (err) {
		fprintf(stderr, "xcb_connect failed.\n");
		goto out;
	}

	screen = tm_x_get_screen(c, nr_screen);
	v = tm_x_get_visualtype(screen, screen->root_visual);

	win = xcb_generate_id(c);
	if (win == -1) {
		pr_err("xcb_generate_id");
		goto err;
	};

	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = x->x_bg;
	values[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_EXPOSURE;

	xcb_create_window(c,
	/* depth	*/XCB_COPY_FROM_PARENT,
	/* wid		*/win,
	/* parent	*/screen->root,
	/* x		*/x->x,
	/* y		*/x->y,
	/* width	*/x->width,
	/* height	*/x->height,
	/* border_width	*/0,
	/* classs	*/XCB_WINDOW_CLASS_INPUT_OUTPUT,
	/* visual	*/screen->root_visual,
	/* value_mask	*/mask,
	/* values	*/values);

	x->c = c;
	x->win = win;
	x->v = v;
out:
	return err;
err:
	xcb_disconnect(c);
	goto out;
}

static xcb_atom_t tm_x_resolve_atom(struct tm_x *x, const char *atom_str)
{
	xcb_intern_atom_cookie_t cookie;
	xcb_intern_atom_reply_t *reply;
	xcb_connection_t *c;
	xcb_atom_t atom;

	c = x->c;
	atom = 0;

	cookie = xcb_intern_atom(c, 0, strlen(atom_str), atom_str);
	reply = xcb_intern_atom_reply(c, cookie, NULL);
	if (reply) {
		atom = reply->atom;
		free(reply);
	}

	return atom;
}

/* Interaction with WM. */
static void tm_x_wm_init(struct tm_x *x)
{
	struct {
		const char	*atom_str;
		xcb_atom_t	atom;
	} atoms[] = {
		[0]	= {"UTF8_STRING"},
		[1]	= {"_NET_WM_NAME"},
		[2]	= {"_NET_WM_WINDOW_TYPE"},
		[3]	= {"_NET_WM_WINDOW_TYPE_DESKTOP"},
		[4]	= {"_NET_WM_WINDOW_TYPE_DOCK"},
		[5]	= {"_NET_WM_WINDOW_TYPE_TOOLBAR"},
		[6]	= {"_NET_WM_WINDOW_TYPE_MENU"},
		[7]	= {"_NET_WM_WINDOW_TYPE_UTILITY"},
		[8]	= {"_NET_WM_WINDOW_TYPE_SPLASH"},
		[9]	= {"_NET_WM_WINDOW_TYPE_DIALOG"},
		[10]	= {"_NET_WM_WINDOW_TYPE_NORMAL"},
		[11]	= {"_NET_WM_STATE"},
		[12]	= {"_NET_WM_STATE_STICKY"},
		[13]	= {"_NET_WM_STATE_SKIP_TASKBAR"},
		[14]	= {"_NET_WM_STATE_SKIP_PAGER"},
		[15]	= {"_NET_WM_STATE_BELOW"}
	};
	xcb_atom_t wm_state[4];
	xcb_connection_t *c;
	xcb_window_t win;
	int i;

	c = x->c;
	win = x->win;

	for (i = 0; i < ARRAY_SIZE(atoms); i++)
		atoms[i].atom = tm_x_resolve_atom(x, atoms[i].atom_str);

	xcb_change_property(c, XCB_PROP_MODE_REPLACE, win, XCB_ATOM_WM_NAME,
			    XCB_ATOM_STRING, 8, strlen(PACKAGE), PACKAGE);
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, win, atoms[1].atom,
			    atoms[0].atom, 8, strlen(PACKAGE), PACKAGE);

	/* How _NET_WM_WINDOW_TYPE_DESKTOP handled is dependent on WM.
	 * But I think this is the best way to not make title bar decorated.
	 */
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, win, atoms[2].atom,
			    XCB_ATOM_ATOM, 32, 1, &atoms[3].atom);

	wm_state[0] = atoms[12].atom;
	wm_state[1] = atoms[13].atom;
	wm_state[2] = atoms[14].atom;
	wm_state[3] = atoms[15].atom;
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, win, atoms[11].atom,
			    XCB_ATOM_ATOM, 32, 4, &wm_state[0]);
}

static int tm_x_cairo_init(struct tm_x *x)
{
	cairo_surface_t *surface;
	cairo_status_t status;
	cairo_t *cr;
	int err;

	err = 1;

	surface = cairo_xcb_surface_create(x->c, x->win, x->v, x->width,
					   x->height);
	status = cairo_surface_status(surface);
	if (status != CAIRO_STATUS_SUCCESS)
		goto out;

	cr = cairo_create(surface);
	status = cairo_status(cr);
	cairo_surface_destroy(surface);
	if (status != CAIRO_STATUS_SUCCESS)
		goto out;

	cairo_set_antialias(cr, x->cairo_antialias);
	cairo_set_line_width(cr, x->cairo_line_width);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

	x->cr = cr;
	x->surface = surface;

	err = 0;
out:
	return err;
}

static void tm_x_pango_init(struct tm_x *x)
{
	PangoFontDescription *desc;
	PangoLayout *layout;

	layout = pango_cairo_create_layout(x->cr);
	desc = pango_font_description_from_string(x->font_desc);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	x->layout = layout;
}

static void
tm_x_draw_rounded_rectangle(cairo_t *cr, double pos_x, double pos_y,
			    double width, double height, double radius)
{
	cairo_new_sub_path(cr);
#define DEGREES	(M_PI / 180.0)
	cairo_arc(cr, pos_x + width - radius, pos_y + radius, radius,
		  -90 * DEGREES, 0 * DEGREES);
	cairo_arc(cr, pos_x + width - radius, pos_y + height - radius, radius,
		  0 * DEGREES, 90 * DEGREES);
	cairo_arc(cr, pos_x + radius, pos_y + height - radius, radius,
		  90 * DEGREES, 180 * DEGREES);
	cairo_arc(cr, pos_x + radius, pos_y + radius, radius,
		  180 * DEGREES, 270 * DEGREES);
	cairo_close_path(cr);
}

static int tm_x_shape(struct tm_x *x)
{
	u32 data_len, mask, values[2];
	double width, height, margin;
	cairo_surface_t *surface;
	cairo_status_t status;
	xcb_connection_t *c;
	xcb_pixmap_t bitmap;
	xcb_gcontext_t gc;
	xcb_window_t win;
	int err, stride;
	cairo_t *cr;
	u8 *data;

	err = 1;
	c = x->c;
	win = x->win;
	width = x->width;
	height = x->height;
	margin = x->margin;

	surface = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
	status = cairo_surface_status(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "cairo_surface_create failed.\n");
		goto out;
	}

	cr = cairo_create(surface);
	status = cairo_status(cr);
	cairo_surface_destroy(surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "cairo_create failed.\n");
		goto out;
	}

	/* Exclude all area. */
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	/* Include rounded rectangle. */
	tm_x_draw_rounded_rectangle(cr, 0, 0, width, height, margin);
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	cairo_fill(cr);
	cairo_surface_flush(surface);

	data = cairo_image_surface_get_data(surface);
	stride = cairo_image_surface_get_stride(surface);
	data_len = stride * height;

	bitmap = xcb_generate_id(c);
	if (bitmap == -1) {
		fprintf(stderr, "xcb_generate_id failed.\n");
		goto err;
	}
	xcb_create_pixmap(c, 1, bitmap, win, width, height);

	gc = xcb_generate_id(c);
	mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
	values[0] = 1;	/* FOREGROUND: included area. */
	values[1] = 0;	/* BACKGROUND: excluded area. */
	xcb_create_gc(c, gc, bitmap, mask, values);

	/* Set cairo drawed data to bitmap. */
	xcb_put_image(c,
	/* format   */XCB_IMAGE_FORMAT_XY_BITMAP,
	/* drawable */bitmap,
	/* gc       */gc,
	/* width    */width,
	/* height   */height,
	/* dst_x    */0,
	/* dst_y    */0,
	/* left_pad */0,
	/* depth    */1,
	/* data_len */data_len,
	/* data     */data);

	/* Set bitmap to shape extension. */
	xcb_shape_mask(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, win, 0, 0,
		       bitmap);

	/* Now shape is prepared.
	 * Free resources which are no longer needed.
	 */
	xcb_free_gc(c, gc);
	xcb_free_pixmap(c, bitmap);
	cairo_destroy(cr);

	err = 0;
out:
	return err;
err:
	cairo_destroy(cr);
	goto out;
}

static void tm_x_move_window(struct tm_x *x)
{
	u32 value_list[2];
	u16 value_mask;

	value_mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
	value_list[0] = x->x;
	value_list[1] = x->y;

	xcb_configure_window(x->c, x->win, value_mask, value_list);
}

static void tm_x_get_max_font_height(struct tm_x *x)
{
	PangoLayout *layout;
	int height, margin;

	layout = x->layout;

#define SAMPLE_TEXT	" !\"#$%&'()*+,-./0123456789:;<=>?@"	\
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"	\
			"abcdefghijklmnopqrstuvwxyz{|}~"
	pango_layout_set_text(layout, SAMPLE_TEXT, sizeof(SAMPLE_TEXT) - 1);
	pango_layout_get_pixel_size(layout, NULL, &height);

	x->font_max_height = height;

	/* margin is calculated based on font height.
	 * We force margin even, since we want margin_icon is integer.
	 */
	margin = height / 2.0;
	if (margin & 1)
		margin++;
	x->margin = margin;
}

static void tm_x_get_dot_width(struct tm_x *x)
{
	PangoLayout *layout;
	int width;

	layout = x->layout;

	pango_layout_set_text(layout, ".", 1);
	pango_layout_get_pixel_size(layout, &width, NULL);

	x->font_dot_width = width;
}

static void tm_x_get_max_digit_width(struct tm_x *x)
{
	PangoLayout *layout;
	int i, width;

	layout = x->layout;
	width = 0;

	for (i = 0; i < 10; i++) {
		char buf[4];
		int wid;

		buf[0] = '0' + i;

		pango_layout_set_text(layout, buf, 1);
		pango_layout_get_pixel_size(layout, &wid, NULL);
		if (width < wid)
			width = wid;
	}

	x->font_max_digit_width = width;
}

static void tm_x_get_max_unit_width(struct tm_x *x)
{
	const char *units = "kMGTPE";
	PangoLayout *layout;
	int width;

	layout = x->layout;
	width = 0;

	while (*units) {
		int wid;

		pango_layout_set_text(layout, units, 1);
		pango_layout_get_pixel_size(layout, &wid, NULL);
		if (width < wid)
			width = wid;

		units++;
	}

	x->font_max_unit_width = width;
}

static int tm_x_event_key_press(struct tm_context *tc, const void *event)
{
	const xcb_key_press_event_t *e;

	e = event;

	if (e->detail == 9)
		tc->should_stop = true;

	return 0;
}

static int tm_x_event_expose(struct tm_context *tc, const void *event)
{
	const xcb_expose_event_t *e;

	e = event;
#if 0
	/* xcb_expose_event_t provides which area is exposed.
	 * Maybe we can use this infomation to restrict the redraw operation, 
	 * and also xcb_flush should be called only when e->count reaches to
	 * zero.
	 */
	tm_draw_all(tc);

	if (!e->count)
		tm_x_flush(tc);
#else
	if (!e->count) {
		pthread_mutex_lock(&tc->main_wake_lock);
		tc->draw_all = true;
		pthread_cond_signal(&tc->main_wake_cond);
		pthread_mutex_unlock(&tc->main_wake_lock);
	}
#endif

	return 0;
}

static void *tm_x_wait_events(void *data)
{
	struct tm_context *tc;
	xcb_connection_t *c;
	struct tm_x *x;
	int err;

	tc = data;
	x = tm_x(tc);
	c = x->c;

	/* Wait for all initializations have done. */
	pthread_mutex_lock(&tc->init_lock);
	while (!tc->init_done)
		pthread_cond_wait(&tc->init_cond, &tc->init_lock);
	pthread_mutex_unlock(&tc->init_lock);

	/* Map the window. */
	xcb_map_window(c, x->win);
	tm_x_flush(tc);

	/* WM may ignore x, y coordinates which are specified at window
	 * creation time. Try once more after window is mapped.
	 */
	tm_x_move_window(x);

	err = 0;
	/* Handle all X events. */
	while (!tc->should_stop && !err) {
		xcb_generic_event_t *e;

		e = xcb_wait_for_event(c);

		if (tc->should_stop)
			break;

		if (!e) {
			fprintf(stderr, "xcb_wait_for_event: NULL.\n");
			err = 1;
		} else if (e->response_type == XCB_KEY_PRESS) {
			err = tm_x_event_key_press(tc, e);
		} else if (e->response_type == XCB_EXPOSE) {
			/* Redraw all objects. */
			err = tm_x_event_expose(tc, e);
		} else {
			fprintf(stderr, "response_type: %d\n",
				e->response_type);
		}

		free(e);
	}

	if (err)
		tc->should_stop = true;

	if (tc->should_stop)
		pthread_cond_signal(&tc->main_wake_cond);

	return NULL;
}

static void tm_x_destroy_pango(struct tm_x *x)
{
	g_object_unref(x->layout);
}

static void tm_x_destroy_cairo(struct tm_x *x)
{
	cairo_destroy(x->cr);
}

static void tm_x_destroy_window(struct tm_x *x)
{
	xcb_connection_t *c;

	c = x->c;

	xcb_destroy_window(c, x->win);
	xcb_disconnect(c);
}

static int tm_x_init(struct tm_context *tc, int argc, char **argv)
{
	struct tm_x *x;
	int err;

	x = tm_x(tc);

	/* Initialize members. */
	x->display = NULL;
	x->x_bg = 0xfff6d5;
	x->x_fg = 0x806600;
	x->x = 100;
	x->y = 100;
	x->width = 500;
	x->height = 700;
	x->cairo_antialias = CAIRO_ANTIALIAS_DEFAULT;
	x->cairo_line_width = 2.0;
	x->dashes = 12.0;
	x->icon_scale_factor = 2.2;
	x->side_icon_scale_factor = 1.1;
	x->font_desc = "sans-serif bold 18";

	err = tm_x_parse_opts(x, argc, argv);
	if (err)
		goto out;

	/* Create a window. Geometry is specified by user. */
	err = tm_x_create_window(x);
	if (err)
		goto out;

	tm_x_wm_init(x);

	/* Create a cairo surface. */
	err = tm_x_cairo_init(x);
	if (err)
		goto err;

	/* Create a pango layout. Font is specified by user. */
	tm_x_pango_init(x);

	/* Calculate max font height. */
	tm_x_get_max_font_height(x);
	/* Calculate '.' width. */
	tm_x_get_dot_width(x);
	/* Calculate max digit font width. */
	tm_x_get_max_digit_width(x);
	/* Calculate max unit width. */
	tm_x_get_max_unit_width(x);

	/* Use shape-extension. */
	err = tm_x_shape(x);
	if (err)
		goto err_destroy_pango;

	/* Create a thread which handles all X events. */
	err = pthread_create(&x->tid_wait_ev, NULL, tm_x_wait_events, tc);
	if (err) {
		errno = err;
		pr_err("pthread_create");
		goto err_destroy_pango;
	}
out:
	return err;
err_destroy_pango:
	tm_x_destroy_pango(x);
	tm_x_destroy_cairo(x);
err:
	tm_x_destroy_window(x);
	goto out;
}

static void tm_x_exit(struct tm_context *tc)
{
	struct tm_x *x;

	x = tm_x(tc);

	/* Wait for tm_x_wait_events joining. */
	pthread_join(x->tid_wait_ev, NULL);

	/* Free all resources. */
	tm_x_destroy_pango(x);
	tm_x_destroy_cairo(x);
	tm_x_destroy_window(x);
}

static void tm_x_help(struct tm_context *tc)
{
	printf("\n\tX\n"
	       "\t--display <DISPLAY>\n"
	       "\t\tSpecify X Display.\n"
	       "\t--x_bg <COLOR>\n"
	       "\t\tbackground color.\n"
	       "\t\te.g.: 0xfff6d5\n"
	       "\t--x_fg <COLOR>\n"
	       "\t\tforeground used for lines.\n"
	       "\t--x <INTEGER>\n"
	       "\t\tSpecify x coordinate of window.\n"
	       "\t--y <INTEGER>\n"
	       "\t\tSpecify y coordinate of window.\n"
	       "\t--width <INTEGER>\n"
	       "\t\tSpecify width of window.\n"
	       "\t--height <INTEGER>\n"
	       "\t\tSpecify height of window.\n"
	       "\t--line_width <DOUBLE>\n"
	       "\t\tSpecify line width.\n"
	       "\t--dashes <DOUBLE>\n"
	       "\t\tSpecify alternate lengths of on and off stroke portions.\n"
	       "\t--icon_scale <DOUBLE>\n"
	       "\t--side_icon_scale <DOUBLE>\n"
	       "\t\tSpecify how large the icon should be relative to font height.\n"
	       "\t--font <FONT-DESCRIPTION>\n"
	       "\t\te.g.: \"sans-serif bold 18\"\n"
	       "\t\tSee https://developer.gnome.org/pango/unstable/pango-Fonts.html#pango-font-description-from-string\n");
}

static void tm_x_draw(struct tm_context *tc)
{
	struct tm_x *x;
	double margin;
	cairo_t *cr;

	x = tm_x(tc);
	cr = x->cr;
	margin = x->margin;

	tm_x_clear_area(x, 0, 0, x->width, x->height);

	/* Draw rounded rectangle with dash. */
	tm_x_draw_rounded_rectangle(cr, margin, margin, x->width - margin * 2,
				    x->height - margin * 2, margin);

	cairo_save(cr);
	tm_x_set_dash(x);
	tm_x_set_source_rgb(cr, x->x_fg);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static struct tm_object tm_object_x = {
	.obj_size	= sizeof(struct tm_x),
	.init		= tm_x_init,
	.exit		= tm_x_exit,
	.help		= tm_x_help,
	.draw		= tm_x_draw
};

__attribute__((constructor))
static void tm_x_constructor(void)
{
	int err;

	err = tm_object_register(TM_OBJECT_X, &tm_object_x);
	if (err)
		panic();
}
