/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except
// in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0
// //// Unless required by applicable law or agreed to in writing, software// distributed under the
// License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
// express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _ENCODE_TRANSFORM_H_
#define _ENCODE_TRANSFORM_H_

#include "MediaFoundationUtils.h"
#include "tra/module.h"
#include "tra/types.h"
#include <Propvarutil.h>
#include <cstdint>
#include <mfapi.h>
#include <mftransform.h>

namespace tra {
  struct EncoderOutput {
    byte *pEncodedData;
    DWORD numBytes;
    LONGLONG timestamp;
    LONGLONG duration;
    HRESULT returnCode;
  } typedef EncoderOutput;

  class MediaFoundationEncoder {
  public:
    /**
     * initiate required variables
     * @returns <0 if an error occurs otherwise 0
     */
    int init(tra_encoder_settings *cfg);

    /**
     * encode @param data
     * @returns <0 if an error occurs otherwise 0
     */
    int encode(tra_memory_image *data);

    /**
     * destroy all variables
     * @returns <0 if an error occurs otherwise 0
     */
    int shutdown();

    /**
     * flush the transform
     * @returns <0 if an error occurs otherwise 0
     */
    int flush();

    /**
     * drain the transform
     * @returns <0 if an error occurs otherwise 0
     */
    int drain();

    /**
     * create input media type
     * @returns <0 if an error occurs otherwise 0
     */
    static int createInputMediaType(IMFMediaType **input_media_type, tra_encoder_settings &settings, IMFTransform* encoder_transform, uint32_t  stream_input_id);

    /**
     * create output media type
     * @returns <0 if an error occurs otherwise 0
     */
    static int createOutputMediaType(IMFMediaType **output_media_type, tra_encoder_settings &settings);

    /**
     * initiate transform with correct encoder
     * @returns <0 if an error occurs otherwise 0
     */
    static int findEncoder(const GUID &inputSubtype, const GUID &outputSubtype, IMFTransform**encoderTransform);

  private:
    /**
     * create input buffers
     * @returns <0 if an error occurs otherwise 0
     */
    int createInputSample();

    /**
     * create output buffers
     * @returns <0 if an error occurs otherwise 0
     */
    int createOutputSample();

    /**
     * load stream ids into variables
     * @returns <0 if an error occurs otherwise 0
     */
    int queryStreamCapabilities();

    /**
     * checks if mft uses recounts for buffers
     * @return <0 if an error occurs otherwise 0 for false and 1 for true
     */
    int doesInputStreamUseRefCounts();

    /**
     * set input type
     * @returns <0 if an error occurs otherwise 0
     */
    int setInputMediaType();

    /**
     * set output type
     * @returns <0 if an error occurs otherwise 0
     */
    int setOutputMediaType();

    /**
     * load input stream info into variables
     * @returns <0 if an error occurs otherwise 0
     */
    int queryInputStreamInfo();

    /**
     * load output stream info into variables
     * @returns <0 if an error occurs otherwise 0
     */
    int queryOutputStreamInfo();

    /**
     * processes the data through the transform
     * @returns <0 if an error occurs otherwise 0
     */
    int processSample();

    /**
     * this function retrieves the data from the transform and calls callback in settings
     * @returns <0 if an error occurs otherwise 0
     */
    int getOutput();

    /**
     * this function inputs the data into the transform
     * @returns <0 if an error occurs otherwise 0
     */
    int addSample(tra_memory_image *data);

    /**
     * sends the start of stream message
     * @returns <0 if an error occurs otherwise 0
     */
    int sendStreamStartMessage();

    /**
     * sends the end of stream message
     * @returns <0 if an error occurs otherwise 0
     */
    int sendStreamEndMessage();

    IMFTransform *encoder_transform = NULL; // pointer to the encoder MFT

    MFT_INPUT_STREAM_INFO input_stream_info;
    MFT_OUTPUT_STREAM_INFO output_stream_info;

    DWORD stream_input_id = UINT32_MAX;
    DWORD stream_output_id = UINT32_MAX;

    // Store stream limit info to determine pipeline capabailities
    DWORD inputStreamMin = 0;
    DWORD inputStreamMax = 0;
    DWORD outputStreamMin = 0;
    DWORD outputStreamMax = 0;

    uint32_t *image_size = NULL;

    LONGLONG timestamp = 0;
    IMFMediaBuffer *input_buffer = NULL;
    IMFSample *input_sample = NULL;
    IMFSample *output_sample = NULL;
    tra_encoder_settings *settings;

    uint32_t tmp_frame_num = 0;
    bool input_uses_refcount = false;
  };

} // namespace tra
#endif // _ENCODE_TRANSFORM_H_
