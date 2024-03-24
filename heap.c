#include "heap.h"



struct memory_manager_t manager;
uint8_t heap_loaded = 0;

int heap_setup(void) {
    void *heap_ptr = custom_sbrk(0);
    if (heap_ptr == (void *) -1) {
        return -1;
    }
    heap_loaded = 1;
    manager.heap_start = heap_ptr;
    manager.first_memory_chunk = NULL;
    manager.heap_size = 0;
    return 0;
}

void heap_clean(void) {
    manager.first_memory_chunk = NULL;
    custom_sbrk(-1 * ((long) manager.heap_size));
    manager.heap_start = NULL;
    manager.heap_size = 0;
}

size_t calculate_checksum(struct memory_chunk_t *chunk) {
    size_t checksum = 0;
    for (size_t i = 0; i < CHUNK_SIZE - sizeof(size_t); i++) {
        checksum += (*((uint8_t *) chunk + i)) * (i + 1);
    }
    return checksum;
}

void *heap_malloc(size_t size) {
    if (size == 0 || heap_validate())
        return NULL;

    if (manager.first_memory_chunk == NULL) {//freshly initialized heap
        while (size + FENCE_SIZE * 2 + CHUNK_SIZE > manager.heap_size) {
            if (custom_sbrk(BLOCK_SIZE) == (void *) -1) {
                return NULL;
            }
            manager.heap_size += BLOCK_SIZE;
        }
        struct memory_chunk_t *new_chunk = (struct memory_chunk_t *) manager.heap_start;
        manager.first_memory_chunk = new_chunk;
        new_chunk->size = size;
        new_chunk->free = 0;
        new_chunk->next = NULL;
        new_chunk->prev = NULL;
        memset((char *) new_chunk + CHUNK_SIZE, '#', FENCE_SIZE);
        memset((char *) new_chunk + CHUNK_SIZE + FENCE_SIZE + size, '#', FENCE_SIZE);
        new_chunk->checksum = calculate_checksum(new_chunk);
        return (void *) ((char *) new_chunk + CHUNK_SIZE + FENCE_SIZE);
    }

    struct memory_chunk_t *temp = manager.first_memory_chunk; //looking for freed chunks <= required size
    while (temp) {
        if (temp->free == 1 && temp->size >= size + FENCE_SIZE * 2) {
            temp->free = 0;
            temp->size = size;
            memset((char *) temp + CHUNK_SIZE, '#', FENCE_SIZE);
            memset((char *) temp + CHUNK_SIZE + FENCE_SIZE + size, '#', FENCE_SIZE);
            temp->checksum = calculate_checksum(temp);
            return (void *) ((char *) temp + sizeof(struct memory_chunk_t) + FENCE_SIZE);
        }
        temp = temp->next;
    }

    size_t mem_allocated = 0;                       //finding out how much of the heap is in use rn
    temp = manager.first_memory_chunk;
    while (temp) {
        mem_allocated += temp->size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2;
        temp = temp->next;
    }
    temp = manager.first_memory_chunk;
    while (temp->next) {
        mem_allocated += temp->size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2;
        temp = temp->next;
    }
    while ((mem_allocated + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2 + size) >
           (char *) manager.heap_start + manager.heap_size - (char *) temp - temp->size - FENCE_SIZE * 2 -
           CHUNK_SIZE) //setting the size accordingly
    {
        void *res = custom_sbrk(BLOCK_SIZE);
        if (res == (void *) -1 || custom_sbrk(0) <= res) {
            return NULL;
        }
        manager.heap_size += BLOCK_SIZE;
    }
    temp = manager.first_memory_chunk;
    while (temp != NULL)                            //going to the last element, preparing to place a new chunk
    {
        if (temp->next == NULL)
            break;
        temp = temp->next;
    }
    if ((size_t) (size + CHUNK_SIZE + FENCE_SIZE * 2) <= (size_t) ((char *) manager.heap_size + manager.heap_size -
                                                                   (char *) temp))  //maybe dont need this if statement? I am checking for memory b4 but idk
    {
        struct memory_chunk_t *new_chunk = (struct memory_chunk_t *) ((char *) temp + CHUNK_SIZE + temp->size +
                                                                      FENCE_SIZE * 2);
        new_chunk->free = 0;
        new_chunk->next = NULL;
        temp->next = new_chunk;
        temp->checksum = calculate_checksum(temp);
        new_chunk->size = size;
        new_chunk->prev = temp;
        new_chunk->checksum = calculate_checksum(new_chunk);
        memset((char *) new_chunk + CHUNK_SIZE, '#', FENCE_SIZE);
        memset((char *) new_chunk + CHUNK_SIZE + FENCE_SIZE + size, '#', FENCE_SIZE);
        return (void *) ((char *) new_chunk + CHUNK_SIZE + FENCE_SIZE);
    }
    return NULL;
}

