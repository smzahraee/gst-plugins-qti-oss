plugin_LTLIBRARIES = libgstqticodec2venc.la

WRAPPER_CFLAGS  = -I$(CURDIR)/../wrapper/include
COMMON_CFLAGS   = -I$(CURDIR)/../common
WRAPPER_LDFLAGS = -L$(CURDIR)/../wrapper/.libs
COMMON_LDFLAGS  = -L$(CURDIR)/../common/.libs

# sources used to compile this plug-in
libgstqticodec2venc_la_SOURCES = gstqticodec2venc.c

# compiler flags used to compile this plugin, set in configure.ac
libgstqticodec2venc_la_CFLAGS = $(GST_CFLAGS) \
                                $(GST_PLUGINS_BASE_CFLAGS) \
                                $(GST_BASE_CFLAGS) \
                                $(GST_IONBUF_CFLAGS) \
                                $(WRAPPER_CFLAGS) \
                                $(COMMON_CFLAGS)

# linker flags used to compile this plugin, set in configure.ac
libgstqticodec2venc_la_LIBADD = $(GST_LIBS) \
                                $(GST_PLUGINS_BASE_LIBS) \
                                $(GST_BASE_LIBS) \
                                -lgstvideo-$(GST_API_VERSION) \
                                -lgstallocators-$(GST_API_VERSION) \
                                -lcodec2wrapper \
                                -lgstqticodec2common

libgstqticodec2venc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(WRAPPER_LDFLAGS) $(COMMON_LDFLAGS)

libgstqticodec2venc_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS) \
                                      --tag=disable-static
