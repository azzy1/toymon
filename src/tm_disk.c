#include "tm_main.h"
#include "tm_x.h"
#include "tm_thread.h"
#include "tm_item.h"
#include <sys/vfs.h>
#include <stdio.h>
#include <string.h>
#include <linux/magic.h>

struct tm_disk {
	/* User configuration variables. */
	const char	*mount_point;
	u32		disk_fg;
	u32		disk_hi;

	/* icon */
	struct tm_icon	icon_disk;
	struct tm_icon	icon4;

	/* disk */
	struct tm_item	item_disk[6];
};

static struct tm_disk *tm_disk(struct tm_context *tc)
{
	return tm_get_object(tc, TM_OBJECT_DISK);
}

static int tm_disk_parse_opts(struct tm_disk *disk, int argc, char **argv)
{
	int i, err;

	err = 1;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--mount_point")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--mount_point needs argument.\n");
				goto out;
			}
			disk->mount_point = argv[i];
		} else if (!strcmp(argv[i], "--disk_fg")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--disk_fg needs argument.\n");
				goto out;
			}
			disk->disk_fg = tm_x_get_color_from_str(argv[i]);
		} else if (!strcmp(argv[i], "--disk_hi")) {
			if (++i >= argc) {
				fprintf(stderr,
					"--disk_hi needs argument.\n");
				goto out;
			}
			disk->disk_hi = tm_x_get_color_from_str(argv[i]);
		}
	}

	err = 0;
out:
	return err;
}

static const char *tm_disk_fs_str(unsigned int f_type)
{
	/* Toooooooo limited support ;) Please extend.
	 * See /usr/include/linux/magic.h or man statfs(2).
	 */
	if (f_type == EXT4_SUPER_MAGIC)
		return "ext4";
	else if (f_type == TMPFS_MAGIC)
		return "tmpfs";
	else if (f_type == BTRFS_SUPER_MAGIC)
		return "btrfs";
	else
		return "unknown";
}

static int tm_disk_getfs(const char *path, char *str)
{
	struct statfs buf;
	int err;

	err = statfs(path, &buf);
	if (err) {
		pr_err("statfs");
		goto out;
	}

	sprintf(str, "%s (%s): ", path, tm_disk_fs_str(buf.f_type));
out:
	return err;
}

/**
 * "/ (ext4): XXX GiB / YYY GiB"
 *
 * "/ (ext4): "	item_disk[0]	fixed-width
 * "XXX":	item_disk[1]	3-digit+dot, right-align
 * " GiB":	item_disk[2]	fixed-width(max-unit-width)
 * " / ":	item_disk[3]	fixed-width
 * "YYY":	item_disk[4]	3-digit+dot, right-align
 * " GiB":	item_disk[5]	fixed-width(max-unit-width)
 */
static int tm_disk_item_disk_init(struct tm_context *tc)
{
	double x, y, avail, ut_wid, height;
	struct tm_icon *icon_disk;
	struct tm_disk *disk;
	u32 disk_fg, disk_hi;
	char mtpt_str[64];
	int err;

	disk = tm_disk(tc);
	icon_disk = &disk->icon_disk;

	disk_fg = disk->disk_fg;
	disk_hi = disk->disk_hi;

	avail = tm_x_font_dot_width(tc) + tm_x_font_max_digit_width(tc) * 3.0;
	height = (double)tm_x_font_max_height(tc);

	x = icon_disk->width + tm_x_margin_icon(tc);
	y = icon_disk->height / 2.0 - height;

	tm_x_text_size(tc, " iB", 3, &ut_wid, NULL);
	ut_wid += (double)tm_x_font_max_unit_width(tc);

	err = tm_disk_getfs(disk->mount_point, mtpt_str);
	if (err)
		goto out;

	tm_item_init(tc, &disk->item_disk[0], TM_OBJECT_DISK, disk_fg, &x, &y,
		     0, height, mtpt_str, TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &disk->item_disk[1], TM_OBJECT_DISK, disk_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &disk->item_disk[2], TM_OBJECT_DISK, disk_fg, &x, &y,
		     ut_wid, height, NULL, TM_ITEM_ALIGN_LEFT);
	tm_item_init(tc, &disk->item_disk[3], TM_OBJECT_DISK, disk_fg, &x, &y,
		     0, height, " / ", TM_ITEM_WIDTH_FIXED);
	tm_item_init(tc, &disk->item_disk[4], TM_OBJECT_DISK, disk_hi, &x, &y,
		     avail, height, NULL, TM_ITEM_ALIGN_RIGHT);
	tm_item_init(tc, &disk->item_disk[5], TM_OBJECT_DISK, disk_fg, &x, &y,
		     ut_wid, height, NULL, TM_ITEM_ALIGN_LEFT);
out:
	return err;
}

