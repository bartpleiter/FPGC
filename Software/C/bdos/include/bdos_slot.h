#ifndef BDOS_SLOT_H
#define BDOS_SLOT_H

#include "bdos_mem_map.h"

/* Per-slot state (shared with other modules via extern) */
extern int           bdos_active_slot;
extern int           bdos_slot_status[MEM_SLOT_COUNT];
extern char          bdos_slot_name[MEM_SLOT_COUNT][32];
extern unsigned int  bdos_slot_saved_pc[MEM_SLOT_COUNT];
extern unsigned int  bdos_slot_saved_regs[MEM_SLOT_COUNT * 15];
extern unsigned int  bdos_slot_saved_hw_sp[MEM_SLOT_COUNT];
extern unsigned int  bdos_slot_saved_hw_stack[MEM_SLOT_COUNT * 256];

/* Scheduling state (set by HID ISR, checked by interrupt handler) */
extern int bdos_switch_target;
extern int bdos_kill_requested;

/* BDOS main loop stack save points (for recovery after suspend) */
extern unsigned int bdos_loop_saved_sp;
extern unsigned int bdos_loop_saved_bp;

/* Execution trampoline state */
extern unsigned int bdos_run_entry;
extern unsigned int bdos_run_stack;
extern unsigned int bdos_run_saved_sp;
extern unsigned int bdos_run_saved_bp;
extern int          bdos_run_retval;

/* Suspend temp registers (used during context switch) */
extern unsigned int bdos_suspend_temp_regs[15];

/* Slot management API */
void bdos_slot_init(void);
int  bdos_slot_alloc(void);
void bdos_slot_free(int slot);
unsigned int bdos_slot_entry_addr(int slot);
unsigned int bdos_slot_stack_addr(int slot);
int  bdos_exec_program(char *resolved_path);
void bdos_resume_program(int slot);

/* Context switch entries (defined in slot_asm.asm) */
extern void bdos_save_and_switch(void);

#endif /* BDOS_SLOT_H */
