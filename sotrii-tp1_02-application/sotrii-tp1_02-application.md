# Análisis de Arquitectura del Firmware (FreeRTOS - UART)

Este sistema implementa una arquitectura basada en **FreeRTOS** (bajo la capa CMSIS-OS) y las bibliotecas HAL de STMicroelectronics para microcontroladores STM32. El diseño se fundamenta en el desacoplamiento de la capa de aplicación y la capa de bajo nivel, utilizando una interfaz abstracta para interactuar con el periférico **UART** de forma asíncrona.

---

## 1. Arquitectura General y Flujo de Trabajo

El sistema está estructurado jerárquicamente para evitar que las tareas de la aplicación principal bloqueen el procesador al interactuar con el hardware de comunicación. 

Las tareas de la capa de aplicación (`task_sender` y `task_receiver`) interactúan de manera exclusiva con el **middleware de interfaz** (`task_uart_interface`). Este componente es el encargado de administrar el control de la comunicación delegando las tareas físicas a hilos de bajo nivel (`task_uart_tx` y `task_uart_rx`).

+-------------------------------------------------------+
|                 CAPA DE APLICACIÓN                    |
|   [task_sender.c]              [task_receiver.c]      |
+---------+------------------------------^--------------+
| (write_uart)                 | (read_uart)
v                              |
+----------------------------------------+--------------+
|            INTERFAZ / MIDDLEWARE (DRIVER)             |
|             [task_uart_interface.c/.h]                |
+---------+------------------------------+--------------+
|                              ^
v                              |
+---------+------------------------------+--------------+
|                 CAPA DE BAJO NIVEL                    |
|                    [task_uart.c]                      |
+-------------------------------------------------------+

## 2. Análisis Detallado por Archivo

### 📄 `app.c` (Inicialización de la Aplicación)
Es el punto de entrada lógico de la aplicación a través de la función `app_init()`.
* **Inicialización de Monitores:** Configura los contadores globales para las estadísticas de uso del sistema (`g_app_tick_cnt`, `g_task_idle_cnt`).
* **Creación de Tareas de Aplicación:** Instancia de forma dinámica dos hilos de FreeRTOS con prioridad `1` (`tskIDLE_PRIORITY + 1ul`):
  * `task_sender`: Hilo encargado de la lógica de envío de datos.
  * `task_receiver`: Hilo encargado de la lógica de recepción de datos.
* **Inicialización del Driver:** Invoca a `open_uart(&huart2)` para preparar la infraestructura del controlador serial antes de habilitar las interrupciones mediante `app_it_init()`.

### 📄 `task_uart_interface.c` y `.h` (Interfaz del Driver UART)
Este módulo actúa como una capa de abstracción de hardware (HAL/OS Wrapper) definiendo las APIs del driver.
* **`open_uart()` / `release_uart()`**: En esta versión base, actúan como marcadores de posición (*placeholders*) listos para inicializar y liberar las estructuras de datos, colas/semáforos o hilos necesarios para administrar el puerto serial (`huart2`).
* **`write_uart()` / `read_uart()` / `ioctl_uart()`**: Proveen los prototipos y funciones de envoltura seguras expuestas a la capa superior para realizar operaciones de escritura, lectura y control de Entrada/Salida del periférico.

### 📄 `task_sender.c` (Tarea Productora de Aplicación)
Es una tarea periódica perteneciente a la capa de aplicación superior.
* Se ejecuta en un bucle infinito e incrementa su propio contador de control `g_task_sender_cnt` en cada ciclo.
* En su ciclo actual, está diseñada para invocar las operaciones de la interfaz y ceder el procesador de forma voluntaria durante 250 milisegundos mediante la directiva `vTaskDelay(TASK_SENDER_DEL_MAX)`, liberando la CPU para otras tareas.

### 📄 `task_uart.c` (Gestor de Hardware de Bajo Nivel)
Contiene las funciones internas de los hilos que manipulan directamente los registros o las funciones HAL del microcontrolador.
* **`task_uart_tx`**: Se ejecuta de forma cíclica e incrementa el contador `g_task_xxxx_tx_cnt`. Realiza un parpadeo de hardware (`HAL_GPIO_TogglePin` sobre `LED_A_PIN`), resetea y mide el tiempo exacto de ejecución en microsegundos usando el contador de ciclos (`cycle_counter_get_time_us`), y se bloquea por 250 ms.
* **`task_uart_rx`**: Actúa en paralelo administrando la recepción. Incrementa su respectivo contador (`g_task_xxxx_rx_cnt`), mide su propio tiempo de ejecución y entra en modo de espera periódica de 250 ms.

### 📄 `task_receiver.c` (Tarea Consumidora de Aplicación)
Es la contraparte de la tarea de envío en la capa de aplicación. Actualmente posee una estructura de bucle infinito que incrementa el contador de control de recepción de aplicación `g_task_receiver_cnt` y entra en un estado de bloqueo por 250 ms. Está pensada para procesar los datos extraídos por la interfaz una vez que el hardware los reciba.

