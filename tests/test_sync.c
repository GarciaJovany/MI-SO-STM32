/* Semaforos, mutex, colas y kill: ejecuta el codigo real del kernel con una
 * fibra por tarea y ticks (expropiaciones) aleatorios reproducibles. */
#include "kernel/os.h"
#include "kernel/hw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
typedef LPVOID fiber_t;
#else
#include <ucontext.h>
typedef ucontext_t *fiber_t;
#endif

os_tcb_t os_tasks[OS_MAX_TASKS];
os_tcb_t *os_current;
volatile uint32_t os_ticks;
uint32_t host_primask, host_pending;

static fiber_t fibers[OS_MAX_TASKS], main_fiber;
static uint32_t canary[OS_MAX_TASKS];
static uint32_t seed, seed0, limit;
static int running, finished;
static const char *test_name;

#define CHECK(c) do {if (!(c)) {printf("FALLO %s seed=%u linea %d: %s\n", \
    test_name,(unsigned)seed0,__LINE__,#c);exit(1);}} while (0)

static uint32_t rnd(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
static void swap_to(fiber_t from, fiber_t to) {
#ifdef _WIN32
    (void)from; SwitchToFiber(to);
#else
    swapcontext(from, to);
#endif
}
static void leave(void) {swap_to(fibers[os_current->id], main_fiber);}

/* Se abren las interrupciones: quiza llega un tick y luego PendSV. */
void host_irq_open(void) {
    if (!running) {host_pending = 0;return;}
    if (rnd() % 4U == 0) {
        host_primask = 1; os_tick(); host_primask = 0;
        if (os_ticks > limit) {finished = 0;running = 0;leave();}
    }
    while (host_pending) {
        host_pending = 0;
        os_tcb_t *old = os_current;
        sched_choose();
        if (old != os_current) swap_to(fibers[old->id], fibers[os_current->id]);
    }
}
void os_panic(uint32_t k) {printf("panic %u\n",(unsigned)k);exit(1);}
void os_terminate_self(uint8_t reason) {
    uint32_t s = irq_save();
    os_task_end(os_current, reason);
    irq_restore(s);
    printf("FALLO: una tarea terminada siguio ejecutando\n");
    exit(1);
}
static void idle(void) {for (;;) {uint32_t s = irq_save();irq_restore(s);}}
#ifdef _WIN32
static VOID CALLBACK tramp(LPVOID p) {(void)p;os_current->entry();os_terminate_self(1);}
#else
static void tramp(void) {os_current->entry();os_terminate_self(1);}
#endif
static int spawned;
static int spawn(void (*f)(void), uint8_t prio) {
    int i = spawned++;
    os_tcb_t *p = &os_tasks[i];
    p->id = (uint8_t)i; p->entry = f; p->priority = prio;
    canary[i] = OS_STACK_CANARY; p->stack_base = &canary[i];
#ifdef _WIN32
    fibers[i] = CreateFiber(65536, tramp, NULL);
#else
    fibers[i] = calloc(1, sizeof(ucontext_t));
    getcontext(fibers[i]);
    fibers[i]->uc_stack.ss_sp = malloc(65536);
    fibers[i]->uc_stack.ss_size = 65536;
    makecontext(fibers[i], tramp, 0);
#endif
    if (i) {p->state = T_LISTO;sched_ready_push(p);}
    else p->state = T_EJECUTANDO;
    return i;
}
static void begin(const char *name, uint32_t s, uint8_t mode) {
    test_name = name; seed0 = s; seed = s * 2654435761U + 1U;
    running = 0; host_primask = host_pending = 0; finished = 0;
    while (sched_ready_pop()) {}
    memset(os_tasks, 0, sizeof os_tasks);
    for (int i = 0; i < OS_MAX_TASKS; i++) os_tasks[i].state = T_TERMINADO;
    os_mode = mode; os_quantum = 2; os_ticks = 0;
    os_current = &os_tasks[0];
    spawned = 0;
    spawn(idle, 255);
}
static int run(uint32_t max) {
    limit = max; running = 1;
    swap_to(main_fiber, fibers[0]);
    running = 0; host_primask = 0;
    for (int i = 0; i < OS_MAX_TASKS; i++) if (fibers[i]) {
#ifdef _WIN32
        DeleteFiber(fibers[i]);
#else
        free(fibers[i]->uc_stack.ss_sp); free(fibers[i]);
#endif
        fibers[i] = NULL;
    }
    return finished;
}
static void done(void) {irq_save();finished = 1;running = 0;leave();}

/* --- productor-consumidor con mutex --- */
static os_sem_t empty, full, sig;
static os_mutex_t lk, lk2;
static int inside, level, made, eaten, lives, rounds, other;
static void put(void) {
    sem_wait_hold(&empty); os_yield(); mutex_lock(&lk,0);
    CHECK(inside++ == 0); os_yield(); level++; made++; CHECK(level <= 4);
    inside--; mutex_unlock(&lk); sem_signal_release(&full);
}
static void get(void) {
    sem_wait_hold(&full); os_yield(); mutex_lock(&lk,0);
    CHECK(inside++ == 0); os_yield(); level--; eaten++; CHECK(level >= 0);
    inside--; mutex_unlock(&lk); sem_signal_release(&empty);
}
static void p100(void) {for (int i=0;i<100;i++) put();}
static void c100(void) {for (int i=0;i<100;i++) get(); if (++lives==2) done();}
static void pforever(void) {for (;;) put();}
static void cforever(void) {for (;;) get();}
static void pc_reset(void) {
    sem_init(&empty,4); sem_init(&full,0); mutex_init(&lk);
    inside = level = made = eaten = lives = 0;
}
/* kill en momento aleatorio: los supervivientes deben seguir trabajando */
static uint8_t victims[2];
static void killer(void) {
    os_sleep(5 + rnd() % 40U);
    for (int i = 0; i < 2; i++) {int r = os_kill(victims[i]);CHECK(r == 0 || r == 1);}
    os_sleep(300);
    for (int i = 0; i < 2; i++) {
        CHECK(os_tasks[victims[i]].state == T_TERMINADO);
        CHECK(os_tasks[victims[i]].end_reason == 3);
    }
    int m = made, e = eaten;
    os_sleep(300);
    CHECK(made > m + 5 && eaten > e + 5);
    /* Ningun hueco perdido: libres + ocupados + en transaccion = 4. */
    uint32_t s = irq_save();
    int slots = empty.count + full.count;
    for (int i = 1; i <= 6; i++) if (os_tasks[i].hold) slots++;
    CHECK(slots == 4);
    irq_restore(s);
    done();
}
/* --- aviso perdido: el despertado muere antes de consumir --- */
static void waiter(void) {sem_wait(&sig); other++; for (;;) os_sleep(50);}
static void lost(void) {
    os_sleep(20);
    uint32_t s = irq_save();
    sem_signal(&sig);
    CHECK(os_kill((uint8_t)(os_tasks[1].state == T_LISTO ? 1 : 2)) == 0);
    irq_restore(s);
    os_sleep(20);
    CHECK(other == 1); CHECK(sig.count == 0);
    done();
}
static os_mq_t q;
static int got;
static void rxer(void) {
    uint8_t v;
    mq_recv(&q,&v); CHECK(v == 77); got++;
    for (;;) os_sleep(50);
}
static void qlost(void) {
    os_sleep(20);
    uint32_t s = irq_save();
    mq_send(&q,77);
    CHECK(os_kill((uint8_t)(os_tasks[1].state == T_LISTO ? 1 : 2)) == 0);
    irq_restore(s);
    os_sleep(20);
    CHECK(got == 1 && q.count == 0);
    done();
}
static void txer(void) {mq_send(&q,9); got++; for (;;) os_sleep(50);}
static void qtxlost(void) {
    uint8_t v;
    os_sleep(20);               /* cola de 1 llena y dos emisores dormidos */
    uint32_t s = irq_save();
    mq_recv(&q,&v);
    CHECK(os_kill((uint8_t)(os_tasks[1].state == T_LISTO ? 1 : 2)) == 0);
    irq_restore(s);
    os_sleep(20);
    CHECK(got == 1 && q.count == 1);
    done();
}
static int next_tx, next_rx;
static void mqtx(void) {for (int i=0;i<200;i++) mq_send(&q,(uint8_t)next_tx++);}
static void mqrx(void) {
    uint8_t v;
    for (int i=0;i<200;i++) {mq_recv(&q,&v);CHECK(v==(uint8_t)next_rx);next_rx++;}
    done();
}
/* --- mutex --- */
static void holder(void) {
    CHECK(mutex_lock(&lk,0) == MUTEX_OK);
    CHECK(mutex_lock(&lk,0) == MUTEX_ERROR);
    os_sleep(60); CHECK(mutex_unlock(&lk) == MUTEX_OK); rounds = 1;
    for (;;) os_sleep(50);
}
static void timed(void) {
    os_sleep(10);
    CHECK(mutex_unlock(&lk) == MUTEX_ERROR);
    CHECK(mutex_lock(&lk,15) == MUTEX_TIMEOUT);
    CHECK(mutex_lock(&lk,500) == MUTEX_OK);
    CHECK(mutex_unlock(&lk) == MUTEX_OK && !lk.owner && !os_current->hold);
    done();
}
static void killholder(void) {
    os_sleep(20);
    CHECK(os_kill(1) == OS_KILL_DEFERRED);
    CHECK(os_tasks[1].state != T_TERMINADO);
    CHECK(mutex_lock(&lk,500) == MUTEX_OK);   /* lo solto antes de morir */
    os_sleep(20);
    CHECK(os_tasks[1].state == T_TERMINADO && os_tasks[1].end_reason == 3);
    CHECK(rounds == 0 && os_kill(1) == -2 && os_kill(0) == -1);
    done();
}
static void nested(void) {
    mutex_lock(&lk,0); mutex_lock(&lk2,0); os_sleep(60);
    mutex_unlock(&lk); os_sleep(60); mutex_unlock(&lk2);
    rounds = 1;
    for (;;) os_sleep(9);
}
static void nested_kill(void) {
    os_sleep(20); CHECK(os_kill(1) == OS_KILL_DEFERRED);
    CHECK(mutex_lock(&lk,500) == MUTEX_OK && os_tasks[1].state != T_TERMINADO);
    CHECK(mutex_lock(&lk2,500) == MUTEX_OK);
    os_sleep(20); CHECK(os_tasks[1].state == T_TERMINADO && rounds == 0);
    done();
}
static void two_exit(void) {mutex_lock(&lk,0); mutex_lock(&lk2,0); os_sleep(40);}
static void two_canary(void) {
    mutex_lock(&lk,0); mutex_lock(&lk2,0); os_sleep(40);
    canary[1] = 0;
    for (;;) os_sleep(5);
}
static void two_wait(void) {
    os_sleep(20);
    CHECK(mutex_lock(&lk,500) == MUTEX_OK && mutex_lock(&lk2,500) == MUTEX_OK);
    CHECK(os_tasks[1].state == T_TERMINADO);
    done();
}

#define RUN(max) do {if (!run(max)) {printf("FALLO %s seed=%u modo=%d: bloqueado\n", \
    test_name,(unsigned)seed0,mode);exit(1);} count++;} while (0)

