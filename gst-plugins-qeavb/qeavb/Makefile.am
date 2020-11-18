plugin_LTLIBRARIES = libgstqeavb.la

libgstqeavb_la_SOURCES = \
							  gstqeavb.c \
							  gstqeavbcommon.c \
							  gstqeavbpcmsrc.c \
							  gstqeavbtssrc.c

libgstqeavb_la_CFLAGS = \
							 $(GST_PLUGINS_BASE_CFLAGS) \
							 $(GST_BASE_CFLAGS) \
							 $(GST_CFLAGS)
libgstqeavb_la_LIBADD = \
							 $(GST_PLUGINS_BASE_LIBS) \
							 $(GST_BASE_LIBS) \
							 $(GST_LIBS)
libgstqeavb_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstqeavb_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = \
				 gstqeavbpcmsrc.h gstqeavbcommon.h eavb_shared.h gstqeavbtssrc.h