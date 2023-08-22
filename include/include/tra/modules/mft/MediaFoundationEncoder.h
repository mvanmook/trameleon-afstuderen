#ifndef TRA_MFT_ENCODER_H
#define TRA_MFT_ENCODER_H

/*

  ┌─────────────────────────────────────────────────────────────────────────────────────┐
  │                                                                                     │
  │   ████████╗██████╗  █████╗ ███╗   ███╗███████╗██╗     ███████╗ ██████╗ ███╗   ██╗   │
  │   ╚══██╔══╝██╔══██╗██╔══██╗████╗ ████║██╔════╝██║     ██╔════╝██╔═══██╗████╗  ██║   │
  │      ██║   ██████╔╝███████║██╔████╔██║█████╗  ██║     █████╗  ██║   ██║██╔██╗ ██║   │
  │      ██║   ██╔══██╗██╔══██║██║╚██╔╝██║██╔══╝  ██║     ██╔══╝  ██║   ██║██║╚██╗██║   │
  │      ██║   ██║  ██║██║  ██║██║ ╚═╝ ██║███████╗███████╗███████╗╚██████╔╝██║ ╚████║   │
  │      ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═══╝   │
  │                                                                 www.trameleon.org   │
  └─────────────────────────────────────────────────────────────────────────────────────┘

  MEDIA FOUNDATION ENCODER
  ========================

  GENERAL INFO:

    The `MediaFoundationEncoder` provides a wrapper around the
    Windows Media Foundation Transforms. This implements a H264
    encoder.

    If you want to get an understanding of how MFTs work, start
    by reading [About MFTs][3]. A great introduction to the Media
    Foundation Transforms is the [Basic Processing Model][0]
    article. You should also be ware of [Media Foundation and
    COM][1] which explains why we use COINIT_MULTITHREADED.

  REFERENCES: 

    [0]: https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model "Basic Processing Model"
    [1]: https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-and-com "Media Foundation and COM"
    [2]: https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex "CoInitializeEx" 
    [3]: https://docs.microsoft.com/en-us/windows/win32/medfound/about-mfts "About MFTs"
    [4]: https://msdn.microsoft.com/en-us/library/windows/desktop/dd318776(v=vs.85).aspx "eAVEncH264VProfile enumeration"
    [5]: https://docs.microsoft.com/en-us/windows/win32/medfound/h-264-video-encoder "H.264 Video Encoder"
    [6]: https://source.chromium.org/chromium/chromium/src/+/main:media/gpu/windows/media_foundation_video_encode_accelerator_win.cc?q=media_foundation_video_encode_accelerator_win.cc&ss=chromium%2Fchromium%2Fsrc "Chromium implementation"
    [7]: https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-getinputavailabletype 
    [8]: https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-processinput
    [9]: https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-processoutput 

 */

/* ------------------------------------------------------- */

#define WIN32_LEAN_AND_MEAN
#include <Mftransform.h> /* IMFTransform. */

/* ------------------------------------------------------- */

#include <fstream> /* @todo remove; onlu used during development to test if the data we received was usable. */

/* ------------------------------------------------------- */

struct tra_sample;

/* ------------------------------------------------------- */

namespace tra {

  /* ------------------------------------------------------- */
  
  class MediaFoundationEncoderSettings {
  public:
    bool isValid();

  public:

    int (*on_encoded_data)(uint32_t type, void* data, void* user);

    uint32_t image_format = 0;
    uint32_t image_width = 0;
    uint32_t image_height = 0;
    uint32_t bitrate = 0;
    uint32_t fps_num = 0;
    uint32_t fps_den = 0;
    void* user = NULL;
  };

  /* ------------------------------------------------------- */

  class MediaFoundationEncoder {
  public:
    MediaFoundationEncoder() = default;
    ~MediaFoundationEncoder();
    int init(MediaFoundationEncoderSettings& cfg);
    int shutdown();
    int encode(tra_sample* inSample, uint32_t type, void* data);  /* Encode the `data`; the type defines how we should cast the `data`. See `tra/types.h` for the available `TRA_MEMORY_TYPE_*` types. */

  private:
    int setOutputMediaType();
    int setInputMediaType();
    int createOutputBuffers();
    int doesOutputProvideBuffers(bool& doesProvide);
    int doesInputStreamUseRefCounts(bool& useRefCount); /* When `useRefCount` is set to true, it means that the input streams holds a reference to the samples we pass into `ProcessInput()` and we're not allowed to (re)use them until they are released. */
    int processOutput(); /* The counter part of `encode()`. This will extract the coded data from the encoder and is called internally. */
    int processOutputWithClientBuffers(); /* This implementation must provide buffers. */
    int processOutputWithTransformBuffers(); /* The transform provides output buffers. */

  private:
    MediaFoundationEncoderSettings settings; /* The settings passed into `init()`. */
    IMFTransform* encoder_transform = NULL;
    IMFSample* output_sample = NULL;
    IMFMediaBuffer* output_media_buffer = NULL;
    bool output_provides_buffers = false;
    bool input_uses_refcount = false; /* Set via `init()`, when true, we are not allowed to use the samples we pass into `ProcessInput()` until the MFT releases them. */
    DWORD stream_output_id = UINT32_MAX;
    DWORD stream_input_id = UINT32_MAX;

    uint32_t tmp_frame_num = 0;
    uint64_t tmp_frame_time = 0;
    std::ofstream tmp_ofs;
  };

  /* ------------------------------------------------------- */
  
}; /* namespace tra */

/* ------------------------------------------------------- */

#endif
