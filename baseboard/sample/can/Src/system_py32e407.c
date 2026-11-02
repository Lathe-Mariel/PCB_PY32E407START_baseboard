/**
  ******************************************************************************
  * @file    system_py32e407.c
  * @author  MCU Application Team
  * @brief   CMSIS Cortex-M4 Device Peripheral Access Layer System Source File
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

/** @addtogroup CMSIS
  * @{
  */

/** @addtogroup py32e407_system
  * @{
  */  
  
/** @addtogroup py32e407_System_Private_Includes
  * @{
  */

#include "py32e4xx.h"

#if !defined  (HSE_VALUE) 
  #define HSE_VALUE    ((uint32_t)24000000) /*!< Default value of the External oscillator in Hz */
#endif /* HSE_VALUE */

#if !defined  (HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)16000000) /*!< Value of the Internal oscillator in Hz*/
#endif /* HSI_VALUE */

/**
  * @}
  */

/*!< Uncomment the following line if you need to relocate your vector Table in
     Internal SRAM. */
/* #define VECT_TAB_SRAM */
#define VECT_TAB_OFFSET  0x00 /*!< Vector Table base offset field. 
                                   This value must be a multiple of 0x200. */
/******************************************************************************/


/** @addtogroup PY32E407_System_Private_Variables
  * @{
  */
  /* This variable is updated in three ways:
      1) by calling CMSIS function SystemCoreClockUpdate()
      2) by calling HAL API function HAL_RCC_GetHCLKFreq()
      3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency 
         Note: If you use this function to configure the system clock; then there
               is no need to call the 2 first functions listed above, since SystemCoreClock
               variable is updated automatically.
  */
uint32_t SystemCoreClock = HSI_VALUE;
const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};
/**
  * @}
  */

/** @addtogroup PY32E407_System_Private_Functions
  * @{
  */

/**
  * @brief  Setup the microcontroller system
  *         Initialize the FPU setting, vector table location and External memory 
  *         configuration.
  * @param  None
  * @retval None
  */
void SystemInit(void)
{
  RCC->AHB2ENR |= RCC_AHB2ENR_IOPFEN;
  while(1)
  {
    if (((FLASH->OPTR2 & FLASH_OPTR2_NRST_MODE) == FLASH_OPTR2_NRST_MODE_1) || 
        (((FLASH->OPTR2 & FLASH_OPTR2_NRST_MODE) != FLASH_OPTR2_NRST_MODE_1) && ((GPIOF->IDR & GPIO_PIN_5) != RESET)))
    {
      break;
    } 
  }
  RCC->AHB2ENR &= ~RCC_AHB2ENR_IOPFEN;

  /* Enable PWR clock */
  SET_BIT(RCC->APB1ENR1, RCC_APB1ENR1_PWREN);
  /* Reset PWR_CR1_VOS */
  CLEAR_BIT(PWR->CR1, PWR_CR1_VOS);
  /* Reset PWR_CR1_SRAM_RETV */
  CLEAR_BIT(PWR->CR1, PWR_CR1_SRAM_RETV);
  /* Disable PWR clock */
  CLEAR_BIT(RCC->APB1ENR1, RCC_APB1ENR1_PWREN);
  
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
#endif

  /* Configure the Vector Table location add offset address ------------------*/
#ifdef VECT_TAB_SRAM
  SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal SRAM */
#else
  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal FLASH */
#endif
}

