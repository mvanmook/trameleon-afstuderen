#ifndef VTBOX_UTILS_H
#define VTBOX_UTILS_H

/* ------------------------------------------------------- */

int vtbox_print_pixelbufferpool(CVPixelBufferPoolRef pool);
int vtbox_print_pixelbuffer(CVPixelBufferRef buf);
int vtbox_cvpixelformat_to_imageformat(OSType fmt, uint32_t* outFormat); /* Converts a CVPixelBufferFormat to an image format like `TRA_IMAGE_FORMAT_{...}`.. */
const char* vtbox_pixelformat_to_string(int fmt);

/* ------------------------------------------------------- */

#endif
