# -----------------------------------------------------------------
#
# OpenGL interop module for Trameleon
#
# -----------------------------------------------------------------

set(tra_base_dir ${CMAKE_CURRENT_LIST_DIR}/../..)
set(tra_mod_dir ${tra_base_dir}/src/tra/modules)

# -----------------------------------------------------------------

# The OpenGL API 
tra_add_module(
  NAME opengl-api
  SOURCES
    ${tra_mod_dir}/opengl/opengl-api.c
  )

# -----------------------------------------------------------------

# The OpenGL Graphics API 
tra_add_module(
  NAME opengl-gfx
  SOURCES
    ${tra_mod_dir}/opengl/opengl-gfx.c
  )

# -----------------------------------------------------------------

# The OpenGL CUDA Interop API
# tra_add_module(
#   NAME opengl-interop-cuda
#   SOURCES
#     ${tra_mod_dir}/opengl/opengl-interop-cuda.c
#   )

# -----------------------------------------------------------------

# The OpenGL Converter API
tra_add_module(
  NAME opengl-converter
  SOURCES
    ${tra_mod_dir}/opengl/opengl-converter.c
  )

# -----------------------------------------------------------------
