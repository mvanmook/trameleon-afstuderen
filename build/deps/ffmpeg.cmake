# -----------------------------------------------------------------
#
# Compiles ffmpeg that we mostly use during the development of
# trameleon; e.g. we use ffmpeg to generate test data in the
# test-[module]-{encoder,decoder}.c sources.
#
# We use `--enable-gpl` to make sure that we can use the
# `libx264` when generating test h264 data.
#
# -----------------------------------------------------------------

# Apple and Linux
if (UNIX)
  
  if (TRA_FORCE_REBUILD OR NOT EXISTS ${CMAKE_INSTALL_PREFIX}/lib/libavutil.a)

    include(ExternalProject)
    include(${deps_dir}/nasm.cmake)
    include(${deps_dir}/x264.cmake)

    # Generate the configure line; we need to create a `list`
    # type like this, otherwise CMake will add extra quotes
    # around the `extra-{ldflags,cflags}`.
    
    list(APPEND ffmpeg_config
      "./configure"
      "--prefix=${CMAKE_INSTALL_PREFIX}"
      "--enable-gpl"
      "--enable-libx264"
      "--extra-ldflags=-L ${CMAKE_INSTALL_PREFIX}/lib/"
      "--extra-cflags=-I ${CMAKE_INSTALL_PREFIX}/include/"
    )

    ExternalProject_Add(
      ffmpeg
      DEPENDS nasm x264
      GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
      GIT_TAG n5.0
      BUILD_IN_SOURCE 1
      CONFIGURE_COMMAND PATH=$ENV{PATH}:${CMAKE_INSTALL_PREFIX}/bin/ "${ffmpeg_config}"
      BUILD_COMMAND PATH=$ENV{PATH}:${CMAKE_INSTALL_PREFIX}/bin/ make VERBOSE=1
      )

    list(APPEND tra_deps ffmpeg)

  endif()
  
endif()

# -----------------------------------------------------------------

include_directories(${CMAKE_INSTALL_PREFIX}/include)

# -----------------------------------------------------------------

# Linux 
if (UNIX AND NOT APPLE)
  
  list(APPEND tra_libs
    ${CMAKE_INSTALL_PREFIX}/lib/libavfilter.a
    ${CMAKE_INSTALL_PREFIX}/lib/libavformat.a
    ${CMAKE_INSTALL_PREFIX}/lib/libavcodec.a
    ${CMAKE_INSTALL_PREFIX}/lib/libswscale.a
    ${CMAKE_INSTALL_PREFIX}/lib/libavdevice.a
    ${CMAKE_INSTALL_PREFIX}/lib/libswresample.a
    ${CMAKE_INSTALL_PREFIX}/lib/libavutil.a
    ${CMAKE_INSTALL_PREFIX}/lib/libpostproc.a
    m
    va
    va-drm
    va-x11
    pthread
    z
    X11
    vdpau
    bz2
    lzma
    Xext
    )
  
endif()

# -----------------------------------------------------------------

if (APPLE)

  find_library(fr_security Security)
  find_library(fr_corefoundation CoreFoundation)
  find_library(fr_coreaudio CoreAudio)
  find_library(fr_coregfx CoreGraphics)
  find_library(fr_audiounit AudioUnit)
  find_library(fr_audiotb AudioToolbox)
  find_library(fr_coremedia CoreMedia)
  find_library(fr_corevideo CoreVideo)
  find_library(fr_videokit VideoToolbox)
  find_library(fr_vda VideoDecodeAcceleration)
  find_library(fr_security Security)
  find_library(fr_gl OpenGL)
  find_library(fr_metal Metal)
  find_library(fr_coreimage CoreImage)
  find_library(fr_coreservices CoreServices)
  find_library(fr_appkit AppKit)

  list(APPEND tra_libs
    ${ffmpeg_prefix}/lib/libavformat.a
    ${ffmpeg_prefix}/lib/libavcodec.a
    ${ffmpeg_prefix}/lib/libswscale.a
    ${ffmpeg_prefix}/lib/libavdevice.a
    ${ffmpeg_prefix}/lib/libavutil.a
    ${ffmpeg_prefix}/lib/libavfilter.a
    ${ffmpeg_prefix}/lib/libswresample.a
    ${fr_corefoundation}                  
    ${fr_coreaudio}
    ${fr_audiounit}
    ${fr_coregfx}                       
    ${fr_audiotb}
    ${fr_coremedia}
    ${fr_corevideo}
    ${fr_videokit}
    ${fr_vda}
    ${fr_security}
    ${fr_gl}
    ${fr_metal}
    ${fr_coreimage}
    ${fr_coreservices}
    ${fr_appkit}
    m
    bz2
    z
    lzma
    iconv
  )

endif()

# -----------------------------------------------------------------

if (WIN32)

  set(ffmpeg_lib_dir ${ffmpeg_prefix}/src/ffmpeg/lib)
  set(ffmpeg_bin_dir ${ffmpeg_prefix}/src/ffmpeg/bin)
  set(ffmpeg_inc_dir ${ffmpeg_prefix}/src/ffmpeg/include)

  list(APPEND tra_libs
    ${ffmpeg_lib_dir}/libavcodec.dll.a
    ${ffmpeg_lib_dir}/libavdevice.dll.a
    ${ffmpeg_lib_dir}/libavfilter.dll.a
    ${ffmpeg_lib_dir}/libavformat.dll.a
    ${ffmpeg_lib_dir}/libavutil.dll.a
    ${ffmpeg_lib_dir}/libswresample.dll.a
    ${ffmpeg_lib_dir}/libswscale.dll.a
    )

  list(APPEND tra_inc_dirs
    ${ffmpeg_inc_dir}
    )

  if (NOT EXISTS ${inst_dir}/bin/avcodec-59.dll)
    set(ffmpeg_dlls
      ${ffmpeg_bin_dir}/avcodec-59.dll
      ${ffmpeg_bin_dir}/avdevice-59.dll
      ${ffmpeg_bin_dir}/avfilter-8.dll
      ${ffmpeg_bin_dir}/avformat-59.dll
      ${ffmpeg_bin_dir}/avutil-57.dll
      ${ffmpeg_bin_dir}/swresample-4.dll
      ${ffmpeg_bin_dir}/swscale-6.dll
      )
    install(FILES ${ffmpeg_dlls} DESTINATION bin)
  endif()

  if (NOT EXISTS ${ffmpeg_prefix}/src/ffmpeg/lib/avcodec.lib)

    include(ExternalProject)
    include(${deps_dir}/yasm.cmake)

    ExternalProject_Add(
      ffmpeg
      URL https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n5.0-latest-win64-lgpl-shared-5.0.zip
      UPDATE_COMMAND ""
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
      PREFIX ${ffmpeg_prefix}
      )

    list(APPEND tra_deps
      ffmpeg
      )
    
  endif()

endif()

# -----------------------------------------------------------------
