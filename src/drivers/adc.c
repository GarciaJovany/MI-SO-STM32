#include "kernel/os.h"
#include "kernel/hw.h"

void adc_init(void) {
    RCC_APB2ENR |= (1UL<<9);
    /* PA5 ya está configurado como entrada analogica por gpio_init. */
    ADC1_SMPR2 = (ADC1_SMPR2 & ~(7UL<<15)) | (7UL<<15);
    ADC1_SQR1 = 0;
    ADC1_SQR3 = 5; /* ADC canal 5 = PA5 */
    ADC1_CR2 = 1;
    for (volatile uint32_t i = 0; i < 3000; i++) {}
    ADC1_CR2 |= 1UL<<3; /* reset de calibracion */
    while (ADC1_CR2 & (1UL<<3)) {}
    ADC1_CR2 |= 1UL<<2; /* autocalibracion */
    while (ADC1_CR2 & (1UL<<2)) {}
    ADC1_CR2 |= (7UL<<17) | (1UL<<20); /* start de software */
}
uint16_t adc_read(void) {
    /* El ADC es un unico periferico: no permitir accesos simultaneos
       entre shell y productor. Conversion ~60 us a 4 MHz ADC. */
    uint32_t old = irq_save();
    ADC1_SR = 0;
    ADC1_CR2 |= 1UL<<22;
    while (!(ADC1_SR & 2UL)) {}
    uint16_t value = (uint16_t)(ADC1_DR & 0x0FFFUL);
    irq_restore(old);
    return value;
}
