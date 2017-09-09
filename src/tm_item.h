#ifndef _TM_ITEM_H
#define _TM_ITEM_H

#include "tm.h"

/**
 * @TM_ITEM_WIDTH_FIXED: The contents is static. width is fixed size.
 *
 * @TM_ITEM_WIDTH_CHANGEABLE:
 * The width of the contents would change anytime. So always update the
 * tm_item::width member is needed to clear the area properly.
 *
 * @TM_ITEM_ALIGN_LEFT, @TM_ITEM_ALIGN_RIGHT:
 * The contents is not static, and struct tm_item::width member holds maximum
 * width, not exact one. So there would be little space before or after the
 * contents. These bits control position of the space.
 * No need to set TM_ITEM_ALIGN_LEFT bit explicitly, since left-aligned is
 * default alignment.
 */
enum {
	TM_ITEM_WIDTH_FIXED		= (1 << 0),
	TM_ITEM_WIDTH_CHANGEABLE	= (1 << 1),
	TM_ITEM_ALIGN_LEFT		= (1 << 2),
	TM_ITEM_ALIGN_RIGHT		= (1 << 3)
};

struct tm_item {
	struct list_head	list;
	int			id;
	u32			flags;
#if 0
	u32			bg;
#endif
	u32			fg;
	double			x;
	double			y;
	double			width;
	double			height;
	size_t			len;
#define ITEM_STR_MAX	32
	char			str[ITEM_STR_MAX];
};

extern bool tm_item_update_needed(void);
extern void tm_item_update_replace(struct list_head *head);
extern void tm_item_cmp_and_update(struct tm_item *item, const char *str, int
				   len);
extern void tm_item_data_unit_update(struct tm_item *item_data,
				     struct tm_item *item_unit, double data);
extern void tm_item_init(struct tm_context *tc, struct tm_item *item, int id,
			 u32 fg, double *x, double *y, double width,
			 double height, const char *str, u32 flags);

#endif /* _TM_ITEM_H */
