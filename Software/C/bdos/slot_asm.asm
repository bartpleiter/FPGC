; slot_asm.asm — BDOS context switch assembly routines.
;
; Replaces all inline asm from the old BDOS slot.c, main.c, and syscall.c.
; These functions are called from C code using plain symbol names
; (no Label_ prefix) as emitted by the cproc/QBE toolchain.
;
; B32P3 calling convention:
;   r4-r7  = arguments
;   r1     = return value
;   r8-r11 = callee-saved
;   r13    = stack pointer (SP)
;   r14    = base pointer (BP / frame pointer)
;   r15    = return address (RA)
;   r0     = always zero
;
; HW stack: separate 256-entry CPU call stack via push/pop instructions.
; IO_HW_STACK_PTR (0x1F000004) reads/writes the HW stack pointer.
; IO_PC_BACKUP (0x1F000000) reads/writes the interrupt return PC.

.global bdos_halt
.global bdos_ccache
.global bdos_loop_save_sp_bp
.global bdos_save_and_switch
.global bdos_exec_trampoline
.global bdos_exec_trampoline_return
.global bdos_resume_trampoline
.global bdos_interrupt_redirect_pc
.global bdos_hw_stack_ptr_reset
.global bdos_hw_stack_ptr_write
.global bdos_syscall_exit

; ============================================================
; bdos_halt — halt the CPU (used by bdos_panic)
; ============================================================
bdos_halt:
  halt

; ============================================================
; bdos_ccache — flush the instruction cache
; ============================================================
bdos_ccache:
  ccache
  jumpr 0 r15

; ============================================================
; bdos_loop_save_sp_bp — save SP and BP into globals for loop recovery
; Called at the top of bdos_loop() before entering the while(1).
; ============================================================
bdos_loop_save_sp_bp:
  addr2reg bdos_loop_saved_sp r1
  write 0 r1 r13
  addr2reg bdos_loop_saved_bp r1
  write 0 r1 r14
  jumpr 0 r15

; ============================================================
; bdos_hw_stack_ptr_reset — set HW stack pointer to 0
; Used after kill or after saving user HW stack.
; ============================================================
bdos_hw_stack_ptr_reset:
  load32 0x1F000004 r1
  write 0 r1 r0
  jumpr 0 r15

; ============================================================
; bdos_hw_stack_ptr_write(value) — set HW stack pointer to r4
; ============================================================
bdos_hw_stack_ptr_write:
  load32 0x1F000004 r1
  write 0 r1 r4
  jumpr 0 r15

; ============================================================
; bdos_save_and_switch
;
; Entered via reti redirect when a program is being suspended or killed.
; At this point, Return_Interrupt has popped r1-r15 from HW stack,
; so CPU registers hold the user program's state.
; HW stack still contains: [trampoline:13 entries] [user call chain]
;
; This routine:
;  1. Undoes cproc function prologue to recover TRUE user register state
;  2. Saves user registers to bdos_suspend_temp_regs
;  3. Restores BDOS stack from bdos_loop_saved_sp/bp
;  4. Returns to C code (bdos_save_and_switch_c) for the rest of the logic
;
; NOTE: We cannot do the full save here because the C compiler needs
; to handle HW stack pop, slot state updates, and control flow.
; The C function bdos_save_and_switch_c() picks up after this point.
; ============================================================
bdos_save_and_switch:
  ; At entry, cproc generated a prologue for the C function that called us.
  ; We need to undo: the BP fixup and the SP decrement.
  ; read 0 r14 r14    ; restore old BP from saved frame
  ; add r13 N r13     ; undo SP decrement
  ; Since we arrive via reti redirect, the registers are the user's.
  ; We save them all raw without trying to undo prologue.

  ; Save all user registers to bdos_suspend_temp_regs
  push r1
  addr2reg bdos_suspend_temp_regs r1
  write 4 r1 r2
  write 8 r1 r3
  write 12 r1 r4
  write 16 r1 r5
  write 20 r1 r6
  write 24 r1 r7
  write 28 r1 r8
  write 32 r1 r9
  write 36 r1 r10
  write 40 r1 r11
  write 44 r1 r12
  write 48 r1 r13
  write 52 r1 r14
  write 56 r1 r15
  pop r2
  write 0 r1 r2

  ; Switch to BDOS stack
  addr2reg bdos_loop_saved_sp r1
  read 0 r1 r13
  addr2reg bdos_loop_saved_bp r1
  read 0 r1 r14

  ; Jump to C handler: bdos_save_and_switch_c()
  ; This function handles kill vs suspend, HW stack save, and return to bdos_loop
  jump bdos_save_and_switch_c

