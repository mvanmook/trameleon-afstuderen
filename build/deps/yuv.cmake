# -----------------------------------------------------------------
# This compiles libyuv which is not a dependency of Trameleon,
# though during development it provides some handy features to
# perform pixel format conversion.
# -----------------------------------------------------------------

set(yuv_prefix ${CMAKE_CURRENT_BINARY_DIR}/yuv)
set(yuv_install_prefix ${CMAKE_INSTALL_PREFIX})

# -----------------------------------------------------------------

if (TARGET yuv)
  return()
endif()

# -----------------------------------------------------------------

include(ExternalProject)

# -----------------------------------------------------------------

if (NOT EXISTS ${yuv_install_prefix}/include/libyuv.h OR TRA_FORCE_REBUILD)

  ExternalProject_Add(
    yuv
    GIT_REPOSITORY https://chromium.googlesource.com/libyuv/libyuv
    GIT_SHALLOW 1
    GIT_TAG stable
    UPDATE_COMMAND ""
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${yuv_install_prefix}
    PREFIX ${yuv_prefix}
    )

  list(APPEND tra_deps yuv)

endif()

# -----------------------------------------------------------------

if (UNIX) 
  list(APPEND tra_libs ${yuv_install_prefix}/lib/libyuv.a)
endif()
