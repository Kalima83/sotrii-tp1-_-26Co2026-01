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
#include "task_uart_interface.h"

/********************** macros and definitions *******************************/
#define G_TASK_RECEIVER_CNT_INI	0ul

#define TASK_RECEIVER_DEL_ZERO		(pdMS_TO_TICKS(0ul))
#define TASK_RECEIVER_TIMEOUT		(pdMS_TO_TICKS(100ul)) /* Espera máxima de 100ms por byte */

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_task_receiver_cnt;

/* Variable externa declarada en main.c que representa al hardware de la UART */
extern UART_HandleTypeDef huart2;

/********************** external functions definition ************************/
/* Task thread */
void task_receiver(void *parameters)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	/*  Declare & Initialize Task Function variables */
	g_task_receiver_cnt = G_TASK_RECEIVER_CNT_INI;
	uint8_t rx_char = 0;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		/*
		 * Intenta leer un byte de la interfaz.
		 * Si la cola está vacía, el hilo se bloquea hasta un máximo de TASK_RECEIVER_TIMEOUT.
		 */
		if (read_uart(&huart2, &rx_char, TASK_RECEIVER_TIMEOUT) == pdPASS)
		{
			/* Incrementa el contador solo cuando procesamos datos reales */
			g_task_receiver_cnt++;

			/* Imprime de manera segura el carácter que ingresó por hardware */
			LOGGER_INFO("   ==> Task RECEIVER - Byte Recibido: '%c' (0x%02X)", rx_char, rx_char);
		}
		else
		{
			/*
			 * Cae acá únicamente si expiró el Timeout y no llegó nada.
			 * Sirve para verificar que la tarea no está muerta sino esperando de forma sana.
			 */
			LOGGER_INFO("   ==> Task RECEIVER - Esperando datos...");
		}
	}
}

/********************** end of file ******************************************/
