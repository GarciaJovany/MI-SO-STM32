# Protocolo de pruebas y evidencias — MI-SO STM32

Resultados observados en una placa Blue Pill real (2026-11-25). Las 13 pruebas de hardware resultaron OK.

## Pruebas previas al laboratorio

1. Ejecutar `make` con `arm-none-eabi-gcc` y registrar tamaño de `.text`, `.data` y `.bss` con `arm-none-eabi-size`.
2. Ejecutar `make test-host` para comprobar los algoritmos, el heap, la sincronización y `kill` en computadora.
3. Ejecutar `make flash` y comprobar que la programación y la verificación terminan bien (también sirve STM32CubeProgrammer, usado en estas pruebas).

**Resultado:** compilación sin advertencias (`-Wall -Wextra -Werror`); `.text` 10520, `.data` 4 y `.bss` 11448 bytes. `make test-host`: OK en planificación, heap y sincronización/`kill` (7200 ejecuciones). Estas pruebas corren en PC, no en la placa.

## Matriz de pruebas en hardware

| # | Comando o estímulo | Resultado esperado | Observación real y evidencia |
|---|---|---|---|
| 1 | Reset y `ps` | Shell, prod, cons, blink, button, action e idle; PCB con SP/estado | OK. 7 tareas con PCB, SP y estado; canarios OK; pila máxima 200/640 |
| 2 | PC13 durante 5 s | LED integrado parpadea cada ~300 ms | OK. Parpadeo de 300 ms observado a la vista |
| 3 | `uptime` dos veces | Milisegundos aumentan | OK. 647341 → 650882 ms, sin reiniciar |
| 4 | `mem`, `memtest`, `mem` | Reservas liberadas, heap sin fugas | OK. 3 reservas correctas; usado=0 y 1 bloque tras liberar |
| 5 | `memfit best`, `memfit first` | Cambia estrategia | OK. `modo=best` y `modo=first` |
| 6 | `run pc`, esperar 10 s, `run pc` | Contadores crecen y buffer se mantiene en 0..4 | OK. Producidos 2341 → 2472 (+131), consumidos 2337 → 2468 (+131); nivel 4/4 en ambas |
| 7 | Pulsar PA0 | LED PB12 destella; mensaje por cola de tareas | OK. Destello de PB12 al pulsar, observado a la vista; cola atendida (leída por SWD) |
| 8 | Girar potenciómetro PA5 | `adc` cambia de 0 a aproximadamente 4095; PB13 cambia al cruzar mitad | OK. `adc`=4 (lado GND) y 4095 (lado 3,3 V); PB13 apagado y encendido en los topes, observado a la vista |
| 9 | `sched rr 5`, `run bench` | Respuesta, espera estimada y retorno de A/B | OK. Igual a la tabla |
| 10 | `sched rr 20`, `run bench` | Valores medidos con mayor quantum | OK. Igual a la tabla |
| 11 | `sched prio 5`, `run bench` | Tarea de mayor prioridad obtiene CPU antes | OK. B (prioridad 4) antes que A |
| 12 | `sched fcfs`, `run bench` | FIFO no expropiativo, comparar esperas | OK. A antes que B, sin expropiación |
| 13 | `ps`, `kill 4`, `ps` | Tarea elegida termina (reiniciar para restaurarla) | OK. `blink` pasa a TERMIN con FIN=3; PC13 se detiene y las demás tareas siguen |

## Tabla para comparación de algoritmos

| Algoritmo | Quantum | Respuesta A (ms) | Respuesta B | Espera~ A (ms) | Espera~ B | Retorno A | Retorno B |
|---|---:|---:|---:|---:|---:|---:|---:|
| RR | 5 | 0 | 5 | 30 | 30 | 60 | 60 |
| RR | 20 | 0 | 20 | 20 | 30 | 50 | 60 |
| PRIO | 5 | 30 | 0 | 30 | 0 | 60 | 30 |
| FCFS | No aplica | 0 | 30 | 0 | 30 | 30 | 60 |

**Método (2026-11-25):** placa real, 3 repeticiones por modo con reinicio previo, resultados idénticos. En estas repeticiones los comandos se inyectaron por SWD en el buffer de recepción del shell y los valores se leyeron de `report_a` y `report_b` en RAM. La espera es retorno − CPU (30 ticks por tarea). Después se repitió una vez por puerto serie, sin reiniciar entre modos, con los mismos valores. Sesión completa: `EVIDENCIA_CONSOLA.txt`.

**Nota RR 5:** lo ideal sería retorno A = 55. El último tick de A coincide con el fin de su quantum: se le quita la CPU antes de anotar su final y lo anota 5 ms después, al recuperarla.

La métrica de espera es **aproximada**: `turnaround − ticks de CPU`, resolución de 1 ms; el benchmark corre en un entorno con tareas de sistema también activas, de modo que las cifras pueden variar entre repeticiones.

## Nota sobre interbloqueos

El timeout en `mutex_lock()` es una medida de recuperación local; **no constituye detección formal de interbloqueos ni algoritmo del banquero**. `kill` sobre una tarea que posee un mutex se difiere hasta que lo suelte. El valor agregado de MPU/protección no está implementado.