/**
   * @brief  Update SystemCoreClock variable according to Clock Register Values.
  *         The SystemCoreClock variable contains the core clock (HCLK), it can
  *         be used by the user application to setup the SysTick timer or configure
  *         other parameters.
  *           
  * @note   Each time the core clock (HCLK) changes, this function must be called
  *         to update SystemCoreClock variable value. Otherwise, any configuration
  *         based on this variable will be incorrect.         
  *     
  * @note   - The system frequency computed by this function is not the real 
  *           frequency in the chip. It is calculated based on the predefined 
  *           constant and the selected clock source:
  *             
  *           - If SYSCLK source is HSI, SystemCoreClock will contain the HSI_VALUE(*)
  *                                              
  *           - If SYSCLK source is HSE, SystemCoreClock will contain the HSE_VALUE(**)
  *                          
  *           - If SYSCLK source is PLL, SystemCoreClock will contain the HSE_VALUE(**) 
  *             or HSI_VALUE(*) multiplied/divided by the PLL factors.
  *         
  *         (*) HSI_VALUE is a constant defined in py32e407_hal_conf.h file (default value
  *             16 MHz) but the real value may vary depending on the variations
  *             in voltage and temperature.   
  *    
  *         (**) HSE_VALUE is a constant defined in py32e407_hal_conf.h file (its value
  *              depends on the application requirements), user has to ensure that HSE_VALUE
  *              is same as the real frequency of the crystal used. Otherwise, this function
  *              may have wrong result.
  *                
  *         - The result of this function could be not correct when using fractional
  *           value for HSE crystal.
  *     
  * @param  None
  * @retval None
  */
void SystemCoreClockUpdate(void)
{
  const uint8_t aDivFactorTable[4] = {1, 2, 4, 8};
  
  uint32_t tmpreg = 0U,tmppllreg=0, pllxtprediv = 0U, pllclk = 0U, pllmul = 0U, pllprediv = 0U, pllpostdiv = 0U;
  uint32_t tmp = 0;
  
  tmpreg = RCC->CFGR;
  tmppllreg = RCC->CFGR3;
  /* Get SYSCLK source -------------------------------------------------------*/
  switch (tmpreg & RCC_CFGR_SWS)
  {
    case 0x00:  /* HSI used as system clock source */
    {
      SystemCoreClock = HSI_VALUE;
      break;
    }
    
    case 0x04:  /* HSE used as system clock */
    {
      SystemCoreClock = HSE_VALUE;
      break;
    }
    
    case 0x08:  /* PLL used as system clock */
    {
      pllprediv = aDivFactorTable[(uint32_t)(RCC->CFGR3 & RCC_CFGR3_PLLPREDIV) >> RCC_CFGR3_PLLPREDIV_Pos];
      pllpostdiv = aDivFactorTable[(uint32_t)(RCC->CFGR3 & RCC_CFGR3_PLLPOSTDIV) >> RCC_CFGR3_PLLPOSTDIV_Pos];
      pllmul = (READ_BIT(RCC->CFGR3, RCC_CFGR3_PLLFBDIV) >> RCC_CFGR3_PLLFBDIV_Pos) + 2U;
      if (((tmppllreg & (RCC_CFGR3_PLLSRC | RCC_CFGR3_PLLXTPRE)) == RCC_PLLSOURCE_HSE) || \
          ((tmppllreg & (RCC_CFGR3_PLLSRC | RCC_CFGR3_PLLXTPRE)) == RCC_PLLSOURCE_HSE_DIV2))
      {
        pllxtprediv = aDivFactorTable[(uint32_t)(RCC->CFGR3 & RCC_CFGR3_PLLXTPRE) >> RCC_CFGR3_PLLXTPRE_Pos];
        /* HSE used as PLL clock source : PLLCLK = (HSE / PLLXTPREDIV) / PLLPREDIV * PLLMUL / PLLLPOSTDIV */
        pllclk = (uint32_t)((HSE_VALUE / pllxtprediv) / pllprediv * pllmul / pllpostdiv);
      }
      else
      {
        /* HSI used as PLL clock source : PLLCLK = HSI/2 / PLLPREDIV * PLLMUL / PLLLPOSTDIV */
        pllclk = (uint32_t)((HSI_VALUE >> 1) / pllprediv * pllmul / pllpostdiv);
      }
      SystemCoreClock = pllclk;
      break;
    }
    
    default:
    {      
      break;
    } 
  }

    /* Get HCLK prescaler */
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos)];
    SystemCoreClock >>= tmp;
}

/**
  * @}
  */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
