#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "custom_unistd.h"
#ifndef UNTITLED_HEAP_H
#define UNTITLED_HEAP_H
enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};
struct memory_manager_t
{
    void *heap_start;
    size_t heap_size;
    struct memory_chunk_t *first_memory_chunk;
};
extern struct memory_manager_t manager;
extern uint8_t heap_loaded;


struct memory_chunk_t
{
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    size_t size;
    int free;
    size_t checksum;
};

int heap_setup(void);
void heap_clean(void);
void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t size);
void  heap_free(void* memblock);
size_t heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void* const pointer);
int heap_validate(void);
size_t calculate_checksum(struct memory_chunk_t* chunk);

#define CHUNK_SIZE sizeof(struct memory_chunk_t)
#define FENCE_SIZE 2
#define BLOCK_SIZE 0x1000
#endif