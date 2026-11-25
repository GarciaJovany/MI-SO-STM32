#include "kernel/os.h"
#include "kernel/hw.h"

void sem_init(os_sem_t *s, int16_t initial) {
    s->waiting = NULL;
    s->count = initial;
}
static void acquire(os_sem_t *s, uint8_t hold) {
    for (;;) {
        uint32_t old = irq_save();
        os_current->woken_from = NULL;
        if (s->count > 0) {
            s->count--;
            os_current->hold = (uint8_t)(os_current->hold + hold);
            irq_restore(old);
            return;
        }
        os_current->wait_obj = s;   /* waiting es el primer miembro */
        os_current->wait_kind = OS_WAIT_SEM;
        os_current->state = T_BLOQUEADO;
        wait_push(&s->waiting, os_current);
        irq_restore(old);
        pend_pendsv();
        /* Despertar no garantiza reserva; revisar el contador. */
    }
}
void sem_wait(os_sem_t *s) {acquire(s, 0);}
void sem_wait_hold(os_sem_t *s) {acquire(s, 1);}
void sem_signal(os_sem_t *s) {
    uint32_t old = irq_save();
    if (s->count < 32767) s->count++;
    os_wake_one(&s->waiting);
    irq_restore(old);
}
void sem_signal_release(os_sem_t *s) {
    uint32_t old = irq_save();
    sem_signal(s);
    if (os_current->hold) os_current->hold--;
    irq_restore(old);
    os_kill_point();
}
