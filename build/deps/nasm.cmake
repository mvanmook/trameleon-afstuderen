# -----------------------------------------------------------------

if (TARGET nasm)
  return()
endif()

# -----------------------------------------------------------------

if (UNIX)

  if (EXISTS ${CMAKE_INSTALL_PREFIX}/bin/nasm AND NOT TRA_FORCE_REBUILD)
    return()
  endif()
   
  include(ExternalProject)
  
  ExternalProject_Add(
    nasm
    URL https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.bz2
    UPDATE_COMMAND ""
    BUILD_IN_SOURCE TRUE
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${CMAKE_INSTALL_PREFIX}
    BUILD_COMMAND make VERBOSE=1
  )
  
endif()

# -----------------------------------------------------------------
