# MI-SO v2.0 — Sistema operativo didáctico para STM32F103C8T6

Migración del kernel MI-SO para Arduino AVR hacia una **Blue Pill STM32F103C8T6 (ARM Cortex-M3)**. Es una implementación **bare-metal** en C11 y ensamblador ARM, **sin Arduino IDE, HAL, CMSIS-RTOS, FreeRTOS ni Zephyr**. Se conserva la idea pedagógica del proyecto anterior: procesos/TCB, cambio de contexto expropiativo, dos o más algoritmos, sincronización, memoria propia, drivers y consola.

### Entregables

| Entregable | Dónde está |
|---|---|
| Código fuente por módulos (`kernel/`, `sched/`, `mem/`, `sync/`, `drivers/`, `shell/`) | [`src/`](src/) — compilación y carga: sección *Cómo compilar y cargar* de este README |
| Documento de diseño (capas, PCB, estados, cambio de contexto, tabla de libros) | [`1_DOCUMENTO_DE_DISENO/DISENO.pdf`](1_DOCUMENTO_DE_DISENO/DISENO.pdf) |
| Pruebas y evidencias (planificación, sincronización, memoria) | [`2_PRUEBAS_Y_EVIDENCIAS/PRUEBAS.pdf`](2_PRUEBAS_Y_EVIDENCIAS/PRUEBAS.pdf) |
| Video del sistema funcionando | https://youtu.be/8ksK_f6eTmM (también en [`3_VIDEO/`](3_VIDEO/)) |
| Reporte final (conclusiones y comparación con RTOS comercial) | [`4_REPORTE_FINAL/REPORTE_FINAL.pdf`](4_REPORTE_FINAL/REPORTE_FINAL.pdf) |

### Especificaciones de hardware adoptadas

- **MCU:** STM32F103C8T6, hasta 72 MHz; diseño inicial conservador con **reloj HSI interno de 8 MHz**.
- **64 KB Flash, 20 KB SRAM**; `linker/stm32f103c8.ld` limita explícitamente a esas capacidades aunque algunas placas vendidas como C8 reporten más Flash.
- **SysTick a 1000 Hz (1 ms)** y **PendSV** a prioridad mínima para el cambio de contexto.
- **10 TCB**, pila independiente estática de **640 bytes por tarea** (incluido `idle`), **heap propio de 4096 bytes** separado de las pilas.
- **TCB:** ID, nombre, prioridad, estado, SP/PSP, entradas, ticks CPU, temporizador, listas de espera.
- **Modos:** Round Robin `rr`, prioridades expropiativas `prio` (RR entre prioridades iguales), FCFS real `fcfs` no expropiativo.
- Sin MPU de procesos ni aislamiento de memoria; API de kernel es frontera de software, no una frontera de seguridad.

### Cómo compilar y cargar desde Windows 11

1. Instala **Arm GNU Toolchain** (comando `arm-none-eabi-gcc`), **GNU Make** y **OpenOCD**. Agrégalos al `PATH` o utiliza una terminal **MSYS2/Git Bash** (los comandos `mkdir -p` y `rm` del Makefile requieren una terminal tipo Unix). También puedes crear un proyecto bare-metal equivalente en STM32CubeIDE importando el código y linker.
2. Abre la terminal en la carpeta `MI_SO_STM32F103C8T6`.
3. Ejecuta `make` y comprueba que se crea `build/mi_so_stm32.elf` (también `.bin` y `.map`).
4. Conecta el ST-Link tal como indica [`1_DOCUMENTO_DE_DISENO/CONEXIONES.md`](1_DOCUMENTO_DE_DISENO/CONEXIONES.md), con **BOOT0=0**, y ejecuta `make flash` (usa OpenOCD). Alternativa, usada en las pruebas: `STM32_Programmer_CLI -c port=SWD -w build/mi_so_stm32.bin 0x08000000 -v -rst`.
5. El LED integrado **PC13** debe parpadear. Con un adaptador USB–TTL 3.3 V abre el puerto COM a **115200, 8N1** y prueba `help`, `ps`, `mem`, `uptime`.

