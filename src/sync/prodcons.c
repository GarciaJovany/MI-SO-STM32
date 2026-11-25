#include "kernel/os.h"
#include "kernel/hw.h"

#define PC_CAP 4
static os_sem_t empty, full;
static os_mutex_t lock;
static uint16_t samples[PC_CAP];
static uint8_t rd, wr;
static volatile uint32_t produced, consumed;
static volatile uint8_t level;

static void producer(void) {
    for (;;) {
        uint16_t sample = adc_read();
        sem_wait_hold(&empty);
        mutex_lock(&lock,0);
        samples[wr] = sample;
        wr = (uint8_t)((wr+1U)%PC_CAP);
        produced++;
        level++;
        mutex_unlock(&lock);
        sem_signal_release(&full);
        os_sleep(120);
    }
}
static void consumer(void) {
    bool led = false;
    for (;;) {
        sem_wait_hold(&full);
        mutex_lock(&lock,0);
        uint16_t sample = samples[rd];
        rd = (uint8_t)((rd+1U)%PC_CAP);
        consumed++;
        level--;
        mutex_unlock(&lock);
        sem_signal_release(&empty);
        /* LED PB13 con histeresis: enciende >=2150, apaga <=1950. */
        if (sample >= 2150U) led = true;
        else if (sample <= 1950U) led = false;
        led_set(2,led);
        os_sleep(200);
    }
}
void prodcons_boot(void) {
    sem_init(&empty,PC_CAP);
    sem_init(&full,0);
    mutex_init(&lock);
    rd = wr = level = 0;
    produced = consumed = 0;
    os_create_task("prod",producer,3);
    os_create_task("cons",consumer,3);
}
void prodcons_stats(void) {
    uint32_t s = irq_save();
    uint32_t a = produced,b = consumed;
    uint8_t n = level;
    int16_t e = empty.count, f = full.count;
    irq_restore(s);
    uart_write("Productor-consumidor: producidos=");uart_num(a);
    uart_write(" consumidos=");uart_num(b);
    uart_write(" nivel=");uart_num(n);
    uart_write("/4 sem_empty=");uart_num((uint32_t)e);
    uart_write(" sem_full=");uart_num((uint32_t)f);
    uart_write("\r\n");
}
