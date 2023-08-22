/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>

#include <ni_device_api_logan.h>
#include <ni_rsrc_api_logan.h>
#include <ni_util_logan.h>

#include <tra/modules/netint/netint-utils.h>
#include <tra/modules/netint/netint-dec.h>
#include <tra/module.h>
#include <tra/types.h>
#include <tra/log.h>

/* ------------------------------------------------------- */

struct netint_dec {
    tra_decoder_settings settings;
    ni_logan_device_context_t *device;
    ni_logan_session_context_t session;
    ni_logan_encoder_params_t params;
    uint8_t is_start_of_stream; /* Is set to 1 by `netint_dec_create()` and will be set to 0 after the first packet we've sent to the decoder. */
};

/* ------------------------------------------------------- */

static int netint_dec_read_decoded_data(netint_dec *ctx);

/* ------------------------------------------------------- */

/*

  GENERAL INFO:

    Create and setup the decoder instance.

  TODO

    @todo I have to create a generic way to notify the start/stop of a session. maarten - with create/destroy or a separate function start/stop

*/
int netint_dec_create(tra_decoder_settings *cfg, netint_dec **ctx, int bitDepth) {

    ni_logan_retcode_t code = NI_LOGAN_RETCODE_SUCCESS;
    netint_dec *inst = NULL;
    unsigned long load = 0;
    int r = 0;

    TRAE("@todo current `netint_dec_read_decoded_data()` we need to figure out how to correctly read the decoded data.");
    TRAE("@todo see the todos above `netint_dec_create()` I have to manage memory correctly!");
    TRAE("@todo the `ni_logan_decoder_default_params()` request the fps num/den... why is that?");

    if (NULL == cfg) {
        TRAE("Cannot create the `netint_dec` instance as the given `tra_decoder_settings*` is NULL.");
        r = -10;
        goto error;
    }

    if (0 == cfg->image_width) {
        TRAE("Cannot create the `netint_dec` instance as the `image_width` is 0.");
        r = -20;
        goto error;
    }

    if (0 == cfg->image_height) {
        TRAE("Cannot create the `netint_dec` instance as the `image_height` is 0.");
        r = -30;
        goto error;
    }

    if (NULL == ctx) {
        TRAE("Cannot create the `netint_dec` instance as the given `netint_dec**` is NULL.");
        return -1;
    }

    if (NULL != (*ctx)) {
        TRAE("Cannot create the `netint_dec` as the given `*(netint_dec**)` is not NULL. Already created? Or not set to NULL?");
        return -2;
    }


    inst = calloc(1, sizeof(netint_dec));
    if (NULL == inst) {
        TRAE("Cannot create the `netint_dec` instance, failed to allocate. Out of memory?");
        return -3;
    }

    inst->settings = *cfg;
    inst->is_start_of_stream = 1;

    code = ni_logan_decoder_init_default_params(
            &inst->params,
            cfg->fps_num,
            cfg->fps_den,
            cfg->bitrate,
            cfg->image_width,
            cfg->image_height
    );

    if (NI_LOGAN_RETCODE_SUCCESS != code) {
        TRAE("Cannot create the `netint_dec` instance. Failed to initialize the default decoder parameters.");
        r = -4;
        goto error;
    }

    uint32_t* codec = calloc(1, sizeof(uint32_t));
    r = convert_trameleon_to_netint_codec(codec, inst->settings.output_type);
    if (0 > r) {
        TRAE("unable to convert codec format from trameleon to netint");
        goto error;
    }

    /* Use NETINTs own resource management logic. */
    inst->device = ni_logan_rsrc_allocate_auto(
            NI_LOGAN_DEVICE_TYPE_DECODER,
            EN_ALLOC_LEAST_LOAD,
            *codec,
            cfg->image_width,
            cfg->image_height,
            25,
            &load
    );

    if (NULL == inst->device) {
        TRAE("Cannot create the `netint_dec` instance. Failed to allocate the device context.");
        r = -5;
        goto error;
    }

    /* Sets some defaults after which we can specify some custom values. */
    ni_logan_device_session_context_init(&inst->session);

    uint32_t* codec_format = calloc(1, sizeof(uint32_t));
    r = convert_trameleon_to_netint_codec_format(codec_format, inst->settings.output_type);

    inst->session.keep_alive_timeout = 10;                    /* Heartbeat timeout that is used to keep the connection between the libxcoder and hardware open. */
    inst->session.session_id = NI_LOGAN_INVALID_SESSION_ID;
    inst->session.p_session_config = &inst->params;
    inst->session.codec_format = *codec_format;
    inst->session.hw_id = 0;                                  /* The device to use. Use `ni_logan_rsrc_mon` to show a list of indices for each device. */
    inst->session.src_bit_depth = bitDepth;                   /* Bit depth of the source. */
    inst->session.src_endian = NI_LOGAN_FRAME_LITTLE_ENDIAN;        /* We expect data to be in little endian. */

    inst->session.bit_depth_factor = 1;
    if (10 == bitDepth) {
        inst->session.bit_depth_factor = 2;
    }

    code = ni_logan_device_session_open(&inst->session, NI_LOGAN_DEVICE_TYPE_DECODER);
    if (NI_LOGAN_RETCODE_SUCCESS != code) {
        TRAE("Failed to open a decoder session.");
        r = -5;
        goto error;
    }

    /* Finally assign the result. */
    *ctx = inst;

    error:

    if (0 > r) {

        if (NULL != inst) {
            netint_dec_destroy(inst);
            inst = NULL;
        }

        if (NULL != ctx) {
            *ctx = NULL;
        }
    }

    return r;
}

