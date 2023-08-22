/* ------------------------------------------------------- */

#include <tra/modules/nvidia/nvidia-converter-kernels.h>
#include <tra/log.h>
#include <cuda_runtime.h>
#include <cuda.h>

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    The kernel that will resize a NV12 buffer. This is based on
    [this][0] and [this][1] example. This code is similar to most
    scalers I've found online. It uses texture memory and the
    `tex2D()` function to perform the scaling; I haven't found
    any clear documentation about why `tex2D()` is used, but it's
    most likely used take advantage of hardware interpolation.

  TODO:

    Although the performance is already amazing (2.8us on
    2080RTX, 4.5us on 1650GTX to resize 1920x1080 to 960x540), we
    might be able to optimize this a bit further by making the
    memory writes 32bit wide instead of 16bit. E.g. we are
    writing `uchar2` and we could potentially write
    `uchar4`. Though if we would like to use 32 bit wide writes,
    we should create two kernels; one for the luma and one for
    the chroma.

    Also, currently we don't use all arguments; currently there
    is not really a reason to keep them, though I might want to
    improve the kernel at some point which is why I keep them.
    
  REFERENCES:

    [0]: https://github.com/NVIDIA/cuda-samples/blob/master/Samples/5_Domain_Specific/NV12toBGRandResize/nv12_resize.cu
    [1]: https://github.com/NVIDIA/video-sdk-samples/blob/master/Samples/Utils/Resize.cu "NVIDIA Video SDK samples."

 */
__global__ static void cuda_resize_nv12_kernel(
  cudaTextureObject_t inputTexY,
  cudaTextureObject_t inputTexUv,
  uint32_t inputWidth,
  uint32_t inputHeight,
  uint32_t inputPitch,
  uint8_t* outputY,
  uint8_t* outputUv,
  uint32_t outputWidth,
  uint32_t outputHeight,
  uint32_t outputPitch,
  float scaleX,
  float scaleY
)
{

  /*
    Calculate the (i,j) coordinate where i and j will/should
    never exceed the `(outputWidth / 2)` and `(outputHeight /2)`
    as this kernel is supposed to run over the chroma plane: note
    that we use i,j directly to fetch the chroma samples below with
    the `tex2D()` call. 
  */
  uint32_t i = (blockIdx.x * blockDim.x) + threadIdx.x;
  uint32_t j = (blockIdx.y * blockDim.y) + threadIdx.y;

  /*
    @todo Are we only executing this kernel over all chroma
    pixels and are we therefore fetching and storing twice from
    the luma plane? 
  */
  uint32_t px = i * 2;
  uint32_t py = j * 2;

  /* Out of bounds? */
  if ((px + 1) >= outputWidth
      || (py + 1) >= outputHeight)
    {
       return;
    }

  uint8_t* p = outputY + (py * outputPitch) + px;

  /* Fetch 2 lines from luma plane and store in the resized buffer. */
  *(uchar2*) p = make_uchar2(
    tex2D<uint8_t>(inputTexY, scaleX * (px + 0), scaleY * (py + 0)),
    tex2D<uint8_t>(inputTexY, scaleX * (px + 1), scaleY * (py + 0))
  );
  
  *(uchar2*)(p + outputPitch) = make_uchar2(
    tex2D<uint8_t>(inputTexY, scaleX * (px + 0), scaleY * (py + 0)),
    tex2D<uint8_t>(inputTexY, scaleX * (px + 1), scaleY * (py + 1))
  );

  /* Fetch from chrome plane and store in the resized buffer. */
  p = outputUv + (j * outputPitch) + px;
  
  *(uchar2*)p = tex2D<uchar2>(
    inputTexUv,
    scaleX * i,
    scaleY * j 
  );
}

/* ------------------------------------------------------- */

