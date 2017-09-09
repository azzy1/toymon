#include "tm_main.h"
#include "tm_thread.h"
#include "tm_x.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

struct tm_cpu_stat {
	union {
		struct {
			unsigned long long	user;
			unsigned long long	nice;
			unsigned long long	system;
			unsigned long long	idle;
			unsigned long long	iowait;
			unsigned long long	irq;
			unsigned long long	softirq;
			unsigned long long	steal;
			unsigned long long	guest;
			unsigned long long	guest_nice;
		};
#define CPU_STAT_MAX	10
		unsigned long long	stat[CPU_STAT_MAX];
	};
};

struct tm_cpu {
	/* User configuration variable. */
	u32			cpu_fg;
	u32			cpu_hi;
	bool			use_symbol;
	const char		*temp_label1;
	const char		*temp_input1;
	const char		*temp_label2;
	const char		*temp_input2;

	struct tm_cpu_stat	stat;
	int			nr_temp;

	/* icons */
	struct tm_icon		icon_cpu;
	struct tm_icon		icon2;

	/* usage */
	struct tm_item		item_usage[7];

	/* freq */
	struct tm_item		item_freq;

	/* coretemp */
	struct tm_item		item_temp[6];
};

static struct tm_cpu *tm_cpu(struct tm_context *tc)
{
	return tm_get_object(tc, TM_OBJECT_CPU);
}

static int tm_cpu_parse_opts(struct tm_cpu *cpu, int argc, char **argv)
{
	int i, err;

	err = 1;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--cpu_fg")) {
			if (++i >= argc) {
				fprintf(stderr, "--cpu_fg needs argument.\n");
				goto out;
			}
			cpu->cpu_fg = tm_x_get_color_from_str(argv[i]);
		} else if (!strcmp(argv[i], "--cpu_hi")) {
			if (++i >= argc) {
				fprintf(stderr, "--cpu_hi needs argument.\n");
				goto out;
			}
			cpu->cpu_hi = tm_x_get_color_from_str(argv[i]);
		} else if (!strcmp(argv[i], "--disable_temp_symbol")) {
			cpu->use_symbol = false;
		} else if (!strcmp(argv[i], "--temp_label1")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--temp_label1 needs argument.\n");
				goto out;
			}
			cpu->temp_label1 = argv[i];
		} else if (!strcmp(argv[i], "--temp_input1")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--temp_input1 needs argument.\n");
				goto out;
			}
			cpu->temp_input1 = argv[i];
		} else if (!strcmp(argv[i], "--temp_label2")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--temp_label2 needs argument.\n");
				goto out;
			}
			cpu->temp_label2 = argv[i];
		} else if (!strcmp(argv[i], "--temp_input2")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--temp_input2 needs argument.\n");
				goto out;
			}
			cpu->temp_input2 = argv[i];
		}
	}

	if (cpu->temp_label1 && cpu->temp_input1)
		cpu->nr_temp++;
	if (cpu->temp_label2 && cpu->temp_input2)
		cpu->nr_temp++;

	err = 0;
out:
	return err;
}

/**
 * "CPU: XX.X us Y.YY sy ZZZ id"
 *
 * "CPU: ":	item_usage[0]	fixed-width
 * "XX.X":	item_usage[1]	3-digit+dot, right-align
 * " us ":	item_usage[2]	fixed-width
 * "YY.Y":	item_usage[3]	3-digit+dot, right-align
 * " sy ":	item_usage[4]	fixed-width
 * "ZZZ":	item_usage[5]	3-digit+dot, right-align
 * " id":	item_usage[6]	fixed-width
 */
static void tm_cpu_item_usage_init(struct tm_context *tc)
{
	double x, y, avail, height;
	struct tm_icon *icon_cpu;
	struct tm_cpu *cpu;
	u32 cpu_fg, cpu_hi;

	cpu = tm_cpu(tc);
	icon_cpu = &cpu->icon_cpu;

	cpu_fg = cpu->cpu_fg;
	cpu_hi = cpu->cpu_hi;

	avail = tm_x_font_dot_width(tc) + tm_x_font_max_digit_width(tc) * 3.0;
	height = (double)tm_x_font_max_height(tc);

	x = icon_cpu->width + tm_x_margin_icon(tc);
	y = icon_cpu->height / 2.0 - height;

	tm_item_init(tc, &cpu->item_usage[0], TM_OBJECT_CPU, cpu_fg, &x, &y,
		     0, height, "CPU: ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &cpu->item_usage[1], TM_OBJECT_CPU, cpu_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &cpu->item_usage[2], TM_OBJECT_CPU, cpu_fg, &x, &y,
		     0, height, " us ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &cpu->item_usage[3], TM_OBJECT_CPU, cpu_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &cpu->item_usage[4], TM_OBJECT_CPU, cpu_fg, &x, &y,
		     0, height, " sy ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &cpu->item_usage[5], TM_OBJECT_CPU, cpu_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &cpu->item_usage[6], TM_OBJECT_CPU, cpu_fg, &x, &y,
		     0, height, " id", TM_ITEM_WIDTH_FIXED);
}

