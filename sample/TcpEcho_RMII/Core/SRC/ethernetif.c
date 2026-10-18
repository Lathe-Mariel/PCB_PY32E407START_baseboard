/**
  ******************************************************************************
  * @file    ethernetif.c
  * @author  MCU Application Team
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by Puya under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"
#include "ethernetif.h"
#include <string.h>
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Network interface name */
#define IFNAME0 'p'
#define IFNAME1 'y'

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
struct ethernetif {
  struct eth_addr *ethaddr;
/* Add whatever per-interface state that is needed here. */
};

extern ETH_HandleTypeDef heth;

#if !NO_SYS
/* RTOS mode (NO_SYS=0) specific: ETH RX semaphore + TX mutex semaphore.
   Refer to SDK example LwIP_HTTP_Server_Netconn_RTOS User/arch/ethernetif.c */
xSemaphoreHandle s_xSemaphore = NULL;

sys_sem_t tx_sem = NULL;
sys_mbox_t eth_tx_mb = NULL;
#endif /* !NO_SYS */

#if defined ( __ICCARM__ )
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef  DMARxDscrTab[ETH_RXBUFNB] __ALIGN_END;

#if defined ( __ICCARM__ )
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef  DMATxDscrTab[ETH_TXBUFNB] __ALIGN_END;

#if defined ( __ICCARM__ )
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __ALIGN_END;

#if defined ( __ICCARM__ )
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __ALIGN_END;

/* Private function prototypes -----------------------------------------------*/
/* static void arp_timer(void *arg); */

