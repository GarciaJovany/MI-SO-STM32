#include "kernel/os.h"
#include "kernel/hw.h"

/* Ciclo de vida y tiempo de las tareas. Sin ensamblador: se compila tambien
 * en PC para tests/test_sync.c. */

void os_unblock(os_tcb_t *p) {
    /* Llamador mantiene interrupciones deshabilitadas. */
    if (p && p->state == T_BLOQUEADO) {
        p->state = T_LISTO;
        p->wait_kind = OS_WAIT_NONE;
        p->wait_obj = NULL;
        sched_ready_push(p);
        pend_pendsv();
    }
}

/* Despierta a un esperador. woken_from recuerda de que lista salio hasta que
 * vuelva a comprobar: si lo matan antes, el aviso pasa al siguiente. */
void os_wake_one(os_tcb_t **list) {
    os_tcb_t *p = wait_pop(list);
    if (p) {p->woken_from = list;os_unblock(p);}
}

/* Fin comun (exit, kill, pila). Interrupciones deshabilitadas. */
void os_task_end(os_tcb_t *p, uint8_t reason) {
    if (p->state == T_LISTO) sched_ready_remove(p);
    if (p->state == T_BLOQUEADO && p->wait_obj)
        wait_remove((os_tcb_t **)p->wait_obj, p);
    p->state = T_TERMINADO;
    p->end_reason = reason;
    p->wait_kind = OS_WAIT_NONE;
    p->wait_obj = NULL;
    p->hold = p->kill_pending = 0;
    if (p->woken_from) {
        os_tcb_t **list = p->woken_from;
        p->woken_from = NULL;
        os_wake_one(list);
    }
    mutex_release_all(p);
    pend_pendsv();
}

/* 0 = terminada, 1 = diferida hasta soltar recursos, <0 = error. */
int os_kill(uint8_t id) {
    if (id == 0 || id >= OS_MAX_TASKS) return -1;
    uint32_t s = irq_save();
    os_tcb_t *p = &os_tasks[id];
    int result = 0;
    if (p->state == T_TERMINADO) result = -2;
    else if (p->hold) {p->kill_pending = 1;result = OS_KILL_DEFERRED;}
    else os_task_end(p, 3);
    irq_restore(s);
    return result;
}

/* La tarea actual acaba de soltar un recurso: aplicar kill diferido. */
void os_kill_point(void) {
    uint32_t s = irq_save();
    bool die = os_current->kill_pending && !os_current->hold;
    irq_restore(s);
    if (die) os_terminate_self(3);
}

void os_stack_fail(os_tcb_t *p) {
    if (p->id == 0) os_panic(OS_FAULT_STACK); /* idle es imprescindible */
    os_task_end(p, 2);
}

/* Cuerpo de SysTick; interrupciones deshabilitadas. */
void os_tick(void) {
    os_ticks++;
    if (os_current && os_current->state == T_EJECUTANDO) {
        os_current->ticks_cpu++;
        if (os_current->id && os_current->quantum_left)
            os_current->quantum_left--;
    }
    for (uint8_t i = 1; i < OS_MAX_TASKS; i++) {
        os_tcb_t *p = &os_tasks[i];
        if (p->state == T_BLOQUEADO &&
            (p->wait_kind == OS_WAIT_SLEEP ||
            (p->wait_kind == OS_WAIT_MUTEX && p->wake_tick != 0)) &&
            (int32_t)(os_ticks - p->wake_tick) >= 0) {
            if (p->wait_kind == OS_WAIT_MUTEX && p->wait_obj) {
                os_mutex_t *m = (os_mutex_t *)p->wait_obj;
                wait_remove(&m->waiting, p);
                p->wake_reason = OS_WAKE_TIMEOUT;
            }
            os_unblock(p);
        }
        /* Canario: la pila de usuario no debe invadir el inicio del bloque. */
        if (p->state != T_TERMINADO && p->stack_base[0] != OS_STACK_CANARY)
            os_stack_fail(p);
    }
    pend_pendsv();
}

uint32_t os_uptime_ms(void) {return os_ticks;}
uint8_t os_pid(void) {return os_current ? os_current->id : 0;}

void os_sleep(uint32_t ms) {
    if (ms == 0) {os_yield(); return;}
    uint32_t s = irq_save();
    os_current->wake_tick = os_ticks + ms;
    os_current->wait_kind = OS_WAIT_SLEEP;
    os_current->wait_obj = NULL;
    os_current->state = T_BLOQUEADO;
    irq_restore(s);
    pend_pendsv();
}

void os_yield(void) {
    uint32_t s = irq_save();
    if (os_current && os_current->state == T_EJECUTANDO && os_current->id) {
        os_current->state = T_LISTO;
        sched_ready_push(os_current);
    }
    irq_restore(s);
    pend_pendsv();
}