void heap_free(void *memblock) {
    if (memblock == NULL || heap_validate())
        return;
    struct memory_chunk_t *chunk = (struct memory_chunk_t *) ((char *) memblock - CHUNK_SIZE - FENCE_SIZE);
    uint8_t found_chunk = 0;
    struct memory_chunk_t *temp = manager.first_memory_chunk;       //for chunk validation
    while (temp) {
        if (temp == chunk) {
            found_chunk = 1;
            break;
        }
        temp = temp->next;
    }
    if (!found_chunk) {
        return;
    }
    chunk->size += FENCE_SIZE * 2;
    chunk->free = 1;

    if (chunk->next)            //setting the size again in case there is free space between chunk and chunk->next
    {
        chunk->size = (size_t) ((char *) chunk->next - (char *) chunk - CHUNK_SIZE);
    }
    if (chunk->next && chunk->next->free)       //merge in the front
    {
        chunk->size += chunk->next->size + CHUNK_SIZE;
        chunk->next = chunk->next->next;
        if (chunk == manager.first_memory_chunk && chunk->next == NULL) {
            manager.first_memory_chunk = NULL;
            return;
        }
    }
    chunk->checksum = calculate_checksum(chunk);
    if (get_pointer_type(chunk)==pointer_control_block)
        return;
    if (get_pointer_type(chunk->prev)==pointer_control_block&& chunk->prev && chunk->prev->free)       //merge in the back
    {
        if (chunk->prev == manager.first_memory_chunk && chunk->next == NULL) {
            manager.first_memory_chunk = NULL;
            return;
        }
        chunk->prev->size += chunk->size + CHUNK_SIZE;
        chunk->prev->next = chunk->next;
        chunk->prev->checksum = calculate_checksum(chunk->prev);
        if (manager.first_memory_chunk && chunk->prev->prev == manager.first_memory_chunk && chunk->next == NULL) {
            manager.first_memory_chunk->next = NULL;
            manager.first_memory_chunk->next->checksum = calculate_checksum(manager.first_memory_chunk->next);
        }
    }

    temp = manager.first_memory_chunk;      //for checking the entire heap in case its free and merging didn't work
    while (temp) {
        if (temp->free == 0) {
            return;
        }
        temp = temp->next;
    }
    manager.first_memory_chunk = NULL;
}

void *heap_calloc(size_t number, size_t size) {
    if (number == 0 || size == 0)
        return NULL;
    void *out = heap_malloc(number * size);
    if (out == NULL)
        return NULL;
    memset(out, '\0', number * size);
    return out;
}
void *heap_realloc(void *memblock, size_t size) {
    if (heap_validate())
        return NULL;
    if (memblock == NULL) {
        return heap_malloc(size);
    }
    if (get_pointer_type(memblock)!=pointer_valid){
        return NULL;
    }
    if (size==0){
        heap_free(memblock);
        return NULL;
    }
    struct memory_chunk_t *chunk = (struct memory_chunk_t *) ((char *) memblock - FENCE_SIZE - CHUNK_SIZE);

    if (chunk->size>=size){
        chunk->size = size;
        memset((char *) chunk + size + FENCE_SIZE + CHUNK_SIZE, '#', FENCE_SIZE);
        chunk->checksum = calculate_checksum(chunk);
        return memblock;
    }
    if (chunk->next == NULL){
        while (size +chunk->size+CHUNK_SIZE+FENCE_SIZE*2 >= manager.heap_size) {
            void *res = custom_sbrk(BLOCK_SIZE);
            if (res == (void *) -1 || custom_sbrk(0) <= res) {
                return NULL;
            }
            manager.heap_size += BLOCK_SIZE;
        }
        chunk->size = size;
        memset((char *) chunk + size + FENCE_SIZE + CHUNK_SIZE, '#', FENCE_SIZE);
        chunk->checksum = calculate_checksum(chunk);
        return memblock;
    }
    size_t chunk_real_size = 0;
    size_t save_prev = chunk->size;
    if (chunk->next) {
        chunk_real_size = (size_t) ((char *) chunk->next - (char *) chunk - CHUNK_SIZE);
    }
    size_t needed_space = size - chunk_real_size;
    if (chunk->next && chunk->next->free && needed_space<=chunk->next->size){
        struct memory_chunk_t *save_next = chunk->next->next;
        memcpy((char *) chunk->next + needed_space, (char *) chunk->next, CHUNK_SIZE);
        chunk->size = size;
        chunk->next = (struct memory_chunk_t *) ((char *) chunk->next + needed_space);
        chunk->next->next = save_next;
        chunk->next->free = 1;
        if (chunk->next->next) {
            chunk->next->next->prev = chunk->next;
            chunk->next->next->checksum = calculate_checksum(chunk->next->next);
        }
        chunk->next->size = chunk->next->size - needed_space;
        memset((char *) chunk + chunk->size + FENCE_SIZE + CHUNK_SIZE, '#', FENCE_SIZE);
        chunk->checksum = calculate_checksum(chunk);
        chunk->next->checksum = calculate_checksum(chunk->next);
        return memblock;
    }
    if (chunk->next && chunk->next->free && needed_space<=chunk->next->size + CHUNK_SIZE){
        if (chunk->next->next) {
            chunk->next->next->prev = chunk;
            chunk->next->next->checksum = calculate_checksum(chunk->next->next);
        }
        chunk->next = chunk->next->next;
        chunk->size = size;
        memset((char *) chunk + chunk->size + FENCE_SIZE + CHUNK_SIZE, '#', FENCE_SIZE);
        chunk->checksum = calculate_checksum(chunk);
        return memblock;
    }





    char *out= heap_malloc(size);
    if(out==NULL) {
        return NULL;
    }

    memcpy(out,memblock,save_prev);

    heap_free(memblock);
    chunk->checksum = calculate_checksum(chunk);
    return out;

}

