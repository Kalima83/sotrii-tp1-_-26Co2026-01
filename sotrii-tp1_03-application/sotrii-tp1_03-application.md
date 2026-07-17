# Análisis de Arquitectura del Firmware (FreeRTOS - ADC)

Este sistema implementa una arquitectura basada en el sistema operativo en tiempo real **FreeRTOS** (utilizando la abstracción CMSIS-OS) y las bibliotecas HAL de STMicroelectronics para microcontroladores STM32. El diseño aplica el desacoplamiento de capas mediante una interfaz abstracta para interactuar con el convertidor analógico-digital (**ADC**) de forma asíncrona, optimizando la eficiencia de la CPU.

---

## 1. Arquitectura General y Flujo de Trabajo

El firmware está diseñado bajo el patrón de arquitectura por capas con un enfoque asíncrono para evitar llamadas bloqueantes directas al hardware desde la aplicación.

Las tareas de la capa superior de aplicación (`task_receiver`, o potenciales tareas consumidoras) interactúan de manera exclusiva con el **middleware de interfaz** (`task_adc_interface`). Esta interfaz abstrae las complejidades del periférico y se comunica con la tarea de bajo nivel (`task_adc`) encargada de coordinar los muestreos físicos, el disparo de conversiones y el procesamiento temporal.


``` text
+-------------------------------------------------------+
|                 CAPA DE APLICACIÓN                    |
|                [task_receiver.c]                      |
+-----------------------+-------------------------------+
| (read_adc / ioctl_adc)
v
+-------------------------------------------------------+
|            INTERFAZ / MIDDLEWARE (DRIVER)             |
|              [task_adc_interface.c/.h]                |
+-----------------------+-------------------------------+
|
v
+-------------------------------------------------------+
|                 CAPA DE BAJO NIVEL                    |
|                    [task_adc.c]                       |
+-------------------------------------------------------+

```

---

## 2. Análisis Detallado por Archivo

### 📄 `app.c` (Inicialización de la Aplicación)
Actúa como el orquestador principal del inicio del software a través de la función `app_init()`.
* **Inicialización de Monitores:** Configura los contadores globales para estadísticas del núcleo del sistema operativo (`g_app_tick_cnt`, `g_task_idle_cnt`).
* **Creación de Tareas de Aplicación:** Instancia dinámicamente el hilo de la capa superior:
  * `task_receiver`: Hilo de aplicación con prioridad `1` (`tskIDLE_PRIORITY + 1ul`).
* **Inicialización del Driver:** Invoca a `open_adc(&hadc1)` para preparar las estructuras internas de control del convertidor antes de inicializar las interrupciones generales con `app_it_init()`.

### 📄 `task_adc_interface.c` y `.h` (Interfaz del Driver ADC)
Este módulo define la capa de abstracción (API) para aislar la manipulación de los registros de hardware del ADC.
* **`open_adc()` / `release_adc()`**: Actúan como funciones contenedoras (*placeholders*) destinadas a reservar recursos del sistema operativo (colas, semáforos o hilos internos del driver) y registrar el manejo del periférico de conversión (`hadc1`).
* **`read_adc()` / `ioctl_adc()`**: Proveen la interfaz pública estandarizada para que la aplicación solicite lecturas de los canales analógicos configurados o configure parámetros del periférico (ganancias, modos de disparo, etc.) de forma controlada.

### 📄 `task_adc.c` (Gestor de Hardware de Bajo Nivel)
Contiene los hilos de ejecución que interactúan directamente con la capa HAL del microcontrolador.
* **`task_adc`**: Se ejecuta de manera cíclica incrementando el contador global de control `g_task_xxxx_tx_cnt`.
* **Medición de Tiempos:** En cada ciclo realiza un parpadeo de hardware (`HAL_GPIO_TogglePin` sobre `LED_A_PIN`), limpia el contador de ciclos (`cycle_counter_reset`), evalúa los tiempos de procesamiento en microsegundos mediante `cycle_counter_get_time_us` (`g_task_xxxx_tx_runtime_us`), y se bloquea por 250 ms con `vTaskDelay(TASK_XXXX_DEL_MAX)`.

### 📄 `task_receiver.c` (Tarea Consumidora de Aplicación)
Representa la tarea de la capa superior encargada de procesar la información obtenida.
* Corre en un ciclo infinito incrementando su propio contador de control `g_task_receiver_cnt`.
* Monitorea o simula la recepción bloqueándose periódicamente cada 250 ms a través de la directiva `vTaskDelay()`, permitiendo que el microcontrolador ceda tiempo de procesamiento a hilos secundarios o de menor prioridad.

### 📄 `app_it.c` (Gestión de Interrupciones)
* **`app_it_init()`**: Configura variables críticas de estado de interrupción (`hal_xxxx_callback_flag`) bajo una sección crítica estricta implementada en ensamblador (`CPSID i` para deshabilitar y `CPSIE i` para habilitar interrupciones globales).
* **Callbacks de Hardware (`HAL_GPIO_EXTI_Callback` / `HAL_ADC_ConvCpltCallback`)**: El callback de ADC está preparado para capturar la interrupción por hardware generada cuando finaliza una conversión analógica a digital (ya sea por ciclo simple o por fin de buffer de DMA), permitiendo notificar inmediatamente al driver que hay nuevos datos listos para ser leídos.