### 📄 `app_it.c` (Gestión de Interrupciones)
* **`app_it_init()`**: Inicializa variables de estado para interrupciones (`hal_xxxx_callback_flag`) bajo una sección crítica deshabilitando (`CPSID i`) y habilitando (`CPSIE i`) temporalmente las interrupciones globales a nivel ensamblador de Cortex-M.
* **`HAL_GPIO_EXTI_Callback()`**: Callback ejecutado por hardware ante una interrupción de pin externo (ej. la pulsación del botón `BTN_A_PIN`).
* **Callbacks de UART (`HAL_UART_TxCpltCallback` / `HAL_UART_RxCpltCallback`)**: Funciones preparadas para ser disparadas de forma asíncrona por el hardware cuando el canal DMA o las interrupciones de la UART terminen de transmitir o recibir un buffer de datos, permitiendo notificar inmediatamente a las tareas bloqueadas.

### 📄 `freertos.c` (Hooks y Eventos del Sistema Operativo)
Centraliza las funciones callback del núcleo de FreeRTOS para el diagnóstico de salud del sistema:
* **`vApplicationIdleHook()`**: Se ejecuta de forma continua cuando no hay tareas de aplicación listas para correr. Incrementa `g_task_idle_cnt` para medir el tiempo de ocio de la CPU.
* **`vApplicationTickHook()`**: Se ejecuta dentro de la ISR del SysTick de FreeRTOS cada 1 ms para actualizar el contador de tiempo del sistema (`g_app_tick_cnt`).
* **`vApplicationStackOverflowHook()`**: Rutina de seguridad. Si un hilo desborda la pila de memoria asignada, detiene por completo el procesador mediante un `configASSERT(0)`, evitando corrupciones de memoria y asistiendo al desarrollador en la depuración.

---

## 3. Matriz de Tareas en Ejecución

| Nombre de la Tarea | Archivo de Origen | Prioridad | Comportamiento Principal |
| :--- | :--- | :--- | :--- |
| **`Task Sender`** | `task_sender.c` | `1` | Tarea de aplicación periódica (bucle cada 250 ms). |
| **`Task Receiver`** | `task_receiver.c` | `1` | Tarea de aplicación periódica (bucle cada 250 ms). |
| **`Task UART Tx`** | `task_uart.c` | `1` | Realiza toggle de un LED, mide tiempos de ejecución de transmisión y espera 250 ms. |
| **`Task UART Rx`** | `task_uart.c` | `1` | Mide tiempos de ejecución de recepción de hardware y espera 250 ms. |
| **`Idle Task`** | Núcleo RTOS | `0` | Corre cuando el sistema no tiene trabajo activo; mide carga de CPU. |

# Informe final
  
## Paso 6: Mediciones de Tiempo de Ejecución (WCET) y Comportamiento Dinámico

Se analizó el comportamiento dinámico del driver ADC implementado bajo el patrón **Latest Input Only** asistido por DMA en una plataforma STM32F446RE. Utilizando el bloque periférico **DWT (Data Watchpoint and Trace)** del núcleo ARM Cortex-M4, se capturaron de manera empírica los tiempos del peor caso de ejecución (WCET).

### 1. Valores Obtenidos en Laboratorio

| Componente Medido | Variable en Código | Ciclos de CPU (WCET) | Tiempo Estimado ($\mu s$) |
| :--- | :--- | :--- | :--- |
| **Función de Interfaz (`read_adc`)** | `g_task_adc_read_runtime_cycles` / `_us` | 22 | 0 |
| **Callback de Interrupción (ISR DMA)** | `g_task_adc_dma_callback_runtime_cycles` / `_us` | 28 | 0 |

### 2. Análisis del Comportamiento Observado

* **Comportamiento de `read_adc` (Capa de Aplicación):** 
  Al ejecutarse bajo el patrón *Latest Input Only* con DMA circular, la función de interfaz `read_adc` posee un WCET extremadamente bajo de apenas **22 ciclos de CPU**. Esto se debe a que la función no bloquea la tarea ni inicia una conversión por software; simplemente actúa como un pasamanos reglamentario de la arquitectura, permitiendo que la tarea consuma el valor de la variable global asíncrona de manera inmediata.
  
* **Comportamiento de `HAL_ADC_ConvCpltCallback` (Capa de Hardware/ISR):**
  La rutina de servicio de interrupción registró un WCET de **28 ciclos de CPU**. Al estar configurado el DMA en modo Circular, el contador de callbacks (`hal_xxxx_callback_cnt`) se estabiliza rápidamente ya que el controlador transfiere los datos a la memoria RAM de forma autónoma en background. El tiempo medido en ciclos representa el costo real de guardar de manera segura la última muestra válida en `g_adc_latest_value` (950 en las pruebas de laboratorio).
  
* **Resolución Temporal en Microsegundos:**
  Debido a la elevada frecuencia de reloj del procesador, ambas operaciones toman una fracción de microsegundo (aprox. $0.12 - 0.15\text{ }\mu\text{s}$). Como consecuencia, las variables de diagnóstico calculadas por división entera (`_runtime_us`) truncan su valor a **0**, demostrando que el impacto o *overhead* del driver sobre el procesador es despreciable y óptimo para las restricciones de tiempo real del sistema.
  
