
#include "tra/modules/mft/MediaFoundationDecoder.h"
#include "tra/log.h"
#include <mfapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>

namespace tra {

  int MediaFoundationDecoder::shutdown() {
    sendStreamEndMessage();

    Release(&decoder_transform);

    return 0;
  }

  int MediaFoundationDecoder::findDecoder(const GUID &input_type, const GUID &output_type) {

    HRESULT hr = S_OK;
    int r = 0;
    UINT32 count = 0;

    CLSID *ppCLSIDs = NULL;

    MFT_REGISTER_TYPE_INFO input_info = {0};
    MFT_REGISTER_TYPE_INFO output_info = {0};

    input_info.guidMajorType = MFMediaType_Video; //@todo do we need audio
    input_info.guidSubtype = input_type;

    output_info.guidMajorType = MFMediaType_Video; //@todo do we need audio
    output_info.guidSubtype = output_type;

    hr = MFTEnum(MFT_CATEGORY_VIDEO_DECODER, 0, &input_info, &output_info, NULL, &ppCLSIDs, &count);

    if (S_OK != hr || 0 == count) {
      TRAE("failed to find codec");
      r = -10;
      goto error;
    }

    hr = CoCreateInstance(ppCLSIDs[0], NULL, CLSCTX_INPROC_SERVER, IID_IMFTransform, (void **)&decoder_transform);
    if (S_OK != hr) {
      TRAE("failed to initiate codec");
      r = -10;
      goto error;
    }

  error:
    CoTaskMemFree(ppCLSIDs);

    return r;
  }

