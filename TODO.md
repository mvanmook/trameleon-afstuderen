# Things to do for Trameleon 

I've paused the development for ~3 months; now, in November 2022
I'm starting up again and I've created this TODO.md to get an
overview of things to improve and implement.

| ID | Info                                                                     |
|---:|:-------------------------------------------------------------------------|
|  1 | Document / streamline all the encoder and decoder create functions.      |
|  2 | Setting naming standard.                                                 |
|  3 | Document why we create a `decoder` and `encoder` struct.                 |
|  4 | Document GFX interop and/or device-2-device copies; see nvidia decoder.  |
|  5 | Move `tra_load` to each separate module instead of using one func.       |
|  6 | Document the API flow between modules.                                   |
|  7 | Do we need to register the PBOs in `opengl-cuda.c` each `upload()` call. |
|  8 | Specify where and how we set members of modules; e.g. after validation?  |
|  9 | Maybe rename tra_{encoded,decoded}_{host,device}_memory?                 |
| 10 | Create examples for e.g. profiling.                                      |
| 11 | Define standard for static function names in modules.                    |
| 12 | Remove `opengl.c`.                                                       |
| 13 | Cleanup memory types.                                                    |
| 14 | Make sure logging is consistent; use type names in logs.                 |
| 15 | Check if all used callbacks are folowing the same convention.            |
| 16 | Make sure we use the term `module` instead of `plugin`.                  |
| 17 | See `nvidia-enc-host.c::tra_nvenchost_encode()`, verify mem copies.      |
| 18 | Check members of `tra_memory_cuda` and `tra_nvenc_resource`              |
| 19 | Make sure that the `tra_{encoder,decoder}_api` is at the top.            |
| 20 | Maybe move the module specific types to the module files!                |
| 21 | Create a code-review guideline.                                          |
| 22 | See `test-opengl-converter.c`, needs cleanup.                            |
| 23 | Add `lib` path to the core settings.                                     |
| 24 | Reread the headers and comment where necessary.                          |
| 25 | Pass the path from where we load the shared libraries to `tra_core`.     |
| 26 | Add feature to flush the encoders; maybe a flag in `tra_sample`?         |
| 27 | Deallocate memory allocated in the cuda nvidia encoder.                  |
| 28 | Shouldn't we use `uint64_t pts` for `tra_sample`?                        |
| 29 | Implement the flush for the x264 module.                                 |
| 30 | Create the public API for the x264 module.                               |
| 31 | Implement the VAAPI encoder flush.                                       |
| 32 | Separate the VAAPI module into separate files, like the NVIDIA mdoule.   |
| 33 | The naming convention in registry.h and core.h is not correct            |
|    |                                                                          |

### 1: Document and streamline create functions

After looking back at the API of each implementation I noticed
that the create functions for each of the modules are a bit
different. My first thoughts were that it wasn't really clean;
though as an end-user you'll most likely never see these
implementation specific functions. The core functions all use the
same API and internally map/call the functions in the way they
should be called. This is actually a good approach as it allows
each implementation to create settings with defaults it needs.

```c

/* encoders */
int vt_enc_create(tra_encoder_settings* cfg, vt_enc** ctx);
int netint_enc_create(tra_encoder_settings* cfg, netint_enc** ctx);
int va_enc_create(va_enc_settings* cfg, va_enc** ctx);
int nv_enc_create(tra_encoder_settings* cfg, tra_nvenc_settings* settings, nv_enc** ctx);

/* decoders */
int vt_dec_create(tra_decoder_settings* cfg, vt_dec** ctx);
int netint_dec_create(tra_decoder_settings* cfg, netint_dec** ctx);
int va_dec_create(va_dec_settings* cfg, va_dec** ctx);
```

### 2: Setting naming standard

When a module needs its own settings struct we have to make sure
that the naming between modules follows the same
standard. e.g. see these names below. We should use
`nvenc_settings`, `vaenc_settings` OR `nv_enc_settings` and
`va_enc_settings`, etc. Same must be checked for the decoder.

