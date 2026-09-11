.text
.global pause_worker
.type pause_worker,%function
pause_worker:
    stp x19,x20,[sp,#-32]!
    mrs x20,tpidr_el0
    str x20,[sp,#16]
    msr tpidr_el0,x0
    mov x19,#0x1919
    movi v0.16b,#0x5a
    mov x2,#0
.global pause_loop_begin
pause_loop_begin:
    ldr w1,[x0]
    cbnz w1,1f
    add x2,x2,#1
    str x2,[x0,#8]
    b pause_loop_begin
.global pause_loop_end
pause_loop_end:
1:
    ldr x20,[sp,#16]
    msr tpidr_el0,x20
    ldp x19,x20,[sp],#32
    ret
.size pause_worker,.-pause_worker
