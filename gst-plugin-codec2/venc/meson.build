gstqticodec2venc_sources = [
  'gstqticodec2venc.c'
]

gstqticodec2venc = library('gstqticodec2venc',
  gstqticodec2venc_sources,
  c_args : gst_plugin_codec2_args,
  include_directories: [configinc, include_directories('../wrapper/include')],
  dependencies : [codec2wrapper_dep, gstqticodec2common_dep, gst_dep, gstvideo_dep, gstallocators_dep],
  install : true,
  install_dir : plugins_install_dir,
)
