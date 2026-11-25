/**
  ******************************************************************************
  * @file    ping.c
  * @brief   Minimal ARP + IPv4 + ICMP echo implementation for 10BASE-T1S.
  *
  *          Frame flow:
  *            1. Ping_Poll() resolves the next hop (target or gateway) with ARP
  *            2. Once the MAC is known it sends an ICMP echo request
  *            3. Ping_HandleFrame() matches the echo reply and prints the RTT
  *
  *          The receive path also answers ARP requests and ICMP echo requests
  *          addressed to the board, so the ping works in both directions.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <string.h>

#include "main.h"
#include "lan8670.h"
#include "ping.h"

/* Private define ------------------------------------------------------------*/
#define ETH_HEADER_LEN                14U
#define ARP_PACKET_LEN                28U
#define IP_HEADER_LEN                 20U
#define ICMP_HEADER_LEN               8U
#define PING_PAYLOAD_LEN              32U
#define ETH_MIN_FRAME_LEN             60U   /* without the 4-byte FCS */

#define ETH_TYPE_IPV4                 0x0800U
#define ETH_TYPE_ARP                  0x0806U

#define ICMP_TYPE_ECHO_REPLY          0U
#define ICMP_TYPE_ECHO_REQUEST        8U

#define IP_PROTO_ICMP                 1U
#define PING_ID                       0x5059U /* "PY" */
#define PING_TTL                      64U

/* Private types -------------------------------------------------------------*/
typedef enum
{
  PING_STATE_IDLE = 0,
  PING_STATE_RESOLVING,
  PING_STATE_WAITING
} Ping_State;

/* Private variables ---------------------------------------------------------*/
static Ping_Config PingCfg;

static Ping_State PingState = PING_STATE_IDLE;

static uint8_t  TargetMac[6];          /* MAC of the current next hop      */
static uint8_t  TargetMacValid;        /* TargetMac[] holds a usable value */
static uint32_t NextHopIp;             /* IP the cached MAC belongs to     */

static uint32_t NextSendTick;          /* when the next request may start  */
static uint32_t ArpStartTick;          /* when the current ARP was sent    */
static uint32_t WaitStartTick;         /* when the echo request was sent   */
static uint8_t  ArpAttempts;
static uint16_t PingSeq;
static uint16_t IpId;

/* Private functions ---------------------------------------------------------*/

/* Big-endian helpers (network byte order). */
static void Put16(uint8_t *p, uint16_t v)
{
  p[0] = (uint8_t)(v >> 8U);
  p[1] = (uint8_t)(v & 0xFFU);
}

static void Put32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v >> 24U);
  p[1] = (uint8_t)(v >> 16U);
  p[2] = (uint8_t)(v >> 8U);
  p[3] = (uint8_t)(v & 0xFFU);
}

static uint16_t Get16(const uint8_t *p)
{
  return (uint16_t)(((uint16_t)p[0] << 8U) | (uint16_t)p[1]);
}

static uint32_t Get32(const uint8_t *p)
{
  return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
         ((uint32_t)p[2] << 8U)  |  (uint32_t)p[3];
}

/**
  * @brief  Internet checksum (RFC 1071), returned in network byte order when
  *         stored with Put16().
  */
static uint16_t Checksum(const uint8_t *Data, uint32_t Length)
{
  uint32_t sum = 0U;

  while (Length > 1U)
  {
    sum += ((uint32_t)Data[0] << 8U) | (uint32_t)Data[1];
    Data += 2U;
    Length -= 2U;
  }
  if (Length != 0U)
  {
    sum += (uint32_t)Data[0] << 8U;
  }

  while ((sum >> 16U) != 0U)
  {
    sum = (sum & 0xFFFFU) + (sum >> 16U);
  }

  return (uint16_t)(~sum & 0xFFFFU);
}

static uint8_t IsBroadcast(const uint8_t *Mac)
{
  uint32_t i;

  for (i = 0U; i < 6U; i++)
  {
    if (Mac[i] != 0xFFU)
    {
      return 0U;
    }
  }
  return 1U;
}

