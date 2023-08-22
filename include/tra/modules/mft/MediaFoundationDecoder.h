/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except
// in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0
// //// Unless required by applicable law or agreed to in writing, software// distributed under the
// License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
// express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _DECODE_TRANSFORM_H_
#define _DECODE_TRANSFORM_H_

#include "MediaFoundationUtils.h"
#include "tra/module.h"
#include "tra/types.h"
#include <Propvarutil.h>
#include <cstdint>
#include <mfobjects.h>
#include <mftransform.h>

namespace tra {
  struct DecoderOutput {
    byte *pDecodedData;
    DWORD numBytes;
    HRESULT returnCode;
  };

  class MediaFoundationDecoder {
  public:
    /**
     * decode @param data
     * @returns <0 if an error occurs otherwise 0
     */
    int decode(tra_memory_h264 *data, LONGLONG timestamp, LONGLONG &duration);

    /**
     * initiate transform with correct decoder
     * @param cfg settings for the decoder
     * @returns <0 if an error occurs otherwise 0
     */
    int init(tra_decoder_settings &cfg);

    /**
     * destroy all variables
     * @returns <0 if an error occurs otherwise 0
     */
    int shutdown();

    /**
     * create input media type
     * @returns <0 if an error occurs otherwise 0
     */
    static HRESULT createInputMediaType(IMFMediaType **input_media_type, tra_decoder_settings &settings);

    /**
     * create output media type
     * @returns <0 if an error occurs otherwise 0
     */
    static HRESULT createOutputMediaType(IMFMediaType *output_media_type, tra_decoder_settings &settings);

  private:
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
     * this function retrieves the data from the transform and calls callback in settings
     * @returns <0 if an error occurs otherwise 0
     */
    int getOutput(MFT_OUTPUT_DATA_BUFFER &outputDataBuffer, LONGLONG &time, LONGLONG &duration);

    /**
     * create output buffers
     * @returns <0 if an error occurs otherwise 0
     */
    int createOutputSample();

    /**
     * processes the data through the transform
     * @returns <0 if an error occurs otherwise 0
     */
    int processSample(IMFSample **output_sample, LONGLONG &time, LONGLONG &duration);

    /**
     * sends the end of stream message
     * @returns <0 if an error occurs otherwise 0
     */
    int sendStreamEndMessage();

    /**
     * sends the start of stream message
     * @returns <0 if an error occurs otherwise 0
     */
    int sendStreamStartMessage();

    /**
     * initiate low latency mode
     * @returns <0 if an error occurs otherwise 0
     */
    int enableLowLatencyDecoding();

    /**
     * initiate hardware decoding
     * @returns <0 if an error occurs otherwise 0
     */
    int enableHardwareDecoding();

    /**
     * load stream ids into variables
     * @returns <0 if an error occurs otherwise 0
     */
    int queryStreamCapabilities();

    /**
     * load output stream info into variables
     * @returns <0 if an error occurs otherwise 0
     */
    int queryOutputStreamInfo();

    /**
     * initiate transform with correct encoder
     * @returns <0 if an error occurs otherwise 0
     */
    int findDecoder(const GUID &subtype, const GUID &output_type);

    IMFTransform *decoder_transform = NULL;
    IMFSample *output_sample = NULL;
    IMFSample *input_sample = NULL;
    IMFMediaBuffer *buffer = NULL;
    DWORD stream_input_id = UINT32_MAX;
    DWORD stream_output_id = UINT32_MAX;
    MFT_OUTPUT_STREAM_INFO output_stream_info;
    tra_decoder_settings settings;
  };

} // namespace tra

#endif // _DECODE_TRANSFORM_H_
