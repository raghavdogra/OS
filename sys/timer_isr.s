.globl   timer_isr
.align   4
 
timer_isr:
    pushq    %rax
    pushq    %rcx
    pushq    %rdx
    pushq    %r8
    pushq    %r9
    pushq    %r10
    pushq    %r11
    cld /* C code following the sysV ABI requires DF to be clear on function entry */
    call timer_irqhandler
    popq    %rax
    popq    %rcx
    popq    %rdx
    popq    %r8
    popq    %r9
    popq    %r10
    popq    %r11

    iret