static int tm_disk_timer_cb(struct tm_context *tc)
{
	struct tm_disk *disk;
	struct statfs buf;
	u64 used, total;
	int err;

	disk = tm_disk(tc);

	err = statfs(disk->mount_point, &buf);
	if (err) {
		pr_err("statfs");
		goto out;
	}

	/* used and total in bytes. */
	used = (buf.f_blocks - buf.f_bfree) * buf.f_bsize;
	total = buf.f_blocks * buf.f_bsize;

	tm_item_data_unit_update(&disk->item_disk[1], &disk->item_disk[2],
				 (double)used);
	tm_item_data_unit_update(&disk->item_disk[4], &disk->item_disk[5],
				 (double)total);
out:
	return err;
}

static struct tm_thread_timer timer_disk = {
	.timer_cb	= tm_disk_timer_cb,
	.expires_msecs	= 10000
};

static int tm_disk_init(struct tm_context *tc, int argc, char **argv)
{
	struct tm_disk *disk;
	int err;

	disk = tm_disk(tc);

	disk->mount_point = "/";
	disk->disk_fg = 0x008066;
	disk->disk_hi = 0xc83737;

	err = tm_disk_parse_opts(disk, argc, argv);
	if (err)
		goto out;

	/* Load disk icon. */
	err = tm_x_load_icon(tc, "storage.svg", TM_ICON_MAIN,
			     &disk->icon_disk);
	if (err)
		goto out;

	err = tm_x_load_icon(tc, "icon4.svg", TM_ICON_SIDE | TM_ICON_FLIP,
			     &disk->icon4);
	if (err)
		goto err1;

	err = tm_disk_item_disk_init(tc);
	if (err)
		goto err2;

	/* Arm timer handler. */
	tm_thread_timer_add(&timer_disk);

	err = tm_disk_timer_cb(tc);
out:
	return err;
err2:
	tm_x_unload_icon(&disk->icon4);
err1:
	tm_x_unload_icon(&disk->icon_disk);
	goto out;
}

static void tm_disk_exit(struct tm_context *tc)
{
	struct tm_disk *disk;

	disk = tm_disk(tc);

	/* Dis-arm timer handler. */
	tm_thread_timer_del(&timer_disk);

	/* Unload disk icon. */
	tm_x_unload_icon(&disk->icon4);
	tm_x_unload_icon(&disk->icon_disk);
}

static void tm_disk_help(struct tm_context *tc)
{
	printf("\n\tdisk\n"
	       "\t--mount_point <DIR>\n"
	       "\t\tSpecify mount point.\n"
	       "\t--disk_fg <COLOR>\n"
	       "\t\tforeground used for text in disk.\n"
	       "\t--disk_hi <COLOR>\n"
	       "\t\thighlight color used for number.\n");
}

static void tm_disk_get_area(struct tm_context *tc, struct tm_area *area)
{
	struct tm_disk *disk;

	disk = tm_disk(tc);

	area->width = tm_x_width(tc);
	area->height = disk->icon_disk.height;
}

static void tm_disk_draw(struct tm_context *tc)
{
	struct tm_disk *disk;
	struct tm_area area;

	disk = tm_disk(tc);

	tm_disk_get_area(tc, &area);

	/* icon */
	tm_x_draw_icon(tc, &disk->icon_disk, 0, 0);
	tm_x_draw_icon(tc, &disk->icon4,
		       (int)(area.width - disk->icon4.width),
		       (int)(area.height - disk->icon4.height));

	/* fs usage */
	tm_x_draw_text(tc, disk->item_disk, ARRAY_SIZE(disk->item_disk));
}

static struct tm_object tm_object_disk = {
	.obj_size	= sizeof(struct tm_disk),
	.init		= tm_disk_init,
	.exit		= tm_disk_exit,
	.help		= tm_disk_help,
	.get_area	= tm_disk_get_area,
	.draw		= tm_disk_draw
};

__attribute__((constructor))
static void tm_disk_constructor(void)
{
	int err;

	err = tm_object_register(TM_OBJECT_DISK, &tm_object_disk);
	if (err)
		panic();
}
