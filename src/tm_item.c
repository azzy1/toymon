#include "tm_item.h"
#include "tm_x.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>

static LIST_HEAD(list_update);

static void tm_item_update_add(struct tm_item *item)
{
	list_add_tail(&item->list, &list_update);
}

bool tm_item_update_needed(void)
{
	return !list_empty(&list_update);
}

void tm_item_update_replace(struct list_head *head)
{
	list_replace_init(&list_update, head);
}

void tm_item_cmp_and_update(struct tm_item *item, const char *str, int len)
{
	if (len >= ITEM_STR_MAX) {
		fprintf(stderr, "%s(%d): str is too long (%d).\n", __func__,
			__LINE__, len);
		len = ITEM_STR_MAX - 1;
	}
	if (strcmp(item->str, str)) {
		strncpy(item->str, str, len);
		item->str[len] = '\0';
		item->len = len;
		tm_item_update_add(item);
	}
}

void tm_item_data_unit_update(struct tm_item *item_data,
			      struct tm_item *item_unit, double data)
{
	const char *units, *p;
	char str[16];
	int len;

	units = p = " kMGTPE";

	while (data >= 1000) {
		data /= 1024.0;
		p++;
	}

	sprintf(str, "%.2f", data);
	if (str[3] == '.') {
		str[3] = '\0';
		len = 3;
	} else {
		str[4] = '\0';
		len = 4;
	}

	tm_item_cmp_and_update(item_data, str, len);

	if (p == units)
		len = sprintf(str, " B");
	else
		len = sprintf(str, " %ciB", *p);
	tm_item_cmp_and_update(item_unit, str, len);
}

void tm_item_init(struct tm_context *tc, struct tm_item *item, int id, u32 fg,
		  double *x, double *y, double width, double height,
		  const char *str, u32 flags)
{
	double wid;

	wid = width;

	*item = (struct tm_item) {
		.id	= id,
		.flags	= flags,
		.fg	= fg,
		.x	= *x,
		.y	= *y,
		.width	= width,
		.height	= height
	};

	if (str) {
		size_t len;

		len = strlen(str);
		if (len >= ITEM_STR_MAX) {
			fprintf(stderr, "%s(%d): str is too long (%zd).\n",
				__func__, __LINE__, len);
			len = ITEM_STR_MAX - 1;
		}
		strncpy(item->str, str, len);
		item->str[len] = '\0';
		item->len = len;

		if (flags & TM_ITEM_WIDTH_FIXED) {
			tm_x_text_size(tc, str, len, &wid, NULL);
			item->width = wid;
		}
	}

	*x += wid;
}
