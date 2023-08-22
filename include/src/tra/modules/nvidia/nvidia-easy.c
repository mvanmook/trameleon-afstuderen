/* ------------------------------------------------------- */

#include <stdlib.h>
#include <tra/modules/nvidia/nvidia-easy.h>
#include <tra/modules/nvidia/nvidia-cuda.h>
#include <tra/registry.h>
#include <tra/module.h>
#include <tra/core.h>
#include <tra/easy.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

static tra_easy_nvapp* g_app = NULL;

/* ------------------------------------------------------- */

static int nvapp_create(tra_easy_nvapp_settings* cfg, tra_easy_nvapp** ctx);
static int nvapp_destroy(); /* This will deallocate the global `tra_easy_nvapp` instance. Should only be called once. */

/* ------------------------------------------------------- */

int tra_easy_nvapp_ensure(tra_easy_nvapp_settings* cfg, tra_easy_nvapp** ctx) {
  return nvapp_create(cfg, ctx);
}

/* ------------------------------------------------------- */

/*
  
  This function makes sure that only one instance of
  `tra_easy_nvapp` will be created.  When an instance has been
  created already, we will set the given `tra_easy_nvapp**` to that
  instance. Each easy implementation of the NVIDIA layer can use
  this `tra_easy_nvapp` instance. This instance is shared
  between the encoder, decoder, etc. of the NVIDIA easy layer.

  This is used to make sure that e.g. we share a CUDA context
  between e.g. NVIDIA based encoders and decoders. This is only
  required when we use the easy layer. When a user wants to use
  the NVIDIA encoders directly, he can allocate a CUDA context
  and pass it as settings into the encoder, decoder, etc. But
  because the easy layer is supposed to be "easy", we take care
  of that.
  
 */
static int nvapp_create(tra_easy_nvapp_settings* cfg, tra_easy_nvapp** ctx) {

  tra_cuda_api* cuda_api = NULL;
  tra_easy_nvapp* inst = NULL;
  tra_core* core_ctx = NULL;
  int r = 0;

  if (NULL == cfg) {
    TRAE("Cannot create the `tra_easy_nvapp` as the given `tra_easy_nvapp_settings` is NULL.");
    r = -10;
    goto error;
  }

  if (NULL == cfg->easy_ctx) {
    TRAE("Cannot create the `tra_easy_nvapp` as the given `tra_easy*` is NULL.");
    r = -10;
    goto error;
  }

  r = tra_easy_get_core_context(cfg->easy_ctx, &core_ctx);
  if (r < 0) {
    TRAE("Cannot create the `tra_easy_nvapp` as we failed to get the `tra_core*` from `tra_easy`.");
    r = -20;
    goto error;
  }

  if (NULL == core_ctx) {
    TRAE("Cannot create the `tra_easy_nvapp`, the `tra_core*` we got from `tra_easy` is NULL.");
    r = -30;
    goto error;
  }
    
  if (NULL == ctx) {
    TRAE("Cannot ensure the `tra_easy_nvapp` as the given `tra_easy_nvapp**` is NULL.");
    r = -20;
    goto error;
  }

  /* Already created? */
  if (NULL != g_app) {
    *ctx = g_app;
    r = 0;
    goto error;
  }

  /* Allocate, we do this only once! Each encoder, decoder shares this. */
  inst = calloc(1, sizeof(tra_easy_nvapp));
  if (NULL == inst) {
    TRAE("Cannot ensure the `tra_easy_nvapp` as we failed to allocate the `tra_easy_nvapp`. Out of memory?");
    r = -30;
    goto error;
  }

  /* Initialize the `tra_easy_nvapp` so it can be used by the easy layer. */
  r = tra_core_api_get(core_ctx, "cuda", &inst->cuda_api);
  if (r < 0) {
    TRAE("Cannot ensure the `tra_easy_nvapp`. Failed to create the CUDA core API.");
    r = -60;
    goto error;
  }

  cuda_api = inst->cuda_api;
  if (NULL == cuda_api) {
    TRAE("Cannot ensure the `tra_easy_nvapp` as we failed to find the `cuda` api.");
    r = -70;
    goto error;
  }

  /* Create the default cuda device. */
  r = cuda_api->init();
  if (r < 0) {
    TRAE("Cannot ensure the `tra_easy_nvapp`. Failed to initialize the CUDA api.");
    r = -80;
    goto error;
  }

  r = cuda_api->device_create(0, &inst->cuda_device);
  if (r < 0) {
    TRAE("Cannot ensure the `tra_easy_nvapp`. Failed to create the CUDA device.");
    r = -90;
    goto error;
  }

  /* Now create the context. */
  r = cuda_api->context_create(inst->cuda_device, 0, &inst->cuda_context);
  if (r < 0) {
    TRAE("Cannot ensure the `tra_easy_nvapp`. Failed to create the CUDA context.");
    r = -100;
    goto error;
  }

  /* Get handles to the context and device. */
  r = cuda_api->device_get_handle(inst->cuda_device, &inst->cuda_device_handle);
  if (r < 0) {
    TRAE("Cannot ensure the `tra_easy_nvapp`. Failed to get the CUDA device handle.");
    r = -110;
    goto error;
  }

  r = cuda_api->context_get_handle(inst->cuda_context, &inst->cuda_context_handle);
  if (r < 0) {
    TRAE("Cannot ensure the `tra_easy_nvapp`. Failed to get a handle to the CUDA context.");
    r = -120;
    goto error;
  }

  /* Note that we set both the `g_app` and `ctx` here; the `g_app` is our singleton instance. */
  g_app = inst;
  *ctx = inst;
  
 error:

  if (r < 0) {
    
    if (NULL != inst) {
      nvapp_destroy();
      inst = NULL;
      g_app = NULL;
    }

    if (NULL != ctx) {
      *ctx = NULL;
    }
  }

  return r;
}

/* ------------------------------------------------------- */

static int nvapp_destroy() {

  int result = 0;
  int r = 0;

  TRAE("@todo destroy the nvapp + resources!");

  if (NULL != g_app) {
    free(g_app);
  }

  /* Make sure we unset the only instance we can create! */
  g_app = NULL;
  
  return result;
}

/* ------------------------------------------------------- */
