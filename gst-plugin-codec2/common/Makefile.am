lib_LTLIBRARIES = libgstqticodec2common.la

WRAPPER_CFLAGS  = -I$(CURDIR)/../wrapper/include
VENC_CFLAGS     = -I$(CURDIR)/../venc

# sources used to compile this plug-in
libgstqticodec2common_la_SOURCES = gstqticodec2bufferpool.c

# compiler flags used to compile this plugin, set in configure.ac
libgstqticodec2common_la_CFLAGS = $(GST_CFLAGS) \
                                  $(GST_PLUGINS_BASE_CFLAGS) \
                                  $(GST_BASE_CFLAGS) \
                                  $(GST_IONBUF_CFLAGS) \
                                  $(WRAPPER_CFLAGS) \
                                  $(VENC_CFLAGS)

@libgstqticodec2common_la_CPPFLAGS = $(AM_CPPFLAGS)

# linker flags used to compile this plugin, set in configure.ac
libgstqticodec2common_la_LIBADD = $(GST_LIBS) \
                                  $(GST_PLUGINS_BASE_LIBS) \
                                  $(GST_BASE_LIBS) \
                                  -lgstvideo-$(GST_API_VERSION) \
                                 -lgstallocators-$(GST_API_VERSION)

libgstqticodec2common_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

libgstqticodec2common_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS) \
                --tag=disable-static