**Ojo:** el dongle ST-Link V2 de tu foto normalmente **no crea un puerto COM para UART**. Se necesita un adaptador **USB–TTL 3.3 V** (por ejemplo CP2102) o un ESP32 usado como puente (ver `1_DOCUMENTO_DE_DISENO/CONEXIONES.md`) para el shell. También se puede probar inicialmente solo con el ST-Link observando PC13. Nunca conectes señales TTL de 5 V a la Blue Pill.

### Pinout utilizado

| Función | Pin de Blue Pill | Conexión externa |
|---|---|---|
| LED integrado | **PC13** | Ya está en la placa, activo en bajo |
| LED de botón | **PB12** | PB12 → resistencia 330 Ω → ánodo LED → cátodo a GND |
| LED productor–consumidor | **PB13** | PB13 → resistencia 330 Ω → ánodo LED → cátodo a GND |
| Pulsador | **PA0** | PA0 → botón → GND (pull-up interno) |
| ADC potenciómetro | **PA5** | Potenciómetro: centro PA5, extremos 3.3V y GND |
| USART1 TX | **PA9** | A RX de USB–TTL 3.3 V |
| USART1 RX | **PA10** | A TX de USB–TTL 3.3 V |
| SWDIO | **PA13** | A SWDIO del ST-Link |
| SWCLK | **PA14** | A SWCLK del ST-Link |

Conecta **GND común** con ST-Link y USB–TTL. Para la alimentación inicial usa **una sola fuente de 3.3 V regulada** (por ejemplo la salida 3.3 V del ST-Link si tu adaptador efectivamente proporciona corriente suficiente y está rotulado correctamente). Evita conectar al mismo tiempo alimentación del puerto USB de la Blue Pill, salida 3.3 V del ST-Link y salida 3.3 V del USB–TTL.

### Estructura del código

| Carpeta | Contenido |
|---|---|
| `src/kernel/` | `startup.S`, `context.S`, `kernel.c`, API y registros del MCU |
| `src/sched/` | PCB, colas de preparados, RR, PRIO y FCFS |
| `src/sync/` | Semáforos, mutex con timeout, mailbox y productor–consumidor |
| `src/mem/` | Heap propio first/best-fit + coalescing |
| `src/drivers/` | GPIO PC13/PB12/PB13/PA0, UART1, ADC1 PA5 |
| `src/shell/` | Intérprete de comandos con 14+ opciones |
| `src/app/` | Demo física y benchmark de dos tareas CPU-bound |
| `linker/` | Reservas de Flash y RAM, ubicaciones de código y datos |
| `tests/` | Pruebas host de planificación, heap y sincronización (sin MCU) |
| `1_DOCUMENTO_DE_DISENO/` … `4_REPORTE_FINAL/` | Entregables (ver tabla al inicio) |

### Comandos del shell

`help`, `ps`, `mem`, `memtest`, `memfit first`, `memfit best`, `sched rr 5`, `sched prio`, `sched fcfs`, `quantum 20`, `run bench`, `run pc`, `led 0 1`, `led 1 1`, `led 2 1`, `adc`, `uptime`, `kill 4`, `ver`.

El benchmark compara `respuesta`, `espera` aproximada y `retorno` para las tareas `benchA` y `benchB`; se debe ejecutar en cada modo y **registrar datos reales de tu placa**. En FCFS, tareas de CPU sin bloqueo pueden impedir por un tiempo la ejecución de otras tareas; es justamente el comportamiento que se busca medir.

### Estado de validación

Compilado con `arm-none-eabi-gcc`, grabado y probado en una Blue Pill real (2026-11-25): las **13 pruebas de hardware** resultaron OK. Los resultados están en [`2_PRUEBAS_Y_EVIDENCIAS/PRUEBAS.md`](2_PRUEBAS_Y_EVIDENCIAS/PRUEBAS.md) y la sesión serie en [`2_PRUEBAS_Y_EVIDENCIAS/EVIDENCIA_CONSOLA.txt`](2_PRUEBAS_Y_EVIDENCIAS/EVIDENCIA_CONSOLA.txt). `make test-host` prueba además planificación, heap y sincronización en la PC; complementa las pruebas en placa, no las sustituye.

Se ha generado una nueva arquitectura; los PDF previos de Arduino **no son evidencias** del STM32.
