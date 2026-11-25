#include "kernel/os.h"
#include "kernel/hw.h"

os_tcb_t os_tasks[OS_MAX_TASKS];
os_tcb_t *os_current;
volatile uint32_t os_ticks;
static uint32_t stacks[OS_MAX_TASKS][OS_STACK_WORDS] __attribute__((aligned(8)));

static void idle_task(void) {
    for (;;) __asm volatile("wfi" ::: "memory");
}

static void task_trampoline(void) {
    os_current->entry();
    os_exit();
}

static void build_task(uint8_t id, const char *name,
                       void (*entry)(void), uint8_t priority) {
    os_tcb_t *p = &os_tasks[id];
    uint32_t *sp = &stacks[id][OS_STACK_WORDS];
    for (uint32_t k = 1; k < OS_STACK_WORDS; k++) stacks[id][k] = OS_STACK_FILL;
    stacks[id][0] = OS_STACK_CANARY;
    /* Excepcion fabrica el frame de hardware en orden ascendente:
       r0,r1,r2,r3,r12,lr,pc,xpsr; luego contexto manual r4..r11. */
    *(--sp) = 0x01000000UL;  /* xPSR, bit T */
    *(--sp) = (uint32_t)(uintptr_t)task_trampoline; /* PC */
    *(--sp) = (uint32_t)(uintptr_t)os_exit;          /* LR */
    for (uint8_t k = 0; k < 5; k++) *(--sp) = 0;   /* r12,r3,r2,r1,r0 */
    for (uint8_t k = 0; k < 8; k++) *(--sp) = 0;   /* r11..r4 */
    p->sp = sp;
    p->stack_base = &stacks[id][0];
    p->name = name;
    p->entry = entry;
    p->priority = priority;
    p->id = id;
    p->state = T_NUEVO;
    p->quantum_left = os_quantum;
    p->next = p->wait_next = NULL;
    p->wait_obj = NULL;
    p->wait_kind = OS_WAIT_NONE;
    p->wake_reason = OS_WAKE_NONE;
    p->end_reason = 0;
    p->woken_from = NULL;
    p->held = NULL;
    p->hold = p->kill_pending = 0;
    p->ticks_cpu = 0;
    p->wake_tick = 0;
}

void os_init(void) {
    os_ticks = 0;
    os_current = NULL;
    os_mode = OS_MODE_RR;
    os_quantum = 5;
    for (uint8_t i = 0; i < OS_MAX_TASKS; i++) {
        os_tasks[i].id = i;
        os_tasks[i].state = T_TERMINADO;
        os_tasks[i].end_reason = 0;
        os_tasks[i].next = os_tasks[i].wait_next = NULL;
    }
    build_task(0, "idle", idle_task, 255);
    os_tasks[0].state = T_BLOQUEADO; /* idle nunca figura en ready queue */
    mem_init();
}

int os_create_task(const char *name, void (*entry)(void), uint8_t priority) {
    if (!name || !entry || priority == 255) return -2;
    uint32_t s = irq_save();
    for (uint8_t i = 1; i < OS_MAX_TASKS; i++) {
        if (os_tasks[i].state == T_TERMINADO) {
            build_task(i, name, entry, priority);
            os_tasks[i].state = T_LISTO;
            sched_ready_push(&os_tasks[i]);
            irq_restore(s);
            if (os_current) pend_pendsv();
            return i;
        }
    }
    irq_restore(s);
    return -1;
}

void os_start(void) {
    uint32_t s = irq_save();
    os_current = sched_ready_pop();
    if (!os_current) os_current = &os_tasks[0];
    os_current->state = T_EJECUTANDO;
    os_current->quantum_left = os_quantum;

    /* 8 MHz HSI de reset. No depende del cristal externo de la placa. */
    SCB_SHPR3 = (SCB_SHPR3 & 0x0000FFFFUL) | (0x80UL << 24) | (0xFFUL << 16);
    SYST_RVR = 8000UL - 1UL;
    SYST_CVR = 0;
    SYST_CSR = 0; /* se activa al terminar SVC, no antes del primer PSP */
    irq_restore(s);
    /* SVC restaura el primer contexto e inicia la ejecucion sobre PSP. */
    __asm volatile("svc 0" ::: "memory");
    for (;;) {}
}

uint32_t os_stack_used(uint8_t id) {
    if (id >= OS_MAX_TASKS || !os_tasks[id].stack_base) return 0;
    uint32_t k = 1;
    while (k < OS_STACK_WORDS && stacks[id][k] == OS_STACK_FILL) k++;
    return (OS_STACK_WORDS - k) * 4U;
}

void SysTick_Handler(void) {
    uint32_t s = irq_save();
    os_tick();
    irq_restore(s);
}

void os_terminate_self(uint8_t reason) {
    uint32_t s = irq_save();
    os_task_end(os_current, reason);
    irq_restore(s);
    pend_pendsv();
    for (;;) __asm volatile("nop");
}

void os_exit(void) {os_terminate_self(1);}

void os_schedule_from_pendsv(void) {
    /* Tarea saliente: context.S ya guardo su SP. */
    os_tcb_t *p = os_current;
    if (p->sp < p->stack_base) os_panic(OS_FAULT_STACK); /* piso memoria ajena */
    if (p->state != T_TERMINADO && p->stack_base[0] != OS_STACK_CANARY)
        os_stack_fail(p);
    sched_choose();
}
