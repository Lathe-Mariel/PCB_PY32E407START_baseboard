#ifndef JPEG_UDP_H
#define JPEG_UDP_H

#include <stdint.h>

#define JPEG_UDP_PORT        5005U
#define JPEG_RX_MAX_BYTES    (64U * 1024U)

void JpegReceiver_Init(uint32_t localIp);
void JpegReceiver_HandleFrame(const uint8_t *frame, uint32_t length);
void JpegReceiver_Poll(void);

#endif
