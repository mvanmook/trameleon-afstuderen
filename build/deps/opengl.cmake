# -----------------------------------------------------------------

# The OpenGL API 
tra_add_module(
  NAME opengl-api
  SOURCES
    ${mod_dir}/opengl/opengl-api.c
  )

# -----------------------------------------------------------------

# The OpenGL Graphics API 
tra_add_module(
  NAME opengl-gfx
  SOURCES
    ${mod_dir}/opengl/opengl-gfx.c
  )

# -----------------------------------------------------------------

# The OpenGL CUDA Interop API
# tra_add_module(
#   NAME opengl-interop-cuda
#   SOURCES
#     ${mod_dir}/opengl/opengl-interop-cuda.c
#   )

# -----------------------------------------------------------------

# The OpenGL Converter API
tra_add_module(
  NAME opengl-converter
  SOURCES
    ${mod_dir}/opengl/opengl-converter.c
  )

# -----------------------------------------------------------------