/*

  GENERAL INFO:
  
    This is an experimental scaling function that resizes NV12
    input to the given output resolution. We assume that you have
    allocated the output buffer as a contiguous block of memory
    that can hold the NV12 buffer.

    This code is based on [nv12_resize.cu][0] and the [Resize.cu][1]
    samples which are pretty similar, though the version from the
    [cuda samples][0] is a bit more readable.

    We first create a `cudaResourceDesc` that describes the input
    data, that we then use to create a `cudaTextureObject_t`
    object for the Y-plane and another one for the UV-plane. The
    Y-plane texture read mode uses `cudaReadModeElementType`
    which reads the element as its "original" value. The UV-plane
    texture uses `uchar2`, e.g. 2 channel data.

  GRID AND BLOCK SIZES

    The grid and block sizes determine how the kernel will
    operate on the data; It's important to use the right
    values. As this is the first time touching CUDA I'm
    using the values from the NVIDIA Video SDK [Samples][3]

  REFERENCES:

    [0]: https://github.com/NVIDIA/cuda-samples/blob/master/Samples/5_Domain_Specific/NV12toBGRandResize/nv12_resize.cu
    [1]: https://github.com/NVIDIA/video-sdk-samples/blob/master/Samples/Utils/Resize.cu "NVIDIA Video SDK samples."
    [2]: https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#texture-object-api "Texture Object API"
    [3]: https://github.com/NVIDIA/video-sdk-samples "NVIDIA Video SDK Samples"
    [4]: https://www.nvidia.com/content/GTC-2010/pdfs/2238_GTC2010.pdf "Better performance at lower occupancy"
    
 */
