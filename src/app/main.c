#include "kernel/os.h"

int main(void) {
    os_init();
    gpio_init();
    uart_init();
    adc_init();
    uart_write("\r\nMI-SO STM32F103C8T6 v2.0 (Cortex-M3)\r\n");
    uart_write("Bare-metal | PendSV + SysTick | 1 ms | HSI 8 MHz\r\n");
    app_boot();
    os_start();
}
