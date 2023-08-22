#include "tra/log.h"
#include "tra/time.h"
#include <tra/modules/imageProcessing/imageProcessor.h>

int destroy(video_processor_object obj) {

}

int create(uint32_t height, uint32_t width, uint32_t anchorx, uint32_t anchory,
       video_processor_object obj) {
  if (0 >= height) {
    TRAE("output height cannot be negative");
    goto error;
  }
  if (0 >= width) {
    TRAE("output height cannot be negative");
    goto error;
  }
  obj.height = height;
  obj.width = width;
  bool isHundred = false;
  for (std::vector<video_frame_settings *>::iterator it = obj.frames.begin();
       it != obj.frames.end(); it++) {
    if (0 > (*it)->framerate) {
      TRAE("framerates need to be >0");
      goto error;
    }
    if ((*it)->framerate > obj.maxframerate)
      obj.maxframerate = (*it)->framerate;
    if (VIDEO_FRAME_MAX_FADE == (*it)->fade_percentage)
      isHundred = true;
  }
  if (!isHundred) {
    TRAE("one image must at %d", VIDEO_FRAME_MAX_FADE);
    goto error;
  }
  return 0;
error:
  destroy(obj);
}

int checkframeRates(video_processor_object obj) {
  for (std::vector<video_frame_settings *>::iterator it = obj.frames.begin();
       it != obj.frames.end(); it++) {
    if (0 == (*it)->framerate) { // single frame images
      (*it)->isready = true;
    }
    if ((*it)->framecnt + (*it)->framerate > obj.maxframerate)
      (*it)->isready = false;
    (*it)->framecnt = ((*it)->framecnt + (*it)->framerate) % obj.maxframerate;
  }
  error:
}

int combine(video_processor_object obj) {
  outputimage;
  for (std::vector<video_frame_settings *>::iterator it = obj.frames.begin();
       it != obj.frames.end(); it++) {

    for (int i = 0; i < obj.width; i++) {
      for (int j = 0; j < obj.height; j++) {
        if (NULL == (*it)->frame[i - (*it)->anchorx - (*it)->offsetx]
                                [j - (*it)->anchory - (*it)->offsety])
          continue;
        if (0 == (*it)->fade_percentage)
          outputimage[i][j] = (*it)->frame[i - (*it)->anchorx - (*it)->offsetx]
                                          [j - (*it)->anchory - (*it)->offsety];
        else {
          outputimage[i][j] =
              outputimage[i][j] +
              ((*it)->frame[i - (*it)->anchorx - (*it)->offsetx]
                           [j - (*it)->anchory - (*it)->offsety] -
               outputimage[i][j]) *
                  (*it)->fade_percentage;
        }
      }
    }
  }
  checkframerates(video_processor_object obj);
error:
}

int addimage(video_processor_object obj, frame, uint32_t index) {
  while (obj.frames[index]->isready)
    tra_sleep_millis(1);
  obj.frames[index]->frame = frame;
  obj.frames[index]->isready = true;
  bool isallready = true;
  for (std::vector<video_frame_settings *>::iterator it = obj.frames.begin();
       it != obj.frames.end(); it++) {
    if (!(*it)->isready)
      isallready = false;
  }
  if (isallready)
    combine(obj);
  error:

}