static void PrintIp(uint32_t Ip)
{
  Console_Printf("%u.%u.%u.%u",
                 (unsigned)((Ip >> 24U) & 0xFFU),
                 (unsigned)((Ip >> 16U) & 0xFFU),
                 (unsigned)((Ip >> 8U) & 0xFFU),
                 (unsigned)(Ip & 0xFFU));
}

static void PrintMac(const uint8_t *Mac)
{
  Console_Printf("%02X:%02X:%02X:%02X:%02X:%02X",
                 (unsigned)Mac[0], (unsigned)Mac[1], (unsigned)Mac[2],
                 (unsigned)Mac[3], (unsigned)Mac[4], (unsigned)Mac[5]);
}

/**
  * @brief  Build and transmit one ARP packet.
  * @param  Oper     1 = request, 2 = reply
  * @param  DstMac   Ethernet destination
  * @param  TargetHw hardware address field in the ARP payload (zeros on request)
  * @param  TargetIp target protocol address
  */
static HAL_StatusTypeDef SendArp(uint16_t Oper, const uint8_t *DstMac,
                                 const uint8_t *TargetHw, uint32_t TargetIp)
{
  uint8_t  frame[ETH_MIN_FRAME_LEN];
  uint8_t *arp = &frame[ETH_HEADER_LEN];
  HAL_StatusTypeDef status;

  memset(frame, 0, sizeof(frame));

  memcpy(&frame[0], DstMac, 6U);
  memcpy(&frame[6], PingCfg.LocalMac, 6U);
  Put16(&frame[12], ETH_TYPE_ARP);

  Put16(&arp[0], 0x0001U);              /* hardware type: Ethernet        */
  Put16(&arp[2], ETH_TYPE_IPV4);        /* protocol type: IPv4            */
  arp[4] = 6U;                          /* hardware address length        */
  arp[5] = 4U;                          /* protocol address length        */
  Put16(&arp[6], Oper);
  memcpy(&arp[8], PingCfg.LocalMac, 6U);/* sender hardware address        */
  Put32(&arp[14], PingCfg.LocalIp);     /* sender protocol address        */
  memcpy(&arp[18], TargetHw, 6U);       /* target hardware address        */
  Put32(&arp[24], TargetIp);            /* target protocol address        */

  status = LAN8670_SendFrame(frame, sizeof(frame));
  if (status == HAL_OK)
  {
    HAL_GPIO_TogglePin(LED_PORT, LED_D3_PIN);
  }

  return status;
}

static HAL_StatusTypeDef SendArpRequest(uint32_t TargetIp)
{
  static const uint8_t broadcast[6] = {0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU};
  static const uint8_t zeroMac[6]   = {0U, 0U, 0U, 0U, 0U, 0U};

  return SendArp(0x0001U, broadcast, zeroMac, TargetIp);
}

static HAL_StatusTypeDef SendArpReply(const uint8_t *ToMac, uint32_t ToIp)
{
  return SendArp(0x0002U, ToMac, ToMac, ToIp);
}

static void PrintTxFailure(const char *FrameType, HAL_StatusTypeDef Status)
{
  Console_Printf("MAC: %s not queued (%s)\r\n", FrameType,
                 (Status == HAL_BUSY) ? "TX descriptor busy" : "TX error");
}

/**
  * @brief  Build and transmit an ICMP echo request to the configured target.
  * @retval HAL_OK when the frame was queued, otherwise the transmit status
  */
