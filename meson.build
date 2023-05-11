project('RocketDynamics',
  'cpp',
  default_options : [
    'cpp_std=c++20',
    'buildtype=debug',
    'warning_level=3',
    'werror=false',
    'default_library=shared'],
  version : '0.0.1')

add_project_arguments(
    '-Wconversion',

    # these are coming from entt
    '-Wno-sign-conversion',
    '-Wno-implicit-int-float-conversion',

    '-DGLFW_INCLUDE_VULKAN',
    language : 'cpp'
)

GTEST = subproject('gtest').get_variable('gtest_main_dep')
GMOCK = subproject('gtest').get_variable('gmock_dep')

GTEST = declare_dependency(
    dependencies : [GTEST, GMOCK],
)

FMT = dependency('fmt', fallback : ['fmt', 'fmt_dep'])
SPDLOG = dependency('spdlog', fallback : ['spdlog', 'spdlog_dep'])
VULKAN = dependency('vulkan')
IMGUI = dependency('imgui', fallback : ['imgui', 'imgui_dep'])
GLFW = dependency('glfw3')
GLM = dependency('glm', fallback : ['glm', 'glm_dep'])
ENTT = dependency('entt', fallback : ['entt', 'entt_dep'])
# VK_HEADERS = subproject('vulkan-headers').get_variable('vulkan_headers_dep')
# VK_VALIDATIONLAYERS = subproject('vulkan-validationlayers').get_variable('vulkan_validationlayers_dep')
# VULKAN = declare_dependency(
#     dependencies : [VULKAN, VK_HEADERS, VK_VALIDATIONLAYERS],
# )

BOOST = dependency(
    'boost',
    modules : [
        'program_options',
    ],
)

INCLUDE = []
INCLUDE += include_directories('include')
INCLUDE += include_directories('external')

# subdir('include')
subdir('data')
subdir('src')
subdir('unittests')

executable(
  'rocketdynamics',
  sources : SOURCES,
  include_directories : INCLUDE,
  dependencies : [
    BOOST,
    FMT,
    GLM,
    GLFW,
    ENTT,
    SHADERS,
    IMGUI,
    SPDLOG,
    VULKAN,
  ],
)

