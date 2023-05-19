
GLSLC = find_program('glslc')
shaders = []

if GLSLC.found()
  sources = [
    'simple.frag',
    'simple.vert',
    'cfd.comp',
    'cfd_render.comp',
  ]

  foreach s : sources
    shaders += custom_target('shader_@0@'.format(s),
      input : s,
      output : '@PLAINNAME@.spv',
      command : [GLSLC, '@INPUT@', '-o', '@OUTPUT@'],
    )
  endforeach
endif

SHADERS = declare_dependency(
  sources : shaders
)
