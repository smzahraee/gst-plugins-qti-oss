#!/bin/sh
#
# gst-plugins-qccodec2 autogen.sh
#
# Run this to generate all the initial makefiles, etc.

autoreconf --verbose --install || {
 echo 'autogen.sh failed';
 exit 1;
}

./configure CC='clang' CXX='clang++' || {
 echo 'configure failed';
 exit 1;
}
