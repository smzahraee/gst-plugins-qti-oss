gstqticodec2common_sources = [
  'gstqticodec2bufferpool.c'
]

gstqticodec2common = library('gstqticodec2common',
  gstqticodec2common_sources,
  c_args : gst_plugin_codec2_args,
  include_directories: [configinc, include_directories('../wrapper/include')],
  dependencies : [codec2wrapper_dep, gst_dep, gstvideo_dep, gstallocators_dep],
  install : true,
)

gstqticodec2common_inc = include_directories('.')
gstqticodec2common_dep = declare_dependency(link_with : gstqticodec2common, include_directories : gstqticodec2common_inc)
