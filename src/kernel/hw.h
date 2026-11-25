#ifndef MISO_HW_H
#define MISO_HW_H
#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define RCC_APB2ENR REG32(0x40021018UL)
#define GPIOA_CRL REG32(0x40010800UL)
#define GPIOA_CRH REG32(0x40010804UL)
#define GPIOA_IDR REG32(0x40010808UL)
#define GPIOA_ODR REG32(0x4001080CUL)
#define GPIOA_BSRR REG32(0x40010810UL)
#define GPIOB_CRH REG32(0x40010C04UL)
#define GPIOB_BSRR REG32(0x40010C10UL)
#define GPIOC_CRH REG32(0x40011004UL)
#define GPIOC_BSRR REG32(0x40011010UL)
#define USART1_SR REG32(0x40013800UL)
#define USART1_DR REG32(0x40013804UL)
#define USART1_BRR REG32(0x40013808UL)
#define USART1_CR1 REG32(0x4001380CUL)
#define ADC1_SR REG32(0x40012400UL)
#define ADC1_CR2 REG32(0x40012408UL)
#define ADC1_SMPR2 REG32(0x40012410UL)
#define ADC1_SQR1 REG32(0x4001242CUL)
#define ADC1_SQR3 REG32(0x40012434UL)
#define ADC1_DR REG32(0x4001244CUL)
#define SCB_ICSR REG32(0xE000ED04UL)
#define SCB_SHPR3 REG32(0xE000ED20UL)
#define SCB_CFSR REG32(0xE000ED28UL)
#define SCB_HFSR REG32(0xE000ED2CUL)
#define SCB_BFAR REG32(0xE000ED38UL)
#define SYST_CSR REG32(0xE000E010UL)
#define SYST_RVR REG32(0xE000E014UL)
#define SYST_CVR REG32(0xE000E018UL)
#define NVIC_ISER1 REG32(0xE000E104UL)

#ifdef OS_HOST_TEST
/* PC: PRIMASK y PendSV simulados; host_irq_open() lo define cada prueba. */
extern uint32_t host_primask, host_pending;
void host_irq_open(void);
static inline uint32_t irq_save(void) {
    uint32_t old = host_primask;
    host_primask = 1;
    return old;
}
static inline void irq_restore(uint32_t old) {
    host_primask = old;
    if (!old) host_irq_open();
}
static inline void pend_pendsv(void) {
    host_pending = 1;
    if (!host_primask) host_irq_open();
}
#else
static inline uint32_t irq_save(void) {
    uint32_t old;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(old) :: "memory");
    return old;
}
static inline void irq_restore(uint32_t old) {
    __asm volatile("msr primask, %0" :: "r"(old) : "memory");
}
static inline void pend_pendsv(void) {
    SCB_ICSR = (1UL << 28);
    __asm volatile("dsb\n isb" ::: "memory");
}
#endif
#endif
