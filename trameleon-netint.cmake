# -----------------------------------------------------------------
#
# NetInt module for Trameleon
#
# -----------------------------------------------------------------

if (WIN32 OR APPLE)
  return()
endif()

# -----------------------------------------------------------------

set(tra_base_dir ${CMAKE_CURRENT_LIST_DIR}/../..)
set(tra_mod_dir ${tra_base_dir}/src/tra/modules)
  
# -----------------------------------------------------------------

set(TRA_NETINT_LIB_DIR "" CACHE PATH "Directory where we can find `libxcoder.a`")
set(TRA_NETINT_INC_DIR "" CACHE PATH "Directory where we can find `ni_device_api.h`")

# -----------------------------------------------------------------

if (NOT EXISTS ${TRA_NETINT_LIB_DIR}/libxcoder.a)
  message(FATAL_ERROR "Cannot find `${TRA_NETINT_LIB_DIR}/libxcoder.a`. Make sure the `TRA_NETINT_LIB_DIR` is set to the directory where we can find `libxcoder.a`.")
endif()

if (NOT EXISTS ${TRA_NETINT_INC_DIR}/ni_device_api.h)
  message(FATAL_ERROR "Cannot find `${TRA_NETINT_LIB_DIR}/ni_device_api.h`. Make sure the `TRA_NETINT_INC_DIR` is set to the directory where we can find `ni_device_api.h`.")
endif()

# -----------------------------------------------------------------

tra_add_module(
  NAME netint
  SOURCES
    ${tra_mod_dir}/netint/netint.c
    ${tra_mod_dir}/netint/netint-utils.c
    ${tra_mod_dir}/netint/netint-enc.c
    ${tra_mod_dir}/netint/netint-dec.c
  LIBS
    tra
    ${TRA_NETINT_LIB_DIR}/libxcoder.a
  PRIVATE_INC_DIRS
    ${TRA_NETINT_INC_DIR}    
  )

# -----------------------------------------------------------------
