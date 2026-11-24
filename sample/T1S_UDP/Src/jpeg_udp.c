#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "jpeg_udp.h"
#include "lcd_spi.h"
#include "main.h"
#include "tjpgd.h"

#define ETH_HEADER_LEN             14U
#define IPV4_HEADER_MIN_LEN        20U
#define UDP_HEADER_LEN             8U
#define JPEG_PACKET_HEADER_LEN     16U
#define JPEG_CHUNK_DATA_LEN        1200U
#define JPEG_ROTATE_TILE_MAX       32U
#define JPEG_MAX_CHUNKS            ((JPEG_RX_MAX_BYTES + JPEG_CHUNK_DATA_LEN - 1U) / JPEG_CHUNK_DATA_LEN)
#define JPEG_CHUNK_BITMAP_SIZE     ((JPEG_MAX_CHUNKS + 7U) / 8U)
#define JPEG_TRANSFER_TIMEOUT_MS  5000U
#define IPV4_PROTOCOL_UDP          17U

typedef struct
{
  uint32_t inputOffset;
  uint16_t offsetX;
  uint16_t offsetY;
  uint16_t sourceWidth;
  uint16_t sourceHeight;
  uint16_t rotatedWidth;
  uint16_t rotatedHeight;
  uint16_t displayWidth;
  uint16_t displayHeight;
  uint16_t scaleNumerator;
  uint16_t scaleDenominator;
  JpegOrientation orientation;
} JpegContext;

static uint8_t JpegBuffer[JPEG_RX_MAX_BYTES];
static uint8_t ChunkBitmap[JPEG_CHUNK_BITMAP_SIZE];
static uint32_t LocalIp;
static uint32_t ActiveFrameId;
static uint32_t ActiveLength;
static uint32_t LastPacketTick;
static uint16_t ActiveChunkCount;
static uint16_t ReceivedChunkCount;
static uint8_t TransferActive;
static JpegOrientation DisplayOrientation = JPEG_ORIENTATION_90_CW;
static JDEC Decoder;
static uint32_t DecoderWork[1024U];
static uint8_t RotatedTile[JPEG_ROTATE_TILE_MAX * JPEG_ROTATE_TILE_MAX * 3U];

static const char *JpegOrientationName(JpegOrientation orientation)
{
  switch (orientation)
  {
    case JPEG_ORIENTATION_0:
      return "0 degrees";
    case JPEG_ORIENTATION_90_CW:
      return "90 degrees CW";
    case JPEG_ORIENTATION_180:
      return "180 degrees";
    case JPEG_ORIENTATION_270_CW:
      return "270 degrees CW";
    default:
      return "unknown";
  }
}

static uint32_t ScaleCeil(const JpegContext *context, uint32_t coordinate)
{
  return (coordinate * context->scaleNumerator +
          context->scaleDenominator - 1U) / context->scaleDenominator;
}

static uint8_t JpegGetRotatedRect(const JpegContext *context, const JRECT *rect,
                                  uint32_t *left, uint32_t *top,
                                  uint32_t *right, uint32_t *bottom)
{
  switch (context->orientation)
  {
    case JPEG_ORIENTATION_0:
      *left = rect->left;
      *top = rect->top;
      *right = (uint32_t)rect->right + 1U;
      *bottom = (uint32_t)rect->bottom + 1U;
      break;
    case JPEG_ORIENTATION_90_CW:
      *left = context->sourceHeight - ((uint32_t)rect->bottom + 1U);
      *top = rect->left;
      *right = context->sourceHeight - rect->top;
      *bottom = (uint32_t)rect->right + 1U;
      break;
    case JPEG_ORIENTATION_180:
      *left = context->sourceWidth - ((uint32_t)rect->right + 1U);
      *top = context->sourceHeight - ((uint32_t)rect->bottom + 1U);
      *right = context->sourceWidth - rect->left;
      *bottom = context->sourceHeight - rect->top;
      break;
    case JPEG_ORIENTATION_270_CW:
      *left = rect->top;
      *top = context->sourceWidth - ((uint32_t)rect->right + 1U);
      *right = (uint32_t)rect->bottom + 1U;
      *bottom = context->sourceWidth - rect->left;
      break;
    default:
      return 0U;
  }
  return 1U;
}

