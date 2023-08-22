#ifndef TRAMELEON_CAPABILITYHANDLER_H
#define TRAMELEON_CAPABILITYHANDLER_H


#include "tra/registry.h"
#include "module.h"
#include <stdint.h>

#define TYPE_UINT 0x0000
#define TYPE_STRING 0x0001


typedef struct tra_encoder_settings tra_encoder_settings;
typedef struct tra_decoder_settings tra_decoder_settings;



struct capability{
  const char* name;
  uint8_t type;
  void* value;
  struct capability* *requirements[];
}typedef capability;


struct capability1{
  const char* codec;
  const char* (*profiles)[];
  uint32_t resolutie[2][2];
  uint8_t cromaformat[3];
  uint64_t bitrate[2];
  uint32_t fps[2][2];
}typedef capability1;


/**
 * checks if the input settings can be handled
 * @returns 1 if trameleon can handle the settings otherwise 0
 */
int checkCapabilityEncoder(tra_registry* reg, tra_encoder_settings* settings);

/**
 * checks if the input settings can be handled
 * @returns 1 if the specified module can handle the settings,
 * @returns -1 if the module cannot be found,
 * @returns 0 if the module cannot be handled by the module.
 */
 int checkCapabilityEncodermodule(tra_registry* reg, tra_encoder_settings* settings, const char* module);

 /**
  * @param output is the array of capabilities of trameleon
  * @return 0 if no error occurs otherwise an error code
  */
 int getPossibleCapabilitiesEncoder(tra_registry* reg, capability* (*output)[], uint32_t size);

 /**
  * @param output is the array of capabilities of the specified module
  * @returns 0 if no error occurs otherwise an error code
  * @returns -1 if the module cannot be found
  */
 int getPossibleCapabilitiesEncodermodule(tra_registry* reg, const char* module, capability* (*output)[], uint32_t size);

 /**
 * checks if the input settings can be handled
 * @returns 1 if trameleon can handle the settings otherwise 0
  */
 int checkCapabilityDecoder(tra_registry* reg, tra_decoder_settings* settings);

 /**
 * checks if the input settings can be handled
 * @returns 1 if the specified module can handle the settings,
 * @returns -1 if the module cannot be found,
 * @returns 0 if the module cannot be handled by the module.
  */
 int checkCapabilityDecodermodule(tra_registry* reg, tra_decoder_settings* settings, const char* module);

 /**
  * @param output is the array of capabilities of trameleon
  * @return 0 if no error occurs otherwise an error code
  */
 int getPossibleCapabilitiesDecoder(tra_registry* reg, capability* (*output)[], uint32_t size);

 /**
  * @param output is the array of capabilities of the specified module
  * @returns 0 if no error occurs otherwise an error code
  * @returns -1 if the module cannot be found
  */
 int getPossibleCapabilitiesDecodermodule(tra_registry* reg, const char* module, capability* (*output)[], uint32_t size);

#endif // TRAMELEON_CAPABILITYHANDLER_H
