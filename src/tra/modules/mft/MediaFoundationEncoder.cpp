#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Mfapi.h>         /* IMFMediaType */
#include <Mferror.h>       /* MF_E_* */
#include <wmcodecdsp.h>    /* CLSID_CMSH264EncoderMFT */
#include <Codecapi.h>      /* eAVEncH264VProfile_High */

#include <tra/modules/mft/MediaFoundationEncoder.h>
#include <tra/modules/mft/MediaFoundationUtils.h>

extern "C" {
#  include <tra/module.h>
#  include <tra/types.h>
#  include <tra/log.h>
}

namespace tra {

  /* ------------------------------------------------------- */
  
  static std::string get_last_error_as_string();
    
  /* ------------------------------------------------------- */

  bool MediaFoundationEncoderSettings::isValid() {

    if (NULL == on_encoded_data) {
      TRAE("`on_encoded_data` is NULL.");
      return false;
    }
    
    if (0 == image_width) {
      TRAE("The `image_width` is 0.");
      return false;
    }

    if (0 == image_height) {
      TRAE("The `image_height` is 0.");
      return false;
    }

    if (0 == bitrate) {
      TRAE("The `bitrate` is 0.");
      return false;
    }

    if (0 == fps_num) {
      TRAE("The `fps_num` is 0.");
      return false;
    }

    if (0 == fps_den) {
      TRAE("The `fps_den` is 0.");
      return false;
    }
      
    return true;
  };

  /* ------------------------------------------------------- */

  MediaFoundationEncoder::~MediaFoundationEncoder() {
    shutdown();
  }

  /* ------------------------------------------------------- */

  int MediaFoundationEncoder::init(MediaFoundationEncoderSettings& cfg) {

    IMFMediaType* media_type = NULL;
    DWORD num_out_streams = 0;
    DWORD num_in_streams = 0;  
    HRESULT hr = S_OK;
    int r = 0;

    TRAE("@todo See https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-processinput and the note regarding MFT_INPUT_STREAM_DOES_NOT_ADDREF");
    TRAE("@todo Check MFT_OUTPUT_STREAM_LAZY_READ in the ProcessInput() documentation. This will discard input samples when set.");
    TRAE("@todo Remove `tmp_ofs` after testing/development. Only used to test if the encoded data was usable.");
    TRAE("@todo Do we still want to use the `input_uses_refcount`?");
    TRAE("@todo Check if we have to explicitly enable the hardware encoder like we do in the decoder. ");

#if 0    
    printf("ABOUYT TO OPEN FILE.\n");
    tmp_ofs.open("./encoded2.h264", std::ios::binary | std::ios::out);
    printf("OPENED FILE\n");
    if (false == tmp_ofs.is_open()) {
      TRAE("Failed to open our test output file.");
      r = -1;
      goto error;
    }
#endif
    
    if (false == cfg.isValid()) {
      TRAE("Cannot initialize the Media Foundation Encoder as the given settings are invalid.");
      return -1;
    }

    if (NULL != encoder_transform) {
      TRAE("Cannot initialize the Media Foundation Encoder as our `encoder_transform` seems to be initialized already. Didn't you call `shutdown()` for a re-init?");
      return -2;
    }


    hr = MFStartup(MF_VERSION);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot initialize the Media Foundation Encoder, we failed to execute `MFStartup()`.");
      return -2;
    }

    settings = cfg;
    