static uint8_t JpegMapRotatedPixel(const JpegContext *context,
                                  uint32_t rotatedX, uint32_t rotatedY,
                                  uint32_t *sourceX, uint32_t *sourceY)
{
  if ((rotatedX >= context->rotatedWidth) ||
      (rotatedY >= context->rotatedHeight))
  {
    return 0U;
  }

  switch (context->orientation)
  {
    case JPEG_ORIENTATION_0:
      *sourceX = rotatedX;
      *sourceY = rotatedY;
      break;
    case JPEG_ORIENTATION_90_CW:
      *sourceX = rotatedY;
      *sourceY = context->sourceHeight - 1U - rotatedX;
      break;
    case JPEG_ORIENTATION_180:
      *sourceX = context->sourceWidth - 1U - rotatedX;
      *sourceY = context->sourceHeight - 1U - rotatedY;
      break;
    case JPEG_ORIENTATION_270_CW:
      *sourceX = context->sourceWidth - 1U - rotatedY;
      *sourceY = rotatedX;
      break;
    default:
      return 0U;
  }
  return 1U;
}

static uint16_t Read16(const uint8_t *p)
{
  return (uint16_t)(((uint16_t)p[0] << 8U) | p[1]);
}

static uint32_t Read32(const uint8_t *p)
{
  return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
         ((uint32_t)p[2] << 8U) | p[3];
}

static void Write16(uint8_t *p, uint16_t value)
{
  p[0] = (uint8_t)(value >> 8U);
  p[1] = (uint8_t)value;
}

static uint32_t ChecksumSum(uint32_t sum, const uint8_t *data, uint32_t length)
{
  while (length > 1U)
  {
    sum += ((uint32_t)data[0] << 8U) | data[1];
    data += 2U;
    length -= 2U;
  }
  if (length != 0U)
  {
    sum += (uint32_t)data[0] << 8U;
  }
  return sum;
}

static uint16_t Checksum(const uint8_t *data, uint32_t length)
{
  uint32_t sum = ChecksumSum(0U, data, length);

  while ((sum >> 16U) != 0U)
  {
    sum = (sum & 0xFFFFU) + (sum >> 16U);
  }
  return (uint16_t)(~sum & 0xFFFFU);
}

static uint8_t UdpChecksumValid(const uint8_t *ip, const uint8_t *udp,
                                uint16_t udpLength)
{
  uint8_t pseudoHeader[12];
  uint32_t sum;

  memcpy(&pseudoHeader[0], &ip[12], 8U);
  pseudoHeader[8] = 0U;
  pseudoHeader[9] = IPV4_PROTOCOL_UDP;
  Write16(&pseudoHeader[10], udpLength);

  sum = ChecksumSum(0U, pseudoHeader, sizeof(pseudoHeader));
  sum = ChecksumSum(sum, udp, udpLength);
  while ((sum >> 16U) != 0U)
  {
    sum = (sum & 0xFFFFU) + (sum >> 16U);
  }
  return (sum == 0xFFFFU) ? 1U : 0U;
}

static void ResetTransfer(void)
{
  TransferActive = 0U;
  ActiveLength = 0U;
  ActiveChunkCount = 0U;
  ReceivedChunkCount = 0U;
  memset(ChunkBitmap, 0, sizeof(ChunkBitmap));
}

static size_t JpegInputFunction(JDEC *decoder, uint8_t *buffer, size_t length)
{
  JpegContext *context = (JpegContext *)decoder->device;
  uint32_t remaining;

  if ((context == NULL) || (context->inputOffset > ActiveLength))
  {
    return 0U;
  }
  remaining = ActiveLength - context->inputOffset;
  if (length > remaining)
  {
    length = remaining;
  }

  if (buffer != NULL)
  {
    memcpy(buffer, &JpegBuffer[context->inputOffset], length);
  }
  context->inputOffset += (uint32_t)length;
  return length;
}

