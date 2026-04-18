#ifndef BDOS_HEAP_H
#define BDOS_HEAP_H

void           bdos_heap_init(void);
unsigned int * bdos_heap_alloc(unsigned int size_bytes);
void           bdos_heap_free_all(void);

/*
 * Mark/release: snapshot the current bump pointer and later rewind
 * to it. Used by proc_spawn to scope per-process arena allocations
 * to the lifetime of a single child process.
 */
unsigned int   bdos_heap_mark(void);
void           bdos_heap_release(unsigned int mark);

#endif /* BDOS_HEAP_H */