static HAL_StatusTypeDef SendEchoRequest(void)
{
  uint8_t  frame[ETH_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN + PING_PAYLOAD_LEN];
  uint8_t *ip   = &frame[ETH_HEADER_LEN];
  uint8_t *icmp = &ip[IP_HEADER_LEN];
  const uint32_t IcmpLen = ICMP_HEADER_LEN + PING_PAYLOAD_LEN;
  const uint32_t IpLen   = IP_HEADER_LEN + IcmpLen;
  uint16_t seq = (uint16_t)(PingSeq + 1U);
  uint32_t i;
  HAL_StatusTypeDef status;

  /* Ethernet header */
  memcpy(&frame[0], TargetMac, 6U);
  memcpy(&frame[6], PingCfg.LocalMac, 6U);
  Put16(&frame[12], ETH_TYPE_IPV4);

  /* IPv4 header */
  memset(ip, 0, IP_HEADER_LEN);
  ip[0] = 0x45U;                        /* version 4, header length 5     */
  ip[1] = 0x00U;                        /* DSCP / ECN                     */
  Put16(&ip[2], (uint16_t)IpLen);       /* total length                   */
  Put16(&ip[4], ++IpId);                /* identification                 */
  Put16(&ip[6], 0x0000U);               /* flags / fragment offset        */
  ip[8] = PING_TTL;
  ip[9] = IP_PROTO_ICMP;
  Put16(&ip[10], 0U);                   /* checksum placeholder           */
  Put32(&ip[12], PingCfg.LocalIp);
  Put32(&ip[16], PingCfg.TargetIp);
  Put16(&ip[10], Checksum(ip, IP_HEADER_LEN));

  /* ICMP echo request */
  icmp[0] = ICMP_TYPE_ECHO_REQUEST;
  icmp[1] = 0U;
  Put16(&icmp[2], 0U);                  /* checksum placeholder           */
  Put16(&icmp[4], PING_ID);
  Put16(&icmp[6], seq);
  for (i = 0U; i < PING_PAYLOAD_LEN; i++)
  {
    icmp[ICMP_HEADER_LEN + i] = (uint8_t)('a' + (i % 26U));
  }
  Put16(&icmp[2], Checksum(icmp, IcmpLen));

  status = LAN8670_SendFrame(frame, sizeof(frame));
  if (status != HAL_OK)
  {
    return status;
  }

  PingSeq       = seq;
  WaitStartTick = HAL_GetTick();
  PingState     = PING_STATE_WAITING;

  HAL_GPIO_TogglePin(LED_PORT, LED_D3_PIN);
  return HAL_OK;
}

/**
  * @brief  Answer an ICMP echo request addressed to this board.
  */
static void SendEchoReply(const uint8_t *ReqFrame, uint32_t ReqLength, uint32_t Ihl)
{
  uint8_t  frame[ETH_MAX_PACKET_SIZE];
  const uint8_t *reqIp = &ReqFrame[ETH_HEADER_LEN];
  uint8_t *ip;
  uint8_t *icmp;
  uint32_t total;
  uint32_t icmpLen;
  uint32_t txLen;
  HAL_StatusTypeDef status;

  if (ReqLength < (ETH_HEADER_LEN + Ihl + ICMP_HEADER_LEN))
  {
    return;
  }

  total = Get16(&reqIp[2]);
  if (total > (ReqLength - ETH_HEADER_LEN))
  {
    total = ReqLength - ETH_HEADER_LEN;
  }
  if (total > (sizeof(frame) - ETH_HEADER_LEN))
  {
    total = (uint32_t)(sizeof(frame) - ETH_HEADER_LEN);
  }
  if (total < (Ihl + ICMP_HEADER_LEN))
  {
    return;
  }
  icmpLen = total - Ihl;

  /* Ethernet header: reply to the sender */
  memcpy(&frame[0], &ReqFrame[6], 6U);
  memcpy(&frame[6], PingCfg.LocalMac, 6U);
  Put16(&frame[12], ETH_TYPE_IPV4);

  /* IPv4 header: swap source and destination */
  ip = &frame[ETH_HEADER_LEN];
  memcpy(ip, reqIp, Ihl);
  Put16(&ip[2], (uint16_t)total);
  Put32(&ip[12], PingCfg.LocalIp);
  Put32(&ip[16], Get32(&reqIp[12]));
  ip[8] = PING_TTL;
  Put16(&ip[10], 0U);
  Put16(&ip[10], Checksum(ip, Ihl));

  /* ICMP: keep identifier/payload, flip the type to echo reply */
  icmp = &ip[Ihl];
  memcpy(icmp, &reqIp[Ihl], icmpLen);
  icmp[0] = ICMP_TYPE_ECHO_REPLY;
  icmp[1] = 0U;
  Put16(&icmp[2], 0U);
  Put16(&icmp[2], Checksum(icmp, icmpLen));

  /* Pad to the Ethernet minimum (padding is outside the IP datagram) */
  txLen = ETH_HEADER_LEN + total;
  while ((txLen < ETH_MIN_FRAME_LEN) && (txLen < sizeof(frame)))
  {
    frame[txLen] = 0U;
    txLen++;
  }

  status = LAN8670_SendFrame(frame, txLen);
  if (status == HAL_OK)
  {
    HAL_GPIO_TogglePin(LED_PORT, LED_D3_PIN);
  }
  else
  {
    PrintTxFailure("ICMP echo reply", status);
  }
}

