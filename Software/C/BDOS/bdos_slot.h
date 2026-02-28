#ifndef BDOS_SLOT_H
#define BDOS_SLOT_H

// Slot status values
#define BDOS_SLOT_STATUS_EMPTY     0
#define BDOS_SLOT_STATUS_RUNNING   1
#define BDOS_SLOT_STATUS_SUSPENDED 2

// No active slot
#define BDOS_SLOT_NONE -1

// Maximum program name length
#define BDOS_SLOT_NAME_MAX 32

// Per-slot state arrays
extern int bdos_slot_status[MEM_SLOT_COUNT];
extern char bdos_slot_name[MEM_SLOT_COUNT][BDOS_SLOT_NAME_MAX];

// Multitasking saved state
extern unsigned int bdos_slot_saved_pc[MEM_SLOT_COUNT];
extern unsigned int bdos_slot_saved_regs[MEM_SLOT_COUNT * 15];
extern unsigned int bdos_slot_saved_hw_sp[MEM_SLOT_COUNT];
extern unsigned int bdos_slot_saved_hw_stack[MEM_SLOT_COUNT * 256];

// Currently active slot
extern int bdos_active_slot;

// Multitasking switch/kill request flags
extern int bdos_switch_target;   // -1 = no request, -2 = shell, 0-7 = switch to slot
extern int bdos_kill_requested;  // 1 = kill running program

// Temp register save area
extern unsigned int bdos_suspend_temp_regs[15];

// BDOS loop stack state
extern unsigned int bdos_loop_saved_sp;
extern unsigned int bdos_loop_saved_bp;

// Program execution globals (used by trampoline and EXIT syscall)
extern int bdos_run_retval;

// Slot management functions
void bdos_slot_init();
int bdos_slot_alloc();
void bdos_slot_free(int slot);
unsigned int bdos_slot_entry_addr(int slot);
unsigned int bdos_slot_stack_addr(int slot);
int bdos_exec_program(char* resolved_path);
void bdos_resume_program(int slot);
void bdos_save_and_switch();

#endif // BDOS_SLOT_H
