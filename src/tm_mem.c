#include "tm_main.h"
#include "tm_x.h"
#include "tm_thread.h"
#include <stdio.h>
#include <string.h>

struct tm_mem {
	/* User configuration variables. */
	u32		mem_fg;
	u32		mem_hi;

	/* icon */
	struct tm_icon	icon_mem;
	struct tm_icon	icon3;

	/* ram */
	struct tm_item	item_ram[6];
};

static struct tm_mem *tm_mem(struct tm_context *tc)
{
	return tm_get_object(tc, TM_OBJECT_MEM);
}

static int tm_mem_parse_opts(struct tm_mem *mem, int argc, char **argv)
{
	int i, err;

	err = 1;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--mem_fg")) {
			if (++i >= argc) {
				fprintf(stderr, "--mem_fg needs argument.\n");
				goto out;
			}
			mem->mem_fg = tm_x_get_color_from_str(argv[i]);
		} else if (!strcmp(argv[i], "--mem_hi")) {
			if (++i >= argc) {
				fprintf(stderr, "--mem_hi needs argument.\n");
				goto out;
			}
			mem->mem_hi = tm_x_get_color_from_str(argv[i]);
		}
	}

	err = 0;
out:
	return err;
}

/**
 * "RAM":	item_ram[0]	fixed-width
 *
 * "XXX MiB / YYY GiB"
 *
 * "XXX":	item_ram[1]	3-digit+dot, right-align
 * " MiB":	item_ram[2]	fixed-width(max-unit-width), left-align
 * " / ":	item_ram[3]	fixed-width
 * "YYY":	item_ram[4]	3-digit+dot, right-align
 * " GiB":	item_ram[5]	fixed-widht8max-unit-width), left-align
 */
static void tm_mem_item_mem_init(struct tm_context *tc)
{
	double x, y, avail, ut_wid, height, margin_icon;
	struct tm_icon *icon_mem;
	struct tm_mem *mem;
	u32 mem_fg, mem_hi;

	mem = tm_mem(tc);
	icon_mem = &mem->icon_mem;

	mem_fg = mem->mem_fg;
	mem_hi = mem->mem_hi;

	avail = tm_x_font_dot_width(tc) + tm_x_font_max_digit_width(tc) * 3.0;
	height = (double)tm_x_font_max_height(tc);

	margin_icon = tm_x_margin_icon(tc);
	x = icon_mem->width + margin_icon;
	y = icon_mem->height / 2.0 - height;

	tm_x_text_size(tc, " iB", 3, &ut_wid, NULL);
	ut_wid += (double)tm_x_font_max_unit_width(tc);

	tm_item_init(tc, &mem->item_ram[0], TM_OBJECT_MEM, mem_fg, &x, &y,
		     0, height, "RAM", TM_ITEM_WIDTH_FIXED);

	x = icon_mem->width + margin_icon;
	y += height;

	tm_item_init(tc, &mem->item_ram[1], TM_OBJECT_MEM, mem_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &mem->item_ram[2], TM_OBJECT_MEM, mem_fg, &x, &y,
		     ut_wid, height, NULL, TM_ITEM_ALIGN_LEFT);
	tm_item_init(tc, &mem->item_ram[3], TM_OBJECT_MEM, mem_fg, &x, &y,
		     0, height, " / ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &mem->item_ram[4], TM_OBJECT_MEM, mem_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &mem->item_ram[5], TM_OBJECT_MEM, mem_fg, &x, &y,
		     ut_wid, height, NULL, TM_ITEM_ALIGN_LEFT);
}