int tra_cuda_resize_nv12(
  void* inputDevicePtr,
  uint32_t inputWidth,
  uint32_t inputHeight,
  uint32_t inputPitch,
  void* outputDevicePtr,
  uint32_t outputWidth,
  uint32_t outputHeight,
  uint32_t outputPitch
)
{
  cudaResourceDesc res_desc = { };
  cudaTextureDesc tex_desc = { };
  cudaTextureObject_t tex_y = 0;
  cudaTextureObject_t tex_uv = 0;
  cudaError_t status = cudaSuccess;
  uint8_t* out_ptr_uv = NULL; /* Pointer to the UV-plane of the device buffer. */
  uint8_t* out_ptr_y = NULL;  /* (Casted) pointer to the Y-plane of the device buffer. */
  uint8_t* in_ptr_uv = NULL; /* Pointer to the UV-plane of the input data that we will resize. */
  uint8_t* in_ptr_y = NULL; /* Pointer to the Y-plane of the input data that we will resize. */
  float scale_x = 0.0f;
  float scale_y = 0.0f;
  dim3 block_size;
  dim3 grid_size;
  int r = 0;

  /* ------------------------------------------- */
  /* Validate input                              */
  /* ------------------------------------------- */
  
  if (NULL == inputDevicePtr) {
    TRAE("Cannot scale the NV12 buffer as the given input device pointer is NULL.");
    r = -10;
    goto error;
  }

  if (0 == inputWidth) {
    TRAE("Cannot scale the NV12 buffer as the given `inputWidth` is 0.");
    r = -20;
    goto error;
  }

  if (0 == inputHeight) {
    TRAE("Cannot scale the NV12 buffer as the given `inputHeight` is 0.");
    r = -30;
    goto error;
  }

  if (0 == inputPitch) {
    TRAE("Cannot scale the NV12 buffer as the given `inputPitch` is 0.");
    r = -40;
    goto error;
  }

  if (NULL == outputDevicePtr) {
    TRAE("Cannot scale the NV12 buffer as the given output device pointer is NULL.");
    r = -50;
    goto error;
  }

  if (0 == outputWidth) {
    TRAE("Cannot scale the NV12 buffer as the given `outputWidth` is 0.");
    r = -60;
    goto error;
  }

  if (0 == outputHeight) {
    TRAE("Cannot scale the NV12 buffer as the given `outputHeight` is 0.");
    r = -60;
    goto error;
  }

  if (0 == outputPitch) {
    TRAE("Cannot scale the NV12 buffer as the `outputPitch` is 0.");
    r = -70;
    goto error;
  }

  /* Setup some local aliases/helpers */
  out_ptr_y = (uint8_t*) outputDevicePtr;
  out_ptr_uv = out_ptr_y + (outputHeight * outputPitch);
  in_ptr_y = (uint8_t*) inputDevicePtr;
  in_ptr_uv = in_ptr_y + (inputHeight * inputPitch);
  scale_x = (float) inputWidth / (float) outputWidth;
  scale_y = (float) inputHeight / (float) outputHeight;

  /* ------------------------------------------- */
  /* Create the handles                          */
  /* ------------------------------------------- */

  tex_desc.filterMode = cudaFilterModePoint;   /* Nearest neighbour scaling */
  tex_desc.readMode = cudaReadModeElementType; /* No conversion from type is performed. See [this][2] documentation. */

  res_desc.resType = cudaResourceTypePitch2D;
  res_desc.res.pitch2D.devPtr = inputDevicePtr;
  res_desc.res.pitch2D.width = inputWidth;
  res_desc.res.pitch2D.height = inputHeight;
  res_desc.res.pitch2D.pitchInBytes = inputPitch;

  /* Y-texture */
  res_desc.res.pitch2D.desc = cudaCreateChannelDesc<uint8_t>();

  status = cudaCreateTextureObject(&tex_y, &res_desc, &tex_desc, NULL);
  if (cudaSuccess != status) {
    TRAE("Failed to create the texture object for the Y-plane.");
    r = -80;
    goto error;
  }

  /* UV-texture */
  res_desc.res.pitch2D.desc = cudaCreateChannelDesc<uchar2>();
  res_desc.res.pitch2D.devPtr = in_ptr_uv;
  res_desc.res.pitch2D.width = (inputWidth / 2);  
  res_desc.res.pitch2D.height = (inputHeight / 2);
  
  status = cudaCreateTextureObject(&tex_uv, &res_desc, &tex_desc, NULL);
  if (cudaSuccess != status) {
    TRAE("Failed to create the texture object for the UV-plane.");
    r = -90;
    goto error;
  }

  /*
    
    NOTE: the [Resize.cu][1] implementation seems to run the
    kernel over all the output pixels of the Y-plane. As we fetch
    2 lines in the kernel this is not necessary. We only have to
    run the kernel over all of the UV-plane pixels. To make sure
    we only run the kernel over the UV-plane we use half the
    width and height of the output size.
    
    grid_size.x = (outputWidth + 31) / 32;
    grid_size.y = (outputHeight + 31) / 32;
    
  */
  grid_size.x = ((outputWidth / 2) + 15) / 16;
  grid_size.y = ((outputHeight / 2) + 15) / 16;
  grid_size.z = 1;

  /*
    
    Using 32,32,1 for the block size gave me pretty good results
    on a 2080x RTX (6.5us to resize from 1920x1080 to 960x540)
    but pretty bad results on a 1650gtx: 30us.

    Therefore I changed the block size to 16,16,1 and now I'm
    getting 2.8us (2080RTX) and 4.5us (1650GTX). I've also
    experimented with the `cudaOccupancyMacPotentialBlockSize()`
    function which gives me a number of 14 for the blocks size
    which turns out to be slower than using 16. Also, better
    occupancy doesn't always mean better performance, see
    [this][4].
    
   */
  block_size.x = 16;
  block_size.y = 16;
  block_size.z = 1;

  cuda_resize_nv12_kernel<<<grid_size, block_size>>>(
    tex_y,
    tex_uv,
    inputWidth,
    inputHeight,
    inputPitch,
    out_ptr_y,
    out_ptr_uv,
    outputWidth,
    outputHeight,
    outputPitch,
    scale_x,
    scale_y
  );

  status = cudaGetLastError();
  if (cudaSuccess != status) {
    TRAE("Failed to execute the kernel.");
    r = -100;
    goto error;
  }
  
 error:

  return r;
}

/* ------------------------------------------------------- */