size_t heap_get_largest_used_block_size(void) {
    if (!heap_loaded || manager.heap_start == NULL || manager.first_memory_chunk == NULL || heap_validate())
        return 0;
    struct memory_chunk_t *chunk = manager.first_memory_chunk;
    size_t largest_chunk = 0;
    while (chunk) {
        if (!chunk->free && chunk->size > largest_chunk)
            largest_chunk = chunk->size;
        chunk = chunk->next;
    }
    return largest_chunk;
}

enum pointer_type_t get_pointer_type(const void *const pointer) {
    if (pointer == NULL)
        return pointer_null;
    if (heap_validate())
        return pointer_heap_corrupted;
    if ((const char *) pointer < (char *) manager.heap_start ||
        (const char *) pointer > (char *) manager.heap_start + manager.heap_size)
        return pointer_unallocated;
    struct memory_chunk_t *chunk = manager.first_memory_chunk;
    while (chunk) {
        if ((char *) chunk <= (const char *) pointer && (char *) chunk + CHUNK_SIZE > (const char *) pointer)
            return pointer_control_block;
        if (chunk->free == 0 && (((char *) chunk + CHUNK_SIZE <= (const char *) pointer &&
                                  (char *) chunk + CHUNK_SIZE + FENCE_SIZE > (const char *) pointer) ||
                                 ((char *) chunk + CHUNK_SIZE + FENCE_SIZE + chunk->size <= (const char *) pointer &&
                                  (char *) chunk + CHUNK_SIZE + FENCE_SIZE * 2 + chunk->size > (const char *) pointer)))
            return pointer_inside_fences;
        if ((char *) chunk + FENCE_SIZE + CHUNK_SIZE == (const char *) pointer) {
            if (chunk->free)
                return pointer_unallocated;
            return pointer_valid;

        }
        if ((char *) chunk + FENCE_SIZE + CHUNK_SIZE < (const char *) pointer &&
            (char *) chunk + CHUNK_SIZE + FENCE_SIZE + chunk->size > (const char *) pointer) {
            if (chunk->free)
                return pointer_unallocated;
            return pointer_inside_data_block;

        }
        chunk = chunk->next;
    }
    return pointer_unallocated;
}

int heap_validate(void) {
    if (heap_loaded == 0 || manager.heap_start == NULL) {
        return 2;
    }
    struct memory_chunk_t *chunk = manager.first_memory_chunk;
    while (chunk) {
        if (calculate_checksum(chunk) != chunk->checksum) {
            return 3;
        }
        chunk = chunk->next;
    }


    chunk = manager.first_memory_chunk;
    while (chunk) {
        if (chunk->free == 1) {
            chunk = chunk->next;
            continue;
        }
        for (int i = 0; i < FENCE_SIZE; i++) {
            if (*((char *) chunk + sizeof(struct memory_chunk_t) + i) != '#' ||
                *((char *) chunk + sizeof(struct memory_chunk_t) + i + FENCE_SIZE + chunk->size) != '#') {
                return 1;
            }
        }
        chunk = chunk->next;

    }
    return 0;
}
