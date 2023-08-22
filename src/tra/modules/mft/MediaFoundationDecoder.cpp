/* ------------------------------------------------------- */

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Mfapi.h>         /* IMFMediaType */
#include <wmcodecdsp.h>    /* CLSID_CMSH264DecoderMFT */
#include <Mferror.h>       /* MF_E_*  */
#include <Codecapi.h>      /* eAVEncH264VProfile_High */

#include <tra/modules/mft/MediaFoundationDecoder.h>
#include <tra/modules/mft/MediaFoundationUtils.h>

extern "C" {
#  include <tra/types.h>
#  include <tra/log.h>
}

namespace tra {

  /* ------------------------------------------------------- */

  static std::string get_last_error_as_string();
  
  /* ------------------------------------------------------- */

  bool MediaFoundationDecoderSettings::isValid() {

    if (0 == image_width) {
      TRAE("The `image_width` is 0.");
      return false;
    }

    if (0 == image_height) {
      TRAE("The `image_height` is 0.");
      return false;
    }

    if (NULL == on_decoded_data) {
      TRAE("The `on_decoced_data` is not set.");
      return false;
    }
                               
    return true;
  }

  /* ------------------------------------------------------- */

  MediaFoundationDecoder::~MediaFoundationDecoder() {
    shutdown();
  }

  /* ------------------------------------------------------- */

  /*
    Initialize the decoder and COM. Currently we initialize COM
    using `COINIT_MULTITHREADED` which is [recommended][3] by
    Microsoft, more info [here][2]. After initializing COM, we
    create the decoder transform. Then we make sure we have only
    one input and only one output which should be standard for 
    decoders.
   */
  int MediaFoundationDecoder::init(MediaFoundationDecoderSettings& cfg) {

    TRAE("@todo Maybe create a MediaFoundationUtils.h with some helper/debug functions. Like mft_videoformat_to_string. And get_last_error_as_string from MediaFoundationEncoder.cpp");
    
    IMFMediaType* media_type = NULL;
    DWORD num_in_streams = 0;
    DWORD num_out_streams = 0;
    HRESULT hr = S_OK;
    int r = 0;

    if (false == cfg.isValid()) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as the given settings are invalid.");
      r = -1;
      goto error;
    }
    
