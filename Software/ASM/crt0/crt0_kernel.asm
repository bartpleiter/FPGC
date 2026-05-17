; crt0_kernel.asm — Startup code for BDOS v4 kernel
;
; Based on crt0_bdos.asm but with v4 memory layout:
;   Main stack:      0x107FFC  (KERNEL_STACK_TOP)
;   Syscall stack:   0x10BFFC  (KERNEL_SYSCALL_STACK)
;   Interrupt stack: 0x10FFFC  (KERNEL_INT_STACK)
;
; The C program must define:
;   int main()                  — kernel entry point
;   void interrupt()            — interrupt handler
;   int syscall_dispatch()      — syscall dispatcher (args in r4-r7, return in r1)

.text

Main:
    load32 0 r14                ; initialize frame pointer
    load32 0x107FFC r13         ; v4 kernel main stack
    savpc r15
    add r15 12 r15
    jump main                   ; call C main()
    ; main() returned
    load32 0x1C000000 r2
    write 0 r2 r1              ; send return value over UART
    halt

Int:
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
    push r13
    push r14
    push r15

    load32 0x10FFFC r13         ; v4 interrupt stack
    load32 0 r14
    savpc r15
    add r15 12 r15
    jump interrupt              ; call C interrupt()

.global Return_Interrupt
Return_Interrupt:
    pop r15
    pop r14
    pop r13
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

    reti
    halt                        ; should not get here

Syscall:
    ; Save user registers to proc struct (via current_proc_regs_ptr).
    ; Push r1 to HW stack temporarily; use r1 to hold the pointer.
    push r1
    addr2reg current_proc_regs_ptr r1
    read 0 r1 r1                 ; r1 = &saved_regs[0]

    ; Save r2-r15 to proc struct (offset = reg_number * 4)
    write 8 r1 r2
    write 12 r1 r3
    write 16 r1 r4
    write 20 r1 r5
    write 24 r1 r6
    write 28 r1 r7
    write 32 r1 r8
    write 36 r1 r9
    write 40 r1 r10
    write 44 r1 r11
    write 48 r1 r12
    write 52 r1 r13
    write 56 r1 r14
    write 60 r1 r15

    ; Recover user's r1 from HW stack and save to proc struct
    pop r2
    write 4 r1 r2                ; saved_regs[1] = user's r1

    ; Switch to syscall stack
    load32 0x10BFFC r13
    sub r13 16 r13              ; allocate arg save area for C callee
    load32 0 r14
    ; r4-r7 still hold syscall args (write is read-only for source regs)
    addr2reg Return_Syscall r11
    or r0 r11 r15
    jump syscall_dispatch       ; call C dispatcher (args in r4=num, r5=a1, r6=a2, r7=a3)
    halt                        ; should not get here

Return_Syscall:
    ; r1 = return value from syscall_dispatch
    ; Check if the process was blocked by the syscall
    addr2reg proc_was_blocked r11
    read 0 r11 r11
    beq r11 r0 Return_Syscall_normal  ; if NOT blocked (0), skip to normal return
    jump Return_Syscall_blocked

Return_Syscall_normal:
    ; === Normal return: restore user regs from proc struct ===
    addr2reg current_proc_regs_ptr r11
    read 0 r11 r11                ; r11 = &saved_regs[0]

    ; Restore r2-r10, r12-r15 from proc struct
    read 8 r11 r2
    read 12 r11 r3
    read 16 r11 r4
    read 20 r11 r5
    read 24 r11 r6
    read 28 r11 r7
    read 32 r11 r8
    read 36 r11 r9
    read 40 r11 r10
    ; skip r11 (offset 44) — still using as pointer
    read 48 r11 r12
    read 52 r11 r13               ; user SP
    read 56 r11 r14               ; user FP
    read 60 r11 r15               ; user return address

    ; Restore r11 LAST (clobbers pointer)
    read 44 r11 r11

    ; Return to user (r1 = return value from syscall_dispatch)
    jumpr 0 r15
    halt                        ; should not get here

