# MI-SO v2.0 — Sistema operativo didáctico para STM32F103C8T6

Sistema operativo didáctico para la placa **Blue Pill STM32F103C8T6 (ARM Cortex-M3)**. Es una implementación **bare-metal** (sin otro sistema operativo debajo) en C11 y ensamblador ARM, **sin HAL, CMSIS-RTOS, FreeRTOS ni Zephyr**. Incluye: procesos con PCB, cambio de contexto expropiativo, tres algoritmos de planificación, sincronización, gestor de memoria propio, drivers y una consola de comandos.

Proyecto final de Sistemas Operativos, UAZ.

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

### Cómo compilar y cargar (Windows 10 y Windows 11)

**Programas que se instalan una sola vez:**

| Programa | Para qué sirve |
|---|---|
| **STM32CubeIDE** (o Arm GNU Toolchain + GNU Make) | Trae el compilador `arm-none-eabi-gcc` y `make` |
| **STM32CubeProgrammer** | Trae `STM32_Programmer_CLI` y el driver del ST-Link, para grabar la placa |
| **Git for Windows** | Trae la terminal **Git Bash**; el Makefile usa `mkdir -p` y `rm`, que no existen en CMD |
| **Python 3** + `pip install pyserial` | Para abrir la consola serie con `miniterm` |

**Pasos:**

1. **Conecta la placa** como indica [`1_DOCUMENTO_DE_DISENO/CONEXIONES.md`](1_DOCUMENTO_DE_DISENO/CONEXIONES.md), con **BOOT0 = 0**.
2. **Abre Git Bash** en la carpeta `MI_SO_STM32F103C8T6`. Si usas el compilador de STM32CubeIDE, agrégalo al `PATH` en esa terminal (ajusta la ruta si lo instalaste en otro lugar):
   ```bash
   P=/c/ST/STM32CubeIDE_2.3.0/STM32CubeIDE/plugins
   export PATH="$(ls -d $P/*gnu-tools-for-stm32*/tools/bin):$(ls -d $P/*externaltools.make*/tools/bin):$PATH"
   ```
3. **Compila:**
   ```bash
   make
   ```
   Debe crear `build/mi_so_stm32.bin` (también `.elf` y `.map`).
4. **Graba la placa** con el ST-Link conectado:
   ```bash
   STM32_Programmer_CLI -c port=SWD -w build/mi_so_stm32.bin 0x08000000 -v -rst
   ```
   Si `STM32_Programmer_CLI` no se encuentra, está en `C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin`. Alternativa con OpenOCD: `make flash`.
5. **Comprueba:** el LED integrado **PC13** debe parpadear.
6. **Abre la consola** (cambia `COM6` por el puerto de tu adaptador; se ve en *Administrador de dispositivos → Puertos (COM y LPT)*):
   ```bash
   python -m serial.tools.miniterm COM6 115200
   ```
   Presiona Enter: aparece `miso>`. Prueba `help`, `ps`, `mem` y `uptime`. Para salir: **Ctrl + ]**.

**Ojo:** el ST-Link V2 **no crea un puerto COM** para la consola. Se necesita un adaptador **USB–TTL de 3.3 V** (por ejemplo CP2102) o un **ESP32 como puente** (es lo que se usó en las pruebas; ver `1_DOCUMENTO_DE_DISENO/CONEXIONES.md`). Nunca conectes señales de 5 V a la Blue Pill.

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
| `src/shell/` | Intérprete de comandos (lista abajo) |
| `src/app/` | Demo física y benchmark de dos tareas CPU-bound |
| `linker/` | Reservas de Flash y RAM, ubicaciones de código y datos |
| `tests/` | Pruebas host de planificación, heap y sincronización (sin MCU) |
| `1_DOCUMENTO_DE_DISENO/` … `4_REPORTE_FINAL/` | Entregables (ver tabla al inicio) |

### Comandos del shell

`help`, `ps`, `mem`, `memtest`, `memfit first`, `memfit best`, `sched rr 5`, `sched prio`, `sched fcfs`, `quantum 20`, `run bench`, `run pc`, `led 0 1`, `led 1 1`, `led 2 1`, `adc`, `uptime`, `kill 4`, `ver`.

El benchmark compara `respuesta`, `espera` aproximada y `retorno` para las tareas `benchA` y `benchB` en cada modo; los resultados medidos en la placa están en `2_PRUEBAS_Y_EVIDENCIAS/PRUEBAS.pdf`. En FCFS, tareas de CPU sin bloqueo pueden impedir por un tiempo la ejecución de otras tareas; es justamente el comportamiento que se busca medir.

### Estado de validación

Compilado con `arm-none-eabi-gcc`, grabado y probado en una Blue Pill real (2026-11-25): las **13 pruebas de hardware** resultaron OK. Los resultados están en [`2_PRUEBAS_Y_EVIDENCIAS/PRUEBAS.md`](2_PRUEBAS_Y_EVIDENCIAS/PRUEBAS.md) y la sesión serie en [`2_PRUEBAS_Y_EVIDENCIAS/EVIDENCIA_CONSOLA.txt`](2_PRUEBAS_Y_EVIDENCIAS/EVIDENCIA_CONSOLA.txt). `make test-host` prueba además planificación, heap y sincronización en la PC; complementa las pruebas en placa, no las sustituye.
