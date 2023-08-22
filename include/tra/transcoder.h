#ifndef TRAMELEON_TRANSCODER_H
#define TRAMELEON_TRANSCODER_H
#include "module.h"


struct tra_transcoder_settings{
  tra_encoder_settings* enc_settings;
  tra_decoder_settings* dec_settings;
} typedef tra_transcoder_settings;

struct trans_settings{
  void* dec_settings;
  void* enc_settings;
} typedef trans_settings;

struct tra_transcoder {
  tra_encoder_api* enc_api;
  tra_encoder_object* enc_obj;
  tra_decoder_api* dec_api;
  tra_decoder_object* dec_obj;
  tra_transcoder_settings* settings;
};


int tra_combinded_transcoder_create(tra_encoder_api* encoder_api, tra_decoder_api* decoder_api, tra_transcoder_settings *cfg, trans_settings* settings, tra_transcoder **obj);
int tra_combinded_transcoder_destroy(tra_transcoder* obj);
int tra_combinded_transcoder_transcode(tra_transcoder* obj, uint32_t type, void* data);
int on_decoded_callback(uint32_t type, void* data,  void* user);


#endif // TRAMELEON_TRANSCODER_H
