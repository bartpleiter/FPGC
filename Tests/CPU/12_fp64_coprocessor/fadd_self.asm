Main:
    ; Test fadd writing back to same register (fd == fa)
    ; This is important for Mandelbrot: fadd f2, f2, f0 (z_re += c_re)
    ; f0 = {0, 100}, f2 = {0, 50}
    ; fadd f2 f0 -> f2 = f2 + f0 = {0, 150}

    load 0 r1
    load 100 r2
    fld r1 r2 r0             ; f0 = {0, 100}

    load 0 r1
    load 50 r2
    fld r1 r2 r2             ; f2 = {0, 50}

    ; fadd r2 r0 r2 -> f2 = f2 + f0 = {0, 150}
    fadd r2 r0 r2

    fstlo r2 r0 r3           ; r3 = 150

    add r3 r0 r15            ; expected=150

    halt
