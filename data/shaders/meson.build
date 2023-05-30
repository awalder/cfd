
GLSLC = find_program('glslc')
shaders = []

if GLSLC.found()
  sources = [
    'simple.frag',
    'simple.vert',
    'collision.comp',
    'streaming.comp',
    'boundary.comp',
    'macro.comp',
    'cfd_render.comp',
  ]

  foreach s : sources
    shaders += custom_target('shader_@0@'.format(s),
      input : s,
      output : '@PLAINNAME@.spv',
      depend_files : 'common.glsl',
      command : [GLSLC, '@INPUT@', '-o', '@OUTPUT@'],
    )
  endforeach
endif

SHADERS = declare_dependency(
  sources : shaders
)