static int JpegOutputFunction(JDEC *decoder, void *bitmap, JRECT *rect)
{
  const JpegContext *context = (const JpegContext *)decoder->device;
  const uint8_t *sourcePixels = (const uint8_t *)bitmap;
  const uint16_t sourceTileWidth =
      (uint16_t)(rect->right - rect->left + 1U);
  const uint16_t sourceTileHeight =
      (uint16_t)(rect->bottom - rect->top + 1U);
  uint32_t rotatedLeft;
  uint32_t rotatedTop;
  uint32_t rotatedRight;
  uint32_t rotatedBottom;
  uint32_t destX;
  uint32_t destY;
  uint32_t destXEnd;
  uint32_t destYEnd;
  uint16_t destWidth;
  uint16_t destHeight;
  uint16_t row;
  uint16_t column;

  if (JpegGetRotatedRect(context, rect, &rotatedLeft, &rotatedTop,
                         &rotatedRight, &rotatedBottom) == 0U)
  {
    return 0;
  }
  destX = context->offsetX + ScaleCeil(context, rotatedLeft);
  destY = context->offsetY + ScaleCeil(context, rotatedTop);
  destXEnd = context->offsetX + ScaleCeil(context, rotatedRight);
  destYEnd = context->offsetY + ScaleCeil(context, rotatedBottom);
  if ((destXEnd <= destX) || (destYEnd <= destY))
  {
    return 0;
  }
  destWidth = (uint16_t)(destXEnd - destX);
  destHeight = (uint16_t)(destYEnd - destY);

  if ((sourceTileWidth > JPEG_ROTATE_TILE_MAX) ||
      (sourceTileHeight > JPEG_ROTATE_TILE_MAX) ||
      (destWidth > JPEG_ROTATE_TILE_MAX) ||
      (destHeight > JPEG_ROTATE_TILE_MAX) ||
      (destWidth == 0U) || (destHeight == 0U))
  {
    return 0;
  }

  for (row = 0U; row < destHeight; row++)
  {
    const uint32_t rotatedY =
        ((destY + row - context->offsetY) * context->scaleDenominator) /
        context->scaleNumerator;
    for (column = 0U; column < destWidth; column++)
    {
      const uint32_t rotatedX =
          ((destX + column - context->offsetX) * context->scaleDenominator) /
          context->scaleNumerator;
      uint32_t sourceX;
      uint32_t sourceY;
      uint32_t sourceOffset;
      const uint32_t destOffset = ((uint32_t)row * destWidth + column) * 3U;

      if (JpegMapRotatedPixel(context, rotatedX, rotatedY,
                              &sourceX, &sourceY) == 0U)
      {
        return 0;
      }
      if ((sourceX < rect->left) || (sourceX > rect->right) ||
          (sourceY < rect->top) || (sourceY > rect->bottom))
      {
        return 0;
      }
      sourceOffset = ((sourceY - rect->top) * sourceTileWidth +
                      (sourceX - rect->left)) * 3U;
      memcpy(&RotatedTile[destOffset], &sourcePixels[sourceOffset], 3U);
    }
  }

  LCD_SPI_DrawRgb888((uint16_t)destX, (uint16_t)destY,
                     destWidth, destHeight, RotatedTile);
  return 1;
}