    if (NULL != decoder_transform) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as our `decoder_transform` member is not NULL. Already initialized?");
      r = -2;
      goto error;
    }

    settings = cfg;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to initialize COM.");
      r = -3;
      goto error;
    }

    hr = CoCreateInstance(
      CLSID_CMSH264DecoderMFT,
      NULL,
      CLSCTX_INPROC_SERVER,
      IID_IMFTransform,
      (void**)&decoder_transform
    );

    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot innitialize the `MediaFoundationDecoder` as we failed to create the decoder transform instance.");
      r = -4;
      goto error;
    }

    hr = decoder_transform->GetStreamCount(&num_in_streams, &num_out_streams);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to get the number of input and output streams.");
      r = -5;
      goto error;
    }

    if (1 != num_in_streams) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we expect the number of input streams to be 1 and we got %u.", num_in_streams);
      r = -6;
      goto error;
    }

    if (1 != num_out_streams) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we expect the number of output streams to be 1 and we got %u.", num_out_streams);
      r = -7;
      goto error;
    }

    /*
      Should return E_NOTIMPL, [which means][0]
      - The transform has a fixed number of streams.
      - The transform allows the client to add or remove input streams.
      - The stream identifiers are not consecutive starting from zero
     */
    hr = decoder_transform->GetStreamIDs(1, &stream_input_id, 1, &stream_output_id);
    if (E_NOTIMPL == hr) {
      stream_input_id = 0;
      stream_output_id = 0;
    }

    if (UINT32_MAX == stream_input_id
        || UINT32_MAX == stream_output_id)
      {
        TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to get the input (%u) or output (%u) stream ids.", stream_input_id, stream_output_id);
        r = -8;
        goto error;
      }

    /* 
       When using MFTs it's important to be aware about the order
       of setting up the input and output media types. The
       encoder, for example, requires you to first setup the
       output type and then the input type. The decoder requires
       you to first setup the input type. We can verify this by
       using [GetOutputAvailableType()][4]. When we first have to
       set the input type, we get an error
       `MF_E_TRANSFORM_TYPE_NOT_SET`.
     */
    hr = decoder_transform->GetOutputAvailableType(stream_output_id, 0, &media_type);
    if (MF_E_TRANSFORM_TYPE_NOT_SET != hr) {
      TRAE("Cannot initialize the `MediaFoundationDecoder`. We expect a certain initialization order of the input and output streams (input first) which seems to be different than what we should do for this transform.");
      r = -9;
      goto error;
    }

    r = setInputMediaType();
    if (r < 0) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as failed to set the input media type.");
      r = -10;
      goto error;
    }

    r = setOutputMediaType();
    if (r < 0) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to set the output media type.");
      r = -11;
      goto error;
    }

    r = doesOutputProvideBuffers(output_provides_buffers);
    if (r < 0) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to check if the output provides buffers.");
      r = -12;
      goto error;
    }

    if (true == output_provides_buffers) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as the transform provides buffers. We support only transform which do NOT provide buffers. ");
      r = -13;
      goto error;
    }

    if (false == output_provides_buffers) {
      r = createOutputBuffers();
      if (r < 0) {
        TRAE("Cannot initialize the `MediaFoundationDecoder`. Failed to create output buffers.");
        r = -14;
        goto error;
      }
    }

    /* 
       Check how we have to deal with input buffers. When the MFT
       doesn't use refcounts it means that it copies the data. We 
       currently do not support this method.
    */
    r = doesInputStreamUseRefCounts(input_uses_refcount);
    if (r < 0) {
      TRAE("Cannot initialize the `MediaFoundationDecoder`. Failed to check if the input stream uses reference counting.");
      r = -15;
      goto error;
    }

    if (false == input_uses_refcount) {
      TRAE("Cannot initialize the `MediaFoundationDecoder`. We expect the MFT to manage input samples.");
      r = -16;
      goto error;
    }

    r = enableHardwareDecoding();
    if (r < 0) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to enable hardware decoding.");
      r = -15;
      goto error;
    }

    r = enableLowLatencyDecoding();
    if (r < 0) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to enable low latency decoding.");
      r = -16;
      goto error;
    }

  error:

    if (r < 0) {
      shutdown();
    }
    
    return r;
  }

  /* ------------------------------------------------------- */

  /*
    @todo We need to have a module wide solution to call
    MFStartup()/MFShutdown().  This needs to be called only once
    per "run".
   */
  int MediaFoundationDecoder::shutdown() {

    HRESULT hr = S_OK;
    int status = 0;

    if (NULL != output_sample) {
      output_sample->Release();
    }

    if (NULL != output_media_buffer) {
      output_media_buffer->Release();
    }

    if (NULL != decoder_transform) {
      decoder_transform->Release();
    }

    output_sample = NULL;
    output_media_buffer = NULL;
    decoder_transform = NULL;
    output_provides_buffers = false;
    input_uses_refcount = false;
    stream_output_id = UINT32_MAX;
    stream_input_id = UINT32_MAX;

    return status;
  }

  /* ------------------------------------------------------- */

  /*
    
    GENERAL INFO:

      This function will decode the given access unit were we
      expect the data to use annex-b h264 (e.g. prefixed with a
      00 00 00 01 start code).
      
      When we feed data into the decoder we create a
      `IMFMediaBuffer` that we add to a `IMFSample`. The
      `IMFSample` contains a buffer and adds some attributes.

      Currently we only support the `TRA_MEMORY_TYPE_HOST_H264`.

    TODO:
 
      @todo Currently I'm creating a new `IMFMediaBuffer` each
      time you call `decode()`. I'm not sure if there is a
      better, more optimised solution.
    
   */

  int MediaFoundationDecoder::decode(uint32_t type, void* data) {

    tra_encoded_host_memory* host_mem = NULL;
    IMFMediaBuffer* media_buffer = NULL;
    DWORD media_buffer_curr_length = 0;
    DWORD media_buffer_max_length = 0;
    BYTE* media_buffer_ptr = NULL;
    IMFSample* sample = NULL;
    HRESULT hr = S_OK;
    int r = 0;

    if (TRA_MEMORY_TYPE_HOST_H264 != type) {
      TRAE("Cannot decode the given data as the `type` is not supported.");
      r = -10;
      goto error;
    }
    
    if (NULL == data) {
      TRAE("Cannot decode the given `data` as it's NULL.");
      r = -20;
      goto error;
    }

    // if (0 == nbytes) {
    //   TRAE("Cannot decode the given data as it's 0 bytes.");
    //   r = -2;
    //   goto error;
    // }

    if (NULL == decoder_transform) {
      TRAE("Cannot decode as our `encode_transform` member is NULL. Not initialized?");
      r = -30;
      goto error;
    }

    host_mem = (tra_encoded_host_memory*) data;
    if (NULL == host_mem->data) {
      TRAE("Cannot decode as the `data` member of the `tra_encoded_host_memory` is NULL.");
      r = -40;
      goto error;
    }

    if (0 == host_mem->size) {
      TRAE("Cannot decode as the `size` member of the `tra_encoded_host_memory` is 0.");
      r = -50;
      goto error;
    }

    hr = MFCreateSample(&sample);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot decode, failed to create a sample.");
      r = -60;
      goto error;
    }

    hr = sample->SetSampleDuration(0);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot decode, failed to set the sample duraction on the sample.");
      r = -70;
      goto error;
    }

    hr = MFCreateMemoryBuffer(host_mem->size, &media_buffer);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot decode, failed to create a memory buffer.");
      r = -80;
      goto error;
    }

    hr = media_buffer->Lock(&media_buffer_ptr, &media_buffer_max_length, &media_buffer_curr_length);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot decode, failed to lock the media buffer.");
      r = -90;
      goto error;
    }

    /* @todo Not sure if we want to keep this here. The buffer we allocate must be able to contain the amount of bytes we requested. */
    if (media_buffer_max_length < host_mem->size) {
      TRAE("Cannot decode, the created buffer is too small (?). From my understanding this is not possible; we whould reproduce this.");
      exit(EXIT_FAILURE);
    }

    TRAD(
      "media_buffer_max_length: %u, media_buffer_curr_length: %u, media_buffer_ptr: %p",
      media_buffer_max_length,
      media_buffer_curr_length,
      media_buffer_ptr
    );

    /* Copy the access unit... */
    memcpy(media_buffer_ptr, host_mem->data, host_mem->size);

    hr = media_buffer->Unlock();
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot decode, failed to unlock the media buffer.");
      r = -100;
      goto error;
    }

    /* The the buffer how much data we copied. */
    hr = media_buffer->SetCurrentLength(host_mem->size);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot decode, failed to tell the `IMFMediaBuffer` how much data we copied.");
      r = -110;
      goto error;
    }

    /* We have to wrap our buffer into a `IMFSample`.  */
    hr = sample->AddBuffer(media_buffer);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to add the `IMFMediaBuffer` to the `IMFSample`.");
      r = -120;
      goto error;
    }

    /* Feed the data into the decoder. @todo what if too big forr one process maarten*/
    hr = decoder_transform->ProcessInput(stream_input_id, sample, 0);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to process input.");
      r = -130;
      goto error;
    }

    r = processOutput();
    if (r < 0) {
      TRAE("Failed to process output.");
      r = -140;
      goto error;
    }
    
  error:

    if (NULL != media_buffer) {
      media_buffer->Release();
      media_buffer = NULL;
    }

    if (NULL != sample) {
      sample->Release();
      sample = NULL;
    }

    return r;
  }

  /* ------------------------------------------------------- */
  
  int MediaFoundationDecoder::processOutput() {

    if (true == output_provides_buffers) {
      return processOutputWithTransformBuffers();
    }

    return processOutputWithClientBuffers();
  }

  /* ------------------------------------------------------- */

  /*
    GENERAL INFO:
 
      This function will extract the decoded data. The data is
      stoed in our `IMFSample` (output_sample) which wraps our
      `IMFMediaBuffer()` (output_media_buffer).

      I haven't found the best way to reconstruct a `tra_memory_image`
      from the decode data. I did not find a win32 function that
      allows me to get the width, height, stride, etc. Therefore
      I'll hardcode the image based on the image format that was
      passed with the `MediaFoundationDecoderSettings` into
      `init()`.

    TODO:

       @todo Currently this code requires the user to specify the
       image width and height of the decoded video. We are not
       handling the `MF_E_TRANSFORM_STREAM_CHANGE` event which
       idicates that the width and height of the decoded video
       might be different than the size we used to initialize the
       decoder. We have to implement this to make sure that we
       setup the `decode_image` (see below) correctly.

    REFERENCES:

      [0]: https://github.com/strukturag/LAVFilters/blob/master/decoder/LAVVideo/decoders/wmv9mft.cpp#L573 "Hardcoded strides, planes, etc."
    
   */
  int MediaFoundationDecoder::processOutputWithClientBuffers() {

    MFT_OUTPUT_DATA_BUFFER output_data = { 0 };
    tra_memory_image decoded_image = { 0 };
    BYTE* output_buffer_data_ptr = NULL;
    DWORD output_buffer_data_length = 0;
    DWORD buffer_count = 0;
    uint8_t did_lock = 0;
    HRESULT hr = S_OK;
    DWORD status = 0;
    DWORD i = 0;
    int r = 0;
    
    if (NULL == decoder_transform) {
      TRAE("Cannot process the output as our `decoder_transform` member is NULL.");
      r = -10;
      goto error;
    }

    if (true == output_provides_buffers) {
      TRAE("Cannot process the output using client buffers as the MFT provides buffers.");
      r = -20;
      goto error;
    }

    if (NULL == output_sample) {
      TRAE("Cannot process the output sample as our `output_sample` member is NULL.");
      r = -30;
      goto error;
    }

    /* Currently we only support NV12 as output type. */
    if (TRA_IMAGE_FORMAT_NV12 != output_image_format) {
      TRAE("Cannot process the output sample as the output image format is not supported.");
      r = -35;
      goto error;
    }

    if (NULL == settings.on_decoded_data) {
      TRAE("Cannot process the outptu as the `on_decoded_data` callback is not set. Makes no sense to decode w/o using the decoded data.");
      r = -40;
      goto error;
    }

    /* Check if there is decoded data. */
    while (true) {

      /* 
         We reuse the same `IMFMediaBuffer`. The MFT fills it and
         sets the current length.  When we don't reset, the MFT
         will fail when it can't add any more decoded data,
         therefore we should reset the length.
      */
      hr = output_media_buffer->SetCurrentLength(0);
      if (false == SUCCEEDED(hr)) {
        TRAE("Cannot process the output, failed to reset the current length of our`IMFMediaBuffer`.");
        r = -50;
        goto error;
      }
      
      output_data.dwStreamID = stream_output_id;
      output_data.dwStatus = 0;
      output_data.pEvents = NULL;
      output_data.pSample = output_sample;

      hr = decoder_transform->ProcessOutput(0, 1, &output_data, &status);
      if (MF_E_TRANSFORM_STREAM_CHANGE == hr) {
        IMFTransform::GetOutputAvailableType()
      }

      /* This will most likely happen every 2nd loop which is ok. */
      if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr) {
        r = 0;
        goto error;
      }

      if (S_OK != hr) {
        TRAE("Cannot process the output, unhandled error from `ProcessOutput()`: %08x", hr);
        r = -60;
        goto error;
      }

      /* Access the decoded data. */
      hr = output_sample->GetBufferCount(&buffer_count);
      if (false == SUCCEEDED(hr)) {
        TRAE("Cannot process the output, failed to get the buffer count; cannot extract decoded data.");
        r = -70;
        goto error;
      }

      if (1 != buffer_count) {
        TRAE("Cannot process the output. We allocate only one output buffer but the MFT stored more? (exiting)");
        exit(EXIT_FAILURE);
      }
        
      hr = output_media_buffer->Lock(&output_buffer_data_ptr, &output_buffer_data_length, NULL);
      if (false == SUCCEEDED(hr)) {
        TRAE("Cannot process the output, failed to lock the output buffer.");
        r = -80;
        goto error;
      }

      did_lock = 1;

      /* Setup the output image that we hand over to the user. */
      switch (output_image_format) {
        
        case TRA_IMAGE_FORMAT_NV12: {

          decoded_image.image_format = TRA_IMAGE_FORMAT_NV12;
          decoded_image.image_width = settings.image_width;
          decoded_image.image_height = settings.image_height;
          decoded_image.plane_count = 2;
          
          decoded_image.plane_strides[0] = settings.image_width;
          decoded_image.plane_strides[1] = settings.image_width;
          decoded_image.plane_strides[2] = 0;
          
          decoded_image.plane_heights[0] = settings.image_height;
          decoded_image.plane_heights[1] = settings.image_height / 2;
          decoded_image.plane_heights[2] = 0;
          
          decoded_image.plane_data[0] = output_buffer_data_ptr;
          decoded_image.plane_data[1] = output_buffer_data_ptr + (settings.image_width * settings.image_height);
          decoded_image.plane_data[2] = NULL;
          break;
        }
        
        default: {
          TRAE("Cannot process the output, the `output_image_format` is not supported yet.");
          r = -90;
          goto error;
        }
      }

      /*
        Notify the user about the decode image; the user can
        e.g. render the image or send it over the network etc.
      */
      r = settings.on_decoded_data(
        TRA_MEMORY_TYPE_IMAGE,
        &decoded_image,
        settings.user
      );

      if (r < 0) {
        TRAE("The user failed to handle the decoded image.");
        r = -100;
        goto error;
      }
      
      TRAD("We have %u buffers, len: %u", buffer_count, output_buffer_data_length);

      hr = output_media_buffer->Unlock();
      if (false == SUCCEEDED(hr)) {
        TRAE("Cannot process the output, failed to unlock() our output buffer.");
        r = -110;
        goto error;
      }

      did_lock = 0;
      
    } /* while (true) */

  error:

    /*
      It may happen that the while(true) loop above runs into an
      error. When that happens, the `output_media_buffer` could
      still be locked; therefore we unlock it here.
    */
    if (1 == did_lock) {
      hr = output_media_buffer->Unlock();
      if (false == SUCCEEDED(hr)) {
        TRAE("Cannot process the output, failed to unlock() our output buffer.");
        r = -120;
      }
    }

    return r;
  }

  /* ------------------------------------------------------- */
  
  int MediaFoundationDecoder::processOutputWithTransformBuffers() {
    TRAE("@todo We haven't implemented a function to process the input when the MFT manages the output.");
    return -1;
  }

  /* ------------------------------------------------------- */

  /* 
     For a decoder we first have to specify the input type and
     then the output type. After we've set the input type, the
     output type changes. Only then we can call
     `GetOutputAvailableType()`. We also have to set the size of
     the video. The MF_MT_FRAME_SIZE changes the value of
     `MFT_OUTPUT_STREAM_INFO::cbSize`. See
     `createOutputBuffers()`.
   */
  int MediaFoundationDecoder::setInputMediaType() {
    
    IMFMediaType* media_type = NULL;
    HRESULT hr = S_OK;
    int r = 0;

    if (NULL == decoder_transform) {
      TRAE("Cannot set the input media type as our `decoder_transform` member is NULL.");
      r = -1;
      goto error;
    }

    if (UINT32_MAX == stream_input_id) {
      TRAE("Cannot set the input media type as out `stream_input_id` is not set. Did you call `init()`?");
      r = -2;
      goto error;
    }

    if (false == settings.isValid()) {
      TRAE("Cannot set the output media type as our `settings` member is invalid.");
      r = -3;
      goto error;
    }

    hr = MFCreateMediaType(&media_type);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot set the input media type as we failed to create an `IMFMediaType` instance.");
      r = -4;
      goto error;
    }

    hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot set the input media type as we failed to set the major type to video.");
      r = -5;
      goto error;
    }

    hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot set the input media type as we failed to set the sub type to h264.");
      r = -6;
      goto error;
    }

    hr = MFSetAttributeSize(media_type, MF_MT_FRAME_SIZE, settings.image_width, settings.image_height);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot set the input media type, failed to set the frame size.");
      r = -7;
      goto error;
    }

    hr = decoder_transform->SetInputType(stream_input_id, media_type, 0);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot set the input media type as the call to `setInputMediaType()` failed.");
      r = -8;
      goto error;
    }
                                              
  error:

    if (NULL != media_type) {
      media_type->Release();
      media_type = NULL;
    }
    
    return r;
  }

  /* ------------------------------------------------------- */

  int MediaFoundationDecoder::setOutputMediaType() {

    IMFMediaType* media_type = NULL;
    GUID output_pix_guid = { 0 };
    HRESULT hr = S_OK;
    int r = 0;

    if (NULL == decoder_transform) {
      TRAE("Cannot set the output media type as our `decoder_transform` is NULL.");
      r = -10;
      goto error;
    }

    if (UINT32_MAX == stream_output_id) {
      TRAE("Cannot set the output media type as our `stream_output_id` is invalid. Did you call `init()`?");
      r = -20;
      goto error;
    }

    hr = decoder_transform->GetOutputAvailableType(stream_output_id, 0, &media_type);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot set the output media type as we failed to retrieve an available media type.");
      r = -30;
      goto error;
    }

    hr = decoder_transform->SetOutputType(stream_output_id, media_type, 0);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot set the output media type as the call to `SetOutputType()` failed: %s", get_last_error_as_string().c_str());
      r = -40;
      goto error;
    }

    hr = media_type->GetGUID(MF_MT_SUBTYPE, &output_pix_guid);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot set the output media type as we failed to get the pixel format of the available type.");
      r = -50;
      goto error;
    }

    /* Convert the output video format to a image format that this library understands. */
    r = mft_videoformat_to_imageformat(output_pix_guid, &output_image_format);
    if (r < 0) {
      TRAE("Cannot set the output media type. Failed to convert the pixel format GUID from the output type into a image format.");
      r = -60;
      goto error;
    }

    mft_print_mediatype(media_type);

  error:

    if (NULL != media_type) {
      media_type->Release();
      media_type = NULL;
    }
    
    return r;
  }

  /* ------------------------------------------------------- */

  /*
    GENERAL INFO:
    
      See [About MFTs][3] and the paragraph about "Processing
      Data". There they describe that the `ProcessOutput`
      supports two different allocation modes. Either the MFT
      allocates output buffers or the client must manage output
      buffers.
      
      We use `GetOutputStreamInfo()` to determine if we should
      allocate buffers for the output data.

    TODO: 

      @todo The `MediaFoundationEncoder` has an identical
      function; we might want to create some shared utils that
      checks this. Though for now I think it's fine to have a
      couple of duplicate lines.

    REFERENCES:

      [0]: https://docs.microsoft.com/en-us/windows/win32/api/mftransform/ns-mftransform-mft_output_stream_info "MFT_OUTPUT_STREAM_INFO".

   */
  int MediaFoundationDecoder::doesOutputProvideBuffers(bool& doesProvide) {

    MFT_OUTPUT_STREAM_INFO stream_info = { 0 };
    HRESULT hr = S_OK;
    int r = 0;

    TRAE("@todo check the `cbAlignment` field of the `MFT_OUTPUT_STREAM_INFO` which indicates how we should align our memory buffers.");
    
    if (NULL == decoder_transform) {
      TRAE("Cannot check if the output provides buffers as our `decoder_transform` is NULL. ");
      r = -1;
      goto error;
    }

    if (UINT32_MAX == stream_output_id) {
      TRAE("Cannot check if the output provides buffers as our `stream_output_id` is invalid.");
      r = -2;
      goto error;
    }

    hr = decoder_transform->GetOutputStreamInfo(stream_output_id, &stream_info);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot check if we should create output buffers, `GetOutputStreamInfo()` failed.");
      r = -3;
      goto error;
    }

    doesProvide = (stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) ? true : false;

  error:
    return r;
  }

  /* ------------------------------------------------------- */

  /*
    GENERAL INFO:

      We check if the input uses ref counting or not as this
      tells us how we have to deal with input buffers. When the
      input use ref counting (useRefCount == false), then we can
      directly release the input data or reuse it. Otherwise,
      when the input stream uses refcounting we can only reuse
      the input samples when the MFT released them.

    TODO:

      This function is similar to the one in
      `MediaFoundationEncoder` and we could move them both some
      utils class/func.

   */
  int MediaFoundationDecoder::doesInputStreamUseRefCounts(bool& useRefCount) {

    MFT_INPUT_STREAM_INFO stream_info = { 0 };
    HRESULT hr = S_OK;
    int r = 0;

    /* We always default to false. */
    useRefCount = false;
    
    if (NULL == decoder_transform) {
      TRAE("Cannot check if the input stream uses reference counting, `decoder_transform` is NULL.");
      r = -1;
      goto error;
    }

    if (UINT32_MAX == stream_input_id) {
      TRAE("Cannot check if the input stream uses reference counting, `stream_input_id` is invalid.");
      r = -2;
      goto error;
    }

    hr = decoder_transform->GetInputStreamInfo(stream_input_id, &stream_info);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot check if the input stream uses reference counting, `GetInputStreamInfo()` failed.");
      r = -3;
      goto error;
    }

    useRefCount = (stream_info.dwFlags & MFT_INPUT_STREAM_DOES_NOT_ADDREF) ? false : true;

  error:

    return r;
  }

  /* ------------------------------------------------------- */

   /*
     GENERAL INFO:

      See "Output Buffers" from the [ProcessOutput][5]
      documentation. When the client needs to allocate the output
      buffers, then this function is called (via `init()`). We
      first get the stream info to determine the size of buffer
      we need to allocate. The `MFT_OUTPUT_STREAM_INFO::cbSize`
      member is influence by the `MF_MT_FRAME_SIZE` value which
      we've set for the INPUT stream. See `setInputMediaType()`.

    TODO:

      @todo In `MediaFoundationEncoder` we have a similar function; 
      we should create a utils/helper that does this for us. 

   */
  int MediaFoundationDecoder::createOutputBuffers() {

    MFT_OUTPUT_STREAM_INFO stream_info = { 0 };
    HRESULT hr = S_OK;
    int r = 0;

    TRAE("@todo we should/could create a utils function that creates the output buffers as it's similar to what the encoder does.");
    
    if (NULL == decoder_transform) {
      TRAE("Cannot create output buffers, decoder transform is NULL.");
      r = -1;
      goto error;
    }

    if (UINT32_MAX == stream_output_id) {
      TRAE("Cannot create output buffers, `stream_output_id` is invalid.");
      r = -2;
      goto error;
    }

    /* Make sure the sample or buffer haven't been created already. */
    if (NULL != output_sample) {
      TRAE("Cannot create the output buffers; the `output_sample` member is not NULL. Already created?");
      r = -3;
      goto error;
    }

    if (NULL != output_media_buffer) {
      TRAE("Cannot create the output buffers; the `output_media_buffer` member is not NULL. Already created?");
      r = -4;
      goto error;
    }

    hr = decoder_transform->GetOutputStreamInfo(stream_output_id, &stream_info);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot create output buffers, `GetOutputStreamInfo()` failed.");
      r = -5;
      goto error;
    }

    if (0 == stream_info.cbSize) {
      TRAE("Cannot create output buffers, `cbSize` of the `MFT_OUTPUT_STREAM_INFO` is 0 (?).");
      r = -6;
      goto error;
    }

    hr = MFCreateSample(&output_sample);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot create the output buffers. `MFCreateSample()` failed.");
      r = -7;
      goto error;
    }

    hr = MFCreateMemoryBuffer(stream_info.cbSize, &output_media_buffer);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot create output buffers, failed to create the memory buffer.");
      r = -8;
      goto error;
    }

    hr = output_sample->AddBuffer(output_media_buffer);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot create output buffers, failed to add the media buffer to the sample.");
      r = -9;
      goto error;
    }
    
  error:

    if (r < 0) {
      
      if (NULL != output_sample) {
        output_sample->Release();
        output_sample = NULL;
      }
      
      if (NULL != output_media_buffer) {
        output_media_buffer->Release();
        output_media_buffer = NULL;
      }
    }
    
    return r;    
  }

  /* ------------------------------------------------------- */

  int MediaFoundationDecoder::enableHardwareDecoding() {

    IMFAttributes* attribs = NULL;
    ICodecAPI* codec = NULL;
    HRESULT hr = S_OK;
    int r = 0;
    
    
    if (NULL == decoder_transform) {
      TRAE("Cannot enable hardware decoding as our `decoder_transform` is NULL.");
      r = -1;
      goto error;
    }
    
    hr = decoder_transform->QueryInterface(IID_ICodecAPI, (void**)&codec);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot enable hardware decoder as we failed to get the codec API.");
      r = -2;
      goto error;
    }

    /* Check if HW-accel is supported. */
    hr = codec->IsSupported(&CODECAPI_AVDecVideoAcceleration_H264);
    if (S_OK != hr) {
      TRAD("Hardware acceleration is not supported.");
      r = 0;
      goto error;
    }

    hr = codec->IsModifiable(&CODECAPI_AVDecVideoAcceleration_H264);
    if (S_OK != hr) {
      TRAE("Hardware acceleration cannot be modified.");
      r = 0;
      goto error;
    }

    /* When supported ... try to set it. */
    hr = decoder_transform->GetAttributes(&attribs);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot enable hardware based decoding as we failed to get the transform attributes that we need to enable it.");
      r = -3;
      goto error;
    }
      
    hr = attribs->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot enable hardware based decoder as we failed to set the attribute.");
      r = -4;
      goto error;
    }
    
  error:

    if (NULL != codec) {
      codec->Release();
      codec = NULL;
    }

    if (NULL != attribs) {
      attribs->Release();
      attribs = NULL;
    }
    
    return r;
  }

  /* ------------------------------------------------------- */

  int MediaFoundationDecoder::enableLowLatencyDecoding() {

    IMFAttributes* attribs = NULL;
    HRESULT hr = S_OK;
    int r = 0;
    
    if (NULL == decoder_transform) {
      TRAE("Cannot enable low latency decoding; our `decoder_transform` member is NULL. Did you call `init()`?");
      r = -1;
      goto error;
    }
    
    hr = decoder_transform->GetAttributes(&attribs);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot enable low latency decoding; we failed to get the attributes from the transform.");
      r = -2;
      goto error;
    }
    
    hr = attribs->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot enable low latency decoding; we failed to change the mode.");
      r = -3;
      goto error;
    }
    
  error:

    if (NULL != attribs) {
      attribs->Release();
      attribs = NULL;
    }

    return r;
  }

  /* ------------------------------------------------------- */

  /* 
     Thanks StackOverflow :P 
     
     @todo I have to move this into a utils file; shared with encoder.
        
  */
  static std::string get_last_error_as_string() {
    
    DWORD error_id = ::GetLastError();
    if(error_id == 0) {
      return std::string(); 
    }
    
    LPSTR message_buffer = nullptr;
    size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      error_id,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&message_buffer,
      0,
      NULL
    );
    
    std::string message(message_buffer, size);
    LocalFree(message_buffer);
    
    return message;
  }

  /* ------------------------------------------------------- */
  
}; /* namespace mo */
