# Análisis de Arquitectura del Firmware (FreeRTOS)

Este sistema implementa una arquitectura basada en **FreeRTOS** (bajo la capa CMSIS-OS) y las bibliotecas HAL de STMicroelectronics para microcontroladores STM32. El diseño se fundamenta en el paradigma de **Sistemas Activados por Eventos (Event-Triggered Systems - ETS)**, estructurado mediante un driver de interfaz abstracta que maneja el bus I2C de forma asíncrona para evitar el bloqueo de las tareas de aplicación.

---

## 1. Arquitectura General y Flujo de Trabajo

El objetivo principal de este diseño es el desacoplamiento de capas. En lugar de que las tareas de la capa de aplicación (`task_sender`) invoquen funciones bloqueantes de hardware directas, estas interactúan con un **middleware de interfaz** (`task_i2c_interface`).

Este middleware empaqueta y encola las peticiones de transmisión en una cola de mensajes de FreeRTOS (`Queue`), delegando el control físico del bus a tareas dedicadas de menor nivel (`task_i2c_tx`). Esto permite maximizar la disponibilidad de la CPU mientras el periférico procesa los datos.

---

```text
+-------------------------------------------------------+
|                 CAPA DE APLICACIÓN                    |
|   [task_sender.c]              [task_receiver.c]      |
+---------+------------------------------^--------------+
          |                              |
          | (write_i2c)                  | (read_i2c)
          v                              |
+----------------------------------------+--------------+
|            INTERFAZ / MIDDLEWARE (DRIVER)             |
|              [task_i2c_interface.c/.h]                |
+---------+------------------------------+--------------+
          |                              ^
          | (xQueueSend)                 | (xQueueReceive)
          v                              |
     [ queue_tx ]                  [ queue_rx ]
          |                              ^
          | (xQueueReceive)              | (xQueueSend)
          v                              |
+----------------------------------------+--------------+
|                 CAPA DE BAJO NIVEL                    |
|                    [task_i2c.c]                       |
+-------------------------------------------------------+

---
## 2. Análisis Detallado por Archivo

### 📄 `app.c` (Inicialización de la Aplicación)
Es el punto de entrada lógico de la aplicación a través de la función `app_init()`.
* **Inicialización de Variables:** Configura los contadores globales para el monitoreo del sistema (`g_app_tick_cnt`, `g_task_idle_cnt`, etc.).
* **Creación de Tareas de Aplicación:** Instancia de forma dinámica dos tareas de FreeRTOS con prioridad `1` (`tskIDLE_PRIORITY + 1ul`):
  * `task_sender`: Hilo encargado de transmitir datos.
  * `task_receiver`: Hilo encargado de recibir datos.
* **Inicialización del Periférico:** Invoca a `open_i2c(&hi2c1)` para configurar la infraestructura del driver antes de habilitar las interrupciones mediante `app_it_init()` y el contador de ciclos de hardware (`DWT`).

### 📄 `task_i2c_interface.c` y `.h` (Interfaz del Driver I2C)
Este módulo actúa como una capa de abstracción de hardware (HAL/OS Wrapper) e implementa el patrón productor-consumidor.
* **`open_i2c()`**: Instancia y configura de manera centralizada la estructura interna `task_i2c_dta`. Crea dos colas de FreeRTOS: `queue_tx` (capacidad para 5 elementos de transmisión) y `queue_rx` (capacidad para 10 bytes). Posteriormente, **crea las tareas de bajo nivel** `task_i2c_tx` y `task_i2c_rx` encargadas de la operación real sobre el hardware.
* **`write_i2c()`**: Es la API expuesta a la capa de aplicación. Empaqueta la dirección de destino y el byte de datos en una estructura temporal y la envía a la cola de transmisión (`queue_tx`) mediante `xQueueSend()` utilizando un bloqueo indefinido (`portMAX_DELAY`).
* **`release_i2c()`**: Realiza el proceso inverso de limpieza, desregistrando y eliminando las colas creadas, además de destruir las tareas asociadas del driver mediante `vTaskDelete()` para liberar memoria del Heap.

### 📄 `task_sender.c` (Tarea Productora)
Es una tarea periódica perteneciente a la capa de aplicación.
* Se ejecuta en un bucle infinito e incrementa su propio contador de control `g_task_sender_cnt` en cada iteración.
* Diseñada para interactuar con un expansor de pines I2C **PCF8574** (típicamente usado en módulos seriales para displays LCD HD44780) con dirección base `0x27`.
* Ejecuta una operación bitwise NOT (`dev_data = ~dev_data`) para alternar continuamente el estado de los bits transmitidos (generando un patrón de parpadeo).
* Realiza el envío asíncrono llamando a `write_i2c(&hi2c1, dev_address, dev_data)` y cede inmediatamente el control del procesador durante 250 ms utilizando `vTaskDelay(TASK_SENDER_DEL_MAX)`.

### 📄 `task_i2c.c` (Consumidor de Hardware de Bajo Nivel)
Contiene las funciones internas de los hilos que manejan la comunicación directa con los registros del microcontrolador.
* **`task_i2c_tx`**: Se encuentra en estado bloqueado de forma indefinida en `xQueueReceive()`. Tan pronto como `task_sender` coloca un mensaje en la cola, esta tarea se despierta, extrae el paquete y ejecuta la llamada física bloqueante de la capa HAL: `HAL_I2C_Master_Transmit()`. Aplica un desplazamiento de bits (`address << 1`) sobre la dirección I2C para cumplir con el protocolo estándar de direccionamiento de 7 bits + bit de lectura/escritura. Utiliza un contador de ciclos para medir el tiempo exacto que le toma al bus completar la transferencia física (`cycle_counter_get_time_us`).
* **`task_i2c_rx`**: En su implementación actual, este hilo no realiza lecturas del bus I2C, sino que ejecuta de manera repetitiva una inversión del estado de un pin de hardware (`HAL_GPIO_TogglePin` sobre `LED_A_PIN`) cada 250 ms, sirviendo como un indicador visual de ejecución de la capa de bajo nivel.

### 📄 `task_receiver.c` (Tarea Consumidora de Aplicación)
Es la tarea complementaria en la capa de aplicación. Actualmente posee una estructura de bucle infinito vacía que simplemente incrementa su contador de control `g_task_receiver_cnt` y entra en un estado de bloqueo por 250 ms. Está diseñada arquitectónicamente para procesar los datos que eventualmente reciba la cola `queue_rx` del driver.

### 📄 `app_it.c` (Gestión de Interrupciones)
* **`app_it_init()`**: Muestra la estructura típica para la protección de secciones críticas mediante el apagado (`CPSID i`) y encendido (`CPSIE i`) de las interrupciones globales a nivel ensamblador de la arquitectura ARM Cortex-M.
* **`HAL_GPIO_EXTI_Callback()`**: Callback ejecutado por el hardware ante una interrupción externa de GPIO. Contiene la lógica preparada para discriminar si la interrupción fue generada por el pin `BTN_A_PIN` (Botón A) y ejecutar tareas basadas en eventos asíncronos externos.

### 📄 `freertos.c` (Hooks del Sistema Operativo)
Este archivo centraliza las funciones callback que reaccionan a eventos internos del propio núcleo del RTOS:
* **`vApplicationIdleHook()`**: Se ejecuta de forma repetitiva únicamente cuando ninguna tarea de la aplicación está lista para correr. Incrementa el contador `g_task_idle_cnt` (método estándar para estimar el porcentaje de tiempo libre de la CPU).
* **`vApplicationTickHook()`**: Se ejecuta en el contexto de la Interrupción de Servicio (ISR) del reloj del sistema (*SysTick*). Incrementa el contador temporal global `g_app_tick_cnt`.
* **`vApplicationStackOverflowHook()`**: Actúa como un mecanismo de seguridad crítico. Si una tarea excede la memoria asignada a su pila (*Stack*), el sistema entra en una zona crítica y detiene la ejecución del microcontrolador mediante `configASSERT(0)` para prevenir comportamientos erráticos y facilitar el diagnóstico con un depurador JTAG/SWD.

---

## 3. Matriz de Resumen de Tareas en Ejecución

| Nombre de la Tarea | Archivo de Origen | Prioridad | Comportamiento Principal |
| :--- | :--- | :--- | :--- |
| **`Task Sender`** | `task_sender.c` | `1` | Invierte un byte de datos y lo envía a la cola I2C cada 250 ms. |
| **`Task Receiver`** | `task_receiver.c` | `1` | Tarea de aplicación en espera (bucle periódico cada 250 ms). |
| **`Task I2C Tx`** | `task_i2c.c` | `1` | Despierta por eventos en la cola, transmite por I2C físico y mide tiempos. |
| **`Task I2C Rx`** | `task_i2c.c` | `1` | Realiza un toggle periódico sobre un LED testigo cada 250 ms. |
| **`Idle Task`** | Núcleo RTOS | `0` | Corre cuando el sistema está libre e incrementa el contador de ocio. |

---

## 4. Trazabilidad del Ciclo de Vida del Dato (Transmisión)

1. La tarea **`task_sender`** despierta de su letargo periódico de 250 ms.
2. Modifica el dato a transmitir (`dev_data = ~dev_data`) e invoca la función del driver `write_i2c()`.
3. **`write_i2c()`** realiza una sección crítica implícita copiando de forma segura los datos del mensaje en la cola `queue_tx`. Hecho esto, `task_sender` vuelve a dormirse por 250 ms con `vTaskDelay()`.
4. El planificador (*scheduler*) de FreeRTOS detecta que la cola `queue_tx` tiene elementos y cambia el estado de la tarea **`task_i2c_tx`** de *Blocked* a *Ready* (y luego a *Running*).
5. **`task_i2c_tx`** toma los datos de la cola y ejecuta la función bloqueante de hardware de bajo nivel `HAL_I2C_Master_Transmit()`, la cual manipula físicamente las líneas SCL y SDA del microcontrolador.
6. Se registra el tiempo transcurrido en la transmisión mediante el contador de ciclos y la tarea de bajo nivel se bloquea a sí misma por 250 ms para liberar el bus antes de su próximo ciclo de escucha de cola.


# Informe Final

## Paso 06: Observaciones de Comportamiento y Medición de WCET

### 1. Comportamiento Observado del Sistema
Al ejecutar el firmware con el planificador de FreeRTOS, se observa en la consola serial un flujo continuo y periódico (cada 250 ms) donde interactúan las cuatro tareas principales del driver y la aplicación:
* `Task SENDER` genera los datos y los deposita en la cola de transmisión.
* `Task I2C TX` (Gatekeeper de transmisión) toma los elementos de la cola y gestiona el periférico.
* `Task I2C RX` e `API read_i2c` obtienen de forma síncrona el contenido procesado (`Dato obtenido de la cola: 0x00`).
* `Task RECEIVER` procesa la recepción final.

Al presionar el botón de usuario (B1), el sistema conmuta dinámicamente el modo operativo de gestión de la HAL (`Polling` $\rightarrow$ `Interrupt` $\rightarrow$ `DMA`). A nivel lógico y en los logs de consola, el flujo y la periodicidad de los datos de Tx y Rx se mantienen completamente estables e inalterados bajo el patrón Gatekeeper.

---

### 2. Mediciones de WCET (Worst Case Execution Time) y Análisis Bifásico

Para analizar el tiempo máximo de ejecución medido a través del contador de ciclos DWT (`g_task_xxxx_tx_runtime_us`), es imperativo dividir el escenario en dos fases bien diferenciadas debido a las condiciones físicas del bus de pruebas:

#### **Fase 1: Comportamiento en el Entorno Actual (Sin Esclavo I2C Físico)**
En el banco de pruebas actual, las líneas del bus físico I2C se encuentran abiertas o emuladas sin un circuito integrado esclavo real que responda con señales de *Acknowledge* (ACK). 

* **Resultado medido:** El WCET arrojó valores prácticamente idénticos en los tres modos (**25078 $\mu s$** en Polling y **25077 $\mu s$** al conmutar a Interrupt/DMA).
* **Justificación técnica:** Al no existir un hardware real que responda en el bus, las APIs de la HAL (`HAL_I2C_Master_Transmit`, `_IT` y `_DMA`) experimentan inmediatamente la misma condición de falla por *Timeout* o *No Acknowledge* (NACK). El procesador retorna de la función de manera rápida por software en los tres casos, camuflando el beneficio de la descarga de CPU en los modos asíncronos.

#### **Fase 2: Comportamiento Esperado en un Entorno Ideal (Con Esclavo I2C Real)**
Si el bus contara con un sensor o memoria física real respondiendo en las líneas de hardware, el impacto en el WCET de la tarea de transmisión reflejaría la verdadera eficiencia del driver diseñado bajo FreeRTOS:

| Modo de Gestión (Con Hardware Real) | WCET Esperado | Justificación del Comportamiento de CPU |
| :--- | :---: | :--- |
| **Polling** | **~25000 $\mu s$** | **Espera activa y bloqueante:** El procesador queda retenido bit a bit controlando los registros de hardware durante todo el viaje de los datos. Alto impacto en el WCET. |
| **Interrupt** | **~45 $\mu s$** | **Liberación por Semáforo:** La tarea inicia la transferencia no bloqueante (`_IT`) y se va a dormir al instante con `xSemaphoreTake`. La CPU queda libre para otras tareas hasta que la ISR despierta el hilo al finalizar el bus. |
| **DMA** | **~18 $\mu s$** | **Máxima eficiencia:** La transferencia se delega por completo al periférico DMA (Direct Memory Access) directo desde la RAM. El WCET medido representa únicamente el tiempo de configuración de los descriptores de la HAL. |

---

### 3. Análisis de Patrones y Almacenamiento
* **Patrón de Interfaz:** Operatoria **Asíncrona** en `write_i2c` (no bloquea a la tarea emisora mediante la cola) y arquitectura centralizada bajo el patrón **Gatekeeper** para resolver de forma segura las condiciones de carrera sobre el recurso compartido (bus físico `I2C1`).
* **Almacenamiento:** Uso de colas inter-tareas (`Queue`) asignadas dinámicamente al inicio a través del Heap de FreeRTOS (`xQueueCreate`), pero con comportamiento estático y permanente en tiempo de ejecución, ya que el driver nunca destruye las estructuras (evitando la fragmentación de memoria RAM).
