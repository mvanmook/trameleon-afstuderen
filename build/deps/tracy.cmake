# -----------------------------------------------------------------

# TRACY
# =====
#
# By including this .cmake file we build the Tracy client library
# that can then be used to profile the Trameleon library. See
# the <tra/profiler.h> file for more info how to change the
# profiler that is used by Trameleon.
# 
#
# We create a shared library from the Tracy client sources and
# link with that. The reason we're doing this, is because the
# applications that use Trameleon might not link with the C++
# library and are pure C; Tracy needs to link with libstc++ or
# similar. When we create a static lib then the symbols that are
# used by Tracy need to be available when linking; which is not
# the case for C apps.

# -----------------------------------------------------------------

if (UNIX)

  # -----------------------------------------------------------------
  
  if (TRA_FORCE_REBUILD OR NOT EXISTS ${CMAKE_INSTALL_PREFIX}/lib/libTracyClient.so)
    
    include(ExternalProject)

    ExternalProject_Add(
      tracy
      GIT_REPOSITORY https://github.com/wolfpld/tracy.git
      GIT_TAG master
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE
      CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
      -DTRACY_STATIC=OFF
      -DTRACE_NO_SAMPLING=OFF
      -DTRACE_NO_SYSTEM_TRACING=OFF
      -DTRACE_NO_CALLSTACK=OFF
      -DTRACE_NO_CALLSTACK_INLINES=OFF
      )

    list(APPEND tra_deps tracy)
    
  endif()

  # -----------------------------------------------------------------

  list(APPEND tra_libs ${CMAKE_INSTALL_PREFIX}/lib/libTracyClient.so)
  include_directories(${CMAKE_INSTALL_PREFIX}/include)
  
  # -----------------------------------------------------------------
  
endif()
