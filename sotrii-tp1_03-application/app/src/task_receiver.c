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
#include "task_adc_interface.h"

/********************** macros and definitions *******************************/
#define G_TASK_RECEIVER_CNT_INI	0ul

#define TASK_RECEIVER_DEL_ZERO		(pdMS_TO_TICKS(0ul))
#define TASK_RECEIVER_DEL_MAX		(pdMS_TO_TICKS(250ul))

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_task_receiver_cnt;

/* Variables globales para medir el WCET de la función de interfaz Read */
uint32_t g_task_adc_read_runtime_cycles;
uint32_t g_task_adc_read_runtime_us;

/* Variable global compartida que contiene la última conversión del ADC */
extern volatile uint16_t g_adc_latest_value;

/********************** external functions definition ************************/

/* Task thread (Consumidor de Aplicación - Capa Superior) */
void task_receiver(void *parameters)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	uint16_t adc_sample = 0;

	/*  Declare & Initialize Task Function variables */
	g_task_receiver_cnt = G_TASK_RECEIVER_CNT_INI;
	g_task_adc_read_runtime_cycles = 0ul;
	g_task_adc_read_runtime_us = 0ul;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
		{
			/* Update Task Counter */
			g_task_receiver_cnt++;

			/* Resetea el contador para medir la función de interfaz original */
			cycle_counter_reset();

			/* Invoca la interfaz tal como la estructuró la cátedra */
			read_adc(&hadc1);

			/* Captura los tiempos de ejecución de la interfaz */
			g_task_adc_read_runtime_cycles = DWT->CYCCNT;
			g_task_adc_read_runtime_us = cycle_counter_get_time_us();

			/*
			 * Patrón Latest Input Only: Tomamos el dato fresco que actualizó de forma
			 * asíncrona la ISR del DMA en la variable compartida global.
			 */
			adc_sample = g_adc_latest_value;

			/* Imprime la muestra obtenida */
			LOGGER_INFO("   ==> Task RECEIVER - ADC Value: %u", adc_sample);

			/* Bloqueo periódico de 250ms */
			vTaskDelay(TASK_RECEIVER_DEL_MAX);
		}
}

/********************** end of file ******************************************/
