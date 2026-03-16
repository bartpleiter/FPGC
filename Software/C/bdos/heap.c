#include "bdos.h"

static unsigned int bdos_heap_next = MEM_HEAP_START;

void bdos_heap_init(void)
{
  bdos_heap_next = MEM_HEAP_START;
}

unsigned int *bdos_heap_alloc(unsigned int size_words)
{
  unsigned int addr;

  if (size_words == 0)
  {
    return (unsigned int *)0;
  }

  addr = bdos_heap_next;

  if (addr + size_words > MEM_HEAP_END)
  {
    return (unsigned int *)0;
  }

  bdos_heap_next = addr + size_words;
  return (unsigned int *)addr;
}

void bdos_heap_free_all(void)
{
  bdos_heap_next = MEM_HEAP_START;
}
