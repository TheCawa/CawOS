#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

typedef struct {
    size_t total_size;
    size_t used_size;
    size_t free_size;
    int total_blocks;
    int used_blocks;
    int free_blocks;
    size_t largest_free_block;
} heap_stats_t;

void heap_init();
void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);
void heap_get_stats(heap_stats_t* stats);
unsigned short get_total_memory();

#endif
