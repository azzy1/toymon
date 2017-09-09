#ifndef _TM_MAIN_H
#define _TM_MAIN_H

#include "tm.h"
#include <errno.h>

#define panic()			\
	__panic(__func__, __LINE__)

#define pr_err(__s)		\
	__pr_err(__func__, __LINE__, __s, errno)

extern void __panic(const char *func, int line);
extern void __pr_err(const char *func, int line, const char *s, int eno);
extern int tm_object_register(int id, struct tm_object *o);
extern int tm_object_unregister(int id);

#endif /* _TM_MAIN_H */
