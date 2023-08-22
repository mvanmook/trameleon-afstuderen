# -----------------------------------------------------------------

if (WIN32 OR APPLE)
  return()
endif()
  
# -----------------------------------------------------------------

tra_add_module(
  NAME vaapi
  SOURCES
    ${mod_dir}/vaapi/vaapi.c
    ${mod_dir}/vaapi/vaapi-enc.c
    ${mod_dir}/vaapi/vaapi-dec.c
    ${mod_dir}/vaapi/vaapi-utils.c
    )
  
# -----------------------------------------------------------------

