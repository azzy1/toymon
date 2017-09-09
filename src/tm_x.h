#ifndef _TM_X_H
#define _TM_X_H

#include "tm.h"
#include "tm_item.h"
#include <cairo-xcb.h>

enum {
	TM_ICON_FLIP	= (1 << 0),
	TM_ICON_MAIN	= (1 << 1),
	TM_ICON_SIDE	= (1 << 2)
};

struct tm_icon {
	cairo_t		*cr;
	cairo_surface_t	*surface;
	int		width;
	int		height;
};

#define tm_x_load_icon(__tc, __file, __type, __icon)			\
	__tm_x_load_icon(__tc, ICONSDIR __file, __type, __icon)

extern void tm_x_flush(struct tm_context *tc);
extern u32 tm_x_get_color_from_str(const char *s);
extern int tm_x_width(struct tm_context *tc);
extern double tm_x_margin(struct tm_context *tc);
extern double tm_x_margin_icon(struct tm_context *tc);
extern void tm_x_text_size(struct tm_context *tc, const char *s, size_t len,
			   double *width, double *height);
extern int tm_x_font_max_height(struct tm_context *tc);
extern int tm_x_font_dot_width(struct tm_context *tc);
extern int tm_x_font_max_digit_width(struct tm_context *tc);
extern int tm_x_font_max_unit_width(struct tm_context *tc);
extern void tm_x_translate(struct tm_context *tc, double dx, double dy);
extern int __tm_x_load_icon(struct tm_context *tc, const char *file, int type,
			    struct tm_icon *icon);
extern void tm_x_unload_icon(struct tm_icon *icon);
extern void tm_x_draw_icon(struct tm_context *tc, struct tm_icon *icon,
			   double pos_x, double pos_y);
extern void tm_x_draw_line(struct tm_context *tc, double pos_x, double pos_y);
extern void tm_x_draw_text_one(struct tm_context *tc, struct tm_item *item);
extern void tm_x_draw_text(struct tm_context *tc, struct tm_item *items,
			   size_t nr_items);

#endif /* _TM_X_H */