    /* Initialize COM, see [2]. NULL is the reserved parameter. */
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to initialize the Media Foundation Encoder, we failed to execute `CoInitializeEx()`.");
      r = -3;
      goto error;
    }

    /* 
       We could use MFTEnum{Ex} though because we want to create
       a H264 encoder we can use `CoCreateInstance()`
       directly. See [this article][0] for info.
    */
    hr = CoCreateInstance(
      CLSID_CMSH264EncoderMFT,
      NULL,
      CLSCTX_INPROC_SERVER,
      IID_IMFTransform,
      (void**)&encoder_transform
    );

    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to initialize the Media Foundation Encoder, we failed to create an instance of CLSID_CMNSH264EncodeMft");
      r = -4;
      goto error;
    }

    /* Make sure that the number of streams are correct. See [About MFTs > Streams][3].  */
    hr = encoder_transform->GetStreamCount(&num_in_streams, &num_out_streams);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to initialize the Media Foundation Encoder, we failed to get the number of streams.");
      r = -5;
      goto error;
    }

    if (1 != num_in_streams) {
      TRAE("Cannot initialize the Media Foundation Encoder, the number of input streams should be 1 but is %u.", num_in_streams);
      r = -6;
      goto error;
    }

    if (1 != num_out_streams) {
      TRAE("Cannot initialize the Media Foundation Encoder, the number of output streams should be 1 but is %u.", num_out_streams);
      r = -7;
      goto error;
    }

    /*
      Returns E_NOTIMPL, [which means][0]:
      - The transform has a fixed number of streams.
      - The transform allows the client to add or remove input streams.
      - The stream identifiers are not consecutive starting from zero
    */
    hr = encoder_transform->GetStreamIDs(1, &stream_input_id, 1, &stream_output_id);
    if (E_NOTIMPL == hr) {
      stream_input_id = 0;
      stream_output_id = 0;
    }

    if (UINT32_MAX == stream_input_id
        || UINT32_MAX == stream_output_id)
      {
        TRAE("Failed to treieve the stream intput (%u) and/or output (%u) IDs.", stream_input_id, stream_output_id);
        r = -8;
        goto error;
      }
        
    /* 
       Set the output type before the input type, see [About
       MFTs: Streams][3]. This order is important. Also see
       [GetInputAvailableType][7] that can be used to determine
       if we should set the output type first. When we first
       have to set the output type, the result will be
       `MF_E_TRANSFORM_TYPE_NOT_SET`.
     */
    hr = encoder_transform->GetInputAvailableType(stream_input_id, 0, &media_type);
    if (MF_E_TRANSFORM_TYPE_NOT_SET != hr) {
      TRAE("We expect that we must set the output type before the input type. We have to add logic to handle a different case.");
      r = -9;
      goto error;
    }
    
    r = setOutputMediaType();
    if (r < 0) {
      TRAE("Failed to set the output media type.");
      r = -10;
      goto error;
    }

    r = setInputMediaType();
    if (r < 0) {
      TRAE("Failed to set the input media type.");
      r = -11;
      goto error;
    }

    /* Some transforms manage buffer allocation themselves, others not. */
    r = doesOutputProvideBuffers(output_provides_buffers);
    if (r < 0) {
      TRAE("Failed to check if we should create output buffers.");
      r = -12;
      goto error;
    }

    if (true == output_provides_buffers) {
      TRAE("The transform provides buffers; currently we only support transforms that DONT provide buffers.");
      r = -13;
      goto error;
    }

    /* Create the output buffers when needed. */
    if (false == output_provides_buffers) {
      r = createOutputBuffers();
      if (r < 0) {
        TRAE("Failed to create the output buffers.");
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
      TRAE("Failed to check if the input stream uses reference counting.");
      r = -14;
      goto error;
    }

    /* We expect that the input manages samples. */
    if (false == input_uses_refcount) {
      TRAE("The input stream doesn't use ref counting. We do not support this yet.");
      r = -15;
      goto error;
    }

    /* Notify that we're about the start so that the transform can allocate some internal buffers.. */
    hr = encoder_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to notify the encoder that we're about to start encoding.");
      r = -14;
      goto error;
    }

    TRAD("output_provides_buffers: %c", (true == output_provides_buffers) ? 'y' : 'n');
    TRAD("input_uses_refcount: %c", (true == input_uses_refcount) ? 'y' : 'n');

  error:

    if (r < 0) {
      shutdown();
    }
    
    return r;
  }

  /* ------------------------------------------------------- */

  int MediaFoundationEncoder::shutdown() {

    HRESULT hr = S_OK;
    int result = 0;

    /* @todo remove this after testing. */
    /*
    if (true == tmp_ofs.is_open()) {
      tmp_ofs.close();
    }
    */

    if (NULL != output_sample) {
      output_sample->Release();
    }

    if (NULL != output_media_buffer) {
      output_media_buffer->Release();
    }

    if (NULL != encoder_transform) {
      TRAE("@todo we have to cleanup `encoder_transform`. Only releasing for now. We should also flush.");
      encoder_transform->Release();
    }

    /* @todo We probably want to have one init/shutdown call for this! */
    /*
    hr = MFShutdown();
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to cleanly shutdown, `MFShutdown()` returned an error.");
      result -= 1;
    }
    */

    encoder_transform = NULL;
    output_sample = NULL;
    output_media_buffer = NULL;
    output_provides_buffers = false;
    input_uses_refcount = false;
    stream_output_id = UINT32_MAX;
    stream_input_id = UINT32_MAX;
          
    return result;
  }

  /* ------------------------------------------------------- */

  /*

    GENERAL INFO:

      This function is used to provide input data to the encoder.
      See [About MFTs][3] and the documentation about
      [ProcessInput][8]. There are a couple of things we need to 
      take into account:

      An MFT can either hold a reference to the given input data
      (IMFSample) or an MFT might copy the input data and store
      it internally. When the MFT holds a reference you are not
      allowed to use the IMFSample until it's released. This is
      not the case when the MFT copies the data.  You can check
      the MFT_INPUT_STREAM_DOES_NOT_ADDREF, which is set when the
      MFT copies the data.

   */
  int MediaFoundationEncoder::encode(tra_sample* inSample, uint32_t type, void* data) {

    IMFMediaBuffer* media_buffer = NULL;
    BYTE* media_buffer_data = NULL;
    tra_memory_image* src_image = NULL;
    IMFSample* sample = NULL;
    GUID mft_format = { 0 };
    UINT32 image_size = 0;
    HRESULT hr = S_OK;
    uint32_t i = 0;
    int r = 0;

    TRAD("Encoding");
                       
    if (NULL == inSample) {
      TRAE("Cannot encode as the given `tra_sample` is NULL.");
      r = -10;
      goto error;
    }

    if (TRA_MEMORY_TYPE_IMAGE != type) {
      TRAE("Cannto encode as the given `type` is not supported.");
      r = -20;
      goto error;
    }

    if (NULL == data) {
      TRAE("Cannot encode as the given `data` is NULL.");
      r = -30;
      goto error;
    }

    if (NULL == encoder_transform) {
      TRAE("Cannot encode as our `encoder_transform` member is NULL. Did you initialize?");
      r = -40;
      goto error;
    }

    src_image = (tra_memory_image*) data;

    if (src_image->image_width != settings.image_width) {
      TRAE("Cannot encode as the given `tra_memory_image::image_width` is not the same as the settings passed into `init()`.");
      r = -50;
      goto error;
    }

    if (src_image->image_height != settings.image_height) {
      TRAE("Cannot encode as the given `tra_memory_image::image_height` is not the same as the settings passed into `init()`.");
      r = -60;
      goto error;
    }

    r = mft_imageformat_to_videoformat(src_image->image_format, &mft_format);
    if (r < 0) {
      TRAE("Cannot encode as the given `tra_memory_image::image_format` is not supported.");
      r = -70;
      goto error;
    }

    /* @todo we might want to calculate this once and cache it. */
    hr = MFCalculateImageSize(mft_format, settings.image_width, settings.image_height, &image_size);
    if (false == SUCCEEDED(hr)) {
      TRAE("Canont encode as we failed to get the image size.");
      r = -70;
      goto error;
    }

    /* ---------------------------------------------- */
    /* CREATE BUFFER                                  */
    /* ---------------------------------------------- */

    /* @todo We might want to use MFCreateAlignedMemoryBuffer */
    hr = MFCreateMemoryBuffer((DWORD)image_size, &media_buffer);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to allocate the IMFMediaBuffer for the input image.");
      r = -80;
      goto error;
    }

    hr = media_buffer->Lock(&media_buffer_data, NULL, NULL);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to lock the media buffer.");
      r = -90;
      goto error;
    }

    /* 
       @todo I'm currently using plane_stride[i] as the width in
       bytes that I copy, see the comments below. It might happen
       though, that the stride is not the same as the width in
       bytes. Maybe in some cases I want to copy less bytes ;
       e.g. when there is some padding in the input image. As
       long as I don't run into any issues I'll use the stride as
       "width in bytes".
    */
    
    /* Copy the source image into the media buffer. */
    for (i = 0; i < src_image->plane_count; ++i) {
      
      hr = MFCopyImage(
        media_buffer_data,                /* dest buffer */
        settings.image_width,             /* dest stride */
        src_image->plane_data[i],         /* start of the first row of pixels in the source image */
        src_image->plane_strides[i],      /* stride of the source image in bytes. */
        src_image->plane_strides[i],      /* width in bytes to be copied. */
        src_image->plane_heights[i]       /* number of rows to copy */
      );

      if (S_OK != hr) {
        TRAE("Failed to copy the source image into the `IMFMediaBuffer`.");
        r = -100;
        goto error;
      }

      media_buffer_data += (src_image->plane_strides[i] * src_image->plane_heights[i]);
    }
    
    hr = media_buffer->Unlock();
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to cleanly unload the media buffer.");
      r = -8;
      goto error;
    }

    hr = media_buffer->SetCurrentLength(image_size);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the current length.");
      r = -9;
      goto error;
    }

    /* ---------------------------------------------- */
    /* CREATE SAMPLE                                  */
    /* ---------------------------------------------- */

    hr = MFCreateSample(&sample);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to create the sample.");
      r = -10;
      goto error;
    }

    hr = sample->AddBuffer(media_buffer);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to add the buffer to the sample");
      r = -11;
      goto error;
    }

    /* @todo Update our temp vars; we should get those from `inSample`. */
    tmp_frame_time = ((1.0 / 25) * tmp_frame_num) * 1e9; /* In nanos */
    tmp_frame_num = tmp_frame_num + 1;

    hr = sample->SetSampleTime(tmp_frame_time / 100); /* In hundreds of nanos */
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the sample time.");
      r = -12;
      goto error;
    }
    
    /* @todo this is a const we could cache the duration. */
    hr = sample->SetSampleDuration(double(1.0 / 25) * 1e7);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the sample duration.");
      r = -13;
      goto error;
    }

    hr = encoder_transform->ProcessInput(stream_input_id, sample, 0);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to process input. %s", get_last_error_as_string().c_str());
      r = -12;
      goto error;
    }

    r = processOutput();
    if (r < 0) {
      TRAE("Failed to process the encoded data...");
      r = -13;
      goto error;
    }

  error:

    {
      static uint8_t did_log = 0;
      if (0 == did_log) {
        TRAE("@todo I'm currently releasing the `MediaBuffer`. I'm not entirely sure if I have to do that (probably I do). ");
        did_log = 1;
      }
    }
    
    if (NULL != media_buffer) {
      media_buffer->Release();
      media_buffer = NULL;
    }

    if (NULL != sample) {
      sample->Release();
      sample = NULL;
    }

    return 0;
  }

  /* ------------------------------------------------------- */

  /* See ["Output Buffers"][9].  */
  int MediaFoundationEncoder::processOutput() {

    if (true == output_provides_buffers) {
      return processOutputWithTransformBuffers();
    }

    return processOutputWithClientBuffers();
  }

  /* ------------------------------------------------------- */

  int MediaFoundationEncoder::processOutputWithClientBuffers() {

    tra_encoded_host_memory encoded_data = { 0 };
    MFT_OUTPUT_DATA_BUFFER output_data = { 0 };
    IMFMediaBuffer* output_buffer = NULL;
    BYTE* output_buffer_data_ptr = NULL;
    DWORD output_buffer_data_length = 0;
    DWORD buffer_count = 0;
    DWORD status = 0;
    HRESULT hr = S_OK;
    int r = 0;

    if (NULL == encoder_transform) {
      TRAE("Cannot process output as `encoder_transform` is NULL.");
      r = -10;
      goto error;
    }

    if (true == output_provides_buffers) {
      TRAE("Cannot process output using client buffers as the MFT provides buffers.");
      r = -20;
      goto error;
    }

    if (NULL == output_sample) {
      TRAE("Cannot process output using client buffers because the `output_sample` is NULL.");
      r = -30;
      goto error;
    }

    if (NULL == settings.on_encoded_data) {
      TRAE("Cannot process output as our `on_encoded_data` is NULL. Make sure to setup a callback that will receive encoded data.");
      r = -40;
      goto error;
    }

    /* Check if there is coded data. */
    output_data.dwStreamID = stream_output_id;
    output_data.dwStatus = 0;
    output_data.pEvents = NULL;
    output_data.pSample = output_sample;

    hr = encoder_transform->ProcessOutput(0, 1, &output_data, &status);

    if (MF_E_TRANSFORM_STREAM_CHANGE == hr) {
      TRAE("@todo We need to handle stream changes.");
      exit(EXIT_FAILURE);
    }

    /* We need more input data; not an error. */
    if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr) {
      TRAT("Need more input data.");
      r = 0; 
      goto error;
    }

    if (S_OK != hr) {
      TRAE("Unhandled error from ProcessOutput().");
      r = -50;
      goto error;
    }