  // Finds and initializes decoder MFT
  int MediaFoundationDecoder::init(tra_decoder_settings &cfg) {
    settings = cfg;
    HRESULT hr = S_OK;
    int r;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    MFStartup(MF_VERSION);

    GUID input_format;
    GUID output_format;

    r = convert_trameleon_to_mft_image_format(settings.input_format, input_format);
    if (0 > r) {
      TRAE("can't convert trameleon image format to mft format");
      r = -10;
      goto error;
    }

    r = convert_trameleon_to_mft_image_format(settings.output_format, output_format);
    if (0 > r) {
      TRAE("can't convert trameleon image format to mft format");
      r = -10;
      goto error;
    }

    hr = findDecoder(input_format, output_format);
    if (S_OK != hr) {
      TRAE("failed to get decoder");
      r = -10;
      goto error;
    }

    hr = queryStreamCapabilities();
    if (S_OK != hr) {
      TRAE("Failed to GetStreamIDs for MFT");
      r = -10;
      goto error;
    }

    // configure the decoder input media type
    hr = setInputMediaType();
    if (S_OK != hr) {
      TRAE("failed to set input type");
      r = -10;
      goto error;
    }

    // configure the decoder output media type
    hr = setOutputMediaType();
    if (S_OK != hr) {
      TRAE("failed to set output type");
      r = -10;
      goto error;
    }

    r = enableHardwareDecoding();
    if (0 > r) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to enable hardware "
           "decoding.");
      r = -10;
      goto error;
    }

    r = enableLowLatencyDecoding();
    if (0 > r) {
      TRAE("Cannot initialize the `MediaFoundationDecoder` as we failed to enable low latency "
           "decoding.");
      r = -10;
      goto error;
    }

  error:

    return r;
  }

  HRESULT MediaFoundationDecoder::createInputMediaType(IMFMediaType **input_media_type,
                                                          tra_decoder_settings &settings) {
    int r = 0;
    HRESULT hr = S_OK;
    GUID mf_image_format;
    IMFMediaType *mt = NULL;

    hr = MFCreateMediaType(&mt);
    if (S_OK != hr) {
      TRAE("Failed to get IMFMediaType interface.\n");
      r = -10;
      goto error;
    }

    hr = mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (S_OK != hr) {
      TRAE("Failed to Set IMFMediaType major type attribute.\n");
      r = -10;
      goto error;
    }

    r = convert_trameleon_to_mft_image_format(settings.input_format, mf_image_format);
    if (0 > r) {
      TRAE("Failed to convert image format to mft.\n");
      r = -10;
      goto error;
    }

    hr = mt->SetGUID(MF_MT_SUBTYPE, mf_image_format);
    if (S_OK != hr) {
      TRAE("Failed to Set IMFMediaType subtype type attribute.\n");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeSize(mt, MF_MT_FRAME_SIZE, settings.image_width, settings.image_height);
    if (S_OK != hr) {
      TRAE("Failed to set frame size on H264 MFT in type.\n");
      r = -10;
      goto error;
    }

    *input_media_type = mt;

  error:

    return hr;
  }

  /// Set the input attributes for the decoder MFT
  int MediaFoundationDecoder::setInputMediaType() {
    // Set the input media type for the decoder
    IMFMediaType *input_media_type = NULL;
    int r = 0;
    HRESULT hr = S_OK;

    if (NULL == decoder_transform) {
      TRAE("transform not set");
      r = -10;
      goto error;
    }

    hr = createInputMediaType(&input_media_type, settings);
    if(S_OK != hr){
      TRAE("Failed to create input media type");
      r = -10;
      goto error;
    }

    hr = decoder_transform->SetInputType(stream_input_id, input_media_type, 0);
    if (S_OK != hr) {
      TRAE("Failed to set input media type on H.264 decoder MFT.\n");
      r = -10;
      goto error;
    }

  error:

    return r;
  }

  HRESULT MediaFoundationDecoder::createOutputMediaType(IMFMediaType *output_media_type,
                                                           tra_decoder_settings &settings) {
    int r = 0;
    HRESULT hr = S_OK;
    GUID mf_image_format;

    hr = output_media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); //@todo do we need audio?
    if (S_OK != hr) {
      TRAE("Failed to set MF_MT_MAJOR_TYPE.\n");
      r = -10;
      goto error;
    }

    r = convert_trameleon_to_mft_image_format(settings.output_format,mf_image_format);
    if (0 > r) {
      TRAE("Failed to convert image format to mft.\n");
      r = -10;
      goto error;
    }

    hr = output_media_type->SetGUID(MF_MT_SUBTYPE, mf_image_format);
    if (S_OK != hr) {
      TRAE("Failed to set MF_MT_SUBTYPE.\n");
      r = -10;
      goto error;
    }

    hr = output_media_type->SetUINT32(MF_MT_AVG_BITRATE, settings.bitrate);
    if (S_OK != hr) {
      TRAE("Failed to set MF_MT_AVG_BITRATE.\n");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeSize(output_media_type, MF_MT_FRAME_SIZE, settings.image_width, settings.image_height);
    if (S_OK != hr) {
      TRAE("Failed to set frame size on H264 MFT out type.\n");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeRatio(output_media_type, MF_MT_FRAME_RATE, settings.fps_num, settings.fps_den);
    if (S_OK != hr) {
      TRAE("Failed to set frame rate on H264 MFT out type.\n");
      r = -10;
      goto error;
    }

    hr = MFSetAttributeRatio(output_media_type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (S_OK != hr) {
      TRAE("Failed to set aspect ratio on H264 MFT out type.\n");
      r = -10;
      goto error;
    }

    hr = output_media_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (S_OK != hr) {
      TRAE("Failed to set MF_MT_INTERLACE_MODE.\n");
      r = -10;
      goto error;
    }

  error:

    return hr;
  }

  // set the output attributes decoder MFT
  int MediaFoundationDecoder::setOutputMediaType() {
    IMFMediaType *output_madia_type = NULL;
    HRESULT hr = S_OK;
    int r = 0;

    if (NULL == decoder_transform) {
      TRAE("transform not set");
      r = -10;
      goto error;
    }

    hr = decoder_transform->GetOutputAvailableType(stream_output_id, 0, &output_madia_type);
    if (S_OK != hr) {
      TRAE("Cannot set the output media type as we failed to retrieve an available media type.");
      r = -10;
      goto error;
    }

    hr = createOutputMediaType(output_madia_type, settings);
    if (S_OK != hr) {
      TRAE("Failed to create decoder output type")
      r = -10;
      goto error;
    }

    hr = decoder_transform->SetOutputType(stream_output_id, output_madia_type, 0);
    if (S_OK != hr) {
      TRAE("Failed to set output media type on H.264 decoder MFT.\n");
      r = -10;
      goto error;
    }

  error:

    return r;
  }

  // reconstructs the sample from encoded data
  int MediaFoundationDecoder::decode(tra_memory_h264 *data, LONGLONG timestamp, LONGLONG &duration) {

    IMFSample *input_sample = NULL;
    IMFMediaBuffer *input_buffer = NULL;
    BYTE *pData = NULL;
    DWORD media_buffer_curr_length = 0;
    DWORD media_buffer_max_length = 0;
    int r = 0;
    HRESULT hr = S_OK;

    if (NULL == data) {
      TRAE("input not given");
      r = -10;
      goto error;
    }

    // Create a new memory buffer.
    hr = MFCreateMemoryBuffer(data->size, &input_buffer);
    if (S_OK != hr) {
      TRAE("Failed to create Memory Buffer");
      r = -10;
      goto error;
    }

    // Lock the buffer and copy the video frame to the buffer.
    hr = input_buffer->Lock(&pData, &media_buffer_max_length, &media_buffer_curr_length);
    if (S_OK != hr) {
      TRAE("Failed to lock Memory Buffer");
      r = -10;
      goto error;
    }

    memcpy(pData, data->data, data->size);

    input_buffer->SetCurrentLength(data->size);
    input_buffer->Unlock();

    // Create a media sample and add the buffer to the sample.
    hr = MFCreateSample(&input_sample);
    if (S_OK != hr) {
      TRAE("Failed to create sample");
      r = -10;
      goto error;
    }

    hr = input_sample->AddBuffer(input_buffer);
    if (S_OK != hr) {
      TRAE("Failed to add Memory Buffer to sample");
      r = -10;
      goto error;
    }

    // Set the time stamp and the duration.
    hr = input_sample->SetSampleTime(timestamp);
    if (S_OK != hr) {
      TRAE("Failed to set sample time");
      r = -10;
      goto error;
    }

    hr = input_sample->SetSampleDuration(duration);
    if (S_OK != hr) {
      TRAE("Failed to set sample durration");
      r = -10;
      goto error;
    }

    hr = createOutputSample();
    if(S_OK != hr){
      TRAE("failed to create output sample");
      r = -10;
      goto error;
    }

    hr = processSample(&input_sample, timestamp, duration);
    if (S_OK != hr) {
      TRAE("Failed to process sample");
      r = -10;
      goto error;
    }

  error:
    Release(&input_sample);
    Release(&output_sample);

    return r;
  }

  // Query stream limits to determine pipeline characteristics...I.E fix size stream and Stream ID
  int MediaFoundationDecoder::queryStreamCapabilities() {
    HRESULT hr = S_OK;
    int r = 0;
    // Store stream limit info to determine pipeline capabailities
    DWORD mInputStreamMin = 0;
    DWORD mInputStreamMax = 0;
    DWORD mOutputStreamMin = 0;
    DWORD mOutputStreamMax = 0;

    if (NULL == decoder_transform) {
      TRAE("transform not set");
      r = -10;
      goto error;
    }

    // if the min and max input and output stream counts are the same, the MFT is a Fix stream size. So we cannot add or remove streams
    hr = decoder_transform->GetStreamLimits(&mInputStreamMin, &mInputStreamMax, &mOutputStreamMin, &mOutputStreamMax);
    if (S_OK != hr) {
      TRAE("Failed to GetStreamLimits for MFT");
      r = -10;
      goto error;
    }

    hr = decoder_transform->GetStreamIDs(mInputStreamMin, &stream_input_id, mOutputStreamMin, &stream_output_id);
    if (E_NOTIMPL == hr) {
      stream_input_id = 0;
      stream_output_id = 0;
      hr = S_OK;
    }

    if (S_OK != hr) {
      TRAE("Failed to GetStreamIDs for MFT");
      r = -10;
      goto error;
    }

  error:
    return r;
  }

  // Process the incoming sample
  int MediaFoundationDecoder::processSample(IMFSample **input_sample, LONGLONG &time, LONGLONG &duration) {
    DWORD buffer_count = 0;
    MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
    int r = 0;
    HRESULT hr = S_OK;

    if (NULL == output_sample) {
      TRAE("input sample not set");
      r = -10;
      goto error;
    }

    if (NULL == *input_sample) {
      TRAE("input sample not set");
      r = -10;
      goto error;
    }

    if (NULL == decoder_transform) {
      TRAE("transform not set");
      r = -10;
      goto error;
    }

    // Set the encoded_data sample
    output_data_buffer.dwStreamID = stream_output_id;
    output_data_buffer.dwStatus = 0;
    output_data_buffer.pEvents = NULL;
    output_data_buffer.pSample = output_sample;

    do {
      hr = decoder_transform->ProcessInput(stream_input_id, *input_sample, 0);
      if (S_OK != hr && MF_E_NOTACCEPTING != hr) {
        TRAE("failed to process input");
        r = -10;
        goto error;
      }

      hr = output_sample->GetBufferCount(&buffer_count);
      if(S_OK != hr){
        TRAE("failed to get ouput buffer count");
        r = -10;
        goto error;
      }

      // Generate the encoded_data sample
      DWORD test = 0;
      hr = decoder_transform->ProcessOutput(0, buffer_count, &output_data_buffer, &test);

      if (S_OK != hr && MF_E_TRANSFORM_NEED_MORE_INPUT != hr && MF_E_TRANSFORM_STREAM_CHANGE != hr) {
        TRAE("failed to process output");
        r = -10;
        goto error;
      }


      if (S_OK == hr) {
        // get decoded data
        hr = getOutput(output_data_buffer, time, duration);
        if (S_OK != hr) {
          TRAE("failed to output decoded sample");
          r = -10;
          goto error;
        }
        continue;
      }

      if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr) continue;

      // handle MF_E_TRANSFORM_STREAM_CHANGE
      r = queryStreamCapabilities();
      if (0 > r) {
        TRAE("failed to get streamids");
        r = -10;
        goto error;
      }

      IMFMediaType *mediatype = NULL;
      hr = decoder_transform->GetOutputAvailableType(stream_output_id, 0, &mediatype);
      if (S_OK != hr) {
        TRAE("failed to handle MF_E_TRANSFORM_STREAM_CHANGE");
        r = -10;
        goto error;
      }

      hr = decoder_transform->SetOutputType(stream_output_id, mediatype, 0);
      if (S_OK != hr) {
        TRAE("failed to set output type after stream change");
        r = -10;
        goto error;
      }

    } while (hr == MF_E_TRANSFORM_NEED_MORE_INPUT);

  error:

    return r;
  }

  int MediaFoundationDecoder::createOutputSample() {
    bool output_provides_buffers = true;
    IMFMediaBuffer *output_buffer = NULL;
    HRESULT hr = S_OK;
    int r = 0;

    if(NULL != output_sample){
      TRAE("output sample already created");
      r = -10;
      goto error;
    }

    // Flag to specify that the MFT provides the output sample for this stream (thought internal allocation or operating on the input stream)
    if (NULL == &output_stream_info) {
      TRAE("ouput_stream_info is not initialized");
      return -10;
    }
    output_provides_buffers = MFT_OUTPUT_STREAM_PROVIDES_SAMPLES == output_stream_info.dwFlags || MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES == output_stream_info.dwFlags;

    if(output_provides_buffers){
      output_sample = NULL;
      return 0;
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

  // Gets the buffer requirements and other information for the output stream of the MFT
  int MediaFoundationDecoder::queryOutputStreamInfo() {
    if (!decoder_transform) {
      TRAE("no transform initialised");
      return -1;
    }

    HRESULT hr = decoder_transform->GetOutputStreamInfo(stream_output_id, &output_stream_info);

    if (S_OK != hr) {
      TRAE("Failed to query output stream info");
      return -2;
    }

    return 0;
  }

  // Write the decoded sample out to a file
  int MediaFoundationDecoder::getOutput(MFT_OUTPUT_DATA_BUFFER &outputDataBuffer,
                                               LONGLONG &time, LONGLONG &duration) {
    TRAD("retrieving decoded data");
    DWORD buffer_length;
    tra_memory_image decoded_image;
    IMFMediaBuffer* output_buffer = NULL;
    DWORD output_buffer_max_length = 0;
    DWORD buffer_count = 0;
    byte *output_buffer_data_ptr = NULL;
    int r = 0;
    HRESULT hr = S_OK;

    hr = outputDataBuffer.pSample->SetSampleTime(time);
    if (S_OK != hr) {
      TRAE("cannot get timestamp");
      r = -10;
      goto error;
    }
    hr = outputDataBuffer.pSample->SetSampleDuration(duration);
    if (S_OK != hr) {
      TRAE("cannot get sample duration");
      r = -10;
      goto error;
    }

    hr = output_sample->ConvertToContiguousBuffer(&output_buffer);
    if (S_OK != hr) {
      TRAE("cannot convert decoded buffer to contiguous buffer");
      r = -10;
      goto error;
    }

    hr = output_buffer->GetCurrentLength(&buffer_length);
    if (S_OK != hr) {
      TRAE("cannot get length of decoded buffer");
      r = -10;
      goto error;
    }

    hr = output_sample->GetBufferCount(&buffer_count);
    if(S_OK != hr){
      TRAE("failed to get ouput buffer count");
      r = -10;
      goto error;
    }

    for(int i = 0; i < buffer_count; i++) {
      output_sample->GetBufferByIndex(i, &output_buffer);

      output_buffer->Lock(&output_buffer_data_ptr, &output_buffer_max_length, &buffer_length);

      decoded_image.image_format = settings.output_format;
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

      settings.callbacks.on_decoded_data(TRA_MEMORY_TYPE_IMAGE, &decoded_image, settings.callbacks.user);

      output_buffer->Unlock();
    }

    output_sample->RemoveAllBuffers();

  error:

    return r;
  }

  // Send request to MFT to allocate necessary resources for streaming
  int MediaFoundationDecoder::sendStreamStartMessage() {
    if (!decoder_transform) {
      TRAE("decoder has not been initialised");
      return -1;
    }

    HRESULT hr = decoder_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
    if (S_OK != hr) {
      TRAE("failed to send start message");
      return -2;
    }

    return 0;
  }

  // Send request to MFT to de-allocate necessary resources for streaming
  int MediaFoundationDecoder::sendStreamEndMessage() {
    if (!decoder_transform) {
      TRAE("decoder has not been initialised");
      return -1;
    }

    HRESULT hr = decoder_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
    if (S_OK != hr) {
      TRAE("failed to send end message");
      return -2;
    }

    return 0;
  }

  int MediaFoundationDecoder::enableLowLatencyDecoding() {

    IMFAttributes *attribs = NULL;
    HRESULT hr = S_OK;
    int r = 0;

    if (NULL == decoder_transform) {
      TRAE("Cannot enable low latency decoding; our `decoder_transform` member is NULL. Did you "
           "call `init()`?");
      r = -1;
      goto error;
    }

    hr = decoder_transform->GetAttributes(&attribs);
    if (S_OK != hr) {
      TRAE("Cannot enable low latency decoding; we failed to get the attributes from the "
           "transform.");
      r = -2;
      goto error;
    }

    hr = attribs->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE);
    if (S_OK != hr) {
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

  int MediaFoundationDecoder::enableHardwareDecoding() {

    IMFAttributes *attribs = NULL;
    ICodecAPI *codec = NULL;
    HRESULT hr = S_OK;
    int r = 0;

    if (NULL == decoder_transform) {
      TRAE("Cannot enable hardware decoding as our `decoder_transform` is NULL.");
      r = -1;
      goto error;
    }

    hr = decoder_transform->QueryInterface(IID_ICodecAPI, (void **)&codec);
    if (S_OK != hr) {
      TRAE("Cannot enable hardware decoder as we failed to get the codec API.");
      r = -2;
      goto error;
    }

    /* Check if HW-accel is supported. */
    GUID hardAcc;
    r = convert_trameleon_to_mft_hardAcc(hardAcc, settings.input_format);
    if (0 > r) {
      TRAD("Hardware acceleration codec unavailable.");
      goto error;
    }

    hr = codec->IsSupported(&hardAcc);
    if (S_OK != hr) {
      TRAD("Hardware acceleration is not supported.");
      r = 0;
      goto error;
    }

    hr = codec->IsModifiable(&hardAcc);
    if (S_OK != hr) {
      TRAE("Hardware acceleration cannot be modified.");
      r = 0;
      goto error;
    }

    /* When supported ... try to set it. */
    hr = decoder_transform->GetAttributes(&attribs);
    if (S_OK != hr) {
      TRAE("Cannot enable hardware based decoding as we failed to get the transform attributes "
           "that we need to enable it.");
      r = -3;
      goto error;
    }

    hr = attribs->SetUINT32(hardAcc, TRUE);
    if (S_OK != hr) {
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
} // namespace tra
