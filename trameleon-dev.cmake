# -----------------------------------------------------------------
#
# The `trameleon-dev` dependency adds utils that are used during
# the development of Trameleon. For example, when you include
# this .cmake file we will add the `dev/generator.c` to the
# Trameleon library. This allows you to generate a NV12 stream
# that you can feed into encoder.
#
# -----------------------------------------------------------------

set(tra_base_dir ${CMAKE_CURRENT_LIST_DIR}/../..)
set(tra_deps_dir ${tra_base_dir}/build/deps)
set(tra_src_dir ${tra_base_dir}/src)

# -----------------------------------------------------------------

include(${tra_deps_dir}/ffmpeg.cmake)

# -----------------------------------------------------------------

list(APPEND tra_sources
  ${tra_src_dir}/dev/generator.c
  )

# -----------------------------------------------------------------

