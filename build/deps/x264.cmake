# -----------------------------------------------------------------

if (TARGET x264)
  return()
endif()

if (WIN32)
  return()
endif()

# -----------------------------------------------------------------

if (UNIX)
  
  if (TRA_FORCE_REBUILD OR NOT EXISTS ${CMAKE_INSTALL_PREFIX}/lib/libx264.a)
    
    include(ExternalProject)
    include(${deps_dir}/nasm.cmake)
    
    ExternalProject_Add(
      x264
      DEPENDS nasm
      URL https://code.videolan.org/videolan/x264/-/archive/master/x264-master.tar.bz2
      BUILD_IN_SOURCE TRUE
      CONFIGURE_COMMAND PATH=$ENV{PATH}:${CMAKE_INSTALL_PREFIX}/bin/ <SOURCE_DIR>/configure --prefix=${CMAKE_INSTALL_PREFIX} --enable-static --enable-shared
      BUILD_COMMAND PATH=$ENV{PATH}:${CMAKE_INSTALL_PREFIX}/bin/ make VERBOSE=1
    )

    list(APPEND tra_deps x264)

    # See `tra_add_module` below, where this is used to force the build of x264.
    set(x264_dep x264)

  endif()

  list(APPEND tra_libs ${CMAKE_INSTALL_PREFIX}/lib/libx264.so)
  include_directories(${CMAKE_INSTALL_PREFIX}/include)

  tra_add_module(
    NAME x264
    SOURCES ${mod_dir}/x264/x264.c
    DEPS ${x264_dep}
  )

endif()

# -----------------------------------------------------------------

