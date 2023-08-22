# -----------------------------------------------------------------
#
# NVIDIA module for Trameleon
#
# -----------------------------------------------------------------

set(tra_base_dir ${CMAKE_CURRENT_LIST_DIR}/../../)
set(tra_src_dir ${tra_base_dir}/src)
set(tra_inc_dir ${tra_base_dir}/include)
set(tra_mod_dir ${tra_base_dir}/src/tra/modules)

# -----------------------------------------------------------------

# https://cliutils.gitlab.io/modern-cmake/chapters/packages/CUDA.html
include(CheckLanguage)
enable_language(CUDA)
check_language(CUDA)

# -----------------------------------------------------------------

include_directories(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

# -----------------------------------------------------------------

# `nvenc` decoder.

tra_add_module(
  NAME nvidia
  SOURCES
    ${tra_mod_dir}/nvidia/nvidia-dec.c
    ${tra_mod_dir}/nvidia/nvidia-utils.c
  LIBS
   cuda
   nvcuvid
   nvidia-encode
  )

# -----------------------------------------------------------------

# `nvenchost` encoder: encode `tra_memory_image` buffers.

tra_add_module(
  NAME nvidia-enc-host
  SOURCES
    ${tra_mod_dir}/nvidia/nvidia-enc.c
    ${tra_mod_dir}/nvidia/nvidia-enc-host.c
    ${tra_mod_dir}/nvidia/nvidia-utils.c
  LIBS
   cuda
   nvcuvid
   nvidia-encode
  )

# -----------------------------------------------------------------
  
# `nvenccuda` encoder: encode `tra_memory_cuda` buffers.

tra_add_module(
  NAME nvidia-enc-cuda
  SOURCES
    ${tra_mod_dir}/nvidia/nvidia-enc.c
    ${tra_mod_dir}/nvidia/nvidia-enc-cuda.c
    ${tra_mod_dir}/nvidia/nvidia-utils.c
  LIBS
   cuda
   nvcuvid
   nvidia-encode
  )

# -----------------------------------------------------------------

# `nvencgl` encoder: encode OpenGL textures buffers.

tra_add_module(
  NAME nvidia-enc-opengl
  SOURCES
    ${tra_mod_dir}/nvidia/nvidia-enc.c
    ${tra_mod_dir}/nvidia/nvidia-enc-opengl.c
    ${tra_mod_dir}/nvidia/nvidia-utils.c
  LIBS
   cuda
   nvcuvid
   nvidia-encode
  )

# -----------------------------------------------------------------

# `nvidia-cuda` provides the cuda api.

tra_add_module(
  NAME nvidia-cuda
  SOURCES
    ${tra_mod_dir}/nvidia/nvidia-cuda.c
  LIBS
    cuda
    )

# -----------------------------------------------------------------

# `nvconverter` CUDA based converter.

tra_add_module(
  NAME nvidia-converter
  SOURCES
    ${tra_mod_dir}/nvidia/nvidia-converter.c
  LIBS
    cuda
    tra-nvidia-converter-kernels
    )

# -----------------------------------------------------------------

# EASY layer NVIDIA context

tra_add_library(
  NAME nvidia-easy
  SOURCES
    ${tra_mod_dir}/nvidia/nvidia-easy.c
    )

# -----------------------------------------------------------------


# `nvidia-converter` provides a CUDA based conversion
enable_language("CUDA")
add_library(tra-nvidia-converter-kernels STATIC ${tra_mod_dir}/nvidia/nvidia-converter-kernels.cu)

# -----------------------------------------------------------------
