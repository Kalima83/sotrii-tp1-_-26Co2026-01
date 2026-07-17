/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
#include "task_uart_attribute.h"

/********************** macros and definitions *******************************/
#define G_TASK_XXXX_CNT_INI	0ul
#define G_TASK_XXXX_RUNTIME_US_INI	0ul

#define TASK_XXXX_DEL_ZERO	(pdMS_TO_TICKS(0ul))
#define TASK_XXXX_DEL_MAX	(pdMS_TO_TICKS(250ul))


/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/
void task_uart_tx(void *parameters);
void task_uart_rx(void *parameters);

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_task_xxxx_tx_cnt;
uint32_t g_task_xxxx_tx_runtime_us;

uint32_t g_task_xxxx_rx_cnt;
uint32_t g_task_xxxx_rx_runtime_us;

uint32_t g_task_xxxx_tx_runtime_cycles;
uint32_t g_task_xxxx_rx_runtime_cycles;

/********************** external functions definition ************************/

/* Task UART TX thread (Gatekeeper de Transmisión) */
void task_uart_tx(void *parameters)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	uint8_t tx_data_byte = 0;

	/* Initialize Task Function variables */
	g_task_xxxx_tx_cnt = G_TASK_XXXX_CNT_INI;
	g_task_xxxx_tx_runtime_us = G_TASK_XXXX_RUNTIME_US_INI;
	g_task_xxxx_tx_runtime_cycles = 0ul;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	for (;;)
	{
		/* Bloqueo eterno hasta que un productor meta un byte en la cola de Tx */
		if (xQueueReceive(g_uart_driver.device_tx_queue, &tx_data_byte, portMAX_DELAY) == pdTRUE)
		{
			g_task_xxxx_tx_cnt++;

			/* Reset y medición del WCET que le toma a la CPU configurar la IT */
			cycle_counter_reset();

			HAL_GPIO_TogglePin(LED_A_PORT, LED_A_PIN);

			/* Iniciamos la transmisión física por hardware mediante Interrupciones */
			if (HAL_UART_Transmit_IT(g_uart_driver.h_uart, &tx_data_byte, 1ul) == HAL_OK)
			{
				/* Captura directa de ciclos de clock puros de la CPU */
				g_task_xxxx_tx_runtime_cycles = DWT->CYCCNT;
				g_task_xxxx_tx_runtime_us = cycle_counter_get_time_us();

				/* Bloqueamos el Gatekeeper de Tx liberando CPU. La ISR se encargará de despertarlo */
				xSemaphoreTake(g_uart_driver.tx_sem, portMAX_DELAY);
			}
			else
			{
				/* Resguardo ante errores de bus por hardware */
				g_task_xxxx_tx_runtime_cycles = DWT->CYCCNT;
				g_task_xxxx_tx_runtime_us = cycle_counter_get_time_us();
			}
		}
	}
}

/* Task UART RX thread (Gatekeeper de Recepción) */
void task_uart_rx(void *parameters)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	uint8_t rx_data_byte = 0;

	/* Initialize Task Function variables */
	g_task_xxxx_rx_cnt = G_TASK_XXXX_CNT_INI;
	g_task_xxxx_rx_runtime_us = G_TASK_XXXX_RUNTIME_US_INI;
	g_task_xxxx_rx_runtime_cycles = 0ul;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	for (;;)
	{
		g_task_xxxx_rx_cnt++;

		/* Reset y medición del WCET de configuración de escucha por IT */
		cycle_counter_reset();

		/* Le ordena a la HAL que prepare los registros para recibir 1 byte de forma asíncrona */
		if (HAL_UART_Receive_IT(g_uart_driver.h_uart, &rx_data_byte, 1ul) == HAL_OK)
		{
			/* Captura directa de ciclos de clock puros de la CPU */
			g_task_xxxx_rx_runtime_cycles = DWT->CYCCNT;
			g_task_xxxx_rx_runtime_us = cycle_counter_get_time_us();

			/* Pone a dormir el Gatekeeper de Rx. Despertará al completarse la captura por hardware */
			xSemaphoreTake(g_uart_driver.rx_sem, portMAX_DELAY);

			/* Al despertar, envia inmediatamente el byte de forma asíncrona a la capa superior */
			xQueueSend(g_uart_driver.device_rx_queue, &rx_data_byte, TASK_XXXX_DEL_ZERO);
		}
		else
		{
			g_task_xxxx_rx_runtime_cycles = DWT->CYCCNT;
			g_task_xxxx_rx_runtime_us = cycle_counter_get_time_us();
			/* Espera en caso de error de sobreescritura (Overrun) en el registro del hardware */
			vTaskDelay(pdMS_TO_TICKS(1ul));
		}
	}
}

/********************** end of file ******************************************/
