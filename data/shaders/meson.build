
GLSLC = find_program('glslc')
shaders = []

if GLSLC.found()
  sources = [
    'simple.frag',
    'simple.vert',
    'cfd_source.comp',
    'cfd_diffuse.comp',
    'cfd_advect.comp',
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
