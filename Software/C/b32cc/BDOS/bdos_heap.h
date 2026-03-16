#ifndef BDOS_HEAP_H
#define BDOS_HEAP_H

// Simple bump allocator for the kernel heap region.
// Allocations are freed all at once (e.g., when a user program exits).

// Initialize the heap allocator (call once at boot)
void bdos_heap_init();

// Allocate size_words words from the heap. Returns address, or 0 on failure.
unsigned int* bdos_heap_alloc(unsigned int size_words);

// Free all heap allocations (reset bump pointer to start)
void bdos_heap_free_all();

#endif // BDOS_HEAP_H
