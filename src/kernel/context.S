/* Cortex-M3: excepcion apila r0-r3,r12,lr,pc,xpsr de forma automatica.
 * PendSV guarda r4-r11 en PSP; la llamada en C se ejecuta con MSP. */
.syntax unified
.cpu cortex-m3
.thumb

.section .text.SVC_Handler,"ax",%progbits
.global SVC_Handler
.thumb_func
SVC_Handler:
    ldr r3, =os_current
    ldr r2, [r3]
    ldr r0, [r2]             /* SP, primer campo del TCB */
    ldmia r0!, {r4-r11}
    msr psp, r0
    movs r0, #2
    msr CONTROL, r0          /* Thread privilegiado usando PSP */
    isb
    ldr r3, =0xE000E010
    movs r2, #7
    str r2, [r3]            /* arrancar SysTick solo con PSP preparado */
    cpsie i
    ldr r0, =0xFFFFFFFD     /* retorno Thread usando PSP */
    bx r0

.section .text.PendSV_Handler,"ax",%progbits
.global PendSV_Handler
.thumb_func
PendSV_Handler:
    cpsid i
    mrs r0, psp
    ldr r3, =os_current
    ldr r2, [r3]
    stmdb r0!, {r4-r11}
    str r0, [r2]             /* guarda SP de la tarea desalojada */
    push {r3, lr}            /* 8 bytes: alineacion ABI preservada */
    bl os_schedule_from_pendsv
    pop {r3, lr}
    ldr r3, =os_current
    ldr r2, [r3]
    ldr r0, [r2]
    ldmia r0!, {r4-r11}
    msr psp, r0
    cpsie i
    bx lr                   /* EXC_RETURN recibido por el handler */