; ============================================================
; bdos_exec_trampoline — execute a user program
;
; Called from bdos_exec_program() after setting up:
;   bdos_run_entry = user entry address
;   bdos_run_stack = user stack top
;
; Saves BDOS registers on HW stack, switches to user stack,
; jumps to user program. When user returns, restores BDOS state.
; ============================================================
bdos_exec_trampoline:
  push r1
  push r2
  push r3
  push r4
  push r5
  push r6
  push r7
  push r8
  push r9
  push r10
  push r11
  push r12
  push r15

  ; Save BDOS stack pointers
  addr2reg bdos_run_saved_sp r11
  write 0 r11 r13
  addr2reg bdos_run_saved_bp r11
  write 0 r11 r14

  ; Load user entry and stack
  addr2reg bdos_run_entry r11
  read 0 r11 r11
  addr2reg bdos_run_stack r12
  read 0 r12 r13

  ; Set return address to our return label
  addr2reg bdos_exec_trampoline_return r15

  ; Jump to user program
  jumpr 0 r11

bdos_exec_trampoline_return:
  ; User program returned. Save return value.
  addr2reg bdos_run_retval r11
  write 0 r11 r1

  ; Restore BDOS stack
  addr2reg bdos_run_saved_sp r11
  read 0 r11 r13
  addr2reg bdos_run_saved_bp r11
  read 0 r11 r14

  ; Restore BDOS registers
  pop r15
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop r7
  pop r6
  pop r5
  pop r4
  pop r3
  pop r2
  pop r1

  ; Return to bdos_exec_program C code (cleanup)
  jumpr 0 r15

; ============================================================
; bdos_resume_trampoline — resume a suspended user program
;
; Called from bdos_resume_program() after copying saved state into:
;   bdos_suspend_temp_regs[0..14] = saved user registers
;   bdos_run_entry = saved user PC
;   bdos_slot_saved_hw_stack = saved user HW stack entries
;   bdos_slot_saved_hw_sp = number of user HW stack entries
;
; r4 = slot number (passed as argument)
;
; Steps:
;  1. Push BDOS registers (trampoline)
;  2. Save BDOS stack pointers
;  3. Push saved user HW stack entries (bottom-first)
;  4. Push saved user interrupt registers (r1-r15 in Int order)
;  5. Set IO_PC_BACKUP to saved user PC
;  6. Jump to Return_Interrupt
; ============================================================
bdos_resume_trampoline:
  ; Save BDOS registers to HW stack
  push r1
  push r2
  push r3
  push r4
  push r5
  push r6
  push r7
  push r8
  push r9
  push r10
  push r11
  push r12
  push r15

  ; Save BDOS stack pointers
  addr2reg bdos_run_saved_sp r11
  write 0 r11 r13
  addr2reg bdos_run_saved_bp r11
  write 0 r11 r14

  ; Push saved user HW stack entries (reverse order to restore original stack)
  ; user_sp entries saved in pop order: top-first at index 0
  ; Push from index user_sp-1 down to 0 to restore bottom-first
  addr2reg bdos_slot_saved_hw_sp r1
  addr2reg bdos_active_slot r2
  read 0 r2 r2
  shiftl r2 2 r2
  add r1 r2 r1
  read 0 r1 r3           ; r3 = user_sp (count)

  addr2reg bdos_slot_saved_hw_stack r1
  shiftl r2 8 r4
  add r1 r4 r1           ; r1 = base addr of saved HW stack for this slot

  ; r1 = base addr, r3 = user_sp (count), r4 = byte index (starts at (user_sp-1)*4)
  sub r3 1 r4
  shiftl r4 2 r4
