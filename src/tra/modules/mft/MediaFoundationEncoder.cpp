
#include "tra/modules/mft/MediaFoundationEncoder.h"
#include <mferror.h>

#include "tra/log.h"

namespace tra {

  int MediaFoundationEncoder::createInputSample() {

    GUID input_format;

    HRESULT hr = S_OK;
    int r = 0;

    r = convert_trameleon_to_mft_image_format(settings->input_format, input_format);
    if (0 > r) {
      TRAE("cannot convert image format");
      goto error;
    }

    image_size = (uint32_t *)calloc(1, sizeof(uint32_t));

    hr = MFCalculateImageSize(input_format, settings->image_width, settings->image_height, image_size);
    if (S_OK != hr) {
      TRAE("Failed to MFCalculateImageSize %s", input_format);
      r = -10;
      goto error;
    }

    hr = MFCreateMemoryBuffer(*image_size, &input_buffer);
    if (S_OK != hr) {
      TRAE("Failed to MFCreateMemoryBuffer");
      r = -10;
      goto error;
    }

    hr = MFCreateSample(&input_sample);
    if (S_OK != hr) {
      TRAE("Failed to MFCreateSample");
      r = -10;
      goto error;
    }

    hr = input_sample->AddBuffer(input_buffer);
    if (S_OK != hr) {
      TRAE("Failed to AddBuffer to sample");
      r = -10;
      goto error;
    }

  error:
    return r;
  }

  int MediaFoundationEncoder::createOutputSample() {

    bool output_provides_buffers = true;
    IMFMediaBuffer *output_buffer = NULL;
    HRESULT hr = S_OK;
    int r = 0;

    if(NULL != output_sample){
      TRAE("output sample already created");
      r = -10;
      goto error;
    }

    /* Flag to specify that the MFT provides the output sample for this stream (thought internal allocation or operating on the input stream)*/
    output_provides_buffers = MFT_OUTPUT_STREAM_PROVIDES_SAMPLES == output_stream_info.dwFlags || MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES == output_stream_info.dwFlags;

    if(true == output_provides_buffers){
      output_sample = NULL;
      TRAE("provided buffers not implemented");
      return -1;
    }

    hr = queryOutputStreamInfo();
    if(S_OK != hr){
      TRAE("failed to querry stream info");
      r = -10;
      goto error;
    }

    if (0 == output_stream_info.cbSize) { output_stream_info.cbSize = 1024 * 1024; }


    hr = MFCreateMemoryBuffer(output_stream_info.cbSize, &output_buffer);
    if (S_OK != hr) {
      TRAE("Failed to MFCreateMemoryBuffer");
      r = -10;
      goto error;
    }

    hr = MFCreateSample(&output_sample);
    if (S_OK != hr) {
      TRAE("Failed to MFCreateSample");
      r = -10;
      goto error;
    }

    hr = output_sample->AddBuffer(output_buffer);
    if (S_OK != hr) {
      TRAE("Failed to AddBuffer to sample");
      r = -10;
      goto error;
    }

  error:
    return hr;
  }

