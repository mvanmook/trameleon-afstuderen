# -----------------------------------------------------------------

if (NOT WIN32)
  return()
endif()

# -----------------------------------------------------------------

tra_add_module(
  NAME mft
  SOURCES
    ${mod_dir}/mft/mft.cpp
    ${mod_dir}/mft/MediaFoundationEncoder.cpp
    ${mod_dir}/mft/MediaFoundationDecoder.cpp
    ${mod_dir}/mft/MediaFoundationUtils.cpp
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
