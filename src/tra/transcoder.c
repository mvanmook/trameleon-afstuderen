#include <tra/transcoder.h>
#include <tra/log.h>

int on_decoded_callback(uint32_t type, void* data,  void* user){
  int r = 0;
  tra_transcoder* transcoder = NULL;

  if (NULL == data){
    TRAE("no data returned in callback");
  }

  if(NULL == user){
    TRAE("unable to send data to encoder")
  }

  transcoder = user;
  r = transcoder->enc_api->encode(transcoder->enc_obj, type, data);
  if(0 > r){
    TRAE("failed to encode in transcode");
  }

  return r;
}


int makeSettingsValid(tra_transcoder_settings* settings, tra_transcoder* object) {
  int r = 0;
  if(NULL == settings){
    TRAE("transcoder settings is NULL");
    r = -1;
    goto error;
  }
  if(NULL == settings->dec_settings){
    TRAE("transcoder decoder settings is NULL");
    r = -1;
    goto error;
  }
  if(NULL == settings->enc_settings){
    TRAE("transcoder encoder settings is NULL");
    r = -1;
    goto error;
  }
  if(NULL == object){
    TRAE("transcoder settings is NULL");
    r = -1;
    goto error;
  }
  if(NULL == object->dec_obj){
    TRAE("transcoder decoder object is NULL");
    r = -1;
    goto error;
  }
  if(NULL == object->enc_obj){
    TRAE("transcoder encoder object is NULL");
    r = -1;
    goto error;
  }
  settings->dec_settings->callbacks.on_decoded_data = on_decoded_callback;

error:
  return r;
}




int tra_combinded_transcoder_create(tra_encoder_api* encoder_api, tra_decoder_api* decoder_api, tra_transcoder_settings *cfg,
                                 trans_settings* settings,
                                 tra_transcoder **obj) {
  int r = 0;
  tra_transcoder* inst = NULL;

  if (NULL == encoder_api) {
    TRAE("Cannot create a transcoder instance. Given `tra_encoder_api*` is NULL.");
    return -1;
  }

  if (NULL == decoder_api) {
    TRAE("Cannot create a transcoder instance. Given `tra_decoder_api*` is NULL.");
    return -1;
  }

  //check correct input
  if(NULL == obj){
    r = -1;
    TRAE("transcode object doesn't exist");
    goto error;
  }

  if(NULL == (*obj)->dec_obj){
    r = -1;
    TRAE("transcode decode object doesn't exist");
    goto error;
  }

  if(NULL == (*obj)->enc_obj){
    r = -1;
    TRAE("transcode decode object doesn't exist");
    goto error;
  }

  if(NULL == cfg){
    r = -1;
    TRAE("transcode settings object doesn't exist");
    goto error;
  }

  if(NULL == cfg->dec_settings){
    r = -1;
    TRAE("transcode settings decode object doesn't exist");
    goto error;
  }

  if(NULL == cfg->enc_settings){
    r = -1;
    TRAE("transcode settings encode object doesn't exist");
    goto error;
  }

  if (NULL == inst) {
    TRAE("Cannot create the encoder, failed to allocate the `tra_encoder`.");
    return -4;
  }

  inst->dec_api = decoder_api;
  inst->enc_api = encoder_api;

  r = makeSettingsValid(cfg, inst);
  if(0 > r){
    TRAE("invalid transcode settings");
    goto error;
  }

  //create decoder
  r = inst->dec_api->create(cfg->dec_settings, settings->dec_settings, &inst->dec_obj);
  if(0 > r){
    TRAE("unable to create decoder");
    goto error;
  }

  //create encoder
  r = inst->enc_api->create(cfg->enc_settings, settings->enc_settings, &inst->enc_obj);
  if(0 > r){
    TRAE("unable to create encoder");
    goto error;
  }

  *obj = inst;

error:
  if(0 > r){
    //destroy decoder
    inst->dec_api->destroy(inst->dec_obj);
    //destroy encoder
    inst->enc_api->destroy(inst->enc_obj);
  }

  return r;
}

int tra_combinded_transcoder_destroy(tra_transcoder *obj) {
  int r = 0;

  //check not already destroyed
  if(NULL == obj){
    r = -1;
    TRAE("transcode object doesn't exist");
    goto error;
  }

  if(NULL != obj->dec_obj){
    r = obj->dec_api->destroy(obj->dec_obj);
    if(0 > r){
      TRAE("FAILED to destroy decoder");
      goto error;
    }
    free(obj->dec_obj);
  }

  if(NULL != obj->enc_obj){
    r = obj->enc_api->destroy(obj->enc_obj);
    if(0 > r){
      TRAE("FAILED to destroy encoder");
      goto error;
    }
    free(obj->enc_obj);
  }

  free(obj);

error:

  return r;
}

int tra_transcoder_transcode(tra_transcoder *obj, uint32_t type,
                                    void *data) {
  int r = 0;
  //check correct input
  if(NULL == obj){
    r = -1;
    TRAE("transcode object doesn't exist");
    goto error;
  }

  if(NULL == obj->dec_obj){
    r = -1;
    TRAE("transcode decode object doesn't exist");
    goto error;
  }

  if(NULL == obj->enc_obj){
    r = -1;
    TRAE("transcode decode object doesn't exist");
    goto error;
  }

  if(NULL == data){
    r = -1;
    TRAE("transcode data doesn't exist");
    goto error;
  }

  //decode will automatically encode due to make Valid functions
  r = obj->dec_api->decode(obj->dec_obj,type, data);
  if(0 > r){
    TRAE("failed to transcode");
    goto error;
  }


error:

  return r;
}


