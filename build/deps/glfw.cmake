# -----------------------------------------------------------------
#
# This compiles GLFW that we use during development to create a
# windows and GL context (we use our own function loading API).
#
# -----------------------------------------------------------------

if (TARGET glfw)
  return()
endif()

# -----------------------------------------------------------------

if (UNIX)

  include(ExternalProject)

  ExternalProject_Add(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_SHALLOW 1
    UPDATE_COMMAND ""
    CMAKE_ARGS
    -DBUILD_SHARED_LIBS=OFF
    -DGLFW_BUILD_EXAMPLES=OFF
    -DGLFW_BUILD_TESTS=OFF
    -DGLFW_BUILD_DOCS=OFF
    -DGLFW_INSTALL=ON
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
  )

  list(APPEND tra_deps glfw)
  
endif()

# -----------------------------------------------------------------

if (UNIX)
  
  list(APPEND tra_libs
    ${CMAKE_INSTALL_PREFIX}/lib/libglfw3.a
    rt
    m
    dl
  )
  
endif()

# -----------------------------------------------------------------