static void DecodeAndDisplay(uint32_t frameId, uint32_t imageLength)
{
  JpegContext context = {0};
  JRESULT result;
  uint8_t scale;
  uint16_t outputWidth = 0U;
  uint16_t outputHeight = 0U;

  result = jd_prepare(&Decoder, JpegInputFunction, DecoderWork,
                      sizeof(DecoderWork), &context);
  if (result != JDR_OK)
  {
    Console_Printf("JPEG: header/decode preparation failed (rc=%u)\r\n",
                   (unsigned)result);
    return;
  }

  for (scale = 0U; scale <= 3U; scale++)
  {
    outputWidth = (uint16_t)(Decoder.width >> scale);
    outputHeight = (uint16_t)(Decoder.height >> scale);
    if ((outputWidth <= LCD_SPI_WIDTH) && (outputHeight <= LCD_SPI_HEIGHT) &&
        (outputWidth != 0U) && (outputHeight != 0U))
    {
      break;
    }
  }
  if (scale > 3U)
  {
    Console_Printf("JPEG: image too large (%u x %u)\r\n",
                   (unsigned)Decoder.width, (unsigned)Decoder.height);
    return;
  }

  context.orientation = DisplayOrientation;
  context.sourceWidth = outputWidth;
  context.sourceHeight = outputHeight;
  if ((context.orientation == JPEG_ORIENTATION_90_CW) ||
      (context.orientation == JPEG_ORIENTATION_270_CW))
  {
    context.rotatedWidth = outputHeight;
    context.rotatedHeight = outputWidth;
  }
  else
  {
    context.rotatedWidth = outputWidth;
    context.rotatedHeight = outputHeight;
  }
  context.scaleNumerator = 1U;
  context.scaleDenominator = 1U;
  if ((context.rotatedWidth > LCD_SPI_WIDTH) ||
      (context.rotatedHeight > LCD_SPI_HEIGHT))
  {
    if (((uint32_t)LCD_SPI_WIDTH * context.rotatedHeight) <=
        ((uint32_t)LCD_SPI_HEIGHT * context.rotatedWidth))
    {
      context.scaleNumerator = LCD_SPI_WIDTH;
      context.scaleDenominator = context.rotatedWidth;
    }
    else
    {
      context.scaleNumerator = LCD_SPI_HEIGHT;
      context.scaleDenominator = context.rotatedHeight;
    }
  }
  context.displayWidth =
      (uint16_t)ScaleCeil(&context, context.rotatedWidth);
  context.displayHeight =
      (uint16_t)ScaleCeil(&context, context.rotatedHeight);
  context.offsetX = (uint16_t)((LCD_SPI_WIDTH - context.displayWidth) / 2U);
  context.offsetY = (uint16_t)((LCD_SPI_HEIGHT - context.displayHeight) / 2U);
  LCD_SPI_Clear();
  result = jd_decomp(&Decoder, JpegOutputFunction, scale);
  if (result != JDR_OK)
  {
    Console_Printf("JPEG: decode failed (rc=%u)\r\n", (unsigned)result);
    return;
  }
  Console_Printf("JPEG: displayed frame %lu (%lu bytes, %u x %u, %s)\r\n",
                 (unsigned long)frameId, (unsigned long)imageLength,
                 (unsigned)context.displayWidth,
                 (unsigned)context.displayHeight,
                 JpegOrientationName(context.orientation));
}

static void HandleUdpPayload(const uint8_t *payload, uint16_t payloadLength)
{
  uint32_t frameId;
  uint32_t imageLength;
  uint32_t chunkOffset;
  uint16_t chunkIndex;
  uint16_t chunkCount;
  uint16_t expectedChunkLength;
  uint8_t bitMask;
  uint8_t *bitmapByte;

  if ((payloadLength < JPEG_PACKET_HEADER_LEN) ||
      (memcmp(payload, "JPG1", 4U) != 0))
  {
    return;
  }

  frameId = Read32(&payload[4]);
  imageLength = Read32(&payload[8]);
  chunkIndex = Read16(&payload[12]);
  chunkCount = Read16(&payload[14]);
  if ((imageLength == 0U) || (imageLength > JPEG_RX_MAX_BYTES))
  {
    return;
  }

  if (chunkCount != (uint16_t)((imageLength + JPEG_CHUNK_DATA_LEN - 1U) /
                               JPEG_CHUNK_DATA_LEN) ||
      (chunkCount == 0U) || (chunkCount > JPEG_MAX_CHUNKS) ||
      (chunkIndex >= chunkCount))
  {
    return;
  }

  chunkOffset = (uint32_t)chunkIndex * JPEG_CHUNK_DATA_LEN;
  expectedChunkLength = (uint16_t)(((imageLength - chunkOffset) > JPEG_CHUNK_DATA_LEN)
                                     ? JPEG_CHUNK_DATA_LEN
                                     : (imageLength - chunkOffset));
  if (payloadLength != (uint16_t)(JPEG_PACKET_HEADER_LEN + expectedChunkLength))
  {
    return;
  }

  if ((TransferActive == 0U) || (frameId != ActiveFrameId))
  {
    ResetTransfer();
    TransferActive = 1U;
    ActiveFrameId = frameId;
    ActiveLength = imageLength;
    ActiveChunkCount = chunkCount;
  }
  else if ((imageLength != ActiveLength) || (chunkCount != ActiveChunkCount))
  {
    return;
  }

  LastPacketTick = HAL_GetTick();
  bitmapByte = &ChunkBitmap[chunkIndex >> 3U];
  bitMask = (uint8_t)(1U << (chunkIndex & 7U));
  if ((*bitmapByte & bitMask) == 0U)
  {
    memcpy(&JpegBuffer[chunkOffset], &payload[JPEG_PACKET_HEADER_LEN],
           expectedChunkLength);
    *bitmapByte |= bitMask;
    ReceivedChunkCount++;
  }

  if (ReceivedChunkCount == ActiveChunkCount)
  {
    TransferActive = 0U;
    DecodeAndDisplay(frameId, imageLength);
  }
}

