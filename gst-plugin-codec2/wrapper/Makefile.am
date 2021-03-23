AM_CFLAGS = -Wall \
         -Wundef \
         -Wstrict-prototypes\
         -Wno-trigraphs

AM_CPPFLAGS = -I$(CURDIR)/include

lib_LTLIBRARIES = libcodec2wrapper.la

libcodec2wrapper_la_SOURCES = c2ComponentAdapter.cpp \
                            c2ComponentInterfaceAdapter.cpp \
                            c2ComponentStoreAdapter.cpp \
                            codec2wrapper.cpp

libcodec2wrapper_la_CFLAGS = $(AM_CFLAGS) \
                           $(GST_CFLAGS)

libcodec2wrapper_la_CPPFLAGS = $(AM_CPPFLAGS) \
                            $(GLIB_CFLAGS) \
                            $(GST_CFLAGS) \
                            -DC2PORT \
                            -DHAVE_MOCK_CODEC \
                            -DG_LOG_DOMAIN=\"codec2wrapper\"

libcodec2wrapper_la_CXXFLAGS = -std=c++14

libcodec2wrapper_la_LIBADD = -lcodec2_vndk \
                            -lqcodec2_base \
                            -lqcodec2_utils \
                            -lqcodec2_basecodec \
                            -lqcodec2_platform \
                            -lqcodec2_core \
                            -lqcodec2_hooks \
                            $(GLIB_LDFLAGS) \
                            $(GST_LIBS)

libcodec2wrapper_la_LDFLAGS = -shared -avoid-version

include_HEADERS = codec2wrapper.h