### 📄 `freertos.c` (Hooks del Núcleo)
Centraliza las rutinas de monitoreo y seguridad incorporadas dentro del propio planificador (*scheduler*):
* **`vApplicationIdleHook()`**: Incrementa `g_task_idle_cnt` cada vez que el procesador no tiene tareas de aplicación en estado *Ready*, sirviendo para diagnosticar el porcentaje de tiempo libre de la CPU.
* **`vApplicationTickHook()`**: ISR ejecutada con cada interrupción del *SysTick* (cada 1 ms) para actualizar el contador de tiempo de la aplicación (`g_app_tick_cnt`).
* **`vApplicationStackOverflowHook()`**: Captura errores críticos de dimensionamiento. Si cualquier tarea desborda su pila asignada, el sistema entra en un bucle controlado por un `configASSERT(0)` para salvaguardar la integridad de la memoria e identificar la tarea culpable mediante un depurador.

---

## 3. Matriz de Resumen de Tareas en Ejecución

| Nombre de la Tarea | Archivo de Origen | Prioridad | Comportamiento Principal |
| :--- | :--- | :--- | :--- |
| **`Task Receiver`** | `task_receiver.c` | `1` | Tarea consumidora de aplicación en bucle periódico (cada 250 ms). |
| **`Task ADC`** | `task_adc.c` | `1` | Ejecuta el control periódico del hardware, realiza un toggle de LED y mide tiempos de conversión. |
| **`Idle Task`** | Núcleo RTOS | `0` | Corre cuando el sistema está desocupado; actualiza el contador de ocio de la CPU. |

---

## 4. Trazabilidad del Ciclo de Vida del Evento (Conversión)

1. El hardware o un temporizador interno dispara el inicio de una conversión analógica en el periférico **ADC**.
2. Al completarse la digitalización de la señal, el periférico genera una interrupción y el procesador ejecuta el callback **`HAL_ADC_ConvCpltCallback()`** dentro de `app_it.c`.
3. El callback modifica la variable segura `hal_xxxx_callback_flag` y notifica de manera asíncrona a la interfaz.
4. La tarea de bajo nivel **`task_adc`** registra el tiempo en microsegundos que duró el proceso utilizando el contador de ciclos y modifica el estado del `LED_A_PIN` para dar una indicación visual del muestreo.
5. Los datos quedan disponibles para ser leídos por la capa de aplicación superior mediante las funciones controladas **`read_adc()`** expuestas por el driver.

```
## 4. Mediciones de Tiempo de Ejecución (WCET) y Comportamiento Dinámico

Se analizó el comportamiento dinámico del driver ADC implementado bajo el patrón **Latest Input Only** asistido por DMA en una plataforma STM32F446RE. Utilizando el bloque periférico **DWT (Data Watchpoint and Trace)** del núcleo ARM Cortex-M4, se capturaron de manera empírica los tiempos del peor caso de ejecución (WCET).

### 4.1. Valores Obtenidos en Laboratorio

| Componente Medido | Variable en Código | Ciclos de CPU (WCET) | Tiempo Estimado ($\mu s$) |
| :--- | :--- | :--- | :--- |
| **Función de Interfaz (`read_adc`)** | `g_task_adc_read_runtime_cycles` / `_us` | 22 | 0 |
| **Callback de Interrupción (ISR DMA)** | `g_task_adc_dma_callback_runtime_cycles` / `_us` | 28 | 0 |

### 4.2. Análisis del Comportamiento Observado

* **Comportamiento de `read_adc` (Capa de Aplicación):** 
  Al ejecutarse bajo el patrón *Latest Input Only* con DMA circular, la función de interfaz `read_adc` posee un WCET extremadamente bajo de apenas **22 ciclos de CPU**. Esto se debe a que la función no bloquea la tarea ni inicia una conversión por software; simplemente actúa como un pasamanos reglamentario de la arquitectura, permitiendo que la tarea consuma el valor de la variable global asíncrona de manera inmediata.
  
* **Comportamiento de `HAL_ADC_ConvCpltCallback` (Capa de Hardware/ISR):**
  La rutina de servicio de interrupción registró un WCET de **28 ciclos de CPU**. Al estar configurado el DMA en modo Circular, el contador de callbacks (`hal_xxxx_callback_cnt`) se estabiliza rápidamente ya que el controlador transfiere los datos a la memoria RAM de forma autónoma en background. El tiempo medido en ciclos representa el costo real de guardar de manera segura la última muestra válida en `g_adc_latest_value` (950 en las pruebas de laboratorio).
  
* **Resolución Temporal en Microsegundos:**
  Debido a la elevada frecuencia de reloj del procesador, ambas operaciones toman una fracción de microsegundo (aprox. $0.12 - 0.15\text{ }\mu\text{s}$). Como consecuencia, las variables de diagnóstico calculadas por división entera (`_runtime_us`) truncan su valor a **0**, demostrando que el impacto o *overhead* del driver sobre el procesador es despreciable y óptimo para las restricciones de tiempo real del sistema.

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
  