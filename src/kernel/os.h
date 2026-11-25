#ifndef MISO_OS_H
#define MISO_OS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define OS_MAX_TASKS 10
#define OS_STACK_WORDS 160    /* 640 bytes por tarea, incluyendo idle */
#define OS_STACK_CANARY 0xA55A33CCUL
#define OS_STACK_FILL 0xDEADBEEFUL /* para medir el uso maximo de pila */
#define OS_TICK_HZ 1000UL
#define OS_HEAP_SIZE 4096

typedef enum {
    T_NUEVO=0, T_LISTO, T_EJECUTANDO, T_BLOQUEADO, T_TERMINADO
} os_state_t;
typedef enum {OS_WAIT_NONE=0,OS_WAIT_SLEEP,OS_WAIT_SEM,OS_WAIT_MUTEX,
              OS_WAIT_MQRX,OS_WAIT_MQTX} os_wait_t;
typedef enum {OS_WAKE_NONE=0,OS_WAKE_GRANTED,OS_WAKE_TIMEOUT} os_wake_t;
typedef enum {OS_MODE_RR=0,OS_MODE_PRIO,OS_MODE_FCFS} os_mode_t;

struct os_mutex;
typedef struct os_tcb {
    uint32_t *sp;               /* DEBE SER EL PRIMER CAMPO: context.S */
    uint32_t *stack_base;
    uint32_t ticks_cpu;
    uint32_t wake_tick;
    void (*entry)(void);
    struct os_tcb *next;
    struct os_tcb *wait_next;
    void *wait_obj;
    struct os_tcb **woken_from;  /* lista que lo desperto, aun sin consumir */
    struct os_mutex *held;       /* mutex tomados */
    const char *name;
    uint8_t id, priority, state, quantum_left;
    uint8_t wait_kind, wake_reason, end_reason;
    uint8_t hold, kill_pending;  /* hold>0: kill se difiere */
} os_tcb_t;

extern os_tcb_t os_tasks[OS_MAX_TASKS];
extern os_tcb_t *os_current;
extern volatile uint32_t os_ticks;
extern volatile uint8_t os_mode, os_quantum;

#define OS_FAULT_MAGIC 0xFA017EEDUL
#define OS_FAULT_HARD 1U
#define OS_FAULT_STACK 2U
typedef struct {
    uint32_t magic, kind, task_id, pc, lr, psr, sp, cfsr, hfsr, bfar;
} os_fault_t;
extern volatile os_fault_t os_fault;
void os_panic(uint32_t kind) __attribute__((noreturn));
uint32_t os_stack_used(uint8_t id); /* bytes, maximo historico */

void os_init(void);
int os_create_task(const char *name, void (*entry)(void), uint8_t priority);
void os_start(void) __attribute__((noreturn));
void os_schedule_from_pendsv(void);
void os_sleep(uint32_t ms);
void os_yield(void);
void os_exit(void) __attribute__((noreturn));
#define OS_KILL_DEFERRED 1
int os_kill(uint8_t id);
void os_kill_point(void);
void os_terminate_self(uint8_t reason) __attribute__((noreturn));
void os_task_end(os_tcb_t *p, uint8_t reason);
void os_stack_fail(os_tcb_t *p);
void os_tick(void);
void os_wake_one(os_tcb_t **list);
uint32_t os_uptime_ms(void);
uint8_t os_pid(void);
void sched_set_mode(uint8_t mode, uint8_t quantum);
void sched_ready_push(os_tcb_t *p);
os_tcb_t *sched_ready_pop(void);
os_tcb_t *sched_ready_peek(void);
void sched_ready_remove(os_tcb_t *p);
void sched_choose(void);
void wait_push(os_tcb_t **head, os_tcb_t *p);
os_tcb_t *wait_pop(os_tcb_t **head);
void wait_remove(os_tcb_t **head, os_tcb_t *p);
void os_unblock(os_tcb_t *p);

typedef struct os_sem {os_tcb_t *waiting; int16_t count;} os_sem_t;
void sem_init(os_sem_t *s, int16_t initial);
void sem_wait(os_sem_t *s);
void sem_signal(os_sem_t *s);
/* Transaccion protegida de kill: desde que se obtiene hasta release. */
void sem_wait_hold(os_sem_t *s);
void sem_signal_release(os_sem_t *s);

typedef struct os_mutex {os_tcb_t *waiting; os_tcb_t *owner;
                         struct os_mutex *held_next;} os_mutex_t;
void mutex_release_all(os_tcb_t *p);
void mutex_init(os_mutex_t *m);
int mutex_lock(os_mutex_t *m, uint32_t timeout_ms); /* 0 = infinito */
int mutex_unlock(os_mutex_t *m);
#define MUTEX_OK 0
#define MUTEX_TIMEOUT (-1)
#define MUTEX_ERROR (-2)

#define MQ_CAPACITY 16
typedef struct os_mq {
    os_tcb_t *rx_waiting, *tx_waiting;
    uint8_t data[MQ_CAPACITY], head, tail, count, capacity;
} os_mq_t;
void mq_init(os_mq_t *q, uint8_t capacity);
int mq_send(os_mq_t *q, uint8_t value);
int mq_recv(os_mq_t *q, uint8_t *value);

#define MEM_FIT_FIRST 0
#define MEM_FIT_BEST 1
typedef struct {uint32_t total, used, free_bytes, biggest_free;uint16_t blocks;} mem_stats_t;
void mem_init(void);
void *mem_alloc(size_t bytes);
void mem_free(void *p);
void mem_set_fit(uint8_t mode);
uint8_t mem_fit_mode(void);
void mem_stats(mem_stats_t *out);

void gpio_init(void);
void led_set(uint8_t which, bool on);
int button_read(void);
void uart_init(void);
void uart_putc(char c);
void uart_write(const char *s);
void uart_num(uint32_t n);
bool uart_line_read(char *out, uint8_t max);
void adc_init(void);
uint16_t adc_read(void);
void app_boot(void);
void prodcons_boot(void);
void prodcons_stats(void);
void shell_boot(void);
void run_bench(void);

#endif