/**
  * @brief  Handle an ARP packet.
  */
static void HandleArp(const uint8_t *Frame, uint32_t Length)
{
  const uint8_t *arp = &Frame[ETH_HEADER_LEN];
  const uint8_t *senderHw = &arp[8];
  uint32_t senderIp = Get32(&arp[14]);
  uint32_t targetIp = Get32(&arp[24]);
  uint16_t oper     = Get16(&arp[6]);
  HAL_StatusTypeDef status;

  (void)Length;

  if (oper == 0x0001U)
  {
    /* Someone asks for our address: answer and let the caller learn us. */
    if (targetIp == PingCfg.LocalIp)
    {
      status = SendArpReply(senderHw, senderIp);
      if (status != HAL_OK)
      {
        PrintTxFailure("ARP reply", status);
      }
    }
  }
  else if (oper == 0x0002U)
  {
    /* Reply to the ARP request currently in flight */
    if ((PingState == PING_STATE_RESOLVING) && (senderIp == NextHopIp))
    {
      memcpy(TargetMac, senderHw, 6U);
      TargetMacValid = 1U;

      Console_Write("ARP: ");
      PrintIp(senderIp);
      Console_Write(" is at ");
      PrintMac(senderHw);
      Console_Write("\r\n");

      status = SendEchoRequest();
      if (status != HAL_OK)
      {
        PrintTxFailure("ICMP echo request", status);
        PingState    = PING_STATE_IDLE;
        NextSendTick = HAL_GetTick() + PingCfg.IntervalMs;
      }
    }
  }
}

/**
  * @brief  Handle an ICMP echo reply or request.
  */
