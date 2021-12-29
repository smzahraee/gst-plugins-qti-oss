AM_CFLAGS = -Wall \
         -Wundef \
         -Wstrict-prototypes\
         -Wno-trigraphs

AM_CPPFLAGS = -I$(CURDIR)/include

lib_LTLIBRARIES = libcodec2wrapper.la

libcodec2wrapper_la_SOURCES = c2ComponentAdapter.cpp \
                            c2ComponentInterfaceAdapter.cpp \
                            c2ComponentStoreAdapter.cpp \
                            codec2wrapper.cpp \
                            wrapper_utils.cpp

libcodec2wrapper_la_CFLAGS = $(AM_CFLAGS) \
                           $(GST_CFLAGS) \
                           $(GST_BASE_CFLAGS)

libcodec2wrapper_la_CPPFLAGS = $(AM_CPPFLAGS) \
                            $(GLIB_CFLAGS) \
                            $(GST_CFLAGS) \
                            $(GST_BASE_CFLAGS) \
                            -D_AGL_LINUX_ \
                            -DG_LOG_DOMAIN=\"codec2wrapper\"

libcodec2wrapper_la_CXXFLAGS = -std=c++14

libcodec2wrapper_la_LIBADD = -ldl \
                             -lcodec2_vndk \
                            $(GLIB_LDFLAGS) \
                            $(GST_LIBS)

libcodec2wrapper_la_LDFLAGS = -shared -avoid-version

include_HEADERS = include/codec2wrapper.h