/**
 * "acpi-cpufreq / powersave"
 *
 * "acpi-cpufreq / powersave":	item_freq:	changeable | left-align
 */
static void tm_cpu_item_freq_init(struct tm_context *tc)
{
	struct tm_icon *icon_cpu;
	struct tm_cpu *cpu;
	double x, y;

	cpu = tm_cpu(tc);
	icon_cpu = &cpu->icon_cpu;

	x = icon_cpu->width + tm_x_margin_icon(tc);
	y = icon_cpu->height / 2.0;

	tm_item_init(tc, &cpu->item_freq, TM_OBJECT_CPU, cpu->cpu_fg, &x, &y,
		     0, tm_x_font_max_height(tc), NULL,
		     TM_ITEM_WIDTH_CHANGEABLE | TM_ITEM_ALIGN_LEFT);
}

static int tm_cpu_item_get_one_line(const char *path, char *buf, size_t len)
{
	FILE *fp;
	char *p;
	int err;

	err = 1;

	fp = fopen(path, "r");
	if (!fp) {
		pr_err("fopen");
		goto out;
	}

	p = fgets(buf, len, fp);
	fclose(fp);
	if (!p) {
		fprintf(stderr, "Can't read from scaling_driver.\n");
		goto out;
	}

	p = strchr(buf, '\n');
	if (p)
		*p = '\0';

	err = 0;
out:
	return err;
}

static int
tm_cpu_item_temp_init_one(const char *label, struct tm_context *tc,
			  struct tm_item *item, double *x, double *y)
{
	char buf[32], lbl[64];
	double height, digit2;
	struct tm_cpu *cpu;
	const char *symbol;
	u32 cpu_fg, cpu_hi;
	int err;

	cpu = tm_cpu(tc);

	cpu_fg = cpu->cpu_fg;
	cpu_hi = cpu->cpu_hi;

	symbol = cpu->use_symbol ? " ℃" : " C";

	digit2 = tm_x_font_max_digit_width(tc) * 2.0;
	height = (double)tm_x_font_max_height(tc);

	err = tm_cpu_item_get_one_line(label, buf, sizeof(buf));
	if (err)
		goto out;

	sprintf(lbl, "%s: ", buf);

	tm_item_init(tc, &item[0], TM_OBJECT_CPU, cpu_fg, x, y,
		     0, height, lbl, TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &item[1], TM_OBJECT_CPU, cpu_hi, x, y,
		     digit2, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &item[2], TM_OBJECT_CPU, cpu_fg, x, y,
		     0, height, symbol, TM_ITEM_WIDTH_FIXED);

	*x = 0;
	*y += height;
out:
	return err;
}

/**
 * "core0: 45 ℃"
 * "core2: 52 ℃"
 *
 * "core0: ":	item_temp[0]:	fixed-width
 * "45":	item_temp[1]:	2-digit, right-align
 * " ℃"		item_temp[2]:	fixed-width
 * "core2: ":	item_temp[3]:	fixed-width
 * "52":	item_temp[4]:	2-digit, right-align
 * " ℃"		item_temp[5]:	fixed-width
 */
static int tm_cpu_item_temp_init(struct tm_context *tc)
{
	struct tm_cpu *cpu;
	double x, y;
	int err;

	cpu = tm_cpu(tc);

	x = 0;
	y = cpu->icon_cpu.height + tm_x_margin_icon(tc);

	if (cpu->temp_label1 && cpu->temp_input1) {
		err = tm_cpu_item_temp_init_one(cpu->temp_label1, tc,
						&cpu->item_temp[0], &x, &y);
		if (err)
			goto out;
	}

	if (cpu->temp_label2 && cpu->temp_input2)
		err = tm_cpu_item_temp_init_one(cpu->temp_label2, tc,
						&cpu->item_temp[3], &x, &y);
out:
	return err;
}

