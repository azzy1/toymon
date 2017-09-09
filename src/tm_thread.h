#ifndef _TM_THREAD_H
#define _TM_THREAD_H

#include "tm.h"

struct tm_thread_timer {
	struct list_head	list;
	int			(*timer_cb)(struct tm_context *tc);
	int			expires_msecs;	/* expires in milliseconds. */
	int			orig_expires;	/* Never touch! work area.*/
};

extern void tm_thread_timer_add(struct tm_thread_timer *ttc);
extern void tm_thread_timer_del(struct tm_thread_timer *ttc);

#endif /* _TM_THREAD_H */
