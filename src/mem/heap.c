#include "kernel/os.h"
#include "kernel/hw.h"

/* Heap independiente de las pilas estaticas: algoritmo first/best fit. */
typedef struct {uint32_t length, used;} block_t;
static uint8_t heap[OS_HEAP_SIZE] __attribute__((aligned(8)));
static uint8_t fit;
#define HSIZE (sizeof(block_t))

static block_t *next(block_t *p) {
    return (block_t *)((uint8_t *)(void *)p + HSIZE + p->length);
}

void mem_init(void) {
    block_t *p = (block_t *)(void *)heap;
    p->length = OS_HEAP_SIZE - HSIZE;
    p->used = 0;
    fit = MEM_FIT_FIRST;
}

void *mem_alloc(size_t count) {
    if (!count || count > OS_HEAP_SIZE - HSIZE) return NULL;
    count = (count + 7U) & ~(size_t)7U;
    uint32_t s = irq_save();
    block_t *best = NULL;
    for (block_t *p = (block_t *)(void *)heap;
         (uint8_t *)(void *)p < heap + OS_HEAP_SIZE; p = next(p)) {
        if (!p->used && p->length >= count &&
            (!best || (fit == MEM_FIT_BEST && p->length < best->length))) {
            best = p;
            if (fit == MEM_FIT_FIRST) break;
        }
    }
    if (!best) {irq_restore(s);return NULL;}
    if (best->length >= count + HSIZE + 8U) {
        block_t *split = (block_t *)((uint8_t *)(void *)best + HSIZE + count);
        split->length = best->length - count - HSIZE;
        split->used = 0;
        best->length = count;
    }
    best->used = 1;
    irq_restore(s);
    return (uint8_t *)(void *)best + HSIZE;
}

void mem_free(void *ptr) {
    if (!ptr) return;
    uint32_t s = irq_save();
    /* Busca puntero exacto; doble liberacion no altera la lista. */
    for (block_t *p = (block_t *)(void *)heap;
         (uint8_t *)(void *)p < heap + OS_HEAP_SIZE; p = next(p)) {
        if ((uint8_t *)(void *)p + HSIZE == (uint8_t *)ptr && p->used) {
            p->used = 0;
            break;
        }
    }
    block_t *p = (block_t *)(void *)heap;
    while ((uint8_t *)(void *)p < heap + OS_HEAP_SIZE) {
        block_t *n = next(p);
        if ((uint8_t *)(void *)n >= heap + OS_HEAP_SIZE) break;
        if (!p->used && !n->used) {
            p->length += HSIZE + n->length;
            continue;
        }
        p = n;
    }
    irq_restore(s);
}

void mem_set_fit(uint8_t mode) {
    uint32_t s = irq_save();
    fit = (mode == MEM_FIT_BEST) ? MEM_FIT_BEST : MEM_FIT_FIRST;
    irq_restore(s);
}
uint8_t mem_fit_mode(void) {return fit;}
void mem_stats(mem_stats_t *o) {
    uint32_t s = irq_save();
    o->total = OS_HEAP_SIZE;
    o->used = o->free_bytes = o->biggest_free = 0;
    o->blocks = 0;
    for (block_t *p = (block_t *)(void *)heap;
         (uint8_t *)(void *)p < heap + OS_HEAP_SIZE; p = next(p)) {
        o->blocks++;
        if (p->used) o->used += p->length + HSIZE;
        else {
            o->free_bytes += p->length + HSIZE;
            if (p->length > o->biggest_free) o->biggest_free = p->length;
        }
    }
    irq_restore(s);
}
