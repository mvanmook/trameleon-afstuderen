# -----------------------------------------------------------------

set(yasm_prefix ${CMAKE_CURRENT_BINARY_DIR}/yasm)

# -----------------------------------------------------------------

if (TARGET yasm)
  return()
endif()

# -----------------------------------------------------------------

include(ExternalProject)

# -----------------------------------------------------------------

if (NOT EXISTS ${yasm_prefix}/bin/yasm)

  ExternalProject_Add(
    yasm
    URL http://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
    BUILD_COMMAND make VERBOSE=1
    PREFIX ${yasm_prefix}
    )

endif()

# -----------------------------------------------------------------
