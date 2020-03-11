bin_PROGRAMS=secureappsrc
secureappsrc_SOURCES = secureappsrc.c crypto.c

secureappsrc_LDFLAGS = -lgstvideo-1.0 -lgstbase-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lion -ldl -lgstapp-1.0 -pthread
secureappsrc_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
  crypto.h
