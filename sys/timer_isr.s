.globl   timer_isr
.align   4
 
timer_isr:
    cli
    pushq    %rax
    pushq    %rbx
    pushq    %rcx
    pushq    %rdx
    pushq    %rsi
    pushq    %rdi
    pushq    %r8
    pushq    %r9
    pushq    %r10
    pushq    %r11
    cld /* C code following the sysV ABI requires DF to be clear on function entry */
    call timer_irqhandler
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    popq    %rbx
    popq    %rax
    movb    $0x20, %al
    outb    %al, $0x20
    sti
    iretq
