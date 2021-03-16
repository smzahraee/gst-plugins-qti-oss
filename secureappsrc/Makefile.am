bin_PROGRAMS=secureappsrc
secureappsrc_SOURCES = secureappsrc.c crypto.c
secureappsrc_CFLAGS = $(GST_CFLAGS)
secureappsrc_LDFLAGS = $(GST_LIBS) $(GST_PLUGIN_LDFLAGS) -lion -ldl
secureappsrc_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
  crypto.h

bin_PROGRAMS=secureappsrc2
secureappsrc2_SOURCES = secureappsrc2.c crypto.c
secureappsrc2_CFLAGS = $(GST_CFLAGS)
secureappsrc2_LDFLAGS = $(GST_LIBS) $(GST_PLUGIN_LDFLAGS) -lion -ldl
secureappsrc2_LIBTOOLFLAGS = --tag=disable-static
