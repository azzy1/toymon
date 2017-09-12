#include "tm_thread.h"
#include "tm_main.h"
#include "tm_x.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

void __panic(const char *func, int line)
{
	fprintf(stderr, "panic at %s: %d.\n", func, line);

	*(char *)0 = '\0';
}

void __pr_err(const char *func, int line, const char *s, int eno)
{
	char *p;
	int ret;

#define STRERR_BUFSIZ	256
	p = malloc(STRERR_BUFSIZ);
	if (!p)
		return;

	ret = strerror_r(eno, p, STRERR_BUFSIZ);
	if (ret)
		fprintf(stderr, "%s(%d): %s: %d\n", func, line, s, eno);
	else
		fprintf(stderr, "%s(%d): %s: %s\n", func, line, s, p);

	free(p);
}

/* For used with cairo_translate. */
struct tm_origin {
	double	x;
	double	y;
};

struct tm_main {
	struct tm_origin	origin[TM_OBJECT_MAX];
};

static struct tm_main *tm_main(struct tm_context *tc)
{
	return tm_get_object(tc, TM_OBJECT_MAIN);
}

static struct tm_object *tm_objs[TM_OBJECT_MAX];
static size_t tm_ctx_size = TM_OBJECT_SIZE(sizeof(struct tm_context));

int tm_object_register(int id, struct tm_object *o)
{
	int err;

	err = ERANGE;
	if (id >= TM_OBJECT_MAX)
		goto out;

	err = EEXIST;
	if (!tm_objs[id]) {
		tm_objs[id] = o;
		tm_ctx_size += TM_OBJECT_SIZE(o->obj_size);
		err = 0;
	}
out:
	return err;
}

int tm_object_unregister(int id)
{
	int err;

	err = ERANGE;
	if (id >= TM_OBJECT_MAX)
		goto out;

	err = ENOENT;
	if (tm_objs[id]) {
		tm_objs[id] = NULL;
		err = 0;
	}
out:
	return err;
}

static void tm_draw_all(struct tm_context *tc)
{
	double margin_icon, old_y;
	struct tm_main *ta;
	int i;

	ta = tm_main(tc);
	margin_icon = tm_x_margin_icon(tc);
	old_y = 0;

	for (i = 0; i < TM_OBJECT_MAX; i++) {
		struct tm_object *o;
		double x, y;

		o = tm_objs[i];

		if (!o || !o->draw)
			continue;

		x = ta->origin[i].x;
		y = ta->origin[i].y;

		if (!old_y)
			old_y = y;

		if (y != old_y)
			tm_x_draw_line(tc, x - margin_icon, y - margin_icon);

		tm_x_translate(tc, x, y);
		o->draw(tc);
		tm_x_translate(tc, -x, -y);

		old_y = y;
	}
}

static void tm_draw_list(struct tm_context *tc, struct list_head *head)
{
	struct tm_item *item;
	struct tm_main *ta;

	ta = tm_main(tc);

	list_for_each_entry(item, head, list) {
		double x, y;
		int id;

		id = item->id;

		x = ta->origin[id].x;
		y = ta->origin[id].y;

		tm_x_translate(tc, x, y);
		tm_x_draw_text_one(tc, item);
		tm_x_translate(tc, -x, -y);
	}
}

static int tm_tc_init(struct tm_context *tc)
{
	size_t obj_off;
	int i, err;

	tc->should_stop = false;
	tc->draw_all = false;
	tc->init_done = false;
	INIT_LIST_HEAD(&tc->list_update);

	err = pthread_mutex_init(&tc->init_lock, NULL);
	if (err) {
		errno = err;
		pr_err("pthread_mutex_init");
		goto out;
	}
	err = pthread_cond_init(&tc->init_cond, NULL);
	if (err) {
		errno = err;
		pr_err("pthread_cond_init");
		goto out;
	}

	err = pthread_mutex_init(&tc->main_wake_lock, NULL);
	if (err) {
		errno = err;
		pr_err("pthread_mutex_init");
		goto out;
	}
	err = pthread_cond_init(&tc->main_wake_cond, NULL);
	if (err) {
		errno = err;
		pr_err("pthread_cond_init");
		goto out;
	}

	obj_off = TM_OBJECT_SIZE(sizeof(*tc));
	for (i = 0; i < TM_OBJECT_MAX; i++) {
		tc->offset[i] = obj_off;
		if (tm_objs[i])
			obj_off += TM_OBJECT_SIZE(tm_objs[i]->obj_size);
	}
out:
	return err;
}

static void tm_tc_free(struct tm_context *tc)
{
	free(tc);
}

static struct tm_context *tm_tc_alloc(void)
{
	struct tm_context *tc;

	tc = calloc(1, tm_ctx_size);
	if (tc) {
		int err;

		err = tm_tc_init(tc);
		if (err) {
			tm_tc_free(tc);
			tc = NULL;
		}
	}

	return tc;
}

