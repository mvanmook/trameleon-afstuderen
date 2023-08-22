#ifndef TRAMELEON_IMAGEPROCESSOR_H
#define TRAMELEON_IMAGEPROCESSOR_H

#include <cstdint>
#include <vector>

#define VIDEO_FRAME_MAX_FADE

struct video_frame_settings{
  frame;
  bool isready;
  uint32_t anchorx;
  uint32_t anchory;
  uint32_t offsetx;
  uint32_t offsety;
  uint32_t framerate;
  uint32_t framecnt;
  uint32_t fade_percentage;
} typedef video_frame_settings;

struct video_processor_object{
  std::vector<video_frame_settings*> frames;
  uint32_t height, width, maxframerate;
}

#endif // TRAMELEON_IMAGEPROCESSOR_H
