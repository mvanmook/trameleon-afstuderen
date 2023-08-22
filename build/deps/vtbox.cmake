# -----------------------------------------------------------------

if (NOT APPLE)
  return()
endif()
  
# -----------------------------------------------------------------

find_library(fr_corefoundation CoreFoundation) # CFDictionary, CFRelease, etc.
find_library(fr_coremedia CoreMedia)           # CMSampleBuffer, etc.
find_library(fr_corevideo CoreVideo)           # CVPixelBufferGetBaseAddressOfPlane, CVPixelBufferGetHeightOfPlane, etc.
find_library(fr_vtbox VideoToolbox)            # VTCompressionSession, VTDecompressionSession, etc.

# -----------------------------------------------------------------

tra_add_module(
  NAME vtbox
  SOURCES
    ${mod_dir}/vtbox/vtbox.c
    ${mod_dir}/vtbox/vtbox-enc.c
    ${mod_dir}/vtbox/vtbox-dec.c
    ${mod_dir}/vtbox/vtbox-utils.c 
  LIBS
    tra
    ${fr_corefoundation}
    ${fr_coremedia}
    ${fr_vtbox}
    ${fr_corevideo}
  )

# -----------------------------------------------------------------
