bin_PROGRAMS	= toymon

toymon_CFLAGS	= -DICONSDIR='"$(pkgdatadir)/icons/"'			\
		  $(XCB_SHAPE_CFLAGS) $(CAIRO_XCB_CFLAGS)		\
		  $(PANGOCAIRO_CFLAGS) $(LIBRSVG_CFLAGS) -pthread

toymon_LDFLAGS	= -pthread

toymon_LDADD	= $(XCB_SHAPE_LIBS) $(CAIRO_XCB_LIBS)			\
		  $(PANGOCAIRO_LIBS) $(LIBRSVG_LIBS) -lm

toymon_SOURCES	= tm.h tm_types.h tm_stddef.h tm_list.h			\
		  tm_main.c tm_main.h					\
		  tm_thread.c tm_thread.h				\
		  tm_item.c tm_item.h					\
		  tm_x.c tm_x.h						\
		  tm_clock.c tm_cpu.c tm_mem.c tm_disk.c tm_net.c
