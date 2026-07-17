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
#include "task_uart.h"
#include "task_uart_attribute.h"
#include "task_uart_interface.h"

/********************** macros and definitions *******************************/
#define UART_QUEUE_LENGTH   (16ul)

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data definition *****************************/
/* Estructura del Driver Global */
uart_driver_t g_uart_driver;

/********************** external functions definition ************************/
/* Interface functions */

void open_uart(UART_HandleTypeDef *h_uart_device)
{
    /* Asigna el periférico de hardware a la estructura de control */
    g_uart_driver.h_uart = h_uart_device;
    g_uart_driver.device_id = 1; /* Identificador por defecto */
    g_uart_driver.ioctl_mode = 0; /* Modo por defecto: Interrupt */

    /* Creación de Colas (Almacenamiento Dinámico en Heap al inicio) */
    g_uart_driver.device_tx_queue = xQueueCreate(UART_QUEUE_LENGTH, sizeof(uint8_t));
    configASSERT(g_uart_driver.device_tx_queue != NULL);

    g_uart_driver.device_rx_queue = xQueueCreate(UART_QUEUE_LENGTH, sizeof(uint8_t));
    configASSERT(g_uart_driver.device_rx_queue != NULL);

    /* Creación de Semáforos Binarios para la Sincronización con las ISR */
    g_uart_driver.tx_sem = xSemaphoreCreateBinary();
    configASSERT(g_uart_driver.tx_sem != NULL);

    g_uart_driver.rx_sem = xSemaphoreCreateBinary();
    configASSERT(g_uart_driver.rx_sem != NULL);

    LOGGER_LOG("[info] Driver UART listo (Asinc)\r\n");
}

void release_uart(UART_HandleTypeDef *h_uart_device)
{
    UNUSED(h_uart_device);

    /* Liberación segura de recursos del OS */
    if (g_uart_driver.device_tx_queue != NULL) vQueueDelete(g_uart_driver.device_tx_queue);
    if (g_uart_driver.device_rx_queue != NULL) vQueueDelete(g_uart_driver.device_rx_queue);
    if (g_uart_driver.tx_sem != NULL) vSemaphoreDelete(g_uart_driver.tx_sem);
    if (g_uart_driver.rx_sem != NULL) vSemaphoreDelete(g_uart_driver.rx_sem);
}

BaseType_t write_uart(UART_HandleTypeDef *h_uart_device, uint8_t data)
{
    UNUSED(h_uart_device);

    /* Operación no bloqueante para la aplicación: Encolar y retornar */
    return xQueueSend(g_uart_driver.device_tx_queue, &data, 0ul);
}

BaseType_t read_uart(UART_HandleTypeDef *h_uart_device, uint8_t *p_data, TickType_t xTicksToWait)
{
    UNUSED(h_uart_device);

    /* Bloquea a la tarea consumidora si la cola de recepción está vacía */
    return xQueueReceive(g_uart_driver.device_rx_queue, p_data, xTicksToWait);
}

void ioctl_uart(UART_HandleTypeDef *h_uart_device, uint32_t request, void *arg)
{
    UNUSED(h_uart_device);
    UNUSED(arg);

    /* Reserva para manipulación dinámica del driver (Polling, IT, DMA) */
    g_uart_driver.ioctl_mode = request;
}

/********************** end of file ******************************************/