/* ------------------------------------------------------- */

int netint_dec_destroy(netint_dec *ctx) {

    ni_logan_retcode_t code = NI_LOGAN_RETCODE_SUCCESS;
    int result = 0;

    if (NULL == ctx) {
        TRAE("Cannot destroy the `netint_dec*` as it's NULL.");
        return -1;
    }

    /* Close session */
    if (NI_LOGAN_INVALID_SESSION_ID != ctx->session.session_id) {
        code = ni_logan_device_session_close(&ctx->session, 0, NI_LOGAN_DEVICE_TYPE_DECODER);
        if (NI_LOGAN_RETCODE_SUCCESS != code) {
            TRAE("Failed to cleanly close the decoder session.");
            result -= 1;
        }
    }

    if (NULL != ctx->device) {
        ni_logan_rsrc_free_device_context(ctx->device);
    }

    ctx->device = NULL;
    ctx->session.session_id = NI_LOGAN_INVALID_SESSION_ID;

    free(ctx);
    ctx = NULL;

    return result;
}

/* ------------------------------------------------------- */

/* 
   GENERAL INFO:
   
     The NETINT decoder expects complete pictures.

   TODO:

     @todo The call to `ni_logan_packet_copy()` returns a value which is higher
           than the one passed into it. Why is that?
   
 */
int netint_dec_decode(netint_dec *ctx, uint32_t type, void *data) {
    tra_memory_h264 *host_mem = NULL;
    ni_logan_retcode_t code = NI_LOGAN_RETCODE_SUCCESS;
    ni_logan_session_data_io_t data_io = {};
    ni_logan_packet_t *packet = NULL;
    int nbytes_copied = 0; /* `ni_logan_packet_copy()` may copy more bytes than you pass into it. */
    int nbytes_written = 0;
    int r = 0;

    if (NULL == ctx) {
        TRAE("Cannot decode as the given `netint_dec*` is NULL.");
        r = -10;
        goto error;
    }

    if (NULL == data) {
        TRAE("Cannot decode as the given `data` is NULL.");
        r = -30;
        goto error;
    }

    host_mem = (tra_memory_h264 *) data;
    if (NULL == host_mem->data) {
        TRAE("Cannot decode as the `tra_encoded_host_memory.data` is NULL.");
        r = -40;
        goto error;
    }

    if (0 == host_mem->size) {
        TRAE("Cannot decode as the `tra_encoded_host_memory.size` is 0.");
        r = -50;
        goto error;
    }

    if (NULL == ctx->device) {
        TRAE("Cannot decode as the `device` member of the given `netint_dec` is NULL. Did you create the instance?");
        r = -60;
        goto error;
    }

    if (0 == ctx->settings.image_width) {
        TRAE("Cannot decode as the `netint_dec.settings.image_width` is 0.");
        r = -70;
        goto error;
    }

    if (0 == ctx->settings.image_height) {
        TRAE("Cannot decode as the `netint_dec.settings.image_height` is 0.");
        r = -80;
        goto error;
    }

    TRAD("Decoding %u bytes.", host_mem->size);

    /* Setup the packet and allocate space for it. */
    packet = &data_io.data.packet;
    packet->start_of_stream = ctx->is_start_of_stream;
    packet->end_of_stream = 0;
    packet->video_width = ctx->settings.image_width;
    packet->video_height = ctx->settings.image_height;
    packet->data_len = host_mem->size;

    code = ni_logan_packet_buffer_alloc(packet, host_mem->size);
    if (NI_LOGAN_RETCODE_SUCCESS != code) {
        TRAE("Failed to allocate a packet for the coded data.");
        r = -90;
        goto error;
    }

    /* Copy the input, coded data. */
    nbytes_copied = ni_logan_packet_copy(
            packet->p_data,
            host_mem->data,
            host_mem->size,
            &ctx->session.p_leftover,
            &ctx->session.prev_size
    );

    if (0 > nbytes_copied) {
        TRAE("Failed to copy data into the packet.");
        r = -100;
        goto error;
    }

    nbytes_written = ni_logan_device_session_write(&ctx->session, &data_io, NI_LOGAN_DEVICE_TYPE_DECODER);
    if (0 > nbytes_written) {
        TRAE("Failed to send data to the decoder.");
        r = -110;
        goto error;
    }

    /* We've sent something so we're not "starting" anymore. */
    ctx->is_start_of_stream = 0;

    TRAD(">> copied: %d, written: %d", nbytes_copied, nbytes_written);

    r = netint_dec_read_decoded_data(ctx);
    if (0 > r) {
        TRAE("Failed to read decoded data.");
        r = -120;
        goto error;
    }

    error:

    ni_logan_packet_buffer_free(packet);
    return r;
}