bdos_resume_push_user_loop:
  beq r3 r0 28           ; if count == 0, skip to done (7 instructions * 4 bytes = 28)
  add r1 r4 r5
  read 0 r5 r5
  push r5
  sub r4 4 r4
  sub r3 1 r3
  jump bdos_resume_push_user_loop
bdos_resume_push_user_done:

  ; Push saved interrupt registers (user r1-r15)
  ; temp_regs[0]=r1, temp_regs[1]=r2, ..., temp_regs[14]=r15
  ; Must push in Int order: r1, r2, ..., r15
  ; So Return_Interrupt can pop correctly: r15, r14, ..., r1
  addr2reg bdos_suspend_temp_regs r1
  read 0 r1 r2
  push r2
  read 4 r1 r2
  push r2
  read 8 r1 r2
  push r2
  read 12 r1 r2
  push r2
  read 16 r1 r2
  push r2
  read 20 r1 r2
  push r2
  read 24 r1 r2
  push r2
  read 28 r1 r2
  push r2
  read 32 r1 r2
  push r2
  read 36 r1 r2
  push r2
  read 40 r1 r2
  push r2
  read 44 r1 r2
  push r2
  read 48 r1 r2
  push r2
  read 52 r1 r2
  push r2
  read 56 r1 r2
  push r2

  ; Set IO_PC_BACKUP to saved user PC
  addr2reg bdos_run_entry r1
  read 0 r1 r1
  load32 0x1F000000 r2
  write 0 r2 r1

  ; Jump to Return_Interrupt: pops r15..r1, reti -> user program
  jump Return_Interrupt

; ============================================================
; bdos_interrupt_redirect_pc — redirect PC_BACKUP to save_and_switch
;
; Called from interrupt() when a switch/kill is requested.
; Writes the address of bdos_save_and_switch into IO_PC_BACKUP
; so that when reti executes, the CPU jumps to save_and_switch.
; ============================================================
bdos_interrupt_redirect_pc:
  addr2reg bdos_save_and_switch r1
  load32 0x1F000000 r2
  write 0 r2 r1
  jumpr 0 r15

; ============================================================
; bdos_read_pc_backup — read the current PC_BACKUP value
; Returns the value in r1.
; ============================================================
.global bdos_read_pc_backup
bdos_read_pc_backup:
  load32 0x1F000000 r1
  read 0 r1 r1
  jumpr 0 r15

; ============================================================
; bdos_syscall_exit — handle SYSCALL_EXIT from user program
;
; This is called when a user program invokes EXIT syscall.
; It discards the user's HW stack down to the trampoline depth,
; then jumps directly to the trampoline return label.
;
; r4 = exit code (from syscall argument a1)
; ============================================================
bdos_syscall_exit:
  ; Save exit code as retval
  addr2reg bdos_run_retval r1
  write 0 r1 r4

  ; Discard user HW stack: set HW stack pointer to trampoline depth (13)
  load32 0x1F000004 r1
  load 13 r2
  write 0 r1 r2

  ; Jump to trampoline return
  jump bdos_exec_trampoline_return

; ============================================================
; bdos_save_temp_reg_via_pop — pop one HW stack entry into temp_regs[0]
; Used by the C save_and_switch_c to pop user HW stack entries one at a time.
; ============================================================
.global bdos_pop_to_temp
bdos_pop_to_temp:
  pop r1
  addr2reg bdos_suspend_temp_regs r2
  write 0 r2 r1
  jumpr 0 r15
