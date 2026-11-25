#include "kernel/os.h"
#include "kernel/hw.h"

/* Registro del ultimo fallo fatal. Sin UART se lee por SWD:
 * arm-none-eabi-nm build/mi_so_stm32.elf | grep os_fault */
volatile os_fault_t os_fault;

static void halt_blinking(void) __attribute__((noreturn));
static void halt_blinking(void) {
    __asm volatile("cpsid i" ::: "memory");
    for (bool on = true;; on = !on) {
        led_set(0, on);
        for (volatile uint32_t i = 0; i < 60000U; i++) {} /* parpadeo rapido */
    }
}

void os_panic(uint32_t kind) {
    __asm volatile("cpsid i" ::: "memory");
    os_fault.kind = kind;
    os_fault.task_id = os_pid();
    os_fault.sp = os_current ? (uint32_t)(uintptr_t)os_current->sp : 0;
    os_fault.magic = OS_FAULT_MAGIC;
    halt_blinking();
}

__attribute__((noreturn, used)) void hardfault_report(uint32_t sp) {
    os_fault.kind = OS_FAULT_HARD;
    os_fault.task_id = os_pid();
    os_fault.sp = sp;
    os_fault.cfsr = SCB_CFSR;
    os_fault.hfsr = SCB_HFSR;
    os_fault.bfar = SCB_BFAR;
    /* Leer el frame solo si esta en RAM: otro fallo aqui seria lockup. */
    if (!(sp & 3U) && sp >= 0x20000000UL && sp <= 0x20005000UL - 32U) {
        const uint32_t *frame = (const uint32_t *)(uintptr_t)sp;
        os_fault.lr = frame[5];
        os_fault.pc = frame[6];
        os_fault.psr = frame[7];
    }
    os_fault.magic = OS_FAULT_MAGIC;
    halt_blinking();
}

__attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile(
        "cpsid i\n"
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b hardfault_report\n");
}
