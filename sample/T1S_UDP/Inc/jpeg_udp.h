#ifndef JPEG_UDP_H
#define JPEG_UDP_H

#include <stdint.h>

#define JPEG_UDP_PORT        5005U
#define JPEG_RX_MAX_BYTES    (64U * 1024U)

typedef enum
{
  JPEG_ORIENTATION_0 = 0U,
  JPEG_ORIENTATION_90_CW,
  JPEG_ORIENTATION_180,
  JPEG_ORIENTATION_270_CW
} JpegOrientation;

void JpegReceiver_Init(uint32_t localIp);
void JpegReceiver_HandleFrame(const uint8_t *frame, uint32_t length);
void JpegReceiver_Poll(void);
void JpegReceiver_CycleOrientation(void);

#endif