  int MediaFoundationEncoder::init(tra_encoder_settings *cfg) {
    if (NULL == cfg) {
      TRAE("mft settings not set");
      return -1;
    }

    int r = 0;
    HRESULT hr = MFStartup(MF_VERSION);
    if (S_OK != hr) {
      TRAE("Cannot initialize the Media Foundation Encoder, we failed to execute `MFStartup()`.");
      return -2;
    }

    settings = cfg;

    GUID output_format;
    GUID input_format;

    DWORD num_out_streams = 0;
    DWORD num_in_streams = 0;
    IMFMediaType *media_type;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (S_OK != hr && RPC_E_CHANGED_MODE == hr) {
      TRAE("failed to initialize");
      r = -10;
      goto error;
    }

    r = convert_trameleon_to_mft_image_format(settings->output_format, output_format);
    if (0 > r) {
      TRAE("cannot convert image format");
      goto error;
    }

    r = convert_trameleon_to_mft_image_format(settings->input_format, input_format);
    if (0 > r) {
      TRAE("cannot convert image format");
      goto error;
    }

    hr = findEncoder(input_format, output_format, &encoder_transform);
    if (S_OK != hr) {
      TRAE("failed to find/Create specified encoder mft");
      r = -10;
      goto error;
    }

    /* Make sure that the number of streams are correct. See [About MFTs > Streams][3].  */
    hr = encoder_transform->GetStreamCount(&num_in_streams, &num_out_streams);
    if (S_OK != hr) {
      TRAE("Failed to initialize the Media Foundation Encoder, we failed to get the number of "
           "streams.");
      r = -5;
      goto error;
    }

    if (1 != num_in_streams) {
      TRAE("Cannot initialize the Media Foundation Encoder, the number of input streams should be "
           "1 but is %u.",
           num_in_streams);
      r = -6;
      goto error;
    }

    if (1 != num_out_streams) {
      TRAE("Cannot initialize the Media Foundation Encoder, the number of output streams should be "
           "1 but is %u.",
           num_out_streams);
      r = -7;
      goto error;
    }

    hr = queryStreamCapabilities();
    if (S_OK != hr) {
      TRAE("failed to stream capabilities ");
      r = -10;
      goto error;
    }

    hr = encoder_transform->GetInputAvailableType(stream_input_id, 0, &media_type);
    if (MF_E_TRANSFORM_TYPE_NOT_SET != hr) {
      TRAE("We expect that we must set the output type before the input type. We have to add logic "
           "to handle a different case.");
      r = -9;
      goto error;
    }

    // set the media output info
    hr = setOutputMediaType();
    if (S_OK != hr) {
      TRAE("failed to setOutputMediaType ");
      r = -10;
      goto error;
    }

    // set the media input info
    hr = setInputMediaType();
    if (S_OK != hr) {
      TRAE("failed to setInputMediaType");
      r = -10;
      goto error;
    }

    /*
   Check how we have to deal with input buffers. When the MFT
   doesn't use refcounts it means that it copies the data. We
   currently do not support this method.
*/
    r = doesInputStreamUseRefCounts();
    if (r < 0) {
      TRAE("Failed to check if the input stream uses reference counting.");
      r = -14;
      goto error;
    }

    if (r == 0) {
      TRAE("The input stream doesn't use ref counting. We do not support this yet.");
      r = -15;
      goto error;
    }

    // query media input stream info
    hr = queryInputStreamInfo();
    if (S_OK != hr) {
      TRAE("failed to queryInputStreamInfo");
      r = -10;
      goto error;
    }

    // query media input stream info
    hr = queryOutputStreamInfo();
    if (S_OK != hr) {
      TRAE("failed to queryInputStreamInfo");
      r = -10;
      goto error;
    }

    hr = createOutputSample();
    if (S_OK != hr) {
      TRAE("failed to create output sample");
      r = -10;
      goto error;
    }

    hr = sendStreamStartMessage();
    if (S_OK != hr) {
      TRAE("failed to send start stream message");
      r = -10;
      goto error;
    }

  error:
    if (0 > r) {
      Release(&output_sample);
      Release(&input_buffer);
      Release(&input_sample);
      Release(&encoder_transform);
    }
    return r;
  }

  int MediaFoundationEncoder::shutdown() {
    int r = 0;
    r = sendStreamEndMessage();
    r = Release(&encoder_transform);

    r = Release(&input_buffer);
    r = Release(&input_sample);

    r = Release(&output_sample);

    input_uses_refcount = false;

    return r;
  }