Return_Syscall_blocked:
    ; Process was blocked by the syscall.
    ; User state already saved in proc struct by Syscall entry.
    ; Clear the blocked flag.
    addr2reg proc_was_blocked r11
    write 0 r11 r0                ; proc_was_blocked = 0
    ; HW stack depth is still 13 (context_enter's saves; Syscall
    ; entry's push/pop of r1 was balanced).
    ; Jump to context_enter_return to restore kernel state.
    jump context_enter_return

; --- Assembly helpers for kernel ---

.global kernel_halt
kernel_halt:
    halt

.global kernel_ccache
kernel_ccache:
    ccache
    jumpr 0 r15

; Save current SP and BP for kernel_loop re-entry
; (used when returning from context_enter)
.global kernel_loop_sp
.global kernel_loop_bp
kernel_loop_sp:
    .dw 0
kernel_loop_bp:
    .dw 0

.global kernel_loop_save_sp_bp
kernel_loop_save_sp_bp:
    addr2reg kernel_loop_sp r11
    write 0 r11 r13
    addr2reg kernel_loop_bp r11
    write 0 r11 r14
    jumpr 0 r15

; ============================================================
; context_enter()
;
; Saves kernel state, loads user state from proc struct
; (via current_proc_regs_ptr), and jumps to user code.
;
; saved_regs[15] is used as the jump target:
;   - Fresh process: saved_regs[15] = entry point
;   - Resumed process: saved_regs[15] = return address after syscall
;
; Must set current_proc_regs_ptr before calling.
; Returns (via context_enter_return) when the user process
; exits or blocks.
; ============================================================
.global context_enter
context_enter:
    ; Save kernel registers to HW stack (13 entries)
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

    ; Save kernel SP/BP to globals
    addr2reg kernel_loop_sp r11
    write 0 r11 r13
    addr2reg kernel_loop_bp r11
    write 0 r11 r14

    ; Load user state from proc struct
    addr2reg current_proc_regs_ptr r11
    read 0 r11 r11               ; r11 = &saved_regs[0]

    ; Load r1-r10, r12-r15
    read 4 r11 r1
    read 8 r11 r2
    read 12 r11 r3
    read 16 r11 r4
    read 20 r11 r5
    read 24 r11 r6
    read 28 r11 r7
    read 32 r11 r8
    read 36 r11 r9
    read 40 r11 r10
    ; skip r11 (offset 44) — still using as pointer
    read 48 r11 r12
    read 52 r11 r13               ; user SP
    read 56 r11 r14               ; user FP
    read 60 r11 r15               ; user PC (entry or resume address)

    ; Load r11 LAST (clobbers pointer)
    read 44 r11 r11

    ; Flush instruction cache
    ccache

    ; Jump to user — r15 holds the target address
    jumpr 0 r15

.global context_enter_return
context_enter_return:
    ; User program finished (natural return or EXIT syscall jump)
    ; Save user's r1 (exit code from main) to global
    addr2reg context_enter_retval r11
    write 0 r11 r1

    ; Restore kernel SP/BP
    addr2reg kernel_loop_sp r11
    read 0 r11 r13
    addr2reg kernel_loop_bp r11
    read 0 r11 r14

    ; Restore kernel registers from HW stack (13 entries)
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

    ; Return to C caller (shell_execute)
    jumpr 0 r15

; ============================================================
; syscall_exit_to_kernel()
;
; Called from EXIT syscall handler after proc_exit() cleanup.
; Resets HW stack to context_enter depth (13 entries) and
; jumps to context_enter_return to restore kernel state.
; Does not return.
; ============================================================
.global syscall_exit_to_kernel
syscall_exit_to_kernel:
    ; Reset HW stack pointer to context_enter depth (13 entries)
    load32 0x1F000004 r1
    load 13 r2
    write 0 r1 r2

    ; Jump to context_enter_return
    jump context_enter_return

; Return value from context_enter (r1 from user — legacy, kept for compat)
.global context_enter_retval
context_enter_retval:
    .dw 0

; Pointer to current process's saved_regs[0] array.
; Set by C code before context_enter() and used by Syscall entry/return
; to save/restore user register state.
.global current_proc_regs_ptr
current_proc_regs_ptr:
    .dw 0

; Flag set by C code (proc_waitpid, proc_sleep_ms) when a syscall
; blocks the current process.  Checked by Return_Syscall to exit
; to kernel loop instead of returning to user.
.global proc_was_blocked
proc_was_blocked:
    .dw 0

; Stub for context_switch — Phase 4 will implement real preemptive multitasking
.global context_switch
context_switch:
    jumpr 0 r15
