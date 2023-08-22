# -----------------------------------------------------------------
#
# Experimental VAAPI module for Trameleon
#
# -----------------------------------------------------------------

if (WIN32 OR APPLE)
  return()
endif()
  
# -----------------------------------------------------------------

set(tra_base_dir ${CMAKE_CURRENT_LIST_DIR}/../../)
set(tra_mod_dir ${tra_base_dir}/src/tra/modules)

# -----------------------------------------------------------------

tra_add_module(
  NAME vaapi
  SOURCES
    ${tra_mod_dir}/vaapi/vaapi.c
    ${tra_mod_dir}/vaapi/vaapi-enc.c
    ${tra_mod_dir}/vaapi/vaapi-dec.c
    ${tra_mod_dir}/vaapi/vaapi-utils.c
    )
  
# -----------------------------------------------------------------