void JpegReceiver_Init(uint32_t localIp)
{
  LocalIp = localIp;
  DisplayOrientation = JPEG_ORIENTATION_90_CW;
  ResetTransfer();
  Console_Printf("LCD: image orientation %s (USER button cycles)\r\n",
                 JpegOrientationName(DisplayOrientation));
}

void JpegReceiver_CycleOrientation(void)
{
  if (DisplayOrientation == JPEG_ORIENTATION_270_CW)
  {
    DisplayOrientation = JPEG_ORIENTATION_0;
  }
  else
  {
    DisplayOrientation = (JpegOrientation)(DisplayOrientation + 1);
  }
  Console_Printf("LCD: image orientation %s (applies to next frame)\r\n",
                 JpegOrientationName(DisplayOrientation));
}

void JpegReceiver_HandleFrame(const uint8_t *frame, uint32_t length)
{
  const uint8_t *ip;
  const uint8_t *udp;
  uint32_t ipLength;
  uint32_t headerLength;
  uint16_t totalLength;
  uint16_t udpLength;
  uint16_t payloadLength;

  if ((frame == NULL) || (length < (ETH_HEADER_LEN + IPV4_HEADER_MIN_LEN)))
  {
    return;
  }

  ip = &frame[ETH_HEADER_LEN];
  if ((ip[0] >> 4U) != 4U)
  {
    return;
  }
  headerLength = (uint32_t)(ip[0] & 0x0FU) * 4U;
  ipLength = length - ETH_HEADER_LEN;
  if ((headerLength < IPV4_HEADER_MIN_LEN) || (headerLength > ipLength) ||
      (ip[9] != IPV4_PROTOCOL_UDP) || (Read32(&ip[16]) != LocalIp) ||
      ((Read16(&ip[6]) & 0x3FFFU) != 0U) ||
      (Checksum(ip, headerLength) != 0U))
  {
    return;
  }

  totalLength = Read16(&ip[2]);
  if ((totalLength < (headerLength + UDP_HEADER_LEN)) || (totalLength > ipLength))
  {
    return;
  }

  udp = &ip[headerLength];
  udpLength = Read16(&udp[4]);
  if ((udpLength < UDP_HEADER_LEN) ||
      (udpLength > (uint16_t)(totalLength - headerLength)) ||
      (Read16(&udp[2]) != JPEG_UDP_PORT))
  {
    return;
  }
  if ((Read16(&udp[6]) != 0U) && (UdpChecksumValid(ip, udp, udpLength) == 0U))
  {
    return;
  }

  payloadLength = (uint16_t)(udpLength - UDP_HEADER_LEN);
  HandleUdpPayload(&udp[UDP_HEADER_LEN], payloadLength);
}

void JpegReceiver_Poll(void)
{
  if ((TransferActive != 0U) &&
      ((HAL_GetTick() - LastPacketTick) >= JPEG_TRANSFER_TIMEOUT_MS))
  {
    Console_Printf("JPEG: incomplete transfer discarded (%u/%u chunks)\r\n",
                   (unsigned)ReceivedChunkCount, (unsigned)ActiveChunkCount);
    ResetTransfer();
  }
}
