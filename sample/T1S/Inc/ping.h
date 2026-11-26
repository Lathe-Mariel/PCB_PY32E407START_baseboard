/**
  ******************************************************************************
  * @file    ping.h
  * @brief   Minimal IPv4/ARP/ICMP echo client on top of the LAN8670 driver.
  *
  *          This is a deliberately small stack: it only supports what an ICMP
  *          ping needs on a single 10BASE-T1S segment.
  *            - ARP request/reply so the target's MAC address can be learned
  *            - IPv4 header build/parse (no fragmentation, no options)
  *            - ICMP echo request (type 8) and echo reply (type 0)
  *
  *          It also answers echo requests sent to the board, so a PC on the
  *          segment can ping the MCU in the other direction.
  ******************************************************************************
  */

#ifndef __PING_H
#define __PING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/

/**
  * @brief Network configuration for the ping client.
  *        IP addresses are in host order, build them with PING_IP4() in main.h.
  */
typedef struct
{
  const uint8_t *LocalMac;      /*!< our MAC address (6 bytes)             */
  uint32_t LocalIp;             /*!< our IPv4 address                      */
  uint32_t NetMask;             /*!< subnet mask                           */
  uint32_t GatewayIp;           /*!< default gateway (next hop off-subnet) */
  uint32_t TargetIp;            /*!< host to ping                          */
  uint32_t IntervalMs;          /*!< delay between echo requests           */
  uint32_t TimeoutMs;           /*!< echo reply timeout                    */
  uint32_t ArpTimeoutMs;        /*!< wait for one ARP reply                */
  uint8_t  ArpRetries;          /*!< ARP attempts before giving up         */
} Ping_Config;

typedef struct
{
  uint32_t RxIpv4;
  uint32_t RxMalformed;
  uint32_t RxNotForUs;
  uint32_t RxOtherProtocol;
  uint32_t RxEchoReply;
  uint32_t RxEchoRequest;
  uint32_t RxOtherIcmp;
  uint32_t RxBadIpChecksum;
  uint32_t RxBadIcmpChecksum;
  uint32_t ReplyNotWaiting;
  uint32_t ReplyBadId;
  uint32_t ReplyBadSequence;
  uint32_t ReplyBadSource;
  uint32_t ReplyAccepted;
} Ping_Diagnostics;

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Initialise the ping client.  The configuration structure is copied.
  */
void Ping_Init(const Ping_Config *Config);

/**
  * @brief  Copy and clear receive diagnostics collected since the last call.
  */
void Ping_GetAndClearDiagnostics(Ping_Diagnostics *Diagnostics);

/**
  * @brief  Feed one received Ethernet frame (14-byte header included).
  *         Call it for every frame returned by LAN8670_GetFrame().
  */
void Ping_HandleFrame(const uint8_t *Frame, uint32_t Length);

/**
  * @brief  Drive the state machine.  Call it from the main loop as often as
  *         possible; it is non-blocking.
  */
void Ping_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __PING_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
