/*
 * bdos_heap.h — BDOS kernel heap allocator.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BDOS_HEAP_H
#define BDOS_HEAP_H

void           bdos_heap_init(void);
unsigned int * bdos_heap_alloc(unsigned int size_words);
void           bdos_heap_free_all(void);

#endif /* BDOS_HEAP_H */
