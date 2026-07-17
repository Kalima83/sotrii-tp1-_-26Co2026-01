/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app_it.h"
#include "task_adc_interface.h"

/********************** macros and definitions *******************************/
#define HAL_XXXX_CALLBACK_CNT_INI			0ul
#define HAL_XXXX_CALLBACK_RUNTIME_US_INI	0ul

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
/* Declaración externa de las variables globales del ADC distribuidas por el driver */
extern uint32_t g_adc_buffer_dma[];
extern volatile uint16_t g_adc_latest_value;

volatile bool hal_xxxx_callback_flag;
volatile uint32_t hal_xxxx_callback_cnt;
volatile uint32_t hal_xxxx_callback_runtime_us;

/* Variables globales para medir el WCET del Callback de DMA en Live Expressions */
uint32_t g_task_adc_dma_callback_runtime_cycles;
uint32_t g_task_adc_dma_callback_runtime_us;

/********************** external functions definition ************************/
void app_it_init(void)
{
	/* Protect shared resource */
	__asm("CPSID i");	/* disable interrupts */

	hal_xxxx_callback_flag = false;
	hal_xxxx_callback_cnt = HAL_XXXX_CALLBACK_CNT_INI;
	hal_xxxx_callback_runtime_us = HAL_XXXX_CALLBACK_RUNTIME_US_INI;

	g_task_adc_dma_callback_runtime_cycles = 0ul;
	g_task_adc_dma_callback_runtime_us = 0ul;

	__asm("CPSIE i");	/* enable interrupts */
}

/**
  * @brief  EXTI line detection callbacks.
  * @param  GPIO_Pin Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == BTN_A_PIN)
	{
		/* Work to be done. */
	}
}

/**
  * @brief  Regular conversion complete callback in non blocking mode (Gatekeeper por Hardware)
  * @param  hadc pointer to a ADC_HandleTypeDef structure that contains
  *         the configuration information for the specified ADC.
  * @retval None
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	/* Verifica que provenga de la instancia nativa del ADC1 */
	if (hadc->Instance == ADC1)
	{
		/* Inicia la medición limpia de ciclos de CPU consumidos por la ISR */
		cycle_counter_reset();

		hal_xxxx_callback_flag = true;
		hal_xxxx_callback_cnt++;

		/*
		 * Patrón Latest Input Only: El controlador DMA escribe de forma autónoma en background.
		 * Copia la muestra de forma inmediata pisando el valor anterior en la variable compartida global.
		 */
		g_adc_latest_value = (uint16_t)g_adc_buffer_dma[0];

		/* Captura de tiempos en ciclos puros y microsegundos antes de salir de la ISR */
		g_task_adc_dma_callback_runtime_cycles = DWT->CYCCNT;
		g_task_adc_dma_callback_runtime_us = cycle_counter_get_time_us();
	}
}

/********************** end of file ******************************************/
