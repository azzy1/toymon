#include "tm_thread.h"
#include "tm_main.h"
#include "tm_x.h"
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

struct tm_thread {
	pthread_t	tid_ev_mnger;
};

static struct tm_thread *tm_thread(struct tm_context *tc)
{
	return tm_get_object(tc, TM_OBJECT_THREAD);
}

/* We would add mutex lock for timer_head. */
static LIST_HEAD(timer_head);
/* interval time in milliseconds */
static int interval_msecs = 3000;

void tm_thread_timer_add(struct tm_thread_timer *ttc)
{
	struct tm_thread_timer *timer;

	ttc->orig_expires = ttc->expires_msecs;

	/* Set the smallest expires_msecs to interval_msecs. */
	if (interval_msecs > ttc->expires_msecs)
		interval_msecs = ttc->expires_msecs;

	/* mutex lock would be needed. */
	list_for_each_entry(timer, &timer_head, list) {
		if (timer->expires_msecs > ttc->expires_msecs)
			break;
	}

	list_add(&ttc->list, &timer->list);
}

void tm_thread_timer_del(struct tm_thread_timer *ttc)
{
	struct tm_thread_timer *timer;

	/* We need mutex lock here ? */
	list_for_each_entry(timer, &timer_head, list) {
		if (timer == ttc) {
			list_del(&timer->list);
			return;
		}
	}
}

static int tm_thread_set_signal_handler(int signum, void (*sig_handler)(int))
{
	struct sigaction act;
	sigset_t mask;
	int err;

	/* Install signal handler. */
	act = (struct sigaction){
		.sa_handler = sig_handler
	};
	err = sigfillset(&act.sa_mask);
	if (err) {
		pr_err("sigfillset");
		goto out;
	}
	err = sigaction(signum, &act, NULL);
	if (err) {
		pr_err("sigaction");
		goto out;
	}

	/* Now unblock signum signal. */
	err = sigemptyset(&mask);
	if (err) {
		pr_err("sigemptyset");
		goto out;
	}
	err = sigaddset(&mask, signum);
	if (err) {
		pr_err("sigaddset");
		goto out;
	}
	err = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
	if (err)
		pr_err("pthread_sigmask");
out:
	return err;
}

static int nr_sigint;
static int nr_sigalrm;
static pthread_mutex_t tm_sig_lock = PTHREAD_MUTEX_INITIALIZER;

static void tm_thread_sig_record(int signum)
{
	pthread_mutex_lock(&tm_sig_lock);
	if (signum == SIGINT)
		nr_sigint++;
	else if (signum == SIGALRM)
		nr_sigalrm++;
	pthread_mutex_unlock(&tm_sig_lock);
}

static int tm_thread_set_itimer(int interval_msecs)
{
	struct itimerval it;
	int err;

	it = (struct itimerval){
		.it_interval = {
			.tv_sec = interval_msecs / 1000,
			.tv_usec = (interval_msecs % 1000) * 1000
		},
		.it_value = {
			.tv_sec = interval_msecs / 1000,
			.tv_usec = (interval_msecs % 1000) * 1000
		}
	};
	err = setitimer(ITIMER_REAL, &it, NULL);
	if (err)
		pr_err("setitimer");

	return err;
}

static int tm_thread_sigalrm_handler(struct tm_context *tc)
{
	struct tm_thread_timer *timer;
	int err;

	/* Find which timers are expired. */
	list_for_each_entry(timer, &timer_head, list) {
		timer->expires_msecs -= interval_msecs;
		if (timer->expires_msecs <= 0) {
			timer->expires_msecs += timer->orig_expires;
			err = timer->timer_cb(tc);
			if (err)
				goto out;
		}
	}

	if (tm_item_update_needed()) {
		/* Wake up main thread. */
		pthread_mutex_lock(&tc->main_wake_lock);
		tm_item_update_replace(&tc->list_update);
		pthread_cond_signal(&tc->main_wake_cond);
		pthread_mutex_unlock(&tc->main_wake_lock);
	}

	err = 0;
out:
	return err;
}

static int tm_thread_sigint_handler(struct tm_context *tc)
{
	tc->should_stop = true;

/* XXX */
//tm_x_send_expose_event(tc);

	return 0;
}

static int tm_thread_handle_events(struct tm_context *tc)
{
	int err, sigint_cnt, sigalrm_cnt;

	sigint_cnt = sigalrm_cnt = 0;

	pthread_mutex_lock(&tm_sig_lock);
	/* Avoid write to nr_sigint and nr_sigalrm if possible. */
	if (nr_sigint) {
		sigint_cnt = nr_sigint;
		nr_sigint = 0;
	}
	if (nr_sigalrm) {
		sigalrm_cnt = nr_sigalrm;
		nr_sigalrm = 0;
	}
	pthread_mutex_unlock(&tm_sig_lock);

	while (sigalrm_cnt--) {
		err = tm_thread_sigalrm_handler(tc);
		if (err)
			goto out;
	}
	while (sigint_cnt--) {
		err = tm_thread_sigint_handler(tc);
		if (err)
			goto out;
	}
out:
	return err;
}

static void *tm_thread_event_manager(void *data)
{
	struct tm_context *tc;
	int err;

	tc = data;

	/* Wait for all initializations have done. */
	pthread_mutex_lock(&tc->init_lock);
	while (!tc->init_done)
		pthread_cond_wait(&tc->init_cond, &tc->init_lock);
	pthread_mutex_unlock(&tc->init_lock);

	err = tm_thread_set_signal_handler(SIGINT, SIG_DFL) ||
	      tm_thread_set_signal_handler(SIGALRM, tm_thread_sig_record);
	if (err)
		goto out;

	err = tm_thread_set_itimer(interval_msecs);

	while (!tc->should_stop && !err) {
		pause();

		if (tc->should_stop)
			break;

		err = tm_thread_handle_events(tc);
	}
out:
	if (err)
		tc->should_stop = true;

	if (tc->should_stop)
		pthread_cond_signal(&tc->main_wake_cond);

	return NULL;
}

static int tm_thread_init(struct tm_context *tc, int argc, char **argv)
{
	struct tm_thread *thread;
	int err;

	thread = tm_thread(tc);

	err = pthread_create(&thread->tid_ev_mnger, NULL,
			     tm_thread_event_manager, tc);
	if (err) {
		errno = err;
		pr_err("pthread_create");
	}

	return err;
}

static void tm_thread_exit(struct tm_context *tc)
{
	struct tm_thread *thread;

	thread = tm_thread(tc);

	pthread_join(thread->tid_ev_mnger, NULL);
}

static struct tm_object tm_thread_obj = {
	.obj_size	= sizeof(struct tm_thread),
	.init		= tm_thread_init,
	.exit		= tm_thread_exit
};

__attribute__((constructor))
static void tm_thread_constructor(void)
{
	int err;

	err = tm_object_register(TM_OBJECT_THREAD, &tm_thread_obj);
	if (err)
		panic();
}
