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

#ifndef TASK_I2C_ATTRIBUTE_H_
#define TASK_I2C_ATTRIBUTE_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/
#include "cmsis_os.h" // incluye de forma nativa a FreeRTOS.h, task.h, queue.h y semphr.h

/********************** macros ***********************************************/

/********************** typedef **********************************************/
/* Estructura agregada para control de modos */
typedef enum {
    I2C_BUS_MODE_POLLING = 0,
    I2C_BUS_MODE_INTERRUPT,
    I2C_BUS_MODE_DMA
} i2c_bus_mode_t;

typedef struct {
    uint8_t address;  // Dirección del dispositivo esclavo
    uint8_t data;     // Dato a transmitir
} task_i2c_tx_dta_t;

/* Estructura extendida con control de sincronismo y modo */
typedef struct
{
	I2C_HandleTypeDef * device_id;

	TaskHandle_t		task_tx;
	QueueHandle_t		queue_tx;

	TaskHandle_t		task_rx;
	QueueHandle_t		queue_rx;

    // Campos añadidos para el Paso 06 actividad 1
    SemaphoreHandle_t   hal_sem;     // Sincroniza la ISR con el Gatekeeper
    i2c_bus_mode_t      bus_mode;    // Modo operativo dinámico
} task_i2c_dta_t;

/********************** external data declaration ****************************/

/********************** external functions declaration ***********************/

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* TASK_I2C_ATTRIBUTE_H_ */

/********************** end of file ******************************************/
