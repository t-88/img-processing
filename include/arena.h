#ifndef ARENA_H
#define ARENA_H

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef struct Arena Arena;
typedef struct Arena {
    void* base;
    size_t size;
    size_t offset;
} Arena;



#define ARENA_DEFAULT_ALIGNMENT (sizeof(void*))
#define arena_alloc_of(a,T,n) ((T*)(arena_alloc(a,sizeof(T) * n)))


void arena_init(Arena* a,size_t size);
void arena_dispose(Arena* a);
void* arena_alloc(Arena* arena,size_t size);
void arena_reset(Arena* arena);

#endif //ARENA_H




#ifdef ARENA_IMPLEMENTATION_H

void* arena_alloc(Arena* arena,size_t size) {
    if(!size) return 0;
    
    const size_t _alignment_offset = arena->offset % ARENA_DEFAULT_ALIGNMENT;
    assert(arena->offset + _alignment_offset + size <= arena->size && "Failed to allocate memory, arena doesnt have enough memory");

    arena->offset += _alignment_offset;
    void* addr = arena->base + arena->offset;
    arena->offset += size;

    return addr; 
}

void arena_reset(Arena* arena) { 
    arena->offset = 0;
}

void arena_dispose(Arena* a) {
    assert(a->base != 0 && "Failed to dispose arena, your base is null");
    free(a->base);
}

void arena_init(Arena* a,size_t size) {
    a->offset = 0;
    a->size = size;
    a->base = malloc(size);
    assert(a->base != 0 && "Failed to init arena, you have no memory");
}


#endif // ARENA_IMPLEMENTATION_H