#if !defined(NDEBUG)
    
    hr = output_data.pSample->GetBufferCount(&buffer_count);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot process the output, failed to get the buffer count.");
      r = -60;
      goto error;
    }

    if (1 != buffer_count) {
      TRAE("Cannot process the output, we expect the buffer count to be 1 but it's %u.", buffer_count);
      r = -70;
      goto error;
    }
    
#endif    
    
    /* Access the coded data. */
    hr = output_data.pSample->GetBufferByIndex(0, &output_buffer);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to get the encoded data buffer.");
      r = -70;
      goto error;
    }

    hr = output_buffer->Lock(&output_buffer_data_ptr, NULL, &output_buffer_data_length);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to lock the encoded data buffer.");
      r = -80;
      goto error;
    }

    encoded_data.data = (uint8_t*) output_buffer_data_ptr;
    encoded_data.size = output_buffer_data_length;
    
    r = settings.on_encoded_data(
      TRA_MEMORY_TYPE_HOST_H264,
      &encoded_data,
      settings.user
    );
    
    /* @todo remove; used during development to check if the output stream works. */
    //tmp_ofs.write((char*)output_buffer_data_ptr, output_buffer_data_length);

    hr = output_buffer->Unlock();
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to unlock the output buffer.");
      r = -90;
      goto error;
    }
      
  error:
    
    return r;
  }

  /* ------------------------------------------------------- */

  /*
    TODO:

      @todo As the transforms I've tested don't manage their
      buffers I haven't been able to test and fully implement
      this function. I'm leaving this here until I've got
      something to test.

   */
  int MediaFoundationEncoder::processOutputWithTransformBuffers() {

    MFT_OUTPUT_DATA_BUFFER output_buffer = { 0 };
    DWORD status = 0;
    HRESULT hr = S_OK;
    int r = 0;

    if (NULL == encoder_transform) {
      TRAE("Cannot process output as `encoder_transform` is NULL.");
      r = -1;
      goto error;
    }

    output_buffer.dwStreamID = stream_output_id;
    output_buffer.dwStatus = 0;
    output_buffer.pEvents = NULL;
    output_buffer.pSample = NULL;

    hr = encoder_transform->ProcessOutput(0, 1, &output_buffer, &status);

    switch (hr) {

      case MF_E_TRANSFORM_TYPE_NOT_SET: {
        TRAE("MF_E_TRANSFORM_TYPE_NOT_SET");
        break;
      }
      
      case MF_E_TRANSFORM_STREAM_CHANGE: {
        TRAE("Got MF_E_TRANSFORM_STREAM_CHANGE");
        break;
      }
      
      case MF_E_TRANSFORM_NEED_MORE_INPUT: {
        TRAE("Got MF_E_TRANSFORM_NEED_MORE_INPUT");
        break;
      }
        
      case MF_E_INVALIDSTREAMNUMBER: {
        TRAE("Got MF_E_INVALIDSTREAMNUMBER");
        break;
      }
      
      case E_UNEXPECTED: {
        TRAE("Got E_UNEXPECTED");
        break;
      }
      
      default: {
        TRAE("Unexepected result from `ProcessOutput()`: %s.", get_last_error_as_string().c_str());
        r = -2;
        goto error;
      }
    }

  error:
    return r;
  }

  
  /* ------------------------------------------------------- */

  /*

    GENERAL INFO: 

      See the [Basic Processing Model] where it's described that
      we have to set the output type that we want to use. See
      [H.264 Video Encoder][5] for more information how to setup
      the output type.

   TOOD:

     @todo Look into the different bitrate modes.
     @todo Look into how we should pass the h264 profile in a general way between plugins.

   */
  int MediaFoundationEncoder::setOutputMediaType() {

    IMFMediaType* media_type = NULL;
    HRESULT hr = S_OK;
    int r = 0;

    if (false == settings.isValid()) {
      TRAE("Cannot set the output type as our settings are invalid.");
      r = -1;
      goto error;
    }

    if (NULL == encoder_transform) {
      TRAE("Cannot set the output type as our `encoder_transform` is NULL.");
      r = -2;
      goto error;
    }

    if (UINT32_MAX == stream_output_id) {
      TRAE("Cannot set the output type as our `stream_output_id` is invalid.");
      r = -3;
      goto error;
    }

    hr = MFCreateMediaType(&media_type);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to create the `IMFMediaType` for the output media type.");
      r = -3;
      goto error;
    }

    hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the MF_MT_MAJOR_TYPE to video on the output media type.");
      r = -4;
      goto error;
    }
    
    hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the MF_MT_SUB_TYPE type of the output media type.");
      r = -4;
      goto error;
    }

    hr = MFSetAttributeSize(media_type, MF_MT_FRAME_SIZE, settings.image_width, settings.image_height);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the frame size on the output media type.");
      r = -5;
      goto error;
    }
    
    hr = MFSetAttributeRatio(media_type, MF_MT_FRAME_RATE, settings.fps_den, settings.fps_num);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the frame rate on the output media type.");
      r = -6;
      goto error;
    }

    /* @todo we're using AVG bitrate, we might want this to be configurable. */
    hr = media_type->SetUINT32(MF_MT_AVG_BITRATE, settings.bitrate);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the bitrate on the output media type.");
      r = -7;
      goto error;
    }
    
    hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the interlace mode to progressive on the output media type.");
      r = -8;
      goto error;
    }

    /* (Optional) Tell the encoder we use square pixels. */
    hr = MFSetAttributeRatio(media_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the aspect ratio on the output media type.");
      r = -9;
      goto error;
    }
    
    /* @todo We have to create a global h264 profile and level mapping that can be passed via settings. See [eAVEncH264VProfile][4] */
    hr = media_type->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the H264 profile on the output media type.");
      r = -10;
      goto error;
    }

    /* @todo We have to create a global solution to set the level if required. */
    
    /* (Optional) We use -1 which indicates that the encoder will [select][5] the encoding level. */
    hr = media_type->SetUINT32(MF_MT_MPEG2_LEVEL, (UINT32)-1);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the H264 level on the output media type.");
      r = -11;
      goto error;
    }
    
    /* Finally send it to the encoder. */
    hr = encoder_transform->SetOutputType(stream_output_id, media_type, 0);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the output type on the encoder_transform.");
      r = -12;
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

  int MediaFoundationEncoder::setInputMediaType() {

    IMFMediaType* media_type = NULL;
    GUID mft_format = { 0 };
    HRESULT hr = S_OK;
    int r = 0;

    if (false == settings.isValid()) {
      TRAE("Cannot set the input media type as our settings are invalid.");
      r = -10;
      goto error;
    }

    if (NULL == encoder_transform) {
      TRAE("Cannot set the input media type as our `encoder_transform` is NULL.");
      r = -20;
      goto error;
    }

    if (UINT32_MAX == stream_input_id) {
      TRAE("Cannot set the input media type as our `stream_input_id` is invalid.");
      r = -30;
      goto error;
    }

    r = mft_imageformat_to_videoformat(settings.image_format, &mft_format);
    if (r < 0) {
      TRAE("Cannot set the inptu media format as we couldn't convert the `image_format` into a MFT format.");
      r = -40;
      goto error;
    }

    hr = MFCreateMediaType(&media_type);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to create the media type. Cannot set input for MFT.");
      r = -50;
      goto error;
    }
    
    hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the `MF_MT_MAJOR_TYPE` for input media type type.");
      r = -60;
      goto error;
    }

    /* @todo We should extract the input format from the settings. */
    hr = media_type->SetGUID(MF_MT_SUBTYPE, mft_format);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the `MF_MT_MINOR_TYPE` for the input media type.");
      r = -70;
      goto error;
    }
    
    hr = MFSetAttributeSize(media_type, MF_MT_FRAME_SIZE, settings.image_width, settings.image_height);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the frame size for the input media type.");
      r = -80;
      goto error;
    }
    
    hr = MFSetAttributeRatio(media_type, MF_MT_FRAME_RATE, settings.fps_den, settings.fps_num);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the frame rate on the input media type.");
      r = -90;
      goto error;
    }
    
    hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the interlace mode for the input media type.");
      r = -100;
      goto error;
    }
    
    hr = MFSetAttributeRatio(media_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the pixel aspect ratio for the input media type.");
      r = -110;
      goto error;
    }
    
    hr = encoder_transform->SetInputType(stream_input_id, media_type, 0);
    if (false == SUCCEEDED(hr)) {
      TRAE("Failed to set the input media type on the encoder_transform: %s", get_last_error_as_string().c_str());
      r = -120;
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

      @todo When we have to provide our own buffers we have
      to consider th `stream_info.cbAlignment` value.

    REFERENCES:

      [0]: https://docs.microsoft.com/en-us/windows/win32/api/mftransform/ns-mftransform-mft_output_stream_info "MFT_OUTPUT_STREAM_INFO".

   */
  int MediaFoundationEncoder::doesOutputProvideBuffers(bool& doesProvide) {
    
    MFT_OUTPUT_STREAM_INFO stream_info = { 0 };
    HRESULT hr = S_OK;
    int r = 0;

    TRAE("@todo check the `cbAlignment` field of the `MFT_OUTPUT_STREAM_INFO` which indicates how we should align our memory buffers.");

    if (NULL == encoder_transform) {
      TRAE("Cannot check if we should create output buffers, encoder transform is NULL.");
      r = -1;
      goto error;
    }

    if (UINT32_MAX == stream_output_id) {
      TRAE("Cannot check if we should create output buffers, `stream_output_id` is invalid.");
      r = -2;
      goto error;
    }

    hr = encoder_transform->GetOutputStreamInfo(stream_output_id, &stream_info);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot check if we should create output buffers, `GetOutputStreamInfo()` failed.");
      r = -3;
      goto error;
    }

    doesProvide = (stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) ? true : false;

  error:
    return r;
  }

  /*

    We check if the input uses ref counting or not as this tells
    us how we have to deal with input buffers. When the input
    use ref counting (useRefCount == false), then we
    can directly release the input data or reuse it. Otherwise,
    when the input stream uses refcounting we can only reuse the
    input samples when the MFT released them.

    For more info [see ProcessInput][8]. 

   */
  int MediaFoundationEncoder::doesInputStreamUseRefCounts(bool& useRefCount) {

    MFT_INPUT_STREAM_INFO stream_info = { 0 };
    HRESULT hr = S_OK;

    /* We always default to false. */
    useRefCount = false;
    
    if (NULL == encoder_transform) {
      TRAE("Cannot check if the input stream uses reference counting, `encoder_transform` is NULL.");
      return -1;
    }

    if (UINT32_MAX == stream_input_id) {
      TRAE("Cannot check if the input stream uses reference counting, `stream_input_id` is invalid.");
      return -2;
    }

    hr = encoder_transform->GetInputStreamInfo(stream_input_id, &stream_info);
    if (false == SUCCEEDED(hr)) {
      TRAE("Cannot check if the input stream uses reference counting, `GetInputStreamInfo()` failed.");
      return -3;
    }

    useRefCount = (stream_info.dwFlags & MFT_INPUT_STREAM_DOES_NOT_ADDREF) ? false : true;
 
    return 0;
  }

  /* ------------------------------------------------------- */

  /*
    See "Output Buffers" from the [ProcessOutput][9]
    documentation. When the client needs to allocate the output
    buffers, then this function is called (via `init()`). We first
    get the stream info to determine the size of buffer we need to 
    allocate.
   */
  int MediaFoundationEncoder::createOutputBuffers() {

    MFT_OUTPUT_STREAM_INFO stream_info = { 0 };
    HRESULT hr = S_OK;
    int r = 0;
    
    if (NULL == encoder_transform) {
      TRAE("Cannot create output buffers, encoder transform is NULL.");
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

    hr = encoder_transform->GetOutputStreamInfo(stream_output_id, &stream_info);
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

  /* Thanks StackOverflow :P */
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
  
}; /* namespace tra */
