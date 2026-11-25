#include "kernel/os.h"
#include "kernel/hw.h"

void mutex_init(os_mutex_t *m) {m->waiting = NULL;m->owner = NULL;m->held_next = NULL;}

/* Funciones internas: interrupciones deshabilitadas. */
static void take(os_mutex_t *m, os_tcb_t *p) {
    m->owner = p;
    m->held_next = p->held;
    p->held = m;
    p->hold++;              /* con un mutex tomado, kill se difiere */
}
static void hand_over(os_mutex_t *m) {
    os_tcb_t *p = wait_pop(&m->waiting);
    m->owner = NULL;
    m->held_next = NULL;
    if (p) {take(m, p);p->wake_reason = OS_WAKE_GRANTED;os_unblock(p);}
}
/* El dueno termina (exit o pila danada): nadie debe quedar esperando. Los
 * datos protegidos podrian haber quedado a medias. */
void mutex_release_all(os_tcb_t *p) {
    while (p->held) {
        os_mutex_t *m = p->held;
        p->held = m->held_next;
        hand_over(m);
    }
}

int mutex_lock(os_mutex_t *m, uint32_t timeout_ms) {
    uint32_t s = irq_save();
    os_tcb_t *me = os_current;
    if (!m->owner) {take(m, me);irq_restore(s);return MUTEX_OK;}
    if (m->owner == me) {irq_restore(s);return MUTEX_ERROR;}
    me->wait_kind = OS_WAIT_MUTEX;
    me->wait_obj = m;
    me->wake_reason = OS_WAKE_NONE;
    me->wake_tick = timeout_ms ? os_ticks + timeout_ms : 0;
    me->state = T_BLOQUEADO;
    wait_push(&m->waiting, me);
    irq_restore(s);
    pend_pendsv();
    s = irq_save();
    int result = me->wake_reason == OS_WAKE_GRANTED && m->owner == me ?
                 MUTEX_OK : MUTEX_TIMEOUT;
    me->wake_reason = OS_WAKE_NONE;
    irq_restore(s);
    return result;
}

int mutex_unlock(os_mutex_t *m) {
    uint32_t s = irq_save();
    os_tcb_t *me = os_current;
    if (m->owner != me) {irq_restore(s);return MUTEX_ERROR;}
    os_mutex_t **it = &me->held;
    while (*it && *it != m) it = &(*it)->held_next;
    if (*it) *it = m->held_next;
    if (me->hold) me->hold--;
    hand_over(m);
    irq_restore(s);
    os_kill_point();
    return MUTEX_OK;
}
