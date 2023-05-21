
GLSLC = find_program('glslc')
shaders = []

if GLSLC.found()
  sources = [
    'simple.frag',
    'simple.vert',
    'cfd_dens_source.comp',
    'cfd_dens_diffuse_prep.comp',
    'cfd_dens_diffuse.comp',
    'cfd_dens_advect.comp',
    'cfd_vel_forces.comp',
    'cfd_vel_diffuse.comp',
    'cfd_vel_divergence.comp',
    'cfd_vel_advect.comp',
    'cfd_vel_project.comp',
    'cfd_vel_update.comp',
    'cfd_boundary.comp',
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
