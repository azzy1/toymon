#ifndef _TM_H
#define _TM_H

#include <config.h>
#include "tm_types.h"
#include "tm_list.h"
#include <pthread.h>

#define ARRAY_SIZE(__ary)	\
	(sizeof(__ary) / sizeof((__ary)[0]))

#define ALIGN_MASK(size, mask)	((size + (mask)) & ~(mask))
#define ALIGN(size, a)		ALIGN_MASK(size, a - 1L)
#define TM_OBJECT_SIZE(size)	ALIGN(size, 8L)

/**
 * @TM_OBJECT_MAIN: main context. Prepare first.
 * @TM_OBJECT_X: Prepare X resources.
 * @TM_OBJECT_CLOCK: clock object.
 * @TM_OBJECT_CPU: CPU object.
 * @TM_OBJECT_MEM: memory object.
 * @TM_OBJECT_DISK: disk object.
 * @TM_OBJECT_NET: network object.
 * @TM_OBJECT_THREAD: signal/timer handling. *Must be last.*
 * @TM_OBJECT_MAX: nr objects.
 */
enum {
	TM_OBJECT_MAIN,
	TM_OBJECT_X,
	TM_OBJECT_CLOCK,
	TM_OBJECT_CPU,
	TM_OBJECT_MEM,
	TM_OBJECT_DISK,
	TM_OBJECT_NET,
	TM_OBJECT_THREAD,
	TM_OBJECT_MAX
};

struct tm_area {
	double	width;
	double	height;
};

struct tm_context {
	bool			should_stop;

	bool			draw_all;

	bool			init_done;
	pthread_mutex_t		init_lock;
	pthread_cond_t		init_cond;

	struct list_head	list_update;
	pthread_mutex_t		main_wake_lock;
	pthread_cond_t		main_wake_cond;

	size_t			offset[TM_OBJECT_MAX];
};

/**
 * @obj_size: Ones private size. We don't want to scatter memories which are
 * frequently accessed.
 * @init: Initialization process.
 * @exit: Tear down process.
 * @help: Show help message.
 * @get_area: Calculate and return desiable area size.
 * @draw: Called on translated coordination. No need to worry about
 * coordination. Always guranteed that upper-left corner is the origin.
 */
struct tm_object {
	size_t		obj_size;
	int		(*init)(struct tm_context *, int, char **);
	void		(*exit)(struct tm_context *);
	void		(*help)(struct tm_context *);
	void		(*get_area)(struct tm_context *, struct tm_area *);
	void		(*draw)(struct tm_context *);
};

static inline void *tm_get_object(struct tm_context *tc, int id)
{
	return (char *)tc + tc->offset[id];
}

#endif /* _TM_H */