/* Read /proc/meminfo periodically. */
static int tm_mem_timer_cb(struct tm_context *tc)
{
	unsigned long mem_total, mem_free, buffers, cached, sreclaimable, used;
	struct meminfo {
		const char	*label;
		size_t		label_len;
		const char	*format;
		unsigned long	*data;
		bool		found;
	} minfo[] = {
		{"MemTotal", 8, "MemTotal: %lu kB", &mem_total, false},
		{"MemFree", 7, "MemFree: %lu kB", &mem_free, false},
		{"Buffers", 7, "Buffers: %lu kB", &buffers, false},
		{"Cached", 6, "Cached: %lu kB", &cached, false},
		{"SReclaimable", 12, "SReclaimable: %lu kB", &sreclaimable, false}
	};
	struct tm_mem *mem;
	int err, nr_hits;
	char buf[80];
	FILE *fp;

	err = 1;
	mem = tm_mem(tc);

	fp = fopen("/proc/meminfo", "r");
	if (!fp) {
		pr_err("fopen");
		goto out;
	}

	mem_total = mem_free = buffers = cached = sreclaimable = 0UL;

	nr_hits = 0;
	while (nr_hits < ARRAY_SIZE(minfo) && fgets(buf, sizeof(buf), fp)) {
		int i;

		for (i = 0; i < ARRAY_SIZE(minfo); i++) {
			int ret;

			if (minfo[i].found)
				continue;

			if (strncmp(buf, minfo[i].label, minfo[i].label_len))
				continue;

			ret = sscanf(buf, minfo[i].format, minfo[i].data);
			if (ret != 1) {
				fprintf(stderr, "Can't get meminfo \"%s\".\n",
					minfo[i].label);
				continue;
			}

			minfo[i].found = true;
			nr_hits++;
		}
	}

	fclose(fp);

	if (nr_hits != ARRAY_SIZE(minfo))
		fprintf(stderr, "Couldn't get all meminfo.\n");

	used = mem_total - mem_free - buffers - cached - sreclaimable;

	used *= 1024;
	mem_total *= 1024;

	tm_item_data_unit_update(&mem->item_ram[1], &mem->item_ram[2],
				 (double)used);
	tm_item_data_unit_update(&mem->item_ram[4], &mem->item_ram[5],
				 (double)mem_total);

	err = 0;
out:
	return err;
}

static struct tm_thread_timer timer_mem = {
	.timer_cb	= tm_mem_timer_cb,
	.expires_msecs	= 3000
};

static int tm_mem_init(struct tm_context *tc, int argc, char **argv)
{
	struct tm_mem *mem;
	int err;

	mem = tm_mem(tc);

	mem->mem_fg = 0x668000;
	mem->mem_hi = 0xc83737;

	err = tm_mem_parse_opts(mem, argc, argv);
	if (err)
		goto out;

	/* Load mem icon. */
	err = tm_x_load_icon(tc, "ram.svg", TM_ICON_MAIN,
			     &mem->icon_mem);
	if (err)
		goto out;

	err = tm_x_load_icon(tc, "icon3.svg", TM_ICON_SIDE | TM_ICON_FLIP,
			     &mem->icon3);
	if (err)
		goto err;

	tm_mem_item_mem_init(tc);

	/* Gather memory information. */
	tm_thread_timer_add(&timer_mem);

	err = tm_mem_timer_cb(tc);
out:
	return err;
err:
	tm_x_unload_icon(&mem->icon_mem);
	goto out;
}

static void tm_mem_exit(struct tm_context *tc)
{
	struct tm_mem *mem;

	mem = tm_mem(tc);

	/* Uninstall timer handler. */
	tm_thread_timer_del(&timer_mem);

	/* Unload mem icon. */
	tm_x_unload_icon(&mem->icon3);
	tm_x_unload_icon(&mem->icon_mem);
}

static void tm_mem_help(struct tm_context *tc)
{
	printf("\n\tmem\n"
	       "\t--mem_fg <COLOR>\n"
	       "\t\tforeground used for text in mem.\n"
	       "\t--mem_hi <COLOR>\n"
	       "\t\thighlight color used for number.\n");
}

static void tm_mem_get_area(struct tm_context *tc, struct tm_area *area)
{
	struct tm_mem *mem;

	mem = tm_mem(tc);

	area->width = tm_x_width(tc);
	area->height = mem->icon_mem.height;
}

static void tm_mem_draw(struct tm_context *tc)
{
	struct tm_area area;
	struct tm_mem *mem;

	mem = tm_mem(tc);

	tm_mem_get_area(tc, &area);

	/* icon */
	tm_x_draw_icon(tc, &mem->icon_mem, 0, 0);
	tm_x_draw_icon(tc, &mem->icon3,
		       (int)(area.width - mem->icon3.width),
		       (int)(area.height - mem->icon3.height));

	/* ram */
	tm_x_draw_text(tc, mem->item_ram, ARRAY_SIZE(mem->item_ram));
}

static struct tm_object tm_object_mem = {
	.obj_size	= sizeof(struct tm_mem),
	.init		= tm_mem_init,
	.exit		= tm_mem_exit,
	.help		= tm_mem_help,
	.get_area	= tm_mem_get_area,
	.draw		= tm_mem_draw
};

__attribute__((constructor))
static void tm_mem_constructor(void)
{
	int err;

	err = tm_object_register(TM_OBJECT_MEM, &tm_object_mem);
	if (err)
		panic();
}
