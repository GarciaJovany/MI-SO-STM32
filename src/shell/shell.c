#include "kernel/os.h"
#include "kernel/hw.h"

static bool eq(const char *a,const char *b) {
    while (*a && *b) if (*a++ != *b++) return false;
    return *a == *b;
}
static int number(const char *s) {
    if (!*s) return -1;
    int n = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        n = n * 10 + (*s++ - '0');
        if (n > 100000) return -1;
    }
    return n;
}
static void help(void) {
    uart_write("help, ps, mem, memtest, memfit first|best\r\n");
    uart_write("sched rr|prio|fcfs [quantum], quantum N\r\n");
    uart_write("run bench, run pc, led 0|1|2 0|1, adc, uptime, kill ID, ver\r\n");
}
static const char *state_name(uint8_t st) {
    switch(st) {
    case T_NUEVO:return "NUEVO";
    case T_LISTO:return "LISTO";
    case T_EJECUTANDO:return "EJECUTA";
    case T_BLOQUEADO:return "BLOQ";
    default:return "TERMIN";
    }
}
static void ps(void) {
    uart_write("ID NOMBRE ESTADO PRIORIDAD SP CPU% FIN CANARIO PILA_MAX\r\n");
    uint32_t up = os_uptime_ms();
    for (uint8_t i = 0;i < OS_MAX_TASKS;i++) {
        os_tcb_t *p=&os_tasks[i];
        if (p->state==T_TERMINADO && p->end_reason==0) continue;
        uint32_t s=irq_save();
        uint32_t cpu=p->ticks_cpu;
        uint8_t st=p->state,fin=p->end_reason;
        uintptr_t sp=(uintptr_t)p->sp;
        irq_restore(s);
        uint32_t pct = up ? (uint32_t)(((uint64_t)cpu*100U)/up) : 0;
        if (pct>100)pct=100;
        uart_num(i);uart_write(" ");uart_write(p->name);
        uart_write(" ");uart_write(state_name(st));
        uart_write(" ");uart_num(p->priority);
        uart_write(" ");uart_num((uint32_t)sp);
        uart_write(" ");uart_num(pct);
        uart_write(" ");uart_num(fin);
        uart_write(" ");uart_write(p->stack_base[0]==OS_STACK_CANARY?"OK":"OVFL");
        uart_write(" ");uart_num(os_stack_used(i));uart_write("/640");
        uart_write("\r\n");
    }
}
static void mem(void) {
    mem_stats_t m;mem_stats(&m);
    uart_write("Heap total=");uart_num(m.total);
    uart_write(" usado=");uart_num(m.used);
    uart_write(" libre=");uart_num(m.free_bytes);
    uart_write(" bloque mayor=");uart_num(m.biggest_free);
    uart_write(" bloques=");uart_num(m.blocks);
    uart_write(" modo=");uart_write(mem_fit_mode()==MEM_FIT_BEST?"best":"first");
    uart_write("\r\n");
}
static void memtest(void) {
    void *a=mem_alloc(80),*b=mem_alloc(112),*c=mem_alloc(80);
    uart_write((a&&b&&c)?"3 reservas OK; liberando\r\n":"Falta memoria\r\n");
    mem_free(b);mem_free(a);mem_free(c);
    mem();
}
static void shell_task(void) {
    char line[64];
    uart_write("MI-SO Shell listo. Escribe help.\r\nmiso> ");
    for (;;) {
        if (!uart_line_read(line,sizeof(line))) {os_sleep(3);continue;}
        char *a=line,*b=line,*c=line;
        while (*b && *b!=' ')b++;
        if (*b) { *b++=0; while(*b==' ')b++; }
        c=b;
        while(*c && *c!=' ')c++;
        if (*c) {*c++=0;while(*c==' ')c++;}
        if (eq(a,"help")) help();
        else if (eq(a,"ps")) ps();
        else if (eq(a,"mem")) mem();
        else if (eq(a,"memtest")) memtest();
        else if (eq(a,"memfit")) {
            if (eq(b,"best"))mem_set_fit(MEM_FIT_BEST);
            else if (eq(b,"first"))mem_set_fit(MEM_FIT_FIRST);
            else uart_write("Uso: memfit first|best\r\n");
            mem();
        } else if (eq(a,"sched")) {
            int mode=eq(b,"rr")?OS_MODE_RR:eq(b,"prio")?OS_MODE_PRIO:
                     eq(b,"fcfs")?OS_MODE_FCFS:-1;
            int q=number(c);
            if (mode<0 || (q!=-1 && (q<1 || q>255)))
                uart_write("Uso: sched rr|prio|fcfs [1..255]\r\n");
            else {sched_set_mode((uint8_t)mode,q<1?0:(uint8_t)q);uart_write("OK\r\n");}
        } else if (eq(a,"quantum")) {
            int q=number(b);
            if (q>0 && q<=255) {sched_set_mode(os_mode,(uint8_t)q);uart_write("OK\r\n");}
            else uart_write("Uso: quantum 1..255\r\n");
        } else if (eq(a,"run")) {
            if (eq(b,"bench"))run_bench();
            else if(eq(b,"pc"))prodcons_stats();
            else uart_write("Uso: run bench|pc\r\n");
        } else if (eq(a,"led")) {
            int id=number(b), on=number(c);
            if (id>=0 && id<=2 && on>=0 && on<=1) {led_set((uint8_t)id,on!=0);uart_write("OK\r\n");}
            else uart_write("Uso: led 0|1|2 0|1\r\n");
        } else if (eq(a,"adc")) {
            uart_write("PA5 ADC=");uart_num(adc_read());uart_write("/4095\r\n");
        } else if (eq(a,"uptime")) {
            uart_num(os_uptime_ms());uart_write(" ms\r\n");
        } else if (eq(a,"kill")) {
            int id=number(b);
            if (id == (int)os_pid()) {
                uart_write("No se permite matar el propio shell\r\n");
            } else if (id>=1 && id<OS_MAX_TASKS) {
                int r=os_kill((uint8_t)id);
                uart_write(r==0?"OK\r\n":r==OS_KILL_DEFERRED?
                    "Diferido: terminara al soltar recursos\r\n":"ID no activo\r\n");
            } else uart_write("Uso: kill ID (1..9)\r\n");
        } else if (eq(a,"ver")) {
            uart_write("MI-SO v2.0 Cortex-M3; HSI 8MHz, SysTick=1ms, PendSV\r\n");
        } else if (*a) uart_write("Comando no reconocido. help\r\n");
        uart_write("miso> ");
    }
}
void shell_boot(void) {os_create_task("shell",shell_task,2);}
