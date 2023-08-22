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
    ${mod_dir}/nvidia/nvidia-dec.c
    ${mod_dir}/nvidia/nvidia-utils.c
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
    ${mod_dir}/nvidia/nvidia-enc.c
    ${mod_dir}/nvidia/nvidia-enc-host.c
    ${mod_dir}/nvidia/nvidia-utils.c
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
    ${mod_dir}/nvidia/nvidia-enc.c
    ${mod_dir}/nvidia/nvidia-enc-cuda.c
    ${mod_dir}/nvidia/nvidia-utils.c
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
    ${mod_dir}/nvidia/nvidia-enc.c
    ${mod_dir}/nvidia/nvidia-enc-opengl.c
    ${mod_dir}/nvidia/nvidia-utils.c
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
    ${mod_dir}/nvidia/nvidia-cuda.c
  LIBS
    cuda
    )

# -----------------------------------------------------------------

# `nvconverter` CUDA based converter.

tra_add_module(
  NAME nvidia-converter
  SOURCES
    ${mod_dir}/nvidia/nvidia-converter.c
  LIBS
    cuda
    tra-nvidia-converter-kernels
    )

# -----------------------------------------------------------------

# EASY layer NVIDIA context

tra_add_library(
  NAME nvidia-easy
  SOURCES
    ${mod_dir}/nvidia/nvidia-easy.c
    )

# -----------------------------------------------------------------


# `nvidia-converter` provides a CUDA based conversion
enable_language("CUDA")
add_library(tra-nvidia-converter-kernels STATIC ${mod_dir}/nvidia/nvidia-converter-kernels.cu)

# -----------------------------------------------------------------
