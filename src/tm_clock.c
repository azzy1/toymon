#include "tm_main.h"
#include "tm_x.h"
#include "tm_thread.h"
#include <sys/sysinfo.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

struct tm_clock {
	/* User configuration variables. */
	const char	*date_format;
	u32		clock_fg;
	u32		clock_hi;

	/* clock icons */
	struct tm_icon	icon_clock;
	struct tm_icon	icon1;

	/* date */
	struct tm_item	item_date;

	/* uptime */
	struct tm_item	item_uptime[7];

	/* loadavg */
	struct tm_item	item_loadavg[6];
};

static struct tm_clock *tm_clock(struct tm_context *tc)
{
	return tm_get_object(tc, TM_OBJECT_CLOCK);
}

static int tm_clock_parse_opts(struct tm_clock *clock, int argc, char **argv)
{
	int err, i;

	err = 1;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--date_format")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--date_format needs argument.\n");
				goto out;
			}
			clock->date_format = argv[i];
		} else if (!strcmp(argv[i], "--clock_fg")) {
			if (++i >= argc) {
				fprintf(stderr, "--clock_fg needs argument.\n");
				goto out;
			}
			clock->clock_fg = tm_x_get_color_from_str(argv[i]);
		} else if (!strcmp(argv[i], "--clock_hi")) {
			if (++i >= argc) {
				fprintf(stderr, "--clock_hi needs argument.\n");
				goto out;
			}
			clock->clock_hi = tm_x_get_color_from_str(argv[i]);
		}
	}

	err = 0;
out:
	return err;
}

static void tm_clock_item_date_init(struct tm_context *tc)
{
	struct tm_icon *icon_clock;
	struct tm_clock *clock;
	double x, y, height;

	clock = tm_clock(tc);
	icon_clock = &clock->icon_clock;

	height = (double)tm_x_font_max_height(tc);

	x = icon_clock->width + tm_x_margin_icon(tc);
	y = icon_clock->height / 2.0 - height;

	tm_item_init(tc, &clock->item_date, TM_OBJECT_CLOCK, clock->clock_fg,
		     &x, &y, 0, height, NULL,
		     TM_ITEM_WIDTH_CHANGEABLE | TM_ITEM_ALIGN_LEFT);
}

/* "Uptime: XX d YY h ZZ m"
 *
 * "Uptime: ":	item_uptime[0]	fixed-width
 * "XX":	item_uptime[1]	2-digit, right-align
 * " d ":	item_uptime[2]	fixed-width
 * "YY":	item_uptime[3]	2-digit, right-align
 * " h ":	item_uptime[4]	fixed-width
 * "ZZ":	item_uptime[5]	2-digit, right-align
 * " m":	item_uptime[6]	fixed-width
 */
static void tm_clock_item_uptime_init(struct tm_context *tc)
{
	double x, y, digit2, height;
	struct tm_icon *icon_clock;
	struct tm_clock *clock;
	u32 clock_fg, clock_hi;

	clock = tm_clock(tc);
	icon_clock = &clock->icon_clock;

	clock_fg = clock->clock_fg;
	clock_hi = clock->clock_hi;

	digit2 = tm_x_font_max_digit_width(tc) * 2.0;
	height = (double)tm_x_font_max_height(tc);

	x = icon_clock->width + tm_x_margin_icon(tc);
	y = icon_clock->height / 2.0;

	tm_item_init(tc, &clock->item_uptime[0], TM_OBJECT_CLOCK, clock_fg,
		     &x, &y, 0, height, "Uptime: ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &clock->item_uptime[1], TM_OBJECT_CLOCK, clock_hi,
		     &x, &y, digit2, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &clock->item_uptime[2], TM_OBJECT_CLOCK, clock_fg,
		     &x, &y, 0, height, " d ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &clock->item_uptime[3], TM_OBJECT_CLOCK, clock_hi,
		     &x, &y, digit2, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &clock->item_uptime[4], TM_OBJECT_CLOCK, clock_fg,
		     &x, &y, 0, height, " h ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &clock->item_uptime[5], TM_OBJECT_CLOCK, clock_hi,
		     &x, &y, digit2, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &clock->item_uptime[6], TM_OBJECT_CLOCK, clock_fg,
		     &x, &y, 0, height, " m", TM_ITEM_WIDTH_FIXED);
}