/* Private functions ---------------------------------------------------------*/
HAL_StatusTypeDef Studio_Eth_Init(void)
{
  /* USER CODE BEGIN Studio_Eth_Init 0 */

  /* USER CODE END Studio_Eth_Init 0 */
  HAL_StatusTypeDef ret;
  static uint8_t MACAddr[6];

  MACAddr[0] = 0x02;
  MACAddr[1] = 0x00;
  MACAddr[2] = 0x00;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;

  HAL_ETH_DeInit(&heth);

  heth.Instance = ETH;
  heth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
  heth.Init.PhyAddress = 0;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.RxMode = ETH_RXPOLLING_MODE;
  heth.Init.ChecksumMode = ETH_CHECKSUM_BY_SOFTWARE;
  heth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
  heth.Init.Speed = ETH_SPEED_100M;
  heth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;

  ret = HAL_ETH_Init(&heth);

  HAL_ETH_DMATxDescListInit(&heth, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
  HAL_ETH_DMARxDescListInit(&heth, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

  /* USER CODE BEGIN Studio_Eth_Init 1 */

  /* USER CODE END Studio_Eth_Init 1 */
  return ret;
}

static void low_level_init(struct netif *netif)
{
  /* USER CODE BEGIN low_level_init 0 */

  /* USER CODE END low_level_init 0 */
  HAL_StatusTypeDef hal_eth_init_status;

  /* init bsp—eth */
  hal_eth_init_status = Studio_Eth_Init();

  if (hal_eth_init_status == HAL_OK)
  {
    /* Set netif link flag */
    netif->flags |= NETIF_FLAG_LINK_UP;
  }

#if LWIP_ARP || LWIP_ETHERNET

  /* set MAC hardware address length */
  netif->hwaddr_len = ETH_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] =  heth.Init.MACAddr[0];
  netif->hwaddr[1] =  heth.Init.MACAddr[1];
  netif->hwaddr[2] =  heth.Init.MACAddr[2];
  netif->hwaddr[3] =  heth.Init.MACAddr[3];
  netif->hwaddr[4] =  heth.Init.MACAddr[4];
  netif->hwaddr[5] =  heth.Init.MACAddr[5];

  /* maximum transfer unit */
  netif->mtu = NETIF_MTU;

  #if LWIP_ARP
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
  #else
    netif->flags |= NETIF_FLAG_BROADCAST;
  #endif /* LWIP_ARP */

#endif /* LWIP_ARP || LWIP_ETHERNET */

#if !NO_SYS
  /* RTOS mode: create ETH RX semaphore (posted by ETH IRQ/RxCpltCallback per received frame) */
  s_xSemaphore = xSemaphoreCreateCounting(40,0);

  if(sys_sem_new(&tx_sem , 0) == ERR_OK)
//    PRINT_DEBUG("sys_sem_new ok\n");

  if(sys_mbox_new(&eth_tx_mb , 50) == ERR_OK)
//    PRINT_DEBUG("sys_mbox_new ok\n");

  /* create the task that handles the ETH_MAC */
  sys_thread_new("ETHIN",
                  ethernetif_input,
                  netif,
                  NETIF_IN_TASK_STACK_SIZE,
                  NETIF_IN_TASK_PRIORITY);

/*
  sys_thread_new("ETHTX",
                  ethernetif_output,
                  netif,
                  NETIF_OUT_TASK_STACK_SIZE,
                  NETIF_OUT_TASK_PRIORITY);
*/
#endif /* !NO_SYS */

  HAL_ETH_Start(&heth);

  /* USER CODE BEGIN low_level_init 1 */

  /* USER CODE END low_level_init 1 */
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  LWIP_UNUSED_ARG(netif);
  /* USER CODE BEGIN low_level_output 0 */

  /* USER CODE END low_level_output 0 */
#if !NO_SYS
  /* RTOS mode: TX mutex to prevent concurrent writes to Tx descriptors */
  static sys_sem_t ousem = NULL;
  if(ousem == NULL)
  {
    sys_sem_new(&ousem,0);
    sys_sem_signal(&ousem);
  }
#endif /* !NO_SYS */
  err_t errval;
  struct pbuf *q;

  uint8_t *buffer = (uint8_t *)(heth.TxDesc->Buffer1Addr);
  __IO ETH_DMADescTypeDef *DmaTxDesc;
  uint32_t framelength = 0;
  uint32_t bufferoffset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t payloadoffset = 0;
  DmaTxDesc = heth.TxDesc;
  bufferoffset = 0;

  if((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
  {
    errval = ERR_USE;
    goto error;
  }

#if !NO_SYS
  sys_sem_wait(&ousem);
#endif /* !NO_SYS */

  /* copy frame from pbufs to driver buffers */
  for(q = p; q != NULL; q = q->next)
    {
      /* Get bytes in current lwIP buffer */
      byteslefttocopy = q->len;
      payloadoffset = 0;

      /* Check if the length of data to copy is bigger than Tx buffer size*/
      while( (byteslefttocopy + bufferoffset) > ETH_TX_BUF_SIZE )
      {
        /* Copy data to Tx buffer*/
        memcpy( (uint8_t*)((uint8_t*)buffer + bufferoffset),
        (uint8_t*)((uint8_t*)q->payload + payloadoffset),
        (ETH_TX_BUF_SIZE - bufferoffset) );

        /* Point to next descriptor */
        DmaTxDesc = (ETH_DMADescTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);

        /* Check if the buffer is available */
        if((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
        {
          errval = ERR_USE;
          goto error;
        }

        buffer = (uint8_t *)(DmaTxDesc->Buffer1Addr);

        byteslefttocopy = byteslefttocopy - (ETH_TX_BUF_SIZE - bufferoffset);
        payloadoffset = payloadoffset + (ETH_TX_BUF_SIZE - bufferoffset);
        framelength = framelength + (ETH_TX_BUF_SIZE - bufferoffset);
        bufferoffset = 0;
      }

      /* Copy the remaining bytes */
      memcpy( (uint8_t*)((uint8_t*)buffer + bufferoffset),
      (uint8_t*)((uint8_t*)q->payload + payloadoffset), byteslefttocopy );
      bufferoffset = bufferoffset + byteslefttocopy;
      framelength = framelength + byteslefttocopy;
    }

  /* Prepare transmit descriptors to give to DMA */
  if (HAL_ETH_TransmitFrame(&heth, framelength) != HAL_OK)
  {
    printf("ethernetif: transmit frame failed\r\n");
    errval = ERR_IF;
    goto error;
  }

  errval = ERR_OK;

error:

  if ((heth.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET)
  {
    /* Clear TUS ETHERNET DMA flag */
    heth.Instance->DMASR = ETH_DMASR_TUS;

    /* Resume DMA transmission*/
    heth.Instance->DMATPDR = 0;
  }

#if !NO_SYS
  sys_sem_signal(&ousem);
#endif /* !NO_SYS */

  /* USER CODE BEGIN low_level_output 1 */

  /* USER CODE END low_level_output 1 */

  return errval;
}

static struct pbuf * low_level_input(struct netif *netif)
{
  LWIP_UNUSED_ARG(netif);
  /* USER CODE BEGIN low_level_input 0 */

  /* USER CODE END low_level_input 0 */
  struct pbuf *p = NULL;
  struct pbuf *q = NULL;
  uint16_t len = 0;
  uint8_t *buffer;
  __IO ETH_DMADescTypeDef *dmarxdesc;
  uint32_t bufferoffset = 0;
  uint32_t payloadoffset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t i=0;

  /* get received frame */
  if (HAL_ETH_GetReceivedFrame(&heth) != HAL_OK)
  {
    if ((heth.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)
    {
      /* Clear RBUS ETHERNET DMA flag and resume reception */
      printf("ethernetif: RX buffer unavailable, resuming DMA\r\n");
      heth.Instance->DMASR = ETH_DMASR_RBUS;
      heth.Instance->DMARPDR = 0;
    }
//    PRINT_ERR("receive frame faild\n");
    return NULL;
  }
  /* Obtain the size of the packet and put it into the "len" variable. */
  len = heth.RxFrameInfos.length;
  buffer = (uint8_t *)heth.RxFrameInfos.buffer;

//  PRINT_INFO("receive frame %d len buffer : %s\n", len, buffer);
  if (len > 0)
  {
    /* We allocate a pbuf chain of pbufs from the Lwip buffer pool */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  }

  if (p != NULL)
  {
    dmarxdesc = heth.RxFrameInfos.FSRxDesc;
    bufferoffset = 0;
    for(q = p; q != NULL; q = q->next)
    {
      byteslefttocopy = q->len;
      payloadoffset = 0;

      while( (byteslefttocopy + bufferoffset) > ETH_RX_BUF_SIZE )
      {
        /* Copy data to pbuf */
        memcpy( (uint8_t*)((uint8_t*)q->payload + payloadoffset),
        (uint8_t*)((uint8_t*)buffer + bufferoffset),
        (ETH_RX_BUF_SIZE - bufferoffset));

        /* Point to next descriptor */
        dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
        buffer = (uint8_t *)(dmarxdesc->Buffer1Addr);

        byteslefttocopy = byteslefttocopy - (ETH_RX_BUF_SIZE - bufferoffset);
        payloadoffset = payloadoffset + (ETH_RX_BUF_SIZE - bufferoffset);
        bufferoffset = 0;
      }
      /* Copy remaining data in pbuf */
      memcpy( (uint8_t*)((uint8_t*)q->payload + payloadoffset),
      (uint8_t*)((uint8_t*)buffer + bufferoffset), byteslefttocopy);
      bufferoffset = bufferoffset + byteslefttocopy;
    }
  }

    /* Release descriptors to DMA */
    /* Point to first descriptor */
    dmarxdesc = heth.RxFrameInfos.FSRxDesc;
    /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
    for (i=0; i< heth.RxFrameInfos.SegCount; i++)
    {
      dmarxdesc->Status |= ETH_DMARXDESC_OWN;
      dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
    }

    /* Clear Segment_Count */
    heth.RxFrameInfos.SegCount =0;

  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((heth.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)
  {
    /* Clear RBUS ETHERNET DMA flag */
    printf("ethernetif: RX buffer unavailable, resuming DMA\r\n");
    heth.Instance->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    heth.Instance->DMARPDR = 0;
  }

  /* USER CODE BEGIN low_level_input 1 */

  /* USER CODE END low_level_input 1 */
  return p;
}

/* Bare-metal mode (NO_SYS=1): ethernetif_input is polled from main loop (Studio_LWIP_Process) */
void ethernetif_input(struct netif *netif)
{
  /* USER CODE BEGIN ethernetif_input 0 */

  /* USER CODE END ethernetif_input 0 */
  err_t err;
  struct pbuf *p;

  /* move received packet into a new pbuf */
  p = low_level_input(netif);

  /* no packet could be read, silently ignore this */
  if (p == NULL)
  {
    static uint32_t last_rx_diagnostic = 0;
    uint32_t now = HAL_GetTick();

    if ((now - last_rx_diagnostic) >= 1000U)
    {
      last_rx_diagnostic = now;
      printf("ETH RX wait: DMASR=%08lx DESC=%08lx\r\n",
             (unsigned long)heth.Instance->DMASR,
             (unsigned long)heth.RxDesc->Status);
    }
    return;
  }

  /* entry point to the LwIP stack */
  err = netif->input(p, netif);

  if (err != ERR_OK)
  {
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
    pbuf_free(p);
    p = NULL;
  }
  else
  {
    printf("eth receive success\r\n");
  }

  /* USER CODE BEGIN ethernetif_input 1 */

  /* USER CODE END ethernetif_input 1 */
}

#if !LWIP_ARP

static err_t low_level_output_arp_off(struct netif *netif, struct pbuf *q, const ip4_addr_t *ipaddr)
{
  /* USER CODE BEGIN low_level_output_arp_off 0 */

  /* USER CODE END low_level_output_arp_off 0 */
  err_t errval;
  errval = ERR_OK;
  /* USER CODE BEGIN low_level_output_arp_off 1 */

  /* USER CODE END low_level_output_arp_off 1 */
  return errval;

}
#endif /* LWIP_ARP */

err_t ethernetif_init(struct netif *netif)
{
  /* USER CODE BEGIN ethernetif_init 0 */

  /* USER CODE END ethernetif_init 0 */
  struct ethernetif *ethernetif;

//  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ethernetif = mem_malloc(sizeof(struct ethernetif));

  if (ethernetif == NULL) {
    //PRINT_ERR("ethernetif_init: out of memory\n");
    return ERR_MEM;
  }

  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */
  netif->state = ethernetif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;

#if LWIP_IPV4
#if LWIP_ARP || LWIP_ETHERNET
#if LWIP_ARP
  netif->output = etharp_output;
#else
  netif->output = low_level_output_arp_off;
#endif /* LWIP_ARP */
#endif /* LWIP_ARP || LWIP_ETHERNET */
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  low_level_init(netif);
  ethernetif->ethaddr = (struct eth_addr *) &(netif->hwaddr[0]);

  /* USER CODE BEGIN ethernetif_init 1 */

  /* USER CODE END ethernetif_init 1 */
  return ERR_OK;
}

#if LWIP_NETIF_LINK_CALLBACK

void ethernetif_update_config(struct netif *netif)
{
  /* USER CODE BEGIN ethernetif_update_config 0 */

  /* USER CODE END ethernetif_update_config 0 */
  __IO uint32_t tickstart = 0;
  uint32_t regvalue = 0;

  if(netif_is_link_up(netif))
  {
    /* Restart the auto-negotiation */
    if(heth.Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE)
    {
      /* Enable Auto-Negotiation */
      HAL_ETH_WritePHYRegister(&heth, PHY_BCR, PHY_AUTONEGOTIATION);

      /* Get tick */
      tickstart = HAL_GetTick();

      /* Wait until the auto-negotiation will be completed */
      do
      {
        HAL_ETH_ReadPHYRegister(&heth, PHY_BSR, &regvalue);

        /* Check for the Timeout ( 1s ) */
        if((HAL_GetTick() - tickstart ) > 1000)
        {
          /* In case of timeout */
          goto error;
        }
      } while (((regvalue & PHY_AUTONEGO_COMPLETE) != PHY_AUTONEGO_COMPLETE));

      /* Read the result of the auto-negotiation */
      HAL_ETH_ReadPHYRegister(&heth, PHY_SR, &regvalue);

      if((regvalue & PHY_DUPLEX_STATUS) != (uint32_t)RESET)
      {

        heth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
      }
      else
      {

        heth.Init.DuplexMode = ETH_MODE_HALFDUPLEX;
      }

      if(regvalue & PHY_SPEED_STATUS)
      {
        /* Set Ethernet speed to 10M following the auto-negotiation */
        heth.Init.Speed = ETH_SPEED_10M;
      }
      else
      {
        /* Set Ethernet speed to 100M following the auto-negotiation */
        heth.Init.Speed = ETH_SPEED_100M;
      }
    }
    else /* AutoNegotiation Disable */
    {
    error :
      /* Check parameters */
      assert_param(IS_ETH_SPEED(heth.Init.Speed));
      assert_param(IS_ETH_DUPLEX_MODE(heth.Init.DuplexMode));

      /* Set MAC Speed and Duplex Mode to PHY */
      HAL_ETH_WritePHYRegister(&heth, PHY_BCR,
      ((uint16_t)(heth.Init.DuplexMode >> 3) |
       (uint16_t)(heth.Init.Speed >> 1)));
    }

    /* ETHERNET MAC Re-Configuration */
    HAL_ETH_ConfigMAC(&heth, (ETH_MACInitTypeDef *) NULL);

    /* Restart MAC interface */
    HAL_ETH_Start(&heth);
  }
  else
  {
    /* Stop MAC interface */
    HAL_ETH_Stop(&heth);
  }

  ethernetif_notify_conn_changed(netif);

  /* USER CODE BEGIN ethernetif_update_config 1 */

  /* USER CODE END ethernetif_update_config 1 */
}

void ethernetif_notify_conn_changed(struct netif *netif)
{
  /* USER CODE BEGIN ethernetif_notify_conn_changed 0 */

  /* USER CODE END ethernetif_notify_conn_changed 0 */

  /* USER CODE BEGIN ethernetif_notify_conn_changed 1 */

  /* USER CODE END ethernetif_notify_conn_changed 1 */
}
#endif

/* sys_now is provided by this file in bare-metal mode (NO_SYS=1);
   in RTOS mode it is provided by sys_arch.c (avoid duplicate definition) */
#if NO_SYS
u32_t sys_now(void)
{
  return HAL_GetTick();
}
#endif
/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
