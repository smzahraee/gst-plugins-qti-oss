plugin_LTLIBRARIES = libgstqticodec2vdec.la

AM_CPPFLAGS = -I$(CURDIR)/../wrapper/include

# sources used to compile this plug-in
libgstqticodec2vdec_la_SOURCES = gstqticodec2vdec.c

libgstqticodec2vdec_la_CPPFLAGS = $(AM_CPPFLAGS)

# compiler flags used to compile this plugin, set in configure.ac
libgstqticodec2vdec_la_CFLAGS = $(GST_CFLAGS) \
                $(GST_PLUGINS_BASE_CFLAGS) \
                $(GST_BASE_CFLAGS) \
                $(GST_IONBUF_CFLAGS)

# linker flags used to compile this plugin, set in configure.ac
libgstqticodec2vdec_la_LIBADD = $(GST_LIBS) \
                $(GST_PLUGINS_BASE_LIBS) \
                $(GST_BASE_LIBS) \
                -lgstvideo-$(GST_API_VERSION) \
                -lgstallocators-$(GST_API_VERSION) \
                -lcodec2wrapper

libgstqticodec2vdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -L$(CURDIR)/../wrapper/.libs

libgstqticodec2vdec_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS) \
                --tag=disable-static
