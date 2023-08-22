# -----------------------------------------------------------------

if (NOT APPLE)
  return()
endif()
  
# -----------------------------------------------------------------

set(tra_base_dir ${CMAKE_CURRENT_LIST_DIR}/../..)
set(tra_mod_dir ${tra_base_dir}/src/tra/modules)

# -----------------------------------------------------------------

find_library(fr_corefoundation CoreFoundation) # CFDictionary, CFRelease, etc.
find_library(fr_coremedia CoreMedia)           # CMSampleBuffer, etc.
find_library(fr_corevideo CoreVideo)           # CVPixelBufferGetBaseAddressOfPlane, CVPixelBufferGetHeightOfPlane, etc.
find_library(fr_vtbox VideoToolbox)            # VTCompressionSession, VTDecompressionSession, etc.

# -----------------------------------------------------------------

tra_add_module(
  NAME vtbox
  SOURCES
    ${tra_mod_dir}/vtbox/vtbox.c
    ${tra_mod_dir}/vtbox/vtbox-enc.c
    ${tra_mod_dir}/vtbox/vtbox-dec.c
    ${tra_mod_dir}/vtbox/vtbox-utils.c 
  LIBS
    tra
    ${fr_corefoundation}
    ${fr_coremedia}
    ${fr_vtbox}
    ${fr_corevideo}
  )

# -----------------------------------------------------------------