```c
struct tra_nvenc_settings {
  void* cuda_context_handle; /* Pointer to a CUcontext, e.g. use `tra_cuda_api.context_get_handle()`.  */
  void* cuda_device_handle; /* Pointer to a CUdevice, e.g. use `tra_cuda_api.device_get_handle()`.  */
};

struct va_enc_settings {
  va_gop_settings gop;
  tra_encoder_callbacks* callbacks;    /* [YOURS] The callbacks that will e.g. receive the encoded data. */
  uint32_t image_width;                /* Width of the video. */
  uint32_t image_height;               /* Height of the video. */
  uint32_t image_format;               /* The format of the video frames; e.g. YUV420, NV12, etc. */
  uint32_t num_ref_frames;             /* Maximum number of reference frames to use. Used when we create the SPS. */
};


```

### 3: Why we create a decoder and encoder struct in modules

The reason why we create a separate and private encoder/decoder
struct in the modules (e.g. nvidia.c, netint.c) is because in the
future we might want to add other features that are used
internally. By abstracting this now, we can easily add new
members w/o changing the API/functions.

### 4: Document GFX interop and/or dev-2-dec copies

See `nvidia-dec.c::on_display_calblack` which is called when
we've got a decoded frame in device memory. We should call the
user and allow the user to use the decoded picture with GFX
interop.

### 5: Move `tra_load` to each separate module

While working on the graphics and interop layer for OpenGL I
realised that it's better to create a `tra_load` function for
each separate module. When a user might want to separately load a
module, it should be possible; this means we have to move the
`tra_load` into the source file for each module. I have to 
fix this for most modules; e.g. nvidia.

### 6: Document the API flow between modules

I've tried to make the input/output flow as specific as possibel
though still keeping in mind that things need to be generic. We
have to be generic as we're building a transcoding library with
support for many different encoders and decoders. Currently I
don't want to create a `tra_memory` type like the `GstMemory`
type from GStreamer. The `GstMemory` type makes sense for a media
library like GStreamer but we want to be specific. Being specific
means that the code stays clean/clear and that the types actually
represent whant they are. This is the goal of Trameleon. I have
to document the input/output/upload/download/draw types that act
as the glue between the modules.

### 7: Do we need to register the PBOs in `opengl-cuda.c` each `upload()` call

I've read different things about when one should call
`cuGraphicsGLRegisterBuffer()`. Some posts I found said it's best
to call this function each time you use it (and then get the
pointer to the registere buffer) as OpenGL might move around
buffers internally. [Others][qtav] say you can register it once
when you don't change the texture. To figure out the best
approach I probably need get in touch with NVIDIA.

[qtav]: https://www.qtav.org/blog/cuda_0_copy.html

### 8. Specify where and how we set members of modules; e.g. after validation?

I have to decide where I'm going to setup the members of the
encoders, decoders, interops, etc. in the `_create()` functions.
Currently I don't have a preferred location and it would be
better if the location of where I setup members is the same for
each module; this makes the code easier to understand. I also
should document if we should copy members from the settings or if
we should create the members on the context struct and assign
only the settings we need. This might mean a bit more code, as we
have to create a member for each setting that we need to copy;
though this makes the code more readable. E.g. we don't have to
do `ctx->settings.image_width` but we can directly use
`ctx->image_width`. I prefer the last style.

### 9. Maybe rename tra_{encoded,decoded}_{host,device}_memory?

Maybe I should rename `tra_encoded_host_memory` and
`tra_decoded_device_memory` into just `tra_memory` :). When we do
this we've got a similar API as GStreamer. This might work better
with the scaler too. Though, currently we also have sub
types. For example when we have device memory, it can be
`TRA_DEVICE_MEMORY_TYPE_CUDA` or `TRA_DEVICE_MEMORY_TYPE_OPENGL_NV12`. 

### 10. Create examples for e.g. profiling.

See `profiler.h`; when I'm going to cleanup things I should
create an example that demonstrates how to use both profiling
solutions (custom api, custom macros).

### 11.  Define standard for static function names in modules

Up until now I have used an naming style for private/static
functions in modules where I used a prefix like `nv_` for the
nvidia functions. These function names also show up in
profilers. Therefore I'm thinking it might make sense to prefix
them with `tra_nv` instead. The downside from this prefix is that
function names become larger.