static void tm_exit_all(struct tm_context *tc, int pos)
{
	int i;

	for (i = pos; i >=0; i--) {
		struct tm_object *o;

		o = tm_objs[i];

		if (!o || !o->exit)
			continue;

		o->exit(tc);
	}
}

static int
tm_init_all(struct tm_context *tc,  int argc, char **argv, int *exit_idx)
{
	int i, err;

	for (i = 0; i < TM_OBJECT_MAX; i++) {
		struct tm_object *o;

		o = tm_objs[i];

		if (!o || !o->init)
			continue;

		err = o->init(tc, argc, argv);
		if (err)
			break;
	}

	*exit_idx = i;

	return err;
}

static void tm_generate_object_origin(struct tm_context *tc)
{
	double margin, margin_icon, x, y;
	struct tm_area area;
	struct tm_main *ta;
	int i;

	ta = tm_main(tc);
	margin = tm_x_margin(tc);
	margin_icon = tm_x_margin_icon(tc);

	x = y = margin + margin_icon;
	area.width = area.height = 0;

	for (i = 0; i < TM_OBJECT_MAX; i++) {
		struct tm_object *o;

		o = tm_objs[i];

		if (!o || !o->get_area)
			continue;

#if 0
		/* Currently objects are lined up vetically. */
		if (area.width)
			x += area.width + margin_icon * 2;
#else
		if (area.height)
			y += area.height + margin_icon * 2;
#endif

		/* origin is used for cairo_translate(). When the arguments
		 * of cairo_translate are not integer aligned, painted icons
		 * are get blured!
		 * To avoid this odd behaivor, set the origin to integer
		 * aligned value.
		 */
		ta->origin[i].x = (int)x;
		ta->origin[i].y = (int)y;

		o->get_area(tc, &area);
	}
}

int main(int argc, char **argv)
{
	struct tm_context *tc;
	int err, exit_idx;

	err = 1;

	tc = tm_tc_alloc();
	if (!tc)
		goto out;

	err = tm_init_all(tc, argc, argv, &exit_idx);
	if (err)
		tc->should_stop = true;
	else
		tm_generate_object_origin(tc);

	/* Let other threads start working. */
	pthread_mutex_lock(&tc->init_lock);
	tc->init_done = true;
	pthread_cond_broadcast(&tc->init_cond);
	pthread_mutex_unlock(&tc->init_lock);

	if (err)
		goto err;

	/* All drawing operations are done on main thread. */
	for (;;) {
		LIST_HEAD(list_update);
		bool draw_all;

		pthread_mutex_lock(&tc->main_wake_lock);
		while (!tc->should_stop && list_empty(&tc->list_update) &&
		       !tc->draw_all)
			pthread_cond_wait(&tc->main_wake_cond,
					  &tc->main_wake_lock);

		if (tc->should_stop) {
			pthread_mutex_unlock(&tc->main_wake_lock);
			break;
		}

		if (!list_empty(&tc->list_update))
			list_replace_init(&tc->list_update, &list_update);

		draw_all = tc->draw_all;
		if (tc->draw_all)
			tc->draw_all = false;
		pthread_mutex_unlock(&tc->main_wake_lock);

		if (draw_all)
			tm_draw_all(tc);
		else if (!list_empty(&list_update))
			tm_draw_list(tc, &list_update);

		tm_x_flush(tc);
	}
err:
	tm_exit_all(tc, exit_idx - 1);

	tm_tc_free(tc);
out:
	return err;
}

static void tm_main_help(struct tm_context *tc)
{
	printf("usage: %s [options]\n", PACKAGE);

	printf("\n\t--help\n"
	       "\t\tShow this message and exit with 2.\n");
}

static void tm_main_help_all(struct tm_context *tc)
{
	int i;

	for (i = 0; i < TM_OBJECT_MAX; i++) {
		struct tm_object *o;

		o = tm_objs[i];

		if (!o || !o->help)
			continue;

		o->help(tc);
	}
}

static int tm_main_parse_opts(struct tm_context *tc, int argc, char **argv)
{
	int i, err;

	err = 0;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--help")) {
			tm_main_help_all(tc);
			err = 2;
			break;
		}
	}

	return err;
}

static int tm_main_init(struct tm_context *tc, int argc, char **argv)
{
	sigset_t set;
	int err;

	err = tm_main_parse_opts(tc, argc, argv);
	if (err)
		goto out;

	/* In main thread, We are going to block all signals.
	 * Each threads may unmask their desired signals.
	 */
	err = sigfillset(&set);
	if (err) {
		pr_err("sigfillset");
		goto out;
	}

	err = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (err)
		pr_err("pthread_sigmask");
out:
	return err;
}

static void tm_main_exit(struct tm_context *tc)
{
}

static struct tm_object tm_object_main = {
	.obj_size	= sizeof(struct tm_main),
	.help		= tm_main_help,
	.init		= tm_main_init,
	.exit		= tm_main_exit
};

__attribute__((constructor))
static void tm_main_constructor(void)
{
	int err;
	
	err = tm_object_register(TM_OBJECT_MAIN, &tm_object_main);
	if (err)
		panic();
}
