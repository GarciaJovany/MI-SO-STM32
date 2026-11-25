#include "kernel/os.h"

static os_tcb_t *ready_head, *ready_tail;
void sched_ready_push(os_tcb_t *p) {
    p->next = NULL;
    if (ready_tail) ready_tail->next = p;
    else ready_head = p;
    ready_tail = p;
}
void sched_ready_remove(os_tcb_t *p) {
    os_tcb_t **it = &ready_head, *last = NULL;
    while (*it) {
        if (*it == p) {
            *it = p->next;
            if (ready_tail == p) ready_tail = last;
            p->next = NULL;
            return;
        }
        last = *it;
        it = &(*it)->next;
    }
}
os_tcb_t *sched_ready_peek(void) {
    os_tcb_t *best = ready_head;
    if (os_mode == OS_MODE_PRIO)
        for (os_tcb_t *p = ready_head; p; p = p->next)
            if (p->priority < best->priority) best = p;
    return best;
}
os_tcb_t *sched_ready_pop(void) {
    os_tcb_t *p = sched_ready_peek();
    if (p) sched_ready_remove(p);
    return p;
}
void wait_push(os_tcb_t **head, os_tcb_t *p) {
    p->wait_next = NULL;
    if (!*head) {*head = p; return;}
    os_tcb_t *q = *head;
    while (q->wait_next) q = q->wait_next;
    q->wait_next = p;
}
os_tcb_t *wait_pop(os_tcb_t **head) {
    os_tcb_t *p = *head;
    if (p) {*head = p->wait_next;p->wait_next = NULL;}
    return p;
}
void wait_remove(os_tcb_t **head, os_tcb_t *p) {
    while (*head) {
        if (*head == p) {*head = p->wait_next;p->wait_next = NULL;return;}
        head = &(*head)->wait_next;
    }
}
