codec2wrapper_sources = [
  'c2ComponentAdapter.cpp',
  'c2ComponentInterfaceAdapter.cpp',
  'c2ComponentStoreAdapter.cpp',
  'codec2wrapper.cpp',
  'wrapper_utils.cpp'
]

codec2wrapper_inc = include_directories('include')
codec2_vndk_dep = cxx.find_library('codec2_vndk')
thread_dep = dependency('threads')

codec2wrapper = library('codec2wrapper',
  codec2wrapper_sources,
  cpp_args : [ '-D_AGL_LINUX_' , '-DG_LOG_DOMAIN="codec2wrapper"' , '-std=c++14' ],
  include_directories: [codec2wrapper_inc],
  dependencies : [glib_dep, gstvideo_dep, gst_dep, codec2_vndk_dep, dl_dep, thread_dep],
  install : true,
)

codec2wrapper_dep = declare_dependency(link_with : codec2wrapper, include_directories : codec2wrapper_inc)