  // Query stream limits to determine pipeline characteristics...I.E fix size stream and Stream ID
  int MediaFoundationEncoder::queryStreamCapabilities() {
    HRESULT hr = S_OK;

    // if the min and max input and output stream counts are the same, the MFT is a Fix stream size. So we cannot add or remove streams
    hr = encoder_transform->GetStreamLimits(&inputStreamMin, &inputStreamMax, &outputStreamMin, &outputStreamMax);
    if (S_OK != hr) {
      TRAE("Failed to GetStreamLimits for MFT");
      return -1;
    }

    hr = encoder_transform->GetStreamIDs(inputStreamMin, &stream_input_id, outputStreamMin, &stream_output_id);
    if (E_NOTIMPL == hr) {
      stream_input_id = 0;
      stream_output_id = 0;
      hr = S_OK;
    }

    if (S_OK != hr) {
      TRAE("Failed to GetStreamIDs for MFT");
      return -2;
    }

    return 0;
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
  int MediaFoundationEncoder::doesInputStreamUseRefCounts() {

    MFT_INPUT_STREAM_INFO stream_info = { 0 };
    HRESULT hr = S_OK;

    /* We always default to false. */
    bool useRefCount = false;

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

    return useRefCount;
  }

  int MediaFoundationEncoder::findEncoder(const GUID &inputSubtype, const GUID &outputSubtype, IMFTransform** encoderTransform) {

    HRESULT hr = S_OK;
    int r = 0;
    UINT32 count = 0;

    IMFActivate **ppCLSIDs = NULL;
    IMFTransform* transform = NULL;

    MFT_REGISTER_TYPE_INFO info_input = {0};
    MFT_REGISTER_TYPE_INFO info_output = {0};

    info_input.guidMajorType = MFMediaType_Video; //@todo do we need audio
    info_input.guidSubtype = inputSubtype;

    info_output.guidMajorType = MFMediaType_Video; //@todo do we need audio
    info_output.guidSubtype = outputSubtype;

    hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, 0, &info_input, &info_output, &ppCLSIDs, &count);
    if (S_OK != hr) {
      TRAE("failed to find possible encoders");
      r = -10;
      goto error;
    }

    if (0 == count) {
      TRAE("encoder not found");
      hr = MF_E_TOPO_CODEC_NOT_FOUND;
      r = -10;
      goto error;
    }

    hr = ppCLSIDs[0]->ActivateObject(IID_PPV_ARGS(&transform));
    if (S_OK != hr) {
      TRAE("failed to activate encoder");
      r = -10;
      goto error;
    }

    *encoderTransform = transform;

  error:
    for (int i = 0; i < count; i++) { ppCLSIDs[i]->Release(); }

    CoTaskMemFree(ppCLSIDs);
    return r;
  }

