#include "kernel/os.h"
#include "kernel/hw.h"

/* UART1 @ PA9 TX / PA10 RX. HSI 8 MHz, 115200-8-N-1, adaptador 3.3V. */
#define RX_LEN 128U
static volatile uint8_t rxbuf[RX_LEN], rxhead, rxtail;

void uart_init(void) {
    RCC_APB2ENR |= (1UL<<0) | (1UL<<2) | (1UL<<14);
    /* PA9 = salida alterna push-pull 50 MHz (0xB), PA10 = entrada con pull-up (0x8) */
    GPIOA_BSRR = 1UL<<10;
    GPIOA_CRH = (GPIOA_CRH & ~(0xFFUL<<4)) | (0x8BUL<<4);
    USART1_CR1 = 0;
    USART1_BRR = 0x45; /* 8 MHz / 115200 = 69.44; USARTDIV * 16 = 69 */
    USART1_CR1 = (1UL<<13) | (1UL<<5) | (1UL<<3) | (1UL<<2);
    NVIC_ISER1 = 1U<<(37-32); /* USART1_IRQn=37 */
    rxhead = rxtail = 0;
}
void USART1_IRQHandler(void) {
    uint32_t sr = USART1_SR;
    if (sr & (1UL<<5)) {
        uint8_t ch = (uint8_t)USART1_DR;
        uint8_t next = (uint8_t)((rxhead + 1U) % RX_LEN);
        if (next != rxtail) {rxbuf[rxhead] = ch;rxhead = next;}
    } else if (sr & (1UL<<3)) {
        (void)USART1_DR; /* limpia overrun al leer SR+DR */
    }
}
void uart_putc(char c) {
    while (!(USART1_SR & (1UL<<7))) {}
    USART1_DR = (uint8_t)c;
}
void uart_write(const char *s) {while (*s) uart_putc(*s++);}
void uart_num(uint32_t n) {
    char b[11];uint8_t k = 0;
    do {b[k++] = (char)('0' + n % 10U);n /= 10U;} while (n);
    while (k) uart_putc(b[--k]);
}
bool uart_line_read(char *out, uint8_t max) {
    static char line[64];
    static uint8_t len;
    for (;;) {
        uint32_t s = irq_save();
        if (rxhead == rxtail) {irq_restore(s);return false;}
        uint8_t c = rxbuf[rxtail];
        rxtail = (uint8_t)((rxtail + 1U) % RX_LEN);
        irq_restore(s);
        if (c == '\r' || c == '\n') {
            if (!len) continue; /* CR-LF consecutivos */
            uint8_t i = 0;
            while (i + 1U < max && i < len) {out[i] = line[i];i++;}
            out[i] = 0;
            len = 0;
            uart_write("\r\n");
            return true;
        }
        if (c == 8 || c == 127) {
            if (len) {len--;uart_write("\b \b");}
        } else if (c >= 32 && c <= 126 && len + 1U < sizeof(line)) {
            line[len++] = (char)c;
            uart_putc((char)c);
        }
    }
}
