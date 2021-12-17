project('gst-plugin-codec2', 'c',
  version : '1.0.0',
  meson_version : '>= 0.47',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')
plugins_pkgconfig_install_dir = join_paths(plugins_install_dir, 'pkgconfig')

have_cxx = add_languages('cpp', required : false)
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')
host_system = host_machine.system()

gst_plugin_codec2_args = ['-DHAVE_CONFIG_H']

configinc = include_directories('.')

core_conf = configuration_data()

core_conf.set_quoted('PACKAGE', 'gst-plugin-codec2')
core_conf.set_quoted('PACKAGE_NAME', 'gst-plugin-codec2')
core_conf.set_quoted('VERSION', '1.0')

gst_dep = dependency('gstreamer-1.0',
  fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0',
  fallback : ['gstreamer', 'gst_base_dep'])
gstvideo_dep = dependency('gstreamer-video-1.0',
  fallback : ['gst-plugins-base', 'video_dep'])
gstallocators_dep = dependency('gstreamer-allocators-1.0',
  fallback : ['gst-plugins-base', 'allocators_dep'])
glib_dep = dependency('glib-2.0', version: '>= 2.36', required: false)
dl_dep = cxx.find_library('dl')

configure_file(output : 'config.h', configuration : core_conf)

subdir('wrapper')
subdir('common')
subdir('vdec')
subdir('venc')
