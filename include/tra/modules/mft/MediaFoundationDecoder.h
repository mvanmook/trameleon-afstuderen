#ifndef TRA_MFT_DECODER_H
#define TRA_MFT_DECODER_H

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

  MEDIA FOUNDATION DECODER
  ========================

  GENERAL:
  
    The `MediaFoundationDecoder` class implements a Windows Media
    Foundation based video decoder. Currently we focus on
    decoding H264. The decoder uses Media Foundation Transforms,
    which is a generic media pipeline for Windows. We need to
    take quite a few steps to initialize the transform. This is
    similar to the `MediaFoundationEncoder`.  Although the number
    of steps we have to perform to initialize the decoder, the
    code is simple. Also, Microsoft did a really good job
    documenting Windows Media Transforms.

    See [Basic Processing Model][0] for a great first introduction
    to MFT. Then [About MFTs][1] continues with more in depth
    info that you should read.

  REFERENCES: 

    [0]: https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model "Basic Processing Model"
    [1]: https://docs.microsoft.com/en-us/windows/win32/medfound/about-mfts "About MFTs"
    [2]: https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex "CoInitializeEx" 
    [3]: https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-and-com "Media Foundation and COM"
    [4]: https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-getoutputavailabletype
    [5]: https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-processoutput 

*/

/* ------------------------------------------------------- */

#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <Mftransform.h> /* IMFTransform */

/* ------------------------------------------------------- */

namespace tra {

  class MediaFoundationDecoderSettings {
  public:
    bool isValid();

  public:
    int (*on_decoded_data)(uint32_t type, void* data, void* user); 

    uint32_t image_format = 0;  /* The image format you want to use for the decoded frames. */
    uint32_t image_width = 0;   /* The width of the decoded video. */
    uint32_t image_height = 0;  /* The height of the decoded video. */
    void* user = NULL;          /* User pointer that is passed into `on_decoded_data`. */
  };

  /* ------------------------------------------------------- */
  
  class MediaFoundationDecoder {
  public:
    MediaFoundationDecoder() = default;
    ~MediaFoundationDecoder();
    int init(MediaFoundationDecoderSettings& cfg);
    int shutdown();
    int decode(uint32_t type, void* data); /* Decode using the given type and data. See `tra/types.h` for the available types. */

  private:
    int setInputMediaType();
    int setOutputMediaType();
    int doesOutputProvideBuffers(bool& doesProvide);
    int doesInputStreamUseRefCounts(bool& useRefCount); /* When `useRefCount` is set to true, it means that the input streams holds a reference to the samples we pass into `ProcessInput()` and we're not allowed to (re)use them until they are released. */
    int createOutputBuffers();
    int enableHardwareDecoding();
    int enableLowLatencyDecoding();
    int processOutput(); /* This will extract the decoded data from the decoder and is called internally. */
    int processOutputWithClientBuffers(); /* This implementation must provide buffers. */
    int processOutputWithTransformBuffers(); /* The transform provides output buffers. */
    
  private:
    MediaFoundationDecoderSettings settings;
    IMFTransform* decoder_transform = NULL;
    IMFSample* output_sample = NULL;
    IMFMediaBuffer* output_media_buffer = NULL;
    uint32_t output_image_format = 0; /* The image format of the decoded buffers. */
    DWORD stream_output_id = UINT32_MAX;
    DWORD stream_input_id = UINT32_MAX;
    bool output_provides_buffers = false;
    bool input_uses_refcount = false;
  };

  /* ------------------------------------------------------- */
  
}; /* namespace tra */

#endif
