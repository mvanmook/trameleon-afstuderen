#include "../../include/tra/capabilityHandler.h"
#include "tra/log.h"

int checkCapabilityEncoder(tra_registry *reg, tra_encoder_settings *settings) {
  int r = 0;
  if (NULL == settings) {
    TRAE("no input settings given");
    return -1;
  }
  int i = 0;
  while (TRUE) {
    tra_encoder_api *api = NULL;
    r = tra_registry_get_encoder_api_by_index(reg, i, &api);
    if (-30 == r) { // last encoder has been reached
      r = 0;
      break;
    }
    if (0 > r) {
      TRAE("module cannot be found");
      return -1;
    }
    r = api->isCapable(settings);
    if (0 > r) { break; }
    i++;
  }
  return r;
}

int checkCapabilityEncodermodule(tra_registry *reg, tra_encoder_settings *settings, const char *module) {
  int r = 0;
  if (NULL == settings) {
    TRAE("no input settings given");
    return -2;
  }
  tra_encoder_api *api = NULL;
  r = tra_registry_get_encoder_api(reg, module, &api);
  if (0 > r) {
    TRAE("module cannot be found");
    return -1;
  }
  return api->isCapable(settings);
}

int getPossibleCapabilitiesEncoder(tra_registry *reg, capability *(*output)[], uint32_t size) {
  int r = 0;
  if (NULL != output) {
    TRAE("output array must be empty");
    return -1;
  }
  int i = 0;
  while (TRUE) {
    capability *(*moduleCapability)[] = NULL;
    tra_encoder_api *api = NULL;
    r = tra_registry_get_encoder_api_by_index(reg, i, &api);
    if (-30 == r) {
      r = 0;
      break;
    }
    if (0 > r) {
      TRAE("module cannot be found");
      return -1;
    }
    uint32_t moduleSize = 0;
    if (0 > api->getCapability(moduleCapability, moduleSize)) {
      TRAE("failed to get capabilities of module: %s", api->get_name());
    }

    for (int j = 0; j < moduleSize; j++) { (*output)[i] = (*moduleCapability)[j]; }

    i++;
  }
  size = i;
  return 0;
}

int getPossibleCapabilitiesEncodermodule(tra_registry *reg, const char *module,
                                         capability *(*output)[], uint32_t size) {
  int r = 0;
  if (NULL != output) {
    TRAE("output array must be empty");
    return -2;
  }
  tra_encoder_api *api = NULL;
  r = tra_registry_get_encoder_api(reg, module, &api);
  if (0 > r) {
    TRAE("module cannot be found");
    return -1;
  }
  return api->getCapability(output, size);
}

int checkCapabilityDecoder(tra_registry *reg, tra_decoder_settings *settings) {
  int r = 0;
  if (NULL == settings) {
    TRAE("no input settings given");
    return -1;
  }
  int i = 0;
  while (TRUE) {
    tra_decoder_api *api = NULL;
    r = tra_registry_get_decoder_api_by_index(reg, i, &api);
    if (-30 == r) {//looped through all api
      return 0;
    }
    if (0 > r) {
      TRAE("module cannot be found");
      return -1;
    }
    r = api->isCapable(settings);
    if (0 > r) { return 1; }
    i++;
  }
  return -1;
}

int checkCapabilityDecodermodule(tra_registry *reg, tra_decoder_settings *settings, const char *module) {
  int r = 0;
  if (NULL == settings) {
    TRAE("no input settings given");
    return -2;
  }
  tra_decoder_api *api = NULL;
  r = tra_registry_get_decoder_api(reg, module, &api);
  if (0 > r) {
    TRAE("module cannot be found");
    return -1;
  }
  return api->isCapable(settings);
}

int getPossibleCapabilitiesDecoder(tra_registry *reg, capability *(*output)[], uint32_t size) {
  int r = 0;
  if (NULL != output) {
    TRAE("output array must be empty");
    return -1;
  }
  uint32_t i = 0;
  while (TRUE) {
    tra_decoder_api *api = NULL;
    r = tra_registry_get_decoder_api_by_index(reg, i, &api);
    if (-30 == r) {// last api exceeded
      r = 0;
      break;
    }
    if (0 > r) {
      TRAE("module cannot be found");
      return -1;
    }
    capability *(*moduleCapability)[] = NULL;
    uint32_t moduleSize = 0;
    if (0 > api->getCapability(moduleCapability, moduleSize)) {
      TRAE("failed to get capabilities of module: %s", api->get_name());
    }

    for (int j = 0; j < moduleSize; j++) { (*output)[i] = (*moduleCapability)[j]; }
    i++;
  }
  size = i;
  return 0;
}

int getPossibleCapabilitiesDecodermodule(tra_registry *reg, const char *module,
                                         capability *(*output)[], uint32_t size) {
  int r = 0;
  if (NULL != output) {
    TRAE("output array must be empty");
    return -2;
  }
  tra_decoder_api *api = NULL;
  r = tra_registry_get_decoder_api(reg, module, &api);
  if (0 > r) {
    TRAE("module cannot be found");
    return -1;
  }
  return api->getCapability(output, size);
}