int main(void) {
    unsigned count = 0;
#ifdef _WIN32
    main_fiber = ConvertThreadToFiber(NULL);
#else
    static ucontext_t mc; main_fiber = &mc;
#endif
    for (uint32_t s = 1; s <= 300; s++) for (int m = 0; m < 2; m++) {
        uint8_t mode = m ? OS_MODE_PRIO : OS_MODE_RR;
        begin("prodcons",s,mode); pc_reset();
        spawn(p100,3); spawn(p100,4); spawn(c100,3); spawn(c100,5); RUN(60000);
        CHECK(made == 200 && eaten == 200 && level == 0);
        for (int k = 0; k < 2; k++) {
            begin(k ? "kill consumidores" : "kill productores",s,mode); pc_reset();
            for (int i = 0; i < 3; i++) spawn(pforever,3);
            for (int i = 0; i < 3; i++) spawn(cforever,3);
            victims[0] = (uint8_t)(k ? 4 : 1); victims[1] = (uint8_t)(k ? 5 : 2);
            spawn(killer,2); RUN(5000);
        }
        begin("aviso sem",s,mode); sem_init(&sig,0); other = 0;
        spawn(waiter,3); spawn(waiter,3); spawn(lost,2); RUN(2000);
        begin("aviso cola rx",s,mode); mq_init(&q,4); got = 0;
        spawn(rxer,3); spawn(rxer,3); spawn(qlost,2); RUN(2000);
        begin("aviso cola tx",s,mode); mq_init(&q,1); q.count = 1; got = 0;
        spawn(txer,3); spawn(txer,3); spawn(qtxlost,2); RUN(2000);
        begin("cola fifo",s,mode); mq_init(&q,3); next_tx = next_rx = 0;
        spawn(mqtx,3); spawn(mqrx,3); RUN(60000);
        begin("mutex timeout",s,mode); mutex_init(&lk); rounds = 0;
        spawn(holder,3); spawn(timed,3); RUN(3000);
        begin("kill dueno",s,mode); mutex_init(&lk); rounds = 0;
        spawn(holder,3); spawn(killholder,3); RUN(3000);
        begin("kill anidado",s,mode); mutex_init(&lk); mutex_init(&lk2); rounds = 0;
        spawn(nested,3); spawn(nested_kill,3); RUN(3000);
        begin("exit con mutex",s,mode); mutex_init(&lk); mutex_init(&lk2);
        spawn(two_exit,3); spawn(two_wait,3); RUN(3000);
        CHECK(os_tasks[1].end_reason == 1);
        begin("canario con mutex",s,mode); mutex_init(&lk); mutex_init(&lk2);
        spawn(two_canary,3); spawn(two_wait,3); RUN(3000);
        CHECK(os_tasks[1].end_reason == 2);
    }
    printf("OK: semaforos, colas, mutex y kill (%u ejecuciones)\n",count);
    return 0;
}
