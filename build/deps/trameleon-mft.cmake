# -----------------------------------------------------------------
#
# Media Foundation module for Trameleon
#
# -----------------------------------------------------------------

if (NOT WIN32)
  return()
endif()

# -----------------------------------------------------------------

set(tra_base_dir ${CMAKE_CURRENT_LIST_DIR}/../..)
set(tra_mod_dir ${tra_base_dir}/src/tra/modules)

# -----------------------------------------------------------------

tra_add_module(
  NAME mft
  SOURCES
    ${tra_mod_dir}/mft/mft.cpp
    ${tra_mod_dir}/mft/MediaFoundationEncoder.cpp
    ${tra_mod_dir}/mft/MediaFoundationDecoder.cpp
    ${tra_mod_dir}/mft/MediaFoundationUtils.cpp
  LIBS
    tra                # tra_log_debug, tra_core_* etc. 
    Mfplat.lib         # For MFStartup().
    Mfuuid.lib         # For IID_IMFTransform,
    wmcodecdspuuid.lib # For CLSID_CMSH264EncoderMFT
    Mf.lib
    Mfreadwrite.lib
    Shlwapi.lib
    Strmiids.lib
  )

# -----------------------------------------------------------------
