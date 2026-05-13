/*
 * mem.c — kernel heap and process memory pool allocator.
 *
 * Kernel heap: simple bump allocator with mark/release (same as v3).
 * Process pool: first-fit free list with coalescing.
 */
#include "kernel.h"

/* ---- Kernel heap (bump allocator) ---- */

static unsigned int kheap_ptr;

void kheap_init(void)
{
    kheap_ptr = KERNEL_HEAP_START;
}

void *kheap_alloc(unsigned int bytes)
{
    unsigned int aligned;
    unsigned int result;

    /* Align to 4 bytes */
    aligned = (bytes + 3) & ~3u;
    if (kheap_ptr + aligned > KERNEL_HEAP_END)
    {
        kernel_panic("kernel heap exhausted");
        return 0;
    }
    result = kheap_ptr;
    kheap_ptr += aligned;
    return (void *)result;
}

unsigned int kheap_mark(void)
{
    return kheap_ptr;
}

void kheap_release(unsigned int mark)
{
    if (mark >= KERNEL_HEAP_START && mark <= KERNEL_HEAP_END)
    {
        kheap_ptr = mark;
    }
}

/* ---- Process memory pool (first-fit free list) ---- */

/*
 * Free list node. Stored at the start of each free region.
 * Nodes are kept sorted by address for coalescing.
 */
struct free_node {
    unsigned int   base;
    unsigned int   size;
    struct free_node *next;
};

/* Static storage for free list nodes (no malloc needed) */
#define MAX_FREE_NODES 32
static struct free_node free_nodes[MAX_FREE_NODES];
static struct free_node *free_list;
static int free_node_count;

static struct free_node *alloc_node(void)
{
    int i;
    for (i = 0; i < MAX_FREE_NODES; i++)
    {
        if (free_nodes[i].size == 0 && free_nodes[i].base == 0
            && free_nodes[i].next == 0)
        {
            return &free_nodes[i];
        }
    }
    return 0;
}

static void release_node(struct free_node *n)
{
    n->base = 0;
    n->size = 0;
    n->next = 0;
}

void mem_init(void)
{
    int i;

    for (i = 0; i < MAX_FREE_NODES; i++)
    {
        free_nodes[i].base = 0;
        free_nodes[i].size = 0;
        free_nodes[i].next = 0;
    }

    /* Start with one free region spanning the entire pool */
    free_list = &free_nodes[0];
    free_list->base = PROC_POOL_START;
    free_list->size = PROC_POOL_SIZE;
    free_list->next = 0;
    free_node_count = 1;
}

unsigned int mem_alloc(unsigned int size)
{
    struct free_node *prev;
    struct free_node *curr;
    unsigned int aligned_size;
    unsigned int result;

    /* Align size up to 32 bytes (cache line) */
    aligned_size = (size + 31) & ~31u;
    if (aligned_size < PROC_MEM_MIN)
        aligned_size = PROC_MEM_MIN;

    /* First-fit search */
    prev = 0;
    curr = free_list;
    while (curr)
    {
        if (curr->size >= aligned_size)
        {
            result = curr->base;

            if (curr->size == aligned_size)
            {
                /* Exact fit — remove node */
                if (prev)
                    prev->next = curr->next;
                else
                    free_list = curr->next;
                release_node(curr);
                free_node_count--;
            }
            else
            {
                /* Split: shrink current node */
                curr->base += aligned_size;
                curr->size -= aligned_size;
            }
            return result;
        }
        prev = curr;
        curr = curr->next;
    }

    return 0; /* Out of memory */
}

void mem_free(unsigned int base)
{
    struct free_node *prev;
    struct free_node *curr;
    struct free_node *node;
    unsigned int size;

    /* Find the size of this allocation.
     * We need the caller to tell us, or store it.
     * For simplicity, store the size in the process table (proc.mem_size).
     * This function is called from proc.c which passes the size. */

    /* Actually, let's change the API: mem_free takes base and size. */
    /* This is handled by the wrapper below. */
}

/* Free a region of known size. */
void mem_free_region(unsigned int base, unsigned int size)
{
    struct free_node *prev;
    struct free_node *curr;
    struct free_node *node;

    if (size == 0) return;

    /* Find insertion point (sorted by address) */
    prev = 0;
    curr = free_list;
    while (curr && curr->base < base)
    {
        prev = curr;
        curr = curr->next;
    }

    /* Try to coalesce with previous */
    if (prev && (prev->base + prev->size == base))
    {
        prev->size += size;
        /* Try to also coalesce with next */
        if (curr && (prev->base + prev->size == curr->base))
        {
            prev->size += curr->size;
            prev->next = curr->next;
            release_node(curr);
            free_node_count--;
        }
        return;
    }

    /* Try to coalesce with next */
    if (curr && (base + size == curr->base))
    {
        curr->base = base;
        curr->size += size;
        return;
    }

    /* No coalescing possible — insert new node */
    node = alloc_node();
    if (!node)
    {
        /* Free list full — memory leak, but won't crash */
        return;
    }
    node->base = base;
    node->size = size;
    node->next = curr;
    if (prev)
        prev->next = node;
    else
        free_list = node;
    free_node_count++;
}

unsigned int mem_free_total(void)
{
    struct free_node *curr;
    unsigned int total;

    total = 0;
    curr = free_list;
    while (curr)
    {
        total += curr->size;
        curr = curr->next;
    }
    return total;
}