### 13. Cleanup memory types

Currently the memory types and structs are a bit messy or at
least now, that we're a bit further in the development I think we
can creat a better abstraction. Also check the `opengl.h` where
we have created some device memory types.

### 14. Make sure logging is consistent; use type names in logs

See e.g. `nvidia-enc-opengl.c` and `nvidia-enc.c`. I think that
the logging in `tra_nvencgl_creat()` should be the preferred way
as it's very clear about what type caused an error. I should 
make sure that all modules use this approach.

### 15. Check if all used callbacks are folowing the same convention

We have `on_uploaded` but also `on_decoded_data` and
`on_encoded_data`.  I think it's better to rename those to
`on_decoded` and `on_encoded`.

### 16. Make sure we use the term `module` instead of `plugin`

Check if there are any sources that still use `plugin` instead of
`module`.

### 17. Seee `nvidia-enc-host.c::tra_nvenchost_encode()`, verify mem copies.

I'm currently using `cuMemcpy2D()` but I'm not 100% sure what the
difference int `WidthInBytes` and pitch means.

### 18. Check members of `tra_memory_cuda` and `tra_nvenc_resource`

I'm not entirely sure which version I prefer most. The cuda
memory has short member names like `pitch` and `ptr`. The nvenc
resource uses `resource_pitch`, `resource_ptr` etc. Even though
it's longer my preference got to the longer version because
`pitch` might be `input_pitch`, `device_pitch`, etc.

### 19. Make sure that the `tra_{encoder,decoder}_api` is at the top

See e.g. `nvidia-enc-cuda`; the `tra_encoder_api` variable is
created at the top of the file. I should make sure that all
modules use this approach.

### 20. Maybe move the module specific types to the module files! 

The idea of the defines that we have in `types.h` was that we had
one file where the types were declared; Though at this moment I
think we might need to move some of the define into the module
specific files. See the `opengl-converter` for example where I'm
using this approach. Tbh, I think this is a cleaner approach. This
means that the `TRA_MEMORY_TYPE_CUDA` maybe also needs to be
moved into the NVIDIA encoder.

### 21. Create a code-review guideline

Before I can "sign off" a module I should have walked through a 
code review guideline. Some things to think about:

- Are all members of a context required? if not remove the member.
- Make sure that all local variables are initialized!

### 22.  See `test-opengl-converter.c`, needs cleanup

This test was really a "work-in-progress" case and really needs
to be cleaned up. E.g. I'm using `TEST_VS,FS` for the shaders;
give it a better name.

### 23. Add `lib` path to the core settings. 

See `core.h` and `registry.h`. Currently the path from where we
load the shared/run-time loadable libs, is hardcoded; we have to
make sure that this is configurable.

### 24. Reread the headers and comment where necessary

x api.h
x avc.h
x buffer.h
x core.h
. dict.h
. golomb.h
. log.h
. module.h
. modules
. profiler.h
. registry.h
. time.h
. types.h
. utils.h

### 25. Pass the path from where we load the shared libraries to tra_core

See core.h

### 26. Add feature to flush the encoders; maybe a flag in `tra_sample`?

When the user is done encoding it should have a way to flush the
encoder.  See for example the [nvenc][encapi].

[encapi]: https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/index.html

### 27. Deallocate memory allocated in the cuda nvidia encoder

See `nvidia-enc-host.c` where we allocate cuda buffers that we
still need to deallocate.

### 28. Shouldn't we use `uint64_t pts` for `tra_sample`?

Not 100% sure, but I think we should use `uint64_t` for the `pts`
to prevent undefined behavior that can happen with signed ints
that overflow.

### 29 Implement the flush for the x264 module.

In december 2022 we add a flush function that we still need
to implement for, among others, the x264 module.

### 30 Create the public API for the x264 module.

All modules should be usable w/o the registry and module
wrappers; e.g. `tra_nvencgl_create()` provides a public API to
create a NVENC based encoder with OpenGL interop.

### 33 The naming convention in registry.h and core.h is not correct

See the headers, we have to fix the naming and make sure the
follow the same standard.