static void HandleIpv4(const uint8_t *Frame, uint32_t Length)
{
  const uint8_t *ip = &Frame[ETH_HEADER_LEN];
  const uint8_t *icmp;
  uint32_t ipLength = Length - ETH_HEADER_LEN;
  uint32_t ihl;
  uint32_t total;
  uint32_t icmpLen;

  if (ipLength < IP_HEADER_LEN)
  {
    return;
  }

  ihl = (uint32_t)(ip[0] & 0x0FU) * 4U;
  if ((ihl < IP_HEADER_LEN) || (ipLength < (ihl + ICMP_HEADER_LEN)))
  {
    return;
  }

  /* Only frames addressed to us */
  if (Get32(&ip[16]) != PingCfg.LocalIp)
  {
    return;
  }
  if (ip[9] != IP_PROTO_ICMP)
  {
    return;
  }

  /* The MAC/DMA no longer drops frames on a checksum error, so verify the
     IPv4 header here and flag anything that really is corrupt. */
  if (Checksum(ip, ihl) != 0U)
  {
    Console_Printf("IPv4: bad header checksum (type=%u seq=%u)\r\n",
                   (unsigned)ip[ihl], (unsigned)Get16(&ip[ihl + 6U]));
  }

  total = Get16(&ip[2]);
  if ((total < (ihl + ICMP_HEADER_LEN)) || (total > ipLength))
  {
    total = ipLength;
  }
  icmpLen = total - ihl;
  icmp = &ip[ihl];

  if (icmp[0] == ICMP_TYPE_ECHO_REPLY)
  {
    if ((PingState != PING_STATE_WAITING) ||
        (Get16(&icmp[4]) != PING_ID) ||
        (Get16(&icmp[6]) != PingSeq))
    {
      return;
    }
    if (Get32(&ip[12]) != PingCfg.TargetIp)
    {
      return;
    }

    Console_Write("Reply from ");
    PrintIp(Get32(&ip[12]));
    Console_Printf(": bytes=%u time=%lums ttl=%u seq=%u\r\n",
                   (unsigned)(icmpLen - ICMP_HEADER_LEN),
                   (unsigned long)(HAL_GetTick() - WaitStartTick),
                   (unsigned)ip[8],
                   (unsigned)Get16(&icmp[6]));

    PingState    = PING_STATE_IDLE;
    NextSendTick = HAL_GetTick() + PingCfg.IntervalMs;
  }
  else if (icmp[0] == ICMP_TYPE_ECHO_REQUEST)
  {
    SendEchoReply(Frame, Length, ihl);
  }
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialise the ping client.
  */
void Ping_Init(const Ping_Config *Config)
{
  if (Config == NULL)
  {
    return;
  }

  PingCfg = *Config;

  PingState      = PING_STATE_IDLE;
  TargetMacValid = 0U;
  NextHopIp      = 0U;
  PingSeq        = 0U;
  IpId           = 0U;
  ArpAttempts    = 0U;
  NextSendTick   = HAL_GetTick();       /* first request immediately */
}

/**
  * @brief  Demultiplex one received Ethernet frame.
  */
void Ping_HandleFrame(const uint8_t *Frame, uint32_t Length)
{
  uint16_t etherType;
  uint8_t  forUs;

  if ((Frame == NULL) || (Length < ETH_HEADER_LEN))
  {
    return;
  }

  etherType = Get16(&Frame[12]);
  forUs = ((memcmp(&Frame[0], PingCfg.LocalMac, 6U) == 0) ||
           (IsBroadcast(&Frame[0]) != 0U)) ? 1U : 0U;

  if (etherType == ETH_TYPE_ARP)
  {
    if ((forUs != 0U) && (Length >= (ETH_HEADER_LEN + ARP_PACKET_LEN)))
    {
      HandleArp(Frame, Length);
    }
  }
  else if (etherType == ETH_TYPE_IPV4)
  {
    HandleIpv4(Frame, Length);
  }
}

/**
  * @brief  Non-blocking ping state machine.
  */
void Ping_Poll(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t nextHop;
  HAL_StatusTypeDef status;

  if (PingState == PING_STATE_WAITING)
  {
    if ((now - WaitStartTick) >= PingCfg.TimeoutMs)
    {
      Console_Printf("Request timeout: seq=%u\r\n", (unsigned)PingSeq);
      PingState    = PING_STATE_IDLE;
      NextSendTick = now + PingCfg.IntervalMs;
    }
    return;
  }

  if (PingState == PING_STATE_RESOLVING)
  {
    if ((now - ArpStartTick) >= PingCfg.ArpTimeoutMs)
    {
      if (ArpAttempts < PingCfg.ArpRetries)
      {
        ArpAttempts++;
        ArpStartTick = now;
        status = SendArpRequest(NextHopIp);
        if (status != HAL_OK)
        {
          PrintTxFailure("ARP request", status);
        }
      }
      else
      {
        Console_Write("ARP timeout: no reply from ");
        PrintIp(NextHopIp);
        Console_Write("\r\n");
        PingState    = PING_STATE_IDLE;
        NextSendTick = now + PingCfg.IntervalMs;
      }
    }
    return;
  }

  /* PING_STATE_IDLE */
  if ((int32_t)(now - NextSendTick) < 0)
  {
    return;
  }

  /* Use the target directly when it is on our subnet, the gateway otherwise */
  if ((PingCfg.TargetIp & PingCfg.NetMask) == (PingCfg.LocalIp & PingCfg.NetMask))
  {
    nextHop = PingCfg.TargetIp;
  }
  else
  {
    nextHop = PingCfg.GatewayIp;
  }

  if ((TargetMacValid != 0U) && (NextHopIp == nextHop))
  {
    status = SendEchoRequest();
    if (status != HAL_OK)
    {
      PrintTxFailure("ICMP echo request", status);
      NextSendTick = now + PingCfg.IntervalMs;
    }
  }
  else
  {
    NextHopIp   = nextHop;
    TargetMacValid = 0U;
    ArpAttempts = 0U;
    ArpStartTick = now;
    PingState   = PING_STATE_RESOLVING;

    Console_Write("ARP: resolving ");
    PrintIp(nextHop);
    Console_Write(" ...\r\n");
    status = SendArpRequest(nextHop);
    if (status != HAL_OK)
    {
      PrintTxFailure("ARP request", status);
    }
  }
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