static void tm_cpu_usage_update(struct tm_item *item, double usage)
{
	char buf[8];
	int len;

	len = sprintf(buf, "%.1f", usage);
	if (!strncmp(buf, "100", 3)) {
		buf[3] = '\0';
		len = 3;
	}

	tm_item_cmp_and_update(item, buf, len);
}

static int tm_cpu_item_usage_update(struct tm_context *tc)
{
	struct tm_cpu_stat cur, dif;
	unsigned long long total;
	struct tm_cpu *cpu;
	double us, sy, id;
	int err, ret, i;
	char buf[80];

	cpu = tm_cpu(tc);

	err = tm_cpu_item_get_one_line("/proc/stat", buf, sizeof(buf));
	if (err)
		goto out;

	ret = sscanf(buf,
		     "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
		     &cur.user, &cur.nice, &cur.system, &cur.idle, &cur.iowait,
		     &cur.irq, &cur.softirq, &cur.steal, &cur.guest,
		     &cur.guest_nice);
	if (ret != CPU_STAT_MAX) {
		fprintf(stderr, "/proc/stat: unknown format.\n");
		goto out;
	}

	total = 0;
	for (i = 0; i < CPU_STAT_MAX; i++) {
		dif.stat[i] = cur.stat[i] - cpu->stat.stat[i];
		total += dif.stat[i];
	}
	cpu->stat = cur;

	/* us: user + nice + guest + guest_nice
	 * sy: system + irq + softirq + steal
	 * id: idle + iowait
	 */
	us = (double)(dif.user + dif.nice + dif.guest + dif.guest_nice) / total;
	sy = (double)(dif.system + dif.irq + dif.softirq + dif.steal) / total;
	id = (double)(dif.idle + dif.iowait) / total;

	tm_cpu_usage_update(&cpu->item_usage[1], us * 100);
	tm_cpu_usage_update(&cpu->item_usage[3], sy * 100);
	tm_cpu_usage_update(&cpu->item_usage[5], id * 100);

	err = 0;
out:
	return err;
}

static int tm_cpu_temp_celcius_update(struct tm_item *item, char *input)
{
	int err, ret, cel, len;
	char str[16];

	err = 1;

	ret = sscanf(input, "%d", &cel);
	if (ret != 1) {
		fprintf(stderr, "Cannot get celcius.\n");
		goto out;
	}

	cel /= 1000;

	len = sprintf(str, "%d", cel);
	tm_item_cmp_and_update(item, str, len);

	err = 0;
out:
	return err;
}

static int tm_cpu_item_temp_update(struct tm_context *tc)
{
	char input1[16], input2[16];
	struct tm_cpu *cpu;
	int err;

	cpu = tm_cpu(tc);

	if (cpu->temp_label1 && cpu->temp_input1) {
		err = tm_cpu_item_get_one_line(cpu->temp_input1, input1,
					       sizeof(input1)) ||
		      tm_cpu_temp_celcius_update(&cpu->item_temp[1], input1);
		if (err)
			goto out;
	}

	if (cpu->temp_label2 && cpu->temp_input2) {
		err = tm_cpu_item_get_one_line(cpu->temp_input2, input2,
					       sizeof(input2)) ||
		      tm_cpu_temp_celcius_update(&cpu->item_temp[4], input2);
	}

out:
	return err;
}

static int tm_cpu_item_freq_update(struct tm_context *tc)
{
	char str[80], drv_str[40], gov_str[40];
	struct tm_cpu *cpu;
	int err, len;

	cpu = tm_cpu(tc);

#define CPU_FREQ_PATH	"/sys/devices/system/cpu/cpu0/cpufreq/"
	err= tm_cpu_item_get_one_line(CPU_FREQ_PATH "scaling_driver",
				      drv_str, sizeof(drv_str)) ||
	     tm_cpu_item_get_one_line(CPU_FREQ_PATH "scaling_governor",
				      gov_str, sizeof(gov_str));
	if (err)
		goto out;

	len = sprintf(str, "%s / %s", drv_str, gov_str);
	tm_item_cmp_and_update(&cpu->item_freq, str, len);
out:
	return err;
}

static int tm_cpu_timer_cb_stat_temp(struct tm_context *tc)
{
	return tm_cpu_item_usage_update(tc) ||
	       tm_cpu_item_temp_update(tc);
}

static int tm_cpu_timer_cb_freq(struct tm_context *tc)
{
	return tm_cpu_item_freq_update(tc);
}

static struct tm_thread_timer timer_cpu_stat_temp = {
	.timer_cb	= tm_cpu_timer_cb_stat_temp,
	.expires_msecs	= 3000
};

