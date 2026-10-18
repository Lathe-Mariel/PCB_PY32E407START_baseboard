/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   This file provides code for the configuration
  *          of all used MAIN.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2025 Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by Mcu Studio under BSD 3-Clause license,
  * the License ; You may not use this file except in compliance with the
  * License.You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "rcc.h"
#include "gpio.h"
#include "uart.h"
#include "lwip.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <lwip/opt.h>
#include <lwip/arch.h>
#include "tcpip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/def.h"
#include "lwip/stats.h"
#include "lwip/etharp.h"
#include "lwip/ip.h"
#include "lwip/snmp.h"
#include "lwip/timeouts.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "netif/etharp.h"
#include "netif/ethernet.h"
#include "ethernetif.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void TCP_Echo_Init(void);
static err_t tcpecho_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t tcpecho_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
/* USER CODE END PFP */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  Studio_RCC_Init();
  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  Studio_GPIO_Init();
  Studio_UART1_Init();
  /* USER CODE BEGIN 2 */
  /* Wait for the button to be press */
  while (BSP_PB_GetState() == 0);

  printf("The local port number is: %d\r\n",LOCAL_PORT);
  
  /* LwIP protocol stack init */
  Studio_LWIP_Init();
  TCP_Echo_Init();
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while(1)
  {
    ethernetif_input(&gnetif);
    /* Handling timed events in LwIP */
    sys_check_timeouts();
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */
static void TCP_Echo_Init(void)
{
  struct tcp_pcb *pcb = NULL;
  
  /* Create a TCP control block */
  pcb = tcp_new();
  
  /* Binding TCP control blocks */
  tcp_bind(pcb, IP_ADDR_ANY, TCP_ECHO_PORT);

  /* Go into a listening state */
  pcb = tcp_listen(pcb);

  /* Handling of links */
  tcp_accept(pcb, tcpecho_accept);
}

static err_t tcpecho_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  if (p != NULL) 
  {
    /* Update Window */
    tcp_recved(tpcb, p->tot_len);

    /* Returns the received data */
    tcp_write(tpcb, p->payload, p->tot_len, 1);

    memset(p->payload, 0 , p->tot_len);
    pbuf_free(p);
  } 
  else if (err == ERR_OK) 
  {
    return tcp_close(tpcb);
  }
  return ERR_OK;
}

static err_t tcpecho_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  tcp_recv(newpcb, tcpecho_recv);
  return ERR_OK;
}

void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{

}

void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth)
{
  printf("eth err\n");
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
