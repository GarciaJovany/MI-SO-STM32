#include "kernel/os.h"
#include "kernel/hw.h"

void mq_init(os_mq_t *q, uint8_t capacity) {
    if (!capacity || capacity > MQ_CAPACITY) capacity = MQ_CAPACITY;
    q->rx_waiting = q->tx_waiting = NULL;
    q->head = q->tail = q->count = 0;
    q->capacity = capacity;
}
int mq_send(os_mq_t *q, uint8_t value) {
    for (;;) {
        uint32_t s = irq_save();
        os_current->woken_from = NULL;
        if (q->count < q->capacity) {
            q->data[q->tail] = value;
            q->tail = (uint8_t)((q->tail + 1U) % q->capacity);
            q->count++;
            os_wake_one(&q->rx_waiting);
            irq_restore(s);
            return 0;
        }
        os_current->wait_kind = OS_WAIT_MQTX;
        os_current->wait_obj = &q->tx_waiting;
        os_current->state = T_BLOQUEADO;
        wait_push(&q->tx_waiting, os_current);
        irq_restore(s);
        pend_pendsv();
    }
}
int mq_recv(os_mq_t *q, uint8_t *value) {
    if (!value) return -1;
    for (;;) {
        uint32_t s = irq_save();
        os_current->woken_from = NULL;
        if (q->count) {
            *value = q->data[q->head];
            q->head = (uint8_t)((q->head + 1U) % q->capacity);
            q->count--;
            os_wake_one(&q->tx_waiting);
            irq_restore(s);
            return 0;
        }
        os_current->wait_kind = OS_WAIT_MQRX;
        os_current->wait_obj = &q->rx_waiting;
        os_current->state = T_BLOQUEADO;
        wait_push(&q->rx_waiting, os_current);
        irq_restore(s);
        pend_pendsv();
    }
}
