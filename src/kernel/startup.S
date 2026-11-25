.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector,"a",%progbits
.align 2
.word _estack
.word Reset_Handler
.word Default_Handler /* NMI */
.word HardFault_Handler
.word Default_Handler /* MemManage */
.word Default_Handler /* BusFault */
.word Default_Handler /* UsageFault */
.word 0
.word 0
.word 0
.word 0
.word SVC_Handler
.word Default_Handler /* DebugMon */
.word 0
.word PendSV_Handler
.word SysTick_Handler
/* IRQ 0..36 (USART1 es IRQ 37) */
.rept 37
.word Default_Handler
.endr
.word USART1_IRQHandler
/* IRQ 38..59, reserva para ampliar drivers */
.rept 22
.word Default_Handler
.endr

.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.thumb_func
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bhs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:
    cmp r1, r2
    bhs 4f
    str r3, [r1], #4
    b 3b
4:
    bl main
5:
    b 5b

.section .text.Default_Handler,"ax",%progbits
.global Default_Handler
.thumb_func
Default_Handler:
    b .
.weak HardFault_Handler
.thumb_set HardFault_Handler, Default_Handler