  int MediaFoundationEncoder::createInputMediaType(IMFMediaType **input_media_type,
                                                      tra_encoder_settings &settings, IMFTransform* encoder_transform, uint32_t stream_input_id) {
    GUID InputFormat;
    int r = 0;
    HRESULT hr = S_OK;
    IMFMediaType *mt = NULL;

    if (NULL == input_media_type) {
      TRAE("input_media_type cannot be initialized");
      r = -10;
      goto error;
    }

    hr = encoder_transform->GetInputAvailableType(stream_input_id, 0, &mt);
    if (MF_E_TRANSFORM_TYPE_NOT_SET == hr) {
      TRAE("we must set the output type before the input type.");
      r = -10;
      goto error;
    }

    if (S_OK != hr) {
      TRAE("failed get available input types");
      r = -10;
      goto error;
    }

    hr = mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (S_OK != hr) {
      TRAE("failed to set MF_MT_MAJOR_TYPE");
      r = -10;
      goto error;
    }

    r = convert_trameleon_to_mft_image_format(settings.input_format, InputFormat);
    if (0 > r) {
      TRAE("cannot get InputFormat");
      r = -10;
      goto error;
    }

    hr = mt->SetGUID(MF_MT_SUBTYPE, InputFormat);
    if (S_OK != hr) {
      TRAE("cannot set MF_MT_SUBTYPE");
      r = -10;
      goto error;
    }

    hr = mt->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (S_OK != hr) {
      TRAE("cannot set MF_MT_INTERLACE_MODE");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeSize(mt, MF_MT_FRAME_SIZE, settings.image_width, settings.image_height);
    if (S_OK != hr) {
      TRAE("cannot set FRAME_SIZE");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeRatio(mt, MF_MT_FRAME_RATE, settings.fps_num, settings.fps_den);
    if (S_OK != hr) {
      TRAE("cannot set FRAME_RATE");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeRatio(mt, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (S_OK != hr) {
      TRAE("cannot set PIXEL_ASPECT_RATIO");
      r = -10;
      goto error;
    }

    *input_media_type = mt;

  error:
    if(0 > r){
      Release(&mt);
    }
    return r;
  }

  int MediaFoundationEncoder::setInputMediaType() {
    if (!encoder_transform) { TRAE("encoder has not been initialised"); return -1; }

    IMFMediaType *input_media_type = NULL;
    int r = 0;

    HRESULT hr = createInputMediaType(&input_media_type, *settings, encoder_transform, stream_input_id);
    if (S_OK != hr) {
      TRAE("failed to create input media type");
      r = -10;
      goto error;
    }

    hr = encoder_transform->SetInputType(stream_input_id, input_media_type, 0); //
    if (MF_E_TRANSFORM_TYPE_NOT_SET == hr) {
      TRAE("MF_E_TRANSFORM_TYPE_NOT_SET -> 0xC00D6D60L: You must set the output type first");
      r = -10;
      goto error;
    }
    if (MF_E_INVALIDMEDIATYPE == hr) {
      TRAE("MF_E_INVALIDMEDIATYPE -> 0xc00d36b4: the data specified for the media type is "
           "invalid, inconsistent, or not supported by this object");
      r = -10;
      goto error;
    }

  error:

    return r;
  }

  int MediaFoundationEncoder::createOutputMediaType(IMFMediaType **output_media_type,
                                                       tra_encoder_settings &settings) {
    GUID output_format;
    int r = 0;
    IMFMediaType *mt = NULL;
    HRESULT hr = S_OK;

    if (NULL == output_media_type) {
      TRAE("input_media_type cannot be initialized");
      r = -10;
      goto error;
    }

    hr = MFCreateMediaType(&mt);
    if (S_OK != hr) {
      TRAE("failed to create output media type");
      r = -10;
      goto error;
    }

    hr = mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (S_OK != hr) {
      TRAE("failed to set MF_MT_MAJOR_TYPE");
      r = -10;
      goto error;
    }

    r = convert_trameleon_to_mft_image_format(settings.output_format, output_format);
    if (0 > r) {
      TRAE("failed to convert image format from trameleon format");
      goto error;
    }

    hr = mt->SetGUID(MF_MT_SUBTYPE, output_format);
    if (S_OK != hr) {
      TRAE("failed to set MF_MT_SUBTYPE");
      r = -10;
      goto error;
    }

    hr = mt->SetUINT32(MF_MT_AVG_BITRATE, settings.bitrate);
    if (S_OK != hr) {
      TRAE("failed to set bitrate");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeRatio(mt, MF_MT_FRAME_RATE, settings.fps_num, settings.fps_den);
    if (S_OK != hr) {
      TRAE("failed to set frame rate");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeSize(mt, MF_MT_FRAME_SIZE, settings.image_width, settings.image_height);
    if (S_OK != hr) {
      TRAE("failed to set frame size");
      r = -10;
      goto error;
    }

    hr = mt->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (S_OK != hr) {
      TRAE("failed to set INTERLACE_MODE");
      r = -10;
      goto error;
    }

    uint32_t profile;
    r = convert_trameleon_to_mft_profile(profile, settings.profile);
    if (0 > r) {
      TRAE("cannot get profile from trameleon image format");
      goto error;
    }

    hr = mt->SetUINT32(MF_MT_MPEG2_PROFILE, profile);
    if (S_OK != hr) {
      TRAE("failed to set MF_MT_MPEG2_PROFILE");
      r = -10;
      goto error;
    }

    /* @todo We have to create a global solution to set the level if required. */

    /* (Optional) We use -1 which indicates that the encoder will [select][5] the encoding level. */
    hr = mt->SetUINT32(MF_MT_MPEG2_LEVEL, (UINT32)-1);
    if (S_OK != hr) {
      TRAE("Failed to set the H264 level on the output media type.");
      r = -11;
      goto error;
    }

    hr = MFSetAttributeRatio(mt, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (S_OK != hr) {
      TRAE("failed to set aspect ratio");
      r = -10;
      goto error;
    }

    *output_media_type = mt;

  error:
    if(0 > r){
      Release(&mt);
    }
    return hr;
  }

  int MediaFoundationEncoder::setOutputMediaType() {
    if (!encoder_transform) {
      TRAE("no transform initialised");
      return -1;
    }

    IMFMediaType *output_media_type = NULL;
    int r = 0;

    HRESULT hr = createOutputMediaType(&output_media_type, *settings);
    if (S_OK != hr) {
      TRAE("Failed to create output type");
      r = -10;
      goto error;
    }

    hr = encoder_transform->SetOutputType(stream_output_id, output_media_type, 0);
    if (S_OK != hr) {
      TRAE("Failed to set output type");
      r = -10;
      goto error;
    }

  error:
    if(0 > r){
      Release(&output_media_type);
    }
    return r;
  }

  // Gets the buffer requirements and other information for the input stream of the MFT
  int MediaFoundationEncoder::queryInputStreamInfo() {
    if (!encoder_transform) {
      TRAE("no transform initialised");
      return -1;
    }
    HRESULT hr = encoder_transform->GetInputStreamInfo(stream_input_id, &input_stream_info);
    if (S_OK != hr) {
      TRAE(" Failed to queried input stream info");
      return -2;
    }
    return 0;
  }

  // Gets the buffer requirements and other information for the output stream of the MFT
  int MediaFoundationEncoder::queryOutputStreamInfo() {
    if (!encoder_transform) {
      TRAE("no transform initialised");
      return -1;
    }

    HRESULT hr = encoder_transform->GetOutputStreamInfo(stream_output_id, &output_stream_info);

    if (S_OK != hr) {
      TRAE("Failed to query output stream info");
      return -2;
    }

    return 0;
  }

  // Send request to MFT to allocate necessary resources for streaming
  int MediaFoundationEncoder::sendStreamStartMessage() {
    if (!encoder_transform) {
      TRAE("no transform initialised");
      return -1;
    }

    HRESULT hr = encoder_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
    if (S_OK != hr) {
      TRAE("Failed to send start stream request");
      return -2;
    }

    hr = encoder_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL);
    if (S_OK != hr) {
      TRAE("Failed to send start stream request");
      return -3;
    }

    return 0;
  }

  int MediaFoundationEncoder::drain() {
    int r = 0;
    DWORD dwStatus = 0;
    MFT_OUTPUT_DATA_BUFFER mft_output_data = {0};
    if (!encoder_transform) {
      TRAE("no transform initialised");
      return -1;
    }

    HRESULT hr = encoder_transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL);
    if (S_OK != hr) {
      TRAE("Failed to send end stream request");
      return -2;
    }

    // Set the encoded_data sample
    mft_output_data.dwStreamID = stream_output_id;
    mft_output_data.dwStatus = 0;
    mft_output_data.pEvents = NULL;
    mft_output_data.pSample = output_sample;

do{
    hr = encoder_transform->ProcessOutput(0, 1, &mft_output_data, &dwStatus);
    if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr) break;
    if (S_OK != hr && MF_E_TRANSFORM_STREAM_CHANGE != hr && MF_E_TRANSFORM_NEED_MORE_INPUT != hr) {
      TRAE("failed to process output");
      r = -10;
      goto error;
    }

    if (MF_E_TRANSFORM_STREAM_CHANGE == hr) {
      IMFMediaType *mediatype = NULL;
      hr = encoder_transform->GetOutputAvailableType(stream_output_id, 0, &mediatype);
      if (S_OK != hr) {
        TRAE("failed to handle MF_E_TRANSFORM_STREAM_CHANGE");
        r = -10;
        goto error;
      }

      hr = encoder_transform->SetOutputType(stream_output_id, mediatype, 0);
      if (S_OK != hr) {
        TRAE("failed to set output type");
        r = -10;
        goto error;
      }
    }

    hr = getOutput();
    if (S_OK != hr) {
      TRAE("failed to output encoded sample");
      r = -10;
      goto error;
    }
  }while(MF_E_TRANSFORM_NEED_MORE_INPUT != hr);
    error:

    return r;
  }

  // Send request to MFT to de-allocate necessary resources for streaming
  int MediaFoundationEncoder::sendStreamEndMessage() {
    if (!encoder_transform) {
      TRAE("no transform initialised");
      return -1;
    }

    HRESULT hr = encoder_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
    if (S_OK != hr) {
      TRAE("Failed to send end stream request");
      return -2;
    }

    return 0;
  }

  // Create a new sample for mft input from segemtation image buffer and pass in the timestamp
  int MediaFoundationEncoder::addSample(tra_memory_image *data) {

    BYTE *pData = NULL;

    HRESULT hr = S_OK;
    int r = 0;

    if (NULL == encoder_transform) {
      TRAE("transform not created");
      r = -10;
      goto error;
    }

    if(NULL == data){
      TRAE("no data");
      r = -10;
      goto error;
    }

    r = createInputSample();
    if (0 > r) {
      TRAE("failed to create input sample");
      goto error;
    }

    // Lock the buffer and copy the video frame to the buffer.
    hr = input_buffer->Lock(&pData, NULL, NULL);
    if (S_OK != hr) {
      TRAE("failed to lock input buffer");
      r = -10;
      goto error;
    }

    for (int i = 0; i < data->plane_count; ++i) {

      hr = MFCopyImage(pData,                  // Destination buffer.
                       settings->image_width,  // Destination stride.
                       data->plane_data[i],    // First row in source image.
                       data->plane_strides[i], // Source stride.
                       data->plane_strides[i], // Image width in bytes.
                       data->plane_heights[i]  // Image height in pixels.
      );
      if (S_OK != hr) {
        TRAE("failed to copy input data");
        r = -10;
        goto error;
      }

      pData += (data->plane_strides[i] * data->plane_heights[i]);
    }

    // Set the data length of the buffer.
    hr = input_buffer->SetCurrentLength(*image_size);
    if (S_OK != hr) {
      TRAE("failed to set CurrentLength of input buffer");
      r = -10;
      goto error;
    }

    hr = input_buffer->Unlock();
    if (S_OK != hr) {
      TRAE("failed to unlock input data");
      r = -10;
      goto error;
    }

    /* @todo Update our temp vars; we should get those from `inSample`. */
    timestamp = tmp_frame_num * 1e9 * settings->fps_den / settings->fps_num; /* In nanos */
    tmp_frame_num = tmp_frame_num + 1;

    // Set the time stamp and the duration.
    hr = input_sample->SetSampleTime(timestamp);
    if (S_OK != hr) {
      TRAE("failed to set SampleTime of input buffer");
      r = -10;
      goto error;
    }

    hr = input_sample->SetSampleDuration((double)settings->fps_den / settings->fps_num);
    if (S_OK != hr) {
      TRAE("failed to set SampleDuration of input buffer");
      r = -10;
      goto error;
    }

  error:
    input_buffer->Unlock();
    return r;
  }

  int MediaFoundationEncoder::processSample() {

    HRESULT hr = S_OK;
    int r = 0;

    MFT_OUTPUT_DATA_BUFFER mft_output_data = {0};
    DWORD status = 0;
    DWORD dwStatus = 0;

    if (NULL == encoder_transform) {
      TRAE("transform not set");
      r = -10;
      goto error;
    }

    if (UINT32_MAX == stream_output_id) {
      TRAE("stream_output_id not set");
      r = -10;
      goto error;
    }

    if (UINT32_MAX == stream_input_id) {
      TRAE("stream_input_id not set");
      r = -10;
      goto error;
    }

    if (NULL == output_sample) {
      TRAE("output sample not created");
      r = -10;
      goto error;
    }

    // Set the encoded_data sample
    mft_output_data.dwStreamID = stream_output_id;
    mft_output_data.dwStatus = 0;
    mft_output_data.pEvents = NULL;
    mft_output_data.pSample = output_sample;


    do {
      hr = encoder_transform->ProcessInput(stream_input_id, input_sample, 0);
      if(S_OK == hr) {
        break;
      }

      if (S_OK != hr && MF_E_NOTACCEPTING != hr) {
        TRAE("failed to processInput");
        r = -10;
        goto error;
      }

      status = 0;
      hr = encoder_transform->GetOutputStatus(&status);
      if (S_OK != hr) {
        TRAE("failed to get output status");
        r = -10;
        goto error;
      }

      if (MFT_OUTPUT_STATUS_SAMPLE_READY != status) {
        continue;
      }

      while (MF_E_TRANSFORM_NEED_MORE_INPUT != hr) {
        // Generate the encoded_data sample
        hr = encoder_transform->ProcessOutput(0, 1, &mft_output_data, &dwStatus);
        if(MF_E_TRANSFORM_NEED_MORE_INPUT == hr) {
          break;
        }

        if (S_OK != hr && MF_E_TRANSFORM_NEED_MORE_INPUT != hr && MF_E_TRANSFORM_STREAM_CHANGE != hr) {
          TRAE("failed to process output");
          r = -10;
          goto error;
        }

        if (MF_E_TRANSFORM_STREAM_CHANGE == hr) {
          IMFMediaType *mediatype = NULL;

          hr = encoder_transform->GetOutputAvailableType(stream_output_id, 0, &mediatype);
          if (S_OK != hr) {
            TRAE("failed to handle MF_E_TRANSFORM_STREAM_CHANGE");
            r = -10;
            goto error;
          }

          hr = encoder_transform->SetOutputType(stream_output_id, mediatype, 0);
          if (S_OK != hr) {
            TRAE("failed to set output type");
            r = -10;
            goto error;
          }
        }

        hr = getOutput();
        if (S_OK != hr) {
          TRAE("failed to output encoded sample");
          r = -10;
          goto error;
        }

      }
    } while (hr == MF_E_TRANSFORM_NEED_MORE_INPUT);

  error:

    return r;
  }

  int MediaFoundationEncoder::getOutput() {
    HRESULT hr = S_OK;
    int r = 0;

    tra_memory_h264 encoded_data;
    byte *output_buffer_data_ptr = NULL;
    IMFMediaBuffer *output_buffer = NULL;
    LONGLONG duration = 0;
    LONGLONG time = 0;
    DWORD output_buffer_data_length = 0;
    DWORD max_buffer_length = 0;
    DWORD* buffer_count = NULL;

    if(NULL == output_sample){
      TRAE("no output sample created");
      r = -10;
      goto error;
    }

    if(NULL == settings){
      TRAE("no settings available");
      r = -10;
      goto error;
    }

    if(NULL == settings->callbacks.user){
      TRAE("no callback user available");
      r = -10;
      goto error;
    }

    if(NULL == settings->callbacks.on_encoded_data){
      TRAE("no callback available");
      r = -10;
      goto error;
    }


    hr = output_sample->GetSampleDuration(&duration);
    if (S_OK != hr) {
      TRAE("failed to get SampleDuration");
      r = -10;
      goto error;
    }

    hr = output_sample->GetSampleTime(&time);
    if (S_OK != hr) {
      TRAE("failed to get SampleTime");
      r = -10;
      goto error;
    }

    buffer_count = (DWORD*)calloc(1, sizeof(DWORD));
    hr = output_sample->GetBufferCount(buffer_count);
    if (S_OK != hr){
      TRAE("failed to retreive output buffer count");
      r = -10;
      goto error;
    }

    for(DWORD i = 0; i < *buffer_count; i++) {
      hr = output_sample->GetBufferByIndex(i, &output_buffer);
      if (S_OK != hr) {
        TRAE("failed to retreive output buffer");
        r = -10;
        goto error;
      }

      hr = output_buffer->GetCurrentLength(&output_buffer_data_length);
      if (S_OK != hr) {
        TRAE("failed to get CurrentLength");
        r = -10;
        goto error;
      }

      hr = output_buffer->GetMaxLength(&max_buffer_length);
      if (S_OK != hr) {
        TRAE("failed to get MaxLength");
        r = -10;
        goto error;
      }

      hr = output_buffer->Lock(&output_buffer_data_ptr, &max_buffer_length, &output_buffer_data_length);
      if (S_OK != hr) {
        TRAE("failed to lock encoded Buffer");
        r = -10;
        goto error;
      }

      encoded_data.data = output_buffer_data_ptr;
      encoded_data.size = output_buffer_data_length;
      encoded_data.flags = 0;


      r = settings->callbacks.on_encoded_data(TRA_MEMORY_TYPE_H264, &encoded_data,
                                              settings->callbacks.user);
      hr = output_buffer->Unlock();
    }

  error:

    if (S_OK != hr) {
      hr = output_buffer->Unlock();
      TRAE("Failed to unlock the encoded_data buffer.");
      r = -90;
    }

    return r;
  }

  int MediaFoundationEncoder::encode(tra_memory_image *data) {

    int r = 0;
    HRESULT hr = S_OK;

    if (NULL == data) {
      TRAE("Cannot encode as the given `data` is NULL.");
      r = -30;
      goto error;
    }

    if (NULL == settings) {
      TRAE("Cannot encode as the given `settings` is NULL.");
      r = -30;
      goto error;
    }

    if (NULL == data->image_width) {
      TRAE("Cannot encode as the given `tra_memory_image::image_width` is not set.");
      r = -50;
      goto error;
    }

    if (NULL == settings->image_width) {
      TRAE("Cannot encode as the given `settings.image_width` is not set.");
      r = -50;
      goto error;
    }

    if (data->image_width != settings->image_width) {
      TRAE("Cannot encode as the given `tra_memory_image::image_width` is not the same as the "
           "settings passed into `init()`.");
      r = -50;
      goto error;
    }

    if (NULL == data->image_height) {
      TRAE("Cannot encode as the given `tra_memory_image::image_height` is not set.");
      r = -50;
      goto error;
    }

    if (NULL == settings->image_height) {
      TRAE("Cannot encode as the given `settings.image_height` is not set.");
      r = -50;
      goto error;
    }

    if (data->image_height != settings->image_height) {
      TRAE("Cannot encode as the given `tra_memory_image::image_height` is not the same as the "
           "settings passed into `init()`.");
      r = -60;
      goto error;
    }

    // Add a sample with the frame timestamp
    hr = addSample(data);
    if (S_OK != hr) {
      TRAE("failed to input encode data");
      r = -10;
      goto error;
    }

    hr = processSample();
    if (S_OK != hr) {
      TRAE("failed to process data");
      r = -10;
      goto error;
    }

  error:

    return r;
  }

  int MediaFoundationEncoder::flush() {
    if (!encoder_transform) { return -1; }

    HRESULT hr = encoder_transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
    if (S_OK != hr) {
      TRAE("Failed to send end stream request");
      return -2;
    }

    return 0;
  }

} // namespace tra
