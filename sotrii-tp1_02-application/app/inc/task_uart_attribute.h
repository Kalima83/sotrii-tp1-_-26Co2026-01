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

#ifndef TASK_UART_ATTRIBUTE_H_
#define TASK_UART_ATTRIBUTE_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"

/********************** macros ***********************************************/

/********************** typedef **********************************************/
/* Structure of Task */
typedef struct {
    UART_HandleTypeDef *h_uart;        /* Puntero al objeto de la HAL de ST (ej: &huart2) */
    uint32_t device_id;                /* Identificador único del dispositivo */
    QueueHandle_t device_tx_queue;     /* Cola de mensajes para Transmisión Asíncrona */
    QueueHandle_t device_rx_queue;     /* Cola de mensajes para Recepción Asíncrona */
    SemaphoreHandle_t tx_sem;          /* Semáforo binario para sincronizar el fin de Tx por IT */
    SemaphoreHandle_t rx_sem;          /* Semáforo binario para sincronizar el fin de Rx por IT */
    uint32_t ioctl_mode;               /* Modo de gestión del periférico (Polling, IT, DMA) */
} uart_driver_t;

/* Structure of UART Tx */
typedef struct {
    uint8_t data;                      /* Estructura para empaquetar datos individuales si el driver lo requiere */
} uart_tx_msg_t;

/********************** external data declaration ****************************/
/* Declara la existencia del driver global para que la interfaz e interrupciones lo vean */
extern uart_driver_t g_uart_driver;

/********************** external functions declaration ***********************/

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* TASK_UART_ATTRIBUTE_H_ */

/********************** end of file ******************************************/
