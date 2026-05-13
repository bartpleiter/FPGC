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
    ; Save all registers except r1 (return value) to hardware stack
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

    load32 0x10BFFC r13         ; v4 syscall stack
    sub r13 16 r13              ; allocate arg save area for C callee
    load32 0 r14
    ; Use r11 (temp reg) to avoid clobbering r4-r7 (syscall args)
    addr2reg Return_Syscall r11
    or r0 r11 r15
    jump syscall_dispatch       ; call C dispatcher (args in r4=num, r5=a1, r6=a2, r7=a3)
    halt                        ; should not get here

Return_Syscall:
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

    jumpr 0 r15                 ; return to user program
    halt                        ; should not get here

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
; context_enter(entry_addr, stack_top)
;   r4 = user entry address
;   r5 = user stack top
;
; Saves kernel state, jumps to user program.
; Returns when user program exits (via natural return through
; crt0 pop r15/jumpr, or via EXIT syscall → syscall_exit_to_kernel).
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

    ; Set user stack pointer
    or r0 r5 r13

    ; Set user frame pointer to 0
    load32 0 r14

    ; Set return address — user crt0 will push this and pop it on main() return
    addr2reg context_enter_return r15

    ; Flush instruction cache (binary was just loaded into data memory)
    ccache

    ; Jump to user program entry
    jumpr 0 r4

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

; Return value from context_enter (r1 from user main())
.global context_enter_retval
context_enter_retval:
    .dw 0

; Stub for context_switch — Phase 2 will implement real multitasking
.global context_switch
context_switch:
    jumpr 0 r15
