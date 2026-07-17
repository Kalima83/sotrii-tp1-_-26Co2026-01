/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"
#include "cmsis_os.h"

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"
#include "app_it.h"
#include "task_adc_attribute.h"
#include "task_adc_interface.h"  /* Inclusión del Driver ADC */

/********************** macros and definitions *******************************/
#define G_TASK_XXXX_CNT_INI			0ul
#define G_TASK_XXXX_RUNTIME_US_INI	0ul

#define TASK_XXXX_DEL_ZERO	(pdMS_TO_TICKS(0ul))
#define TASK_XXXX_DEL_MAX	(pdMS_TO_TICKS(250ul))

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/
void task_adc_rx(void *parameters);

/********************** internal data definition *****************************/
const char *p_task_adc_rx_wait_250mS	= "   ==> Task ADC RX - Muestreo Activo (DMA)";

/********************** external data declaration ****************************/
uint32_t g_task_xxxx_rx_cnt;
uint32_t g_task_xxxx_rx_runtime_us;

/* Variable externa para auditar los ciclos del Gatekeeper de hardware en Live Expressions */
extern uint32_t g_task_adc_dma_callback_runtime_cycles;

/********************** external functions definition ************************/
/* Task ADC RX thread (Supervisor / Gatekeeper de bajo nivel) */
void task_adc_rx(void *parameters)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	/*  Declare & Initialize Task Function variables */
	g_task_xxxx_rx_cnt = G_TASK_XXXX_CNT_INI;
	g_task_xxxx_rx_runtime_us = G_TASK_XXXX_RUNTIME_US_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	/* En este esquema por DMA, el hardware convierte y transfiere en loop de forma autónoma */
	for (;;)
	{
		/* Update Task Counter */
		g_task_xxxx_rx_cnt++;

		/* Mide el tiempo de control de la tarea (esta solo genera la traza visual de parpadeo) */
		cycle_counter_reset();

		HAL_GPIO_TogglePin(LED_A_PORT, LED_A_PIN);

		g_task_xxxx_rx_runtime_us = cycle_counter_get_time_us();

    	/* Print out: Estado */
		LOGGER_INFO(p_task_adc_rx_wait_250mS);

		/* Bloqueo eficiente de la tarea por 250ms liberando CPU */
		vTaskDelay(TASK_XXXX_DEL_MAX);
	}
}

/********************** end of file ******************************************/