static struct tm_thread_timer timer_cpu_freq = {
	.timer_cb	= tm_cpu_timer_cb_freq,
	.expires_msecs	= 60000
};

static int tm_cpu_init(struct tm_context *tc, int argc, char **argv)
{
	struct tm_cpu *cpu;
	int err;

	cpu = tm_cpu(tc);

	/* User configuration variables. */
	cpu->cpu_fg = 0x668000;
	cpu->cpu_hi = 0xc83737;
	cpu->use_symbol = true;

	err = tm_cpu_parse_opts(cpu, argc, argv);
	if (err)
		goto out;

	err = tm_x_load_icon(tc, "cpu.svg", TM_ICON_MAIN,
			     &cpu->icon_cpu);
	if (err)
		goto out;

	err = tm_x_load_icon(tc, "icon2.svg", TM_ICON_SIDE | TM_ICON_FLIP,
			     &cpu->icon2);
	if (err)
		goto err1;

	tm_cpu_item_usage_init(tc);
	tm_cpu_item_freq_init(tc);
	err = tm_cpu_item_temp_init(tc);
	if (err)
		goto err2;

	tm_thread_timer_add(&timer_cpu_stat_temp);
	tm_thread_timer_add(&timer_cpu_freq);

	err = tm_cpu_timer_cb_stat_temp(tc) || tm_cpu_timer_cb_freq(tc);
out:
	return err;
err2:
	tm_x_unload_icon(&cpu->icon2);
err1:
	tm_x_unload_icon(&cpu->icon_cpu);
	goto out;
}

static void tm_cpu_exit(struct tm_context *tc)
{
	struct tm_cpu *cpu;

	cpu = tm_cpu(tc);

	tm_thread_timer_del(&timer_cpu_freq);
	tm_thread_timer_del(&timer_cpu_stat_temp);

	tm_x_unload_icon(&cpu->icon2);
	tm_x_unload_icon(&cpu->icon_cpu);
}

static void tm_cpu_help(struct tm_context *tc)
{
	printf("\n\tcpu\n"
	       "\t--cpu_fg <COLOR>\n"
	       "\t\tforeground used for text in cpu.\n"
	       "\t--cpu_hi <COLOR>\n"
	       "\t\thighlight color used for number.\n"
	       "\t--disable_temp_symbol\n"
	       "\t\tWould be helpful if font is lack of degree symbol.\n"
	       "\t--temp_label1\n"
	       "\t--temp_label2\n"
	       "\t\te.g.: '/sys/class/hwmon/hwmon<N>/temp<N>_label'\n"
	       "\t--temp_input1\n"
	       "\t--temp_input2\n"
	       "\t\te.g.: '/sys/class/hwmon/hwmon<N>/temp<N>_input'\n");
}

static void tm_cpu_get_area(struct tm_context *tc, struct tm_area *area)
{
	struct tm_cpu *cpu;

	cpu = tm_cpu(tc);

	area->width = tm_x_width(tc);
	area->height = cpu->icon_cpu.height + tm_x_margin_icon(tc) +
		       tm_x_font_max_height(tc) * cpu->nr_temp;
}

static void tm_cpu_draw(struct tm_context *tc)
{
	struct tm_area area;
	struct tm_cpu *cpu;

	cpu = tm_cpu(tc);

	tm_cpu_get_area(tc, &area);

	/* Draw icons. */
	tm_x_draw_icon(tc, &cpu->icon_cpu, 0, 0);
	tm_x_draw_icon(tc, &cpu->icon2,
		       (int)(area.width - cpu->icon2.width),
		       (int)(area.height - cpu->icon2.height));

	/* usage */
	tm_x_draw_text(tc, cpu->item_usage, ARRAY_SIZE(cpu->item_usage));

	/* freq */
	tm_x_draw_text_one(tc, &cpu->item_freq);

	/* temp */
	tm_x_draw_text(tc, cpu->item_temp, ARRAY_SIZE(cpu->item_temp));
}

static struct tm_object tm_object_cpu = {
	.obj_size	= sizeof(struct tm_cpu),
	.init		= tm_cpu_init,
	.exit		= tm_cpu_exit,
	.help		= tm_cpu_help,
	.get_area	= tm_cpu_get_area,
	.draw		= tm_cpu_draw
};

__attribute__((constructor))
static void tm_cpu_constructor(void)
{
	int err;

	err = tm_object_register(TM_OBJECT_CPU, &tm_object_cpu);
	if (err)
		panic();
}