/* ------------------------------------------------------- */

static int netint_dec_read_decoded_data(netint_dec *ctx) {
    tra_memory_image decoded_image;
    ni_logan_retcode_t code = NI_LOGAN_RETCODE_SUCCESS;
    ni_logan_session_data_io_t data_io = {};
    ni_logan_frame_t *frame = NULL;
    uint8_t should_alloc = 0;
    int nbytes_read = 0;
    int ses_width = 0;
    int ses_height = 0;
    int width = 0;
    int height = 0;
    int r = 0;

    if (NULL == ctx) {
        TRAE("Cannot read decoded data as the given `netint_dec*` is NULL.");
        return -1;
    }

    if (0 == ctx->settings.image_width) {
        TRAE("The `netint_dec.settings.image_width` is 0. We expect it to contain the width of the video.");
        return -2;
    }

    if (0 == ctx->settings.image_height) {
        TRAE("The `netint_dec.settings.image_height` is 0. We expect it to contain the height of the video.");
        return -3;
    }

    ses_width = ctx->session.active_video_width;
    ses_height = ctx->session.active_video_height;
    should_alloc = (ses_width > 0 && ses_height > 0) ? 1 : 0;
    TRAD("Should alloc: %u, ses_w/h: %d x %d", should_alloc, ses_width, ses_height);

    {
        static uint8_t did_log = 0;
        if (0 == did_log) {
            TRAE("@todo See research-netint.org where I describe how/when to set should_alloc to 1. I still have to confirm this with the NETINT developers.");
            did_log = 1;
        }
    }

    frame = &data_io.data.frame;
    uint8_t* alignment = calloc(1, sizeof(uint32_t));
    r = convert_trameleon_to_netint_alignment(alignment, ctx->settings.output_type);
    code = ni_logan_decoder_frame_buffer_alloc(
            ctx->session.dec_fme_buf_pool,
            frame,
            should_alloc,                                                /* Whether to get memory from buffer pool. */
            ctx->settings.image_width,                                   /* Width of the video. */
            ctx->settings.image_height,                                  /* Height of the video. */
            *alignment, /* Whether the width and height need to be aligned. When 1, the allocated buffer will use a width/height that is aligned to the requirements.  */
            ctx->session.bit_depth_factor                                /* 1 for 8 bit, 2 for 10 bit. */
    );

    if (NI_LOGAN_RETCODE_SUCCESS != code) {
        TRAE("Failed to allocate the buffer for the decoded frame: %s", netint_retcode_to_string(code));
        r = -4;
        goto error;
    }

    nbytes_read = ni_logan_device_session_read(&ctx->session, &data_io, NI_LOGAN_DEVICE_TYPE_DECODER);
    if (nbytes_read < 0) {
        TRAE("Failed to read decoded data from the device: %s.", netint_retcode_to_string(nbytes_read));
        r = -5;
        goto error;
    }

    TRAD("READ: %d", nbytes_read);
    if(0 < nbytes_read){


        decoded_image.image_width = frame->video_width;
        decoded_image.image_height = frame->video_height;
        decoded_image.plane_data[0] = frame->p_data[0];
        decoded_image.plane_data[1] = frame->p_data[1];
        decoded_image.plane_data[2] = frame->p_data[2];
        decoded_image.plane_heights[0] = frame->video_height;
        decoded_image.plane_heights[1] = frame->video_height;
        decoded_image.plane_heights[2] = frame->video_height;
        decoded_image.plane_strides[0] = frame->video_width;
        decoded_image.plane_strides[1] = frame->video_width;
        decoded_image.plane_strides[2] = frame->video_width;
        decoded_image.plane_count = 4;

        decoded_image.image_format = TRA_IMAGE_FORMAT_I420;

        ctx->settings.callbacks.on_decoded_data(TRA_MEMORY_TYPE_IMAGE, &decoded_image, ctx->settings.callbacks.user);
    }

    error:

    ni_logan_decoder_frame_buffer_free(frame);

    return r;
}

/* ------------------------------------------------------- */

