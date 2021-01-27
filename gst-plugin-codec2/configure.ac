dnl required version of autoconf
AC_PREREQ(2.62)

dnl initialize autoconf
dnl releases only do -Wall, git and prerelease does -Werror too
dnl use a three digit version number for releases, and four for git/pre
AC_INIT([GStreamer Codec2 Plug-ins],[1.0.0])

AG_GST_INIT

dnl initialize automake
AM_INIT_AUTOMAKE([foreign])

# dnl define the output header for config
AM_CONFIG_HEADER([config.h])

dnl AM_MAINTAINER_MODE only provides the option to configure to enable it
AM_MAINTAINER_MODE([enable])

dnl our libraries and install dirs use GST_API_VERSION in the filename
dnl to allow side-by-side installation of different API versions
GST_API_VERSION=1.0
AC_SUBST(GST_API_VERSION)
AC_DEFINE_UNQUOTED(GST_API_VERSION, "$GST_API_VERSION",
  [GStreamer API Version])

AC_DEFINE(USE_GBM, [], [define USE_GBM macro])

AG_GST_LIBTOOL_PREPARE

dnl required versions of gstreamer and plugins-base
GST_REQUIRED=1.0.0
GSTPB_REQUIRED=1.0.0

dnl Add parameters for aclocal
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

dnl find a compiler
AC_PROG_CC
AC_PROG_CC_STDC

dnl determine c++ compiler
AC_PROG_CXX

dnl required version of libtool
LT_PREREQ([2.2.6])
LT_INIT

dnl give error and exit if we don't have pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([Pkg-config not installed!])
])

dnl Check for the required version of GStreamer core (and gst-plugins-base)
dnl This will export GST_CFLAGS and GST_LIBS variables for use in Makefile.am
PKG_CHECK_MODULES(GST, [
  gstreamer-1.0 >= $GST_REQUIRED
  gstreamer-base-1.0 >= $GST_REQUIRED
  gstreamer-controller-1.0 >= $GST_REQUIRED
], [
  AC_SUBST(GST_CFLAGS)
  AC_SUBST(GST_LIBS)
], [
  AC_MSG_ERROR([
    Required version of GStreamer $GST_REQUIRED not found.
  ])
])

dnl Check for documentation xrefs
GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
GST_PREFIX="`$PKG_CONFIG --variable=prefix gstreamer-$GST_API_VERSION`"
GSTPB_PREFIX="`$PKG_CONFIG --variable=prefix gstreamer-plugins-base-$GST_API_VERSION`"
AC_SUBST(GLIB_PREFIX)
AC_SUBST(GST_PREFIX)
AC_SUBST(GSTPB_PREFIX)

dnl set license and copyright notice
GST_LICENSE="BSD"
AC_DEFINE_UNQUOTED(GST_LICENSE, "$GST_LICENSE", [GStreamer license])
AC_SUBST(GST_LICENSE)

dnl check for debug flag
AC_MSG_CHECKING([Whether to build with debug information])
AC_ARG_ENABLE(debug,
  [AS_HELP_STRING([--enable-debug], [disable support for running in debug mode])],
  [enable_debug=yes],
  []
)
AC_MSG_RESULT([$enable_debug])

dnl every flag in GST_OPTION_CFLAGS, GST_OPTION_CXXFLAGS and GST_OPTION_OBJCFLAGS can be overridden
GST_OPTION_CFLAGS="\$(ERROR_CFLAGS)"
GST_OPTION_CXXFLAGS="\$(ERROR_CXXFLAGS)"
GST_OPTION_OBJCFLAGS="\$(ERROR_OBJCFLAGS)"
AC_SUBST(GST_OPTION_CFLAGS)
AC_SUBST(GST_OPTION_CXXFLAGS)
AC_SUBST(GST_OPTION_OBJCFLAGS)

GLIB_LDFLAGS="`$PKG_CONFIG --libs glib-2.0`"
GLIB_CFLAGS="`$PKG_CONFIG --cflags glib-2.0`"
AC_SUBST(GLIB_LDFLAGS)
AC_SUBST(GLIB_CFLAGS)

dnl our libraries need to be versioned correctly
AC_SUBST(GST_LT_LDFLAGS)

dnl add other flags
GST_CXXFLAGS="$GST_CFLAGS $GLIB_EXTRA_CFLAGS \$(GST_OPTION_CXXFLAGS)"
GST_OBJCFLAGS="$GST_CFLAGS $GLIB_EXTRA_CFLAGS \$(GST_OPTION_OBJCFLAGS)"
GST_CFLAGS="$GST_CFLAGS $GLIB_EXTRA_CFLAGS \$(GST_OPTION_CFLAGS)"
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_CXXFLAGS)
AC_SUBST(GST_OBJCFLAGS)
AC_SUBST(GST_LIBS)

dnl LDFLAGS really should only contain flags, not libs - they get added before
dnl whatevertarget_LIBS and -L flags here affect the rest of the linking
GST_ALL_LDFLAGS="-no-undefined"
AC_SUBST(GST_ALL_LDFLAGS)

dnl set the plugindir where plugins should be installed
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-1.0/plugins"
else
  plugindir="\$(libdir)/gstreamer-1.0"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_CONFIG_FILES(
Makefile 
common/Makefile
vdec/Makefile
venc/Makefile
wrapper/Makefile
)

AC_OUTPUT
