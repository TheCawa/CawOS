#include "kernel/memory.h"
#include "libc/util.h"
#include "drivers/screen.h"
#define HEAP_START 0x00800000

typedef struct block {
    size_t size;
    int free;
    struct block* next;
} block_t;

block_t* free_list = NULL;
static int heap_initialized = 0;
static size_t heap_size = 0;

void heap_init() {
    if (heap_initialized) return;
    size_t probe_addr = HEAP_START;
    while (probe_addr < 0xC0000000) {
        volatile uint32_t* test_ptr = (volatile uint32_t*)(probe_addr + 1024 * 1024 - 4);
        uint32_t backup = *test_ptr;
        *test_ptr = 0x55AA55AA;
        __asm__ volatile("wbinvd" : : : "memory");
        if (*test_ptr != 0x55AA55AA) {
            break;
        }
        *test_ptr = 0xAA55AA55;
        __asm__ volatile("wbinvd" : : : "memory");
        if (*test_ptr != 0xAA55AA55) {
            break; 
        }
        *test_ptr = backup;
        probe_addr += 1024 * 1024;
    }  
    heap_size = probe_addr - HEAP_START;
    if (heap_size < 1024 * 1024) {
        heap_initialized = 0;
        return;
    }
    free_list = (block_t*)HEAP_START;
    free_list->size = heap_size - sizeof(block_t);
    free_list->free = 1;
    free_list->next = NULL;
    heap_initialized = 1;
}

void* malloc(size_t size) {
    if (!heap_initialized) {
        return NULL;
    }
    if (size == 0) return NULL;
    size = (size + 7) & ~7;
    block_t* current = free_list;
    while (current) {
        if (current->free && current->size >= size) {
            if (current->size >= size + sizeof(block_t) + 8) {
                block_t* new_block = (block_t*)((char*)current + sizeof(block_t) + size);
                new_block->size = current->size - size - sizeof(block_t);
                new_block->free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->free = 0;
            return (void*)((char*)current + sizeof(block_t));
        }
        current = current->next;
    }

    return NULL; // Out of memory
}

void free(void* ptr) {
    if (!ptr) return;
    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    block->free = 1;
    if (block->next && block->next->free) {
        block->size += sizeof(block_t) + block->next->size;
        block->next = block->next->next;
    }
    block_t* current = free_list;
    block_t* prev = NULL;

    while (current && current != block) {
        prev = current;
        current = current->next;
    }

    if (prev && prev->free && current == block) {
        prev->size += sizeof(block_t) + block->size;
        prev->next = block->next;
    }
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    if (block->size >= size) {
        return ptr; 
    }

    void* new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        free(ptr);
    }
    return new_ptr;
}

void heap_get_stats(heap_stats_t* stats) {
    if (!stats) return;
    stats->total_size = heap_size;
    stats->used_size = 0;
    stats->free_size = 0;
    stats->total_blocks = 0;
    stats->used_blocks = 0;
    stats->free_blocks = 0;
    stats->largest_free_block = 0;

    if (!heap_initialized) return;

    block_t* current = free_list;
    while (current) {
        stats->total_blocks++;

        if (current->free) {
            stats->free_blocks++;
            stats->free_size += current->size;
            if (current->size > stats->largest_free_block) {
                stats->largest_free_block = current->size;
            }
        } else {
            stats->used_blocks++;
            stats->used_size += current->size;
        }
        current = current->next;
    }
}

unsigned short get_total_memory() {
    if (!heap_initialized) {
        heap_init();
    }
    size_t total_bytes = HEAP_START + heap_size;
    return (unsigned short)(total_bytes / (1024 * 1024));
}