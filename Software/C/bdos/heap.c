#include "bdos.h"

static unsigned int bdos_heap_next = MEM_HEAP_START;

void bdos_heap_init(void)
{
  bdos_heap_next = MEM_HEAP_START;
}

unsigned int *bdos_heap_alloc(unsigned int size_bytes)
{
  unsigned int addr;

  if (size_bytes == 0)
  {
    return (unsigned int *)0;
  }

  /* Round up to word alignment so subsequent allocations stay 4-aligned. */
  size_bytes = (size_bytes + 3u) & ~3u;

  addr = bdos_heap_next;

  if (addr + size_bytes > MEM_HEAP_END)
  {
    return (unsigned int *)0;
  }

  bdos_heap_next = addr + size_bytes;
  return (unsigned int *)addr;
}

void bdos_heap_free_all(void)
{
  bdos_heap_next = MEM_HEAP_START;
}

unsigned int bdos_heap_mark(void)
{
  return bdos_heap_next;
}

void bdos_heap_release(unsigned int mark)
{
  if (mark >= MEM_HEAP_START && mark <= bdos_heap_next)
    bdos_heap_next = mark;
}
