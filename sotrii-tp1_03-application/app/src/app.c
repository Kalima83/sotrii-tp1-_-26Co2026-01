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
#include "app_it.h"
#include "task_receiver.h"
#include "task_adc.h"
#include "task_adc_interface.h"

/********************** macros and definitions *******************************/
#define G_APP_TICK_CNT_INI				0ul
#define G_TASK_IDLE_CNT_INI				0ul
#define G_APP_STACK_OVERFLOW_CNT_INI	0ul

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/
void task_adc_rx(void *parameters);

/********************** internal data definition *****************************/
const char *p_app	= "RTOS - Event-Triggered Systems (ETS)";
const char *p_app_	= "sotrii-tp1_03-application: Demo Code";
const char *p_app__	= "(Source => CESE - Sistemas Operativos de Tiempo Real)";

/********************** external data declaration ****************************/
uint32_t volatile g_app_tick_cnt;
uint32_t g_task_idle_cnt;
uint32_t g_app_stack_overflow_cnt;

/* Handles para el control y referenciación de los hilos del sistema */
TaskHandle_t h_task_receiver;
TaskHandle_t h_task_adc_rx;

/********************** external functions definition ************************/
void task_adc_rx(void *parameters);

void app_init(void)
{
	/*  Declare & Initialize App variables */
	g_app_tick_cnt = G_APP_TICK_CNT_INI;
	g_task_idle_cnt = G_TASK_IDLE_CNT_INI;
	g_app_stack_overflow_cnt = G_APP_STACK_OVERFLOW_CNT_INI;

	/* Print out: Application Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %lu", GET_NAME(app_init), xTaskGetTickCount());

	LOGGER_INFO(" %s is a %s", GET_NAME(app), p_app);
	LOGGER_INFO(" %s is a %s", GET_NAME(app), p_app_);
	LOGGER_INFO(" %s is a %s", GET_NAME(app), p_app__);

	BaseType_t ret;

	/* 1. Creación de la Tarea Consumidora (Capa Superior) a prioridad 1 */
	ret = xTaskCreate(task_receiver,					/* Puntero a la función de la tarea */
					  "Task Receiver",					/* Nombre simbólico descriptivo */
					  (2 * configMINIMAL_STACK_SIZE),	/* Tamaño de la pila asignada */
					  NULL,								/* Parámetros de entrada */
					  (tskIDLE_PRIORITY + 1ul),			/* Prioridad 1 */
					  &h_task_receiver);				/* Handle asociado */
	configASSERT(pdPASS == ret);

	/* 2. Creación de la Tarea Gestora del ADC de Bajo Nivel a prioridad 1 */
	ret = xTaskCreate(task_adc_rx,
					  "Task ADC RX",
					  (2 * configMINIMAL_STACK_SIZE),
					  NULL,
					  (tskIDLE_PRIORITY + 1ul),
					  &h_task_adc_rx);
	configASSERT(pdPASS == ret);

	/* Solicitamos tamaño remanente del Heap para diagnóstico */
	ret = xPortGetFreeHeapSize();

	/* 3. Inicialización del Device Driver ADC (Latest Input Only - DMA) */
	open_adc(&hadc1);

	/* 4. Inicialización de las interrupciones del sistema */
	app_it_init();

	/* 5. Inicialización del contador de ciclos por Hardware */
	cycle_counter_init();
}

/********************** end of file ******************************************/
