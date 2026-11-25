#include "kernel/os.h"
#include "kernel/hw.h"

/* PC13 LED integrado (activo a nivel BAJO).
 * PB12 LED boton y PB13 LED productor-consumidor (salidas activas ALTO).
 * PA0 pulsador a GND, pull-up interno. PA5 ADC analogico. */
void gpio_init(void) {
    RCC_APB2ENR |= (1U<<2) | (1U<<3) | (1U<<4);
    GPIOC_CRH = (GPIOC_CRH & ~(0xFUL<<20)) | (2UL<<20);
    GPIOB_CRH = (GPIOB_CRH & ~(0xFFUL<<16)) | (0x22UL<<16);
    GPIOA_CRL = (GPIOA_CRL & ~((0xFUL<<20) | 0xFUL)) | 0x08UL;
    GPIOA_BSRR = 1U<<0; /* pull-up PA0 */
    led_set(0,false);
    led_set(1,false);
    led_set(2,false);
}
void led_set(uint8_t which, bool on) {
    if (which == 0) GPIOC_BSRR = on ? (1UL<<(13+16)) : (1UL<<13);
    if (which == 1) GPIOB_BSRR = on ? (1UL<<12) : (1UL<<(12+16));
    if (which == 2) GPIOB_BSRR = on ? (1UL<<13) : (1UL<<(13+16));
}
int button_read(void) {return (GPIOA_IDR & 1U) ? 1 : 0;}
