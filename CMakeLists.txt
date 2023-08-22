# -----------------------------------------------------------------

cmake_minimum_required(VERSION 3.24)
project(trameleon C CXX)

# -----------------------------------------------------------------

set(curr_dir "${CMAKE_CURRENT_LIST_DIR}")
set(base_dir "${curr_dir}/../")
set(src_dir "${base_dir}/src")
set(inc_dir "${base_dir}/include")
set(ext_dir "${base_dir}/extern")
set(deps_dir "${curr_dir}/deps")
set(rxtx_dir ${ROXLU_RXTX_DIR})
set(mod_dir "${base_dir}/src/tra/modules")

# -----------------------------------------------------------------

option(TRA_BUILD_STATIC_LIB "Build a static library. This means you need to initialize the modules manually." OFF)

# -----------------------------------------------------------------

string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
if (${build_type} MATCHES "debug")
  set(debug_flag "-debug")
endif()

# -----------------------------------------------------------------

include(${curr_dir}/deps/glfw.cmake)
include(${curr_dir}/deps/tracy.cmake) # experimenting ..
include(${curr_dir}/deps/ffmpeg.cmake)
include(${curr_dir}/deps/glad.cmake)

# -----------------------------------------------------------------

include(${curr_dir}/deps/trameleon.cmake)
include(${curr_dir}/deps/trameleon-core.cmake)
include(${curr_dir}/deps/trameleon-dev.cmake)
include(${curr_dir}/deps/trameleon-nvidia.cmake)
include(${curr_dir}/deps/trameleon-x264.cmake)
include(${curr_dir}/deps/trameleon-vaapi.cmake)
include(${curr_dir}/deps/trameleon-opengl.cmake)

#include(${curr_dir}/deps/trameleon-netint.cmake)
#include(${curr_dir}/deps/trameleon-mft.cmake)
#include(${curr_dir}/deps/trameleon-vtbox.cmake)

# -----------------------------------------------------------------

add_definitions(-DTRA_PROFILER_ENABLED)

# -----------------------------------------------------------------

tra_add_library(
  NAME tra
  SOURCES ${tra_sources}
  DEPS ${tra_deps}
  )

# -----------------------------------------------------------------
  
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
else()
  target_compile_options(
    tra
    PRIVATE
    -Werror=incompatible-pointer-types
    -Werror=implicit-function-declaration
    )
endif()

# -----------------------------------------------------------------

if(UNIX)
  list(APPEND tra_libs pthread)
endif()

# -----------------------------------------------------------------

include_directories(${tra_inc_dirs})

# -----------------------------------------------------------------

# Testing with custom profiler macros. 
add_definitions(-DTRA_PROFILER_USE_CUSTOM_MACROS)
add_definitions(-DTRACY_ENABLE)
include_directories(${inc_dir}/dev/) 

# -----------------------------------------------------------------

tra_create_test(NAME "compile")
#tra_create_test(NAME "log") # uses RXTX (commented for Maarten) 
#tra_create_test(NAME "profiler") # uses RXTX (commented for Maarten)
#tra_create_test(NAME "registry")
#tra_create_test(NAME "modules")
tra_create_test(NAME "module-x264-encoder")
#tra_create_test(NAME "opengl" LIBS "cuda")
tra_create_test(NAME "opengl-converter")
tra_create_test(NAME "easy-encoder")
tra_create_test(NAME "easy-decoder")
tra_create_test(NAME "easy-transcoder")

# -----------------------------------------------------------------

if (UNIX AND NOT APPLE)
  tra_create_test(NAME "module-vaapi-encoder") 
  tra_create_test(NAME "module-vaapi-decoder")
  # tra_create_test(NAME "module-netint-encoder")
  # tra_create_test(NAME "module-netint-decoder")
  tra_create_test(NAME "module-nvidia-encoder")
  tra_create_test(NAME "module-nvidia-decoder")
  tra_create_test(NAME "module-nvidia-transcoder")
#  tra_create_test(NAME "module-nvidia-graphics" LIBS "cuda") # uses RXTX (commented for Maarten)
  tra_create_test(NAME "module-nvidia-converter")
endif()

# -----------------------------------------------------------------

if (APPLE)
  tra_create_test(NAME "module-vtbox-encoder")
  tra_create_test(NAME "module-vtbox-decoder")
endif()

# -----------------------------------------------------------------

if (WIN32)
  tra_create_test(NAME "module-mft-encoder")
  tra_create_test(NAME "module-mft-decoder")
  tra_create_test(NAME "debug")
endif()

# -----------------------------------------------------------------
