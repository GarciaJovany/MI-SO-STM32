# Conexiones fáciles de la Blue Pill STM32F103C8T6

Se usan una placa **Blue Pill** y un programador **ST-Link V2**. Al realizar el cableado, **verifica los nombres impresos de los pines de ambos dispositivos**, porque los colores de los cables no son una identificación eléctrica.

## A. Primero: la placa y tu ST-Link (sin protoboard)

1. Desconecta todos los dispositivos del USB.
2. Verifica que **BOOT0 esté en 0** (arranque desde Flash).
3. Conecta **SWDIO ↔ PA13**, **SWCLK ↔ PA14** y **GND ↔ GND**.
4. Alimenta la placa con **3.3 V regulados** siguiendo las etiquetas verificadas de tu ST-Link o una fuente 3.3 V confiable. No conectes **5V** al pin **3.3V**.
5. Si el ST-Link dispone de **NRST**, puedes conectarlo a **R** / **NRST** de la Blue Pill (opcional, útil al depurar).
6. Comprueba que al cargar el firmware el LED de la placa en **PC13** parpadea cada 300 ms. Este es el primer ensayo.

> Si tu ST-Link solo ofrece la referencia de voltaje y no alimentación confiable, utiliza una fuente externa regulada de 3.3 V. No alimentes simultáneamente desde varios conectores.

## B. Después: botón, dos LEDs externos y potenciómetro

| Componente | Conexión | Qué debería ocurrir |
|---|---|---|
| LED interno | PC13 (ya soldado) | Parpadeo de la tarea `blink` |
| LED 1 + resistencia | PB12 → 330 Ω → pata larga LED → pata corta GND | Destello de 180 ms tras pulsar botón |
| LED 2 + resistencia | PB13 → 330 Ω → pata larga LED → pata corta GND | Enciende con ADC ≥ 2150 y apaga con ADC ≤ 1950; entre ambos conserva su estado |
| Pulsador | PA0 → botón → GND | Evento enviado por cola a `action` |
| Potenciómetro 10kΩ | Centro PA5; extremos 3.3V y GND | `adc` varía de aproximadamente 0 a 4095 |

La resistencia de cada LED se puede colocar también en el lado del cátodo; lo importante es que **esté en serie**. Si el pulsador es de cuatro patas, elige **un terminal de cada lado eléctrico** del pulsador.

## C. Consola por serial: USB–TTL aparte del ST-Link

| Blue Pill | Adaptador USB–TTL 3.3V |
|---|---|
| PA9 — USART1_TX | RXD |
| PA10 — USART1_RX | TXD |
| GND | GND |

Deja sin conectar el pin de alimentación **VCC/5V** del USB–TTL si la placa ya está alimentada por la fuente del apartado A. Configura los niveles de señal del adaptador a **3.3 V**. En Windows 10 u 11 abre PuTTY, Tera Term o `python -m serial.tools.miniterm COMx 115200` y elige `115200`, `8N1`, sin control de flujo. Pulsa RESET para ver el mensaje inicial.

### Alternativa usada en las pruebas: ESP32 como USB–TTL

Con **EN a GND**, el microcontrolador del ESP32 queda en reset y solo se usa su chip USB–serie. Sus pines TX/RX están rotulados desde el lado del ESP32; por eso aquí TX va con TX y RX con RX:

| Placa ESP32 | Conexión |
|---|---|
| EN | GND del mismo ESP32 |
| TX0 | PA9 |
| RX0 | PA10 |
| GND | GND de la Blue Pill |

No conectes 3V3 ni 5V entre las placas. Terminal: `python -m serial.tools.miniterm COM6 115200` (cambia COM6 por tu puerto).

## D. Orden de prueba recomendado

1. **Solo ST-Link + placa:** cargar, ver LED PC13. Si no parpadea, revisar Boot0, SWD, alimentación y carga.
2. **Añadir USB–TTL o ESP32:** ver banner y probar `help`, `ps`, `uptime`.
3. **Añadir pulsador + LED PB12:** pulsar PA0; observar PB12 por 180 ms.
4. **Añadir potenciómetro + LED PB13:** ejecutar `adc` y girar, hasta pasar de 2150 (PB13 enciende) y bajar de 1950 (PB13 apaga).
5. **Sistema operativo:** ejecutar `run pc` dos veces y observar contadores; `mem`, `memtest`, `sched rr 5`, `run bench`; repetir con `prio` y `fcfs`.

**No usar adaptador RS-232 de ±12 V**, ni alimentar sensores o pines con 5 V. Los GPIO del STM32 se usan aquí a **3.3 V**.
