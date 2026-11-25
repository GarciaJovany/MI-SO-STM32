#include "kernel/os.h"
#include <assert.h>
#include <stdio.h>

os_tcb_t os_tasks[OS_MAX_TASKS];
os_tcb_t *os_current;
volatile uint32_t os_ticks;
uint32_t host_primask, host_pending;
void host_irq_open(void) {}

static void reset(void) {
    while (sched_ready_pop()) {}
    for (unsigned i = 0; i < OS_MAX_TASKS; i++) {
        os_tasks[i].id = (uint8_t)i;
        os_tasks[i].state = T_TERMINADO;
        os_tasks[i].next = NULL;
        os_tasks[i].priority = 255;
        os_tasks[i].quantum_left = 5;
    }
    os_current = &os_tasks[0];
    os_current->state = T_EJECUTANDO;
    os_mode = OS_MODE_RR;
    os_quantum = 5;
}
static void ready(unsigned id,unsigned priority) {
    os_tcb_t *p = &os_tasks[id];
    p->priority = (uint8_t)priority;
    p->state = T_LISTO;
    sched_ready_push(p);
}

int main(void) {
    reset();ready(1,5);ready(2,4);
    sched_choose();assert(os_current->id==1);
    os_current->quantum_left=3;
    sched_choose();assert(os_current->id==1);
    os_current->quantum_left=0;
    sched_choose();assert(os_current->id==2);

    reset();ready(3,5);ready(4,4);
    os_mode=OS_MODE_PRIO;
    sched_choose();assert(os_current->id==4);
    ready(5,3);
    sched_choose();assert(os_current->id==5);

    reset();ready(6,5);ready(7,1);
    os_mode=OS_MODE_FCFS;
    sched_choose();assert(os_current->id==6);
    for (int i=0;i<50;i++)sched_choose();
    assert(os_current->id==6);
    os_current->state=T_BLOQUEADO;
    sched_choose();assert(os_current->id==7);

    mem_init();
    mem_stats_t a,b;
    mem_stats(&a);assert(a.total==4096 && a.used==0);
    void *x=mem_alloc(30),*y=mem_alloc(77),*z=mem_alloc(49);
    assert(x && y && z);
    mem_free(y);mem_free(x);mem_free(z);
    mem_free(x); /* doble liberacion ignorada */
    mem_stats(&b);assert(b.used==0 && b.blocks==1);
    mem_set_fit(MEM_FIT_BEST);assert(mem_fit_mode()==MEM_FIT_BEST);
    mem_set_fit(MEM_FIT_FIRST);assert(mem_fit_mode()==MEM_FIT_FIRST);
    puts("OK: RR, prioridad, FCFS, heap y coalescing (pruebas host)");
    return 0;
}
