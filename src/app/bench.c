#include "kernel/os.h"
#include "kernel/hw.h"

#define BENCH_TICKS 30U
typedef struct {uint32_t response, turnaround, cpu; uint8_t finished;} result_t;
static result_t report_a, report_b;
static os_sem_t done;
static uint32_t begin;

static uint32_t cpu_time(uint8_t id) {
    uint32_t s = irq_save();
    uint32_t n = os_tasks[id].ticks_cpu;
    irq_restore(s);
    return n;
}
static void work(result_t *r) {
    uint8_t id = os_pid();
    uint32_t start_cpu = cpu_time(id);
    r->response = os_uptime_ms() - begin;
    while (cpu_time(id) - start_cpu < BENCH_TICKS) {
        __asm volatile("nop" ::: "memory");
    }
    r->turnaround = os_uptime_ms() - begin;
    r->cpu = cpu_time(id) - start_cpu;
    r->finished = 1;
    sem_signal(&done);
    os_exit();
}
static void bench_a(void) {work(&report_a);}
static void bench_b(void) {work(&report_b);}

static void print_one(const char *name,const result_t *r) {
    uint32_t wait = r->turnaround > r->cpu ? r->turnaround - r->cpu : 0;
    uart_write(name);
    uart_write(" respuesta=");uart_num(r->response);
    uart_write("ms espera~=");uart_num(wait);
    uart_write("ms retorno=");uart_num(r->turnaround);
    uart_write("ms CPU=");uart_num(r->cpu);
    uart_write(" ticks\r\n");
}

void run_bench(void) {
    sem_init(&done,0);
    report_a.finished = report_b.finished = 0;
    begin = os_uptime_ms();
    int a = os_create_task("benchA",bench_a,5);
    if (a < 0) {uart_write("ERROR: sin TCB disponible para A\r\n");return;}
    int b = os_create_task("benchB",bench_b,4);
    if (b < 0) {
        os_kill((uint8_t)a);
        uart_write("ERROR: sin TCB disponible para B\r\n");
        return;
    }
    sem_wait(&done);
    sem_wait(&done);
    uart_write("Benchmark: dos tareas CPU-bound, 30 ticks c/u\r\n");
    print_one("A(prio5)",&report_a);
    print_one("B(prio4)",&report_b);
    uart_write("espera aproximada = retorno - CPU; resolucion 1 ms\r\n");
}