/* "Load avg: X.XX / Y.YY / Z.ZZ"
 * "Load avg: ":	item_loadavg[0]: fixed-width
 * "X.XX":		item_loadavg[1]: 3-digit+dot, right-aligned
 * " / ":		item_loadavg[2]: fixed-width
 * "Y.YY":		item_loadavg[3]: 3-digit+dot, right-aligned
 * " / ":		item_loadavg[4]: fixed-width
 * "Z.ZZ":		item_loadavg[5]: 3-digit+dor, right-aligned
 */
static void tm_clock_item_loadavg_init(struct tm_context *tc)
{
	double x, y, avail, height;
	struct tm_clock *clock;
	u32 clock_fg, clock_hi;

	clock = tm_clock(tc);

	clock_fg = clock->clock_fg;
	clock_hi = clock->clock_hi;

	avail = tm_x_font_dot_width(tc) + tm_x_font_max_digit_width(tc) * 3.0;
	height = (double)tm_x_font_max_height(tc);

	x = 0;
	y = clock->icon_clock.height + tm_x_margin_icon(tc);

	tm_item_init(tc, &clock->item_loadavg[0], TM_OBJECT_CLOCK, clock_fg,
		     &x, &y, 0, height, "Load avg: ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &clock->item_loadavg[1], TM_OBJECT_CLOCK, clock_hi,
		     &x, &y, avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &clock->item_loadavg[2], TM_OBJECT_CLOCK, clock_fg,
		     &x, &y, 0, height, " / ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &clock->item_loadavg[3], TM_OBJECT_CLOCK, clock_hi,
		     &x, &y, avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &clock->item_loadavg[4], TM_OBJECT_CLOCK, clock_fg,
		     &x, &y, 0, height, " / ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &clock->item_loadavg[5], TM_OBJECT_CLOCK, clock_hi,
		     &x, &y, avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
}

static int tm_clock_date_update(struct tm_context *tc)
{
	struct tm_clock *clock;
	char buf[ITEM_STR_MAX];
	struct tm tm, *tmp;
	size_t len;
	time_t t;
	int err;

	err = 1;
	clock = tm_clock(tc);

	t = time(NULL);
	tmp = localtime_r(&t, &tm);
	if (!tmp) {
		fprintf(stderr, "localtime_r failed.\n");
		goto out;
	}
	len = strftime(buf, sizeof(buf), clock->date_format, &tm);
	if (!len) {
		fprintf(stderr, "strftime overflow.\n");
		goto out;
	}

	tm_item_cmp_and_update(&clock->item_date, buf, len);

	err = 0;
out:
	return err;
}

static void tm_clock_item_uptime_update(struct tm_item *item, int data)
{
	char str[ITEM_STR_MAX];
	int len;

	len = sprintf(str, "%d", data);
	tm_item_cmp_and_update(item, str, len);
}

static int tm_clock_uptime_update(struct tm_context *tc, long uptime)
{
	int days, hours, minutes;
	struct tm_clock *clock;

	clock = tm_clock(tc);

	days = uptime / (24 * 60 * 60);
	minutes = uptime / 60;
	hours = (minutes / 60) % 24;
	minutes %= 60;

	tm_clock_item_uptime_update(&clock->item_uptime[1], days);
	tm_clock_item_uptime_update(&clock->item_uptime[3], hours);
	tm_clock_item_uptime_update(&clock->item_uptime[5], minutes);

	return 0;
}

static int tm_clock_loadavg_str(char *s, unsigned long load)
{
	double ld;
	int len;

	ld = (double)load / (1 << SI_LOAD_SHIFT);
	sprintf(s, "%.2f", ld);

	if (s[3] == '.') {
		s[3] = '\0';
		len = 3;
	} else {
		s[4] = '\0';
		len = 4;
	}

	return len;
}

static int
tm_clock_loadavg_update(struct tm_context *tc, const unsigned long *loads)
{
	struct tm_clock *clock;
	char buf[ITEM_STR_MAX];
	int i;

	clock = tm_clock(tc);

	for (i = 0; i < 3; i++) {
		struct tm_item *item;
		int len;

		if (i == 0)
			item = &clock->item_loadavg[1];
		else if (i == 1)
			item = &clock->item_loadavg[3];
		else if (i == 2)
			item = &clock->item_loadavg[5];

		len = tm_clock_loadavg_str(buf, loads[i]);
		tm_item_cmp_and_update(item, buf, len);
	}

	return 0;
}

static int tm_clock_timer_cb(struct tm_context *tc)
{
	struct sysinfo info;
	int err;

	err = sysinfo(&info);
	if (err) {
		pr_err("sysinfo");
		goto out;
	}

	err = tm_clock_date_update(tc) ||
	      tm_clock_uptime_update(tc, info.uptime) ||
	      tm_clock_loadavg_update(tc, info.loads);
out:
	return err;
}

static struct tm_thread_timer timer_clock = {
	.timer_cb	= tm_clock_timer_cb,
	.expires_msecs	= 3000
};

static int tm_clock_init(struct tm_context *tc, int argc, char **argv)
{
	struct tm_clock *clock;
	int err;

	clock = tm_clock(tc);

	clock->date_format = "%b %e %a %l:%M %p";
	clock->clock_fg = 0x806600;
	clock->clock_hi = 0xc83737;

	/* Parse user configuration variables. */
	err = tm_clock_parse_opts(clock, argc, argv);
	if (err)
		goto out;

	/* Load clock icon. */
	err = tm_x_load_icon(tc, "clock.svg", TM_ICON_MAIN,
			     &clock->icon_clock);
	if (err)
		goto out;

	/* Load side icon. */
	err = tm_x_load_icon(tc, "icon1.svg", TM_ICON_SIDE | TM_ICON_FLIP,
			     &clock->icon1);
	if (err)
		goto err;

	/* Initialize items. */
	tm_clock_item_date_init(tc);
	tm_clock_item_uptime_init(tc);
	tm_clock_item_loadavg_init(tc);

	/* Install timer handler which checks whether uptime and loadavg are
	 * changed or not. If changed, invoke redraw operation.
	 */
	tm_thread_timer_add(&timer_clock);

	/* Force to execute timer_cb or else clock data will not be updated
	 * until next timer expiration is occurred.
	 */
	err = tm_clock_timer_cb(tc);
out:
	return err;
err:
	tm_x_unload_icon(&clock->icon_clock);
	goto out;
}

static void tm_clock_exit(struct tm_context *tc)
{
	struct tm_clock *clock;

	clock = tm_clock(tc);

	/* Uninstall timer handler. */
	tm_thread_timer_del(&timer_clock);

	/* Free clock icon. */
	tm_x_unload_icon(&clock->icon1);
	tm_x_unload_icon(&clock->icon_clock);
}

static void tm_clock_help(struct tm_context *tc)
{
	printf("\n\tclock\n"
	       "\t--date_format <FORMAT>\n"
	       "\t\tSee strftime(3) for valid conversion specifiers.\n"
	       "\t--clock_fg <COLOR>\n"
	       "\t\tforeground used for date, uptime and load average.\n"
	       "\t--clock_hi <COLOR>\n"
	       "\t\thighlight color used for number.\n");
}

static void tm_clock_get_area(struct tm_context *tc, struct tm_area *area)
{
	struct tm_clock *clock;

	clock = tm_clock(tc);

	area->width = tm_x_width(tc);;
	area->height = clock->icon_clock.height + tm_x_margin_icon(tc) +
		       (double)tm_x_font_max_height(tc);
}

static void tm_clock_draw(struct tm_context *tc)
{
	struct tm_clock *clock;
	struct tm_area area;

	clock = tm_clock(tc);

	tm_clock_get_area(tc, &area);

	/* Draw icons. */
	tm_x_draw_icon(tc, &clock->icon_clock, 0, 0);
	tm_x_draw_icon(tc, &clock->icon1,
		       (int)(area.width - clock->icon1.width),
		       (int)(area.height - clock->icon1.height));

	/* date */
	tm_x_draw_text_one(tc, &clock->item_date);

	/* uptime */
	tm_x_draw_text(tc, clock->item_uptime,
		       ARRAY_SIZE(clock->item_uptime));

	/* loadavg */
	tm_x_draw_text(tc, clock->item_loadavg,
		       ARRAY_SIZE(clock->item_loadavg));
}

static struct tm_object tm_object_clock = {
	.obj_size	= sizeof(struct tm_clock),
	.init		= tm_clock_init,
	.exit		= tm_clock_exit,
	.help		= tm_clock_help,
	.get_area	= tm_clock_get_area,
	.draw		= tm_clock_draw
};

__attribute__((constructor))
static void tm_clock_constructor(void)
{
	int err;

	err = tm_object_register(TM_OBJECT_CLOCK, &tm_object_clock);
	if (err)
		panic();
}
