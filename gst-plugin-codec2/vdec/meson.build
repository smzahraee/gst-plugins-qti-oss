gstqticodec2vdec_sources = [
  'gstqticodec2vdec.c'
]

gstqticodec2vdec = library('gstqticodec2vdec',
  gstqticodec2vdec_sources,
  c_args : gst_plugin_codec2_args,
  include_directories: [configinc, include_directories('../wrapper/include')],
  dependencies : [codec2wrapper_dep, gst_dep, gstvideo_dep, gstallocators_dep],
  install : true,
  install_dir : plugins_install_dir,
)
