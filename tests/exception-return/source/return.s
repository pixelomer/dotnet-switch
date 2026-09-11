// Restricted capability probe: recover only the one explicit load below.
.section .text.probe_fault_roundtrip,"ax",%progbits
.global probe_fault_roundtrip
.type probe_fault_roundtrip,%function
probe_fault_roundtrip:
    stp x19,x20,[sp,#-32]!
    str d8,[sp,#16]
    mov x20,x0
    mrs x9,tpidr_el0
    str x9,[sp,#24]
    msr tpidr_el0,x1
    str x1,[x20,#40]
    mov x9,#0x99
    mov x19,#0x1919
    mov x1,#0x1111
    movi v0.16b,#0x5a
    mov x0,xzr
    cmp x9,x9
.global probe_fault_instruction
probe_fault_instruction:
    ldr x0,[x0]
.global probe_fault_resume
probe_fault_resume:
    mrs x2,nzcv
    str x0,[x20]
    str x9,[x20,#8]
    str x19,[x20,#16]
    str x1,[x20,#24]
    str x2,[x20,#32]
    mrs x2,tpidr_el0
    str x2,[x20,#48]
    ldr x2,[sp,#24]
    msr tpidr_el0,x2
    str q0,[x20,#64]
    ldr d8,[sp,#16]
    ldp x19,x20,[sp],#32
    ret
.size probe_fault_roundtrip,.-probe_fault_roundtrip

// The kernel stores x0..x8/lr/sp/pc/pstate in the user exception frame.
// Use only those scratch registers; x9..x29 and SIMD registers stay untouched.
// Do not call C or any other SVC while handling this probe's exception.
.section .text.__libnx_exception_entry,"ax",%progbits
.global __libnx_exception_entry
.type __libnx_exception_entry,%function
__libnx_exception_entry:
    cbz x1,.Lunknown
    ldr x2,[x1,#0x58]
    adrp x3,probe_fault_instruction
    add x3,x3,:lo12:probe_fault_instruction
    cmp x2,x3
    b.ne .Lunknown
    ldr x2,[x1,#0x70]
    cbnz x2,.Lunknown
    adrp x3,probe_fault_count
    add x3,x3,:lo12:probe_fault_count
.Lcount:
    ldxr x2,[x3]
    add x2,x2,#1
    stxr w4,x2,[x3]
    cbnz w4,.Lcount
    mrs x2,tpidr_el0
    str x2,[x20,#56]
    adrp x2,probe_fault_resume
    add x2,x2,:lo12:probe_fault_resume
    str x2,[x1,#0x58]
    mov x2,#0x42
    str x2,[x1]
    mov w0,wzr
    svc #0x28
    b .
.Lunknown:
    mov w0,#0xf801
    svc #0x28
    b .
.size __libnx_exception_entry,.-__libnx_exception_entry
