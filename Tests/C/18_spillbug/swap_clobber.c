/*
 * Test: swap register clobber bug.
 * When QBE's register allocator needs to resolve a parallel-move cycle,
 * it emits Oswap. The original B32P3 swap template used R12 as a scratch
 * register, which could clobber a live value in R12.
 *
 * This test has two sequential loops with enough register pressure that:
 * 1. A variable gets assigned to R12 and stays live across both loops.
 * 2. The register allocator emits a swap in the second loop path.
 * 3. The swap must not destroy the R12 value.
 */

void interrupt(void) {}

int data[16];
int log_a[4];
int log_b[4];
int log_count;

void init_data(void)
{
    int i;
    for (i = 0; i < 16; i++)
        data[i] = (i % 3 == 0) ? 1 : 0;
    log_count = 0;
}

int check_item(int idx)
{
    if (idx < 16)
        return data[idx];
    return 0;
}

void action_a(int sector)
{
    if (log_count < 4)
        log_a[log_count] = sector;
}

void action_b(int sector)
{
    if (log_count < 4)
        log_b[log_count] = sector;
    log_count++;
}

void report(char *label, int step, int total)
{
    (void)label;
    (void)step;
    (void)total;
}

int main(void)
{
    int total_items;
    int items_per_group_a;
    int items_per_group_b;
    int groups_a;
    int groups_b;
    int progress_total;
    int progress_step;
    int group, i, idx;
    int found;

    total_items = 16;
    items_per_group_a = 4;
    items_per_group_b = 8;
    groups_a = total_items / items_per_group_a;
    groups_b = total_items / items_per_group_b;
    progress_total = groups_a + groups_b;
    progress_step = 0;

    init_data();

    /* First loop: groups of 4 */
    for (group = 0; group < groups_a; group++)
    {
        found = 0;
        for (i = 0; i < items_per_group_a && !found; i++)
        {
            idx = group * items_per_group_a + i;
            if (idx < total_items && check_item(idx))
            {
                found = 1;
            }
        }
        if (found)
        {
            action_a(group);
        }
        progress_step++;
        report("loop-a", progress_step, progress_total);
    }

    /* Second loop: groups of 8 */
    for (group = 0; group < groups_b; group++)
    {
        found = 0;
        for (i = 0; i < items_per_group_b && !found; i++)
        {
            idx = group * items_per_group_b + i;
            if (idx < total_items && check_item(idx))
            {
                found = 1;
            }
        }
        if (found)
        {
            action_b(group);
        }
        progress_step++;
        report("loop-b", progress_step, progress_total);
    }

    /*
     * data dirty at: 0, 3, 6, 9, 12, 15
     * Loop A (groups of 4):
     *   group 0 (0-3):  dirty at 0,3 → action_a(0)
     *   group 1 (4-7):  dirty at 6   → action_a(1)
     *   group 2 (8-11): dirty at 9   → action_a(2)
     *   group 3 (12-15):dirty at 12,15→action_a(3)
     * log_a = 4 actions
     *
     * Loop B (groups of 8):
     *   group 0 (0-7):  dirty at 0,3,6 → action_b(0)
     *   group 1 (8-15): dirty at 9,12,15→action_b(1)
     * log_b = 2 actions, log_count = 2
     *
     * progress_step = 6 (4 from A + 2 from B)
     */
    return log_count * 16 + progress_step; // expected=0x26
    /* 2*16 + 6 = 38 = 0x26 */
}
