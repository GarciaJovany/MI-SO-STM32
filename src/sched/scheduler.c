#include "kernel/os.h"
#include "kernel/hw.h"

volatile uint8_t os_mode = OS_MODE_RR;
volatile uint8_t os_quantum = 5;

void sched_set_mode(uint8_t mode, uint8_t quantum) {
    uint32_t s = irq_save();
    if (mode <= OS_MODE_FCFS) os_mode = mode;
    if (quantum) os_quantum = quantum;
    if (os_current) os_current->quantum_left = os_quantum;
    irq_restore(s);
    pend_pendsv();
}

void sched_choose(void) {
    os_tcb_t *cur = os_current;
    os_tcb_t *best = sched_ready_peek();
    if (cur && cur->state == T_EJECUTANDO && cur->id) {
        if (os_mode == OS_MODE_FCFS) return;
        if (os_mode == OS_MODE_RR && cur->quantum_left > 0) return;
        if (os_mode == OS_MODE_PRIO) {
            if (!best || best->priority > cur->priority) {
                cur->quantum_left = os_quantum;
                return;
            }
            if (best->priority == cur->priority && cur->quantum_left > 0)
                return;
        }
        if (!best) {cur->quantum_left = os_quantum;return;}
        cur->state = T_LISTO;
        sched_ready_push(cur);
    } else if (cur && cur->id == 0) {
        cur->state = T_BLOQUEADO;
    }

    os_tcb_t *next = sched_ready_pop();
    if (!next) next = &os_tasks[0];
    next->state = T_EJECUTANDO;
    next->quantum_left = os_quantum;
    os_current = next;
